#include "view/disasm.h"

#include <cstdio>

namespace tuxdsk {

namespace {

#include "view/z80tables.inc"

void putHex(std::string& text, size_t at, int value, int digits)
{
    static const char kDigits[] = "0123456789ABCDEF";
    while (digits)
        text[at++] = kDigits[(value >> (4 * --digits)) & 0x0F];
}

// "(IX+nn)" with what it is moved by: a minus when the byte is one.
void putDisplacement(std::string& text, uint8_t byte)
{
    const size_t at = text.find("nn");
    if (at == std::string::npos)
        return;
    if (byte < 0x80) {
        putHex(text, at, byte, 2);
    } else {
        putHex(text, at, 0x100 - byte, 2);
        if (at > 0 && text[at - 1] == '+')
            text[at - 1] = '-';
    }
}

}  // namespace

std::string disassembly(std::span<const uint8_t> content, int base)
{
    std::string listing;
    const size_t size = content.size();
    // Past the end of the file the bytes are 0.
    const auto byteAt = [&](size_t at) -> uint8_t { return at < size ? content[at] : uint8_t{0}; };

    char field[24];
    for (size_t at = 0; at < size;) {
        const size_t start = at;
        const uint8_t first = byteAt(at++);
        uint8_t second = 0;
        uint8_t third = 0;
        const char* name = kPlain[first];
        std::string text;
        bool indexedBit = false;
        if (first == 0xCB) {
            second = byteAt(at++);
            name = kCB[second];
        } else if (first == 0xED) {
            second = byteAt(at++);
            name = kED[second];
        } else if (first == 0xDD || first == 0xFD) {
            second = byteAt(at++);
            if (second == 0xCB) {
                // The displacement comes before the instruction's own byte.
                const uint8_t displacement = byteAt(at++);
                third = byteAt(at++);
                name = (first == 0xDD ? kDDCB : kFDCB)[third];
                if (name) {
                    text = name;
                    putDisplacement(text, displacement);
                    indexedBit = true;
                }
            } else {
                name = (first == 0xDD ? kDD : kFD)[second];
            }
        }

        if (!name) {
            std::snprintf(field, sizeof field, "%02X %02X %02X ????", first, second, third);
            text = field;
        } else if (!indexedBit) {
            text = name;
            size_t place = text.find("nnnn");
            if (place != std::string::npos) {
                putHex(text, place, byteAt(at) | byteAt(at + 1) << 8, 4);
                at += 2;
            } else if ((place = text.find("eeee")) != std::string::npos) {
                const int offset = static_cast<int8_t>(byteAt(at++));
                putHex(text, place, (base + static_cast<int>(at) + offset) & 0xFFFF, 4);
            } else {
                // A byte, or two: LD (IX+nn),nn.
                while ((place = text.find("nn")) != std::string::npos)
                    putHex(text, place, byteAt(at++), 2);
            }
        }

        std::snprintf(field, sizeof field, "%04X ", (base + static_cast<int>(start)) & 0xFFFF);
        listing += field;
        for (size_t each = start; each < at; ++each) {
            std::snprintf(field, sizeof field, "%02X ", byteAt(each));
            listing += field;
        }
        for (size_t pad = at - start; pad < 5; ++pad)
            listing += "   ";
        listing += text;
        listing += '\n';
    }
    return listing;
}

}  // namespace tuxdsk
