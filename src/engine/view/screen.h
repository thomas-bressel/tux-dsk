#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// A file's bytes as what the CPC's screen makes of them: ManageDsk's viewer
// in "video memory" mode, with OCP Art Studio's files as it knows them.
//
// The bytes are shown as the screen has them, a line every &800 bytes and
// the next row of characters 80 bytes on, or "flat": one line after the
// other, so many bytes each, from where one says. Flat is how a sprite or a
// window (.WIN) is kept.

namespace tuxdsk {

struct CpcColour {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
};

// One of the CPC's 27 colours, by the number BASIC's INK gives it.
CpcColour cpcColour(int number);
// The same number, from the one the Gate Array is given (&40 to &5F).
int inkOfHardware(uint8_t hardware);

using Inks = std::array<uint8_t, 16>;
// The inks a CPC starts with.
Inks defaultInks();

struct ScreenView {
    int mode = 1;       // 0 to 2
    bool flat = false;
    int columns = 80;   // bytes across
    int lines = 200;
    int offset = 0;     // where the picture starts in the bytes
};

// A picture as pens: `width` is in the pixels of mode 2, eight to a byte,
// whatever the mode, so that a pixel of mode 0 is four of them.
struct ScreenPicture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pens;
};

ScreenPicture screenPicture(std::span<const uint8_t> memory, const ScreenView& view);

// What a mode and inks a file says it is to be seen with.
struct ScreenLook {
    int mode = 1;
    Inks inks{};
};

// A screen's bytes as the CPC's memory has them: unpacked if they are in
// OCP's packing ("MJH" blocks). `content` is the file without its header.
std::vector<uint8_t> screenMemory(std::span<const uint8_t> content);
// The look a screen saved by ConvImgCpc carries with it, if it is one.
std::optional<ScreenLook> lookInScreen(std::span<const uint8_t> memory);
// The look an OCP palette (.PAL) gives. `content` is the file without its
// header.
std::optional<ScreenLook> lookOfPalette(std::span<const uint8_t> content);

// An OCP window (.WIN): its bytes, line after line, then five bytes that
// say its width in bits and its height.
struct OcpWindow {
    std::vector<uint8_t> data;
    int lineBytes = 0;
    int height = 0;
};

std::optional<OcpWindow> readWindow(std::span<const uint8_t> content);
// The window's file, without a header: the lines then the five bytes.
std::vector<uint8_t> windowFile(std::span<const uint8_t> data, int lineBytes, int height);

// Where OCP Art Studio loads a window, and the most a window can be: 255
// lines high, and as wide as its width in bits can say on 16 bits.
constexpr uint16_t kWindowAddress = 0x4000;
constexpr int kWindowMaxHeight = 255;
constexpr int kWindowMaxLineBytes = 0xFFFF / 8;
// The window's file of `count` tiles of `width` bytes by `height` lines,
// one after the other in `tiles`, as Find Graphics gives them: the tiles
// side by side, as it shows them in a row, line after line. Nothing if
// they do not make a window.
std::optional<std::vector<uint8_t>> tilesWindow(std::span<const uint8_t> tiles, int count, int width, int height);

}  // namespace tuxdsk
