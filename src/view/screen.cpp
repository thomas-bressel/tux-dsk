#include "view/screen.h"

#include <algorithm>
#include <cstring>

namespace tuxdsk {

namespace {

constexpr size_t kScreenSize = 0x4000;
constexpr size_t kWindowFooter = 5;

// The Gate Array's number for each of the 27 colours, less &40.
const char kHardware[] = "TDU\\X]LEMVFW^@_NGORBSZY[JCK";

}  // namespace

CpcColour cpcColour(int number)
{
    // Three levels of each of blue, red and green, in that order of weight.
    static const uint8_t kLevels[3] = {0x00, 0x7F, 0xFF};
    if (number < 0 || number > 26)
        number = 0;
    return {kLevels[number / 3 % 3], kLevels[number / 9], kLevels[number % 3]};
}

int inkOfHardware(uint8_t hardware)
{
    const char wanted = static_cast<char>(0x40 | (hardware & 0x1F));
    for (int ink = 0; ink < 27; ++ink)
        if (kHardware[ink] == wanted)
            return ink;
    // Five numbers give a colour another number gives too.
    switch (wanted) {
    case 0x41: return 13;
    case 0x48: return 7;
    case 0x49: return 25;
    case 0x50: return 1;
    case 0x51: return 19;
    }
    return 0;
}

Inks defaultInks()
{
    return {1, 24, 20, 6, 26, 0, 2, 8, 10, 12, 14, 16, 18, 22, 24, 16};
}

ScreenPicture screenPicture(std::span<const uint8_t> memory, const ScreenView& view)
{
    ScreenPicture picture;
    const int columns = std::clamp(view.columns, 1, 256);
    const int lines = std::clamp(view.lines, 1, 1024);
    picture.width = columns * 8;
    picture.height = lines;
    picture.pens.assign(static_cast<size_t>(picture.width) * static_cast<size_t>(lines), 0);

    size_t address = 0;
    for (int line = 0; line < lines; ++line) {
        uint8_t* out = &picture.pens[static_cast<size_t>(line) * static_cast<size_t>(picture.width)];
        for (int column = 0; column < columns; ++column) {
            const size_t at = static_cast<size_t>(std::max(view.offset, 0)) + address + static_cast<size_t>(column);
            const uint8_t byte = at < memory.size() ? memory[at] : uint8_t{0};
            switch (view.mode) {
            case 0: {
                const uint8_t left = static_cast<uint8_t>((byte >> 7 & 1) | (byte >> 3 & 1) << 1 | (byte >> 5 & 1) << 2
                                                          | (byte >> 1 & 1) << 3);
                const uint8_t right = static_cast<uint8_t>((byte >> 6 & 1) | (byte >> 2 & 1) << 1 | (byte >> 4 & 1) << 2
                                                           | (byte & 1) << 3);
                std::fill_n(out, 4, left);
                std::fill_n(out + 4, 4, right);
                break;
            }
            case 2:
                for (int bit = 0; bit < 8; ++bit)
                    out[bit] = byte >> (7 - bit) & 1;
                break;
            default:
                for (int pixel = 0; pixel < 4; ++pixel) {
                    const uint8_t pen =
                        static_cast<uint8_t>((byte >> (7 - pixel) & 1) | (byte >> (3 - pixel) & 1) << 1);
                    out[pixel * 2] = out[pixel * 2 + 1] = pen;
                }
                break;
            }
            out += 8;
        }
        if (view.flat) {
            address += static_cast<size_t>(columns);
        } else {
            // The next line of the row of characters, or the first of the
            // next row.
            address += 0x800;
            if (address >= kScreenSize)
                address -= kScreenSize - 80;
        }
    }
    return picture;
}

std::vector<uint8_t> screenMemory(std::span<const uint8_t> content)
{
    if (content.size() < 3 || std::memcmp(content.data(), "MJH", 3) != 0)
        return {content.begin(), content.end()};
    // Blocks, each "MJH", the length it unpacks to, and its bytes: a byte
    // of 1 announces a count (0 for 256) and the value to repeat.
    std::vector<uint8_t> memory;
    size_t at = 0;
    const auto marked = [&] { return at + 3 <= content.size() && std::memcmp(&content[at], "MJH", 3) == 0; };
    while (memory.size() < kScreenSize && marked() && at + 5 <= content.size()) {
        at += 3;
        const size_t length = static_cast<size_t>(content[at] | content[at + 1] << 8);
        at += 2;
        size_t made = 0;
        while (made < length && at < content.size() && !marked()) {
            uint8_t value = content[at++];
            size_t count = 1;
            if (value == 1 && at + 2 <= content.size()) {
                count = content[at] ? content[at] : 256;
                value = content[at + 1];
                at += 2;
            }
            for (; count > 0 && made < length; --count, ++made)
                memory.push_back(value);
        }
    }
    return memory;
}

std::optional<ScreenLook> lookInScreen(std::span<const uint8_t> memory)
{
    // ConvImgCpc leaves the code that shows the screen in it, and the mode
    // and the inks after that.
    static const uint8_t kMark[4] = {0x3A, 0xD0, 0xD7, 0xCD};
    if (memory.size() < 0x17D1 + 16 || std::memcmp(&memory[0x7D0], kMark, sizeof kMark) != 0)
        return std::nullopt;
    ScreenLook look;
    look.mode = memory[0x17D0] & 3;
    for (size_t ink = 0; ink < 16; ++ink)
        look.inks[ink] = static_cast<uint8_t>(std::min<int>(memory[0x17D1 + ink], 26));
    return look;
}

std::optional<ScreenLook> lookOfPalette(std::span<const uint8_t> content)
{
    // The mode, two bytes for the colours that change, then twelve Gate
    // Array numbers for each ink: the first is the ink's colour.
    if (content.size() < 3 + 16 * 12)
        return std::nullopt;
    ScreenLook look;
    look.mode = content[0] & 3;
    for (size_t ink = 0; ink < 16; ++ink)
        look.inks[ink] = static_cast<uint8_t>(inkOfHardware(content[3 + ink * 12]));
    return look;
}

std::optional<OcpWindow> readWindow(std::span<const uint8_t> content)
{
    if (content.size() <= kWindowFooter)
        return std::nullopt;
    const uint8_t* footer = &content[content.size() - kWindowFooter];
    OcpWindow window;
    window.lineBytes = ((footer[1] | footer[2] << 8) + 7) / 8;
    window.height = footer[3];
    if (window.lineBytes == 0 || window.height == 0)
        return std::nullopt;
    window.data = screenMemory(content.first(content.size() - kWindowFooter));
    return window;
}

std::vector<uint8_t> windowFile(std::span<const uint8_t> data, int lineBytes, int height)
{
    std::vector<uint8_t> file(data.begin(), data.end());
    file.resize(static_cast<size_t>(lineBytes) * static_cast<size_t>(height), 0);
    const int bits = lineBytes * 8;
    file.insert(file.end(), {0, static_cast<uint8_t>(bits & 0xFF), static_cast<uint8_t>(bits >> 8),
                             static_cast<uint8_t>(height), 3});
    return file;
}

}  // namespace tuxdsk
