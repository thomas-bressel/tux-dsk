#include "view/text.h"

#include <cstdio>

namespace tuxdsk {

std::string cpcText(std::string_view listing, bool french)
{
    std::string text;
    text.reserve(listing.size());
    for (const char character : listing) {
        const unsigned char c = static_cast<unsigned char>(character);
        if (french) {
            const char* letter = c == '@'    ? "à"
                                 : c == '\\' ? "ç"
                                 : c == '{'  ? "é"
                                 : c == '|'  ? "ù"
                                 : c == '}'  ? "è"
                                             : nullptr;
            if (letter) {
                text += letter;
                continue;
            }
        }
        text += c == '\n' || c == '\t' || (c >= ' ' && c < 0x7F) ? character : '?';
    }
    return text;
}

std::string asciiListing(std::span<const uint8_t> content)
{
    std::string listing;
    for (const uint8_t byte : content) {
        if (byte == 0x1A)
            break;
        // A CPC ends its lines with CR and LF: the LF is the line's end here.
        if (byte != '\r')
            listing += static_cast<char>(byte);
    }
    return listing;
}

std::string wrapped(std::string_view listing, int width)
{
    std::string text;
    int column = 0;
    for (const char c : listing) {
        if (c == '\n') {
            column = 0;
        } else if (column == width) {
            text += '\n';
            column = 0;
        }
        text += c;
        if (c != '\n')
            ++column;
    }
    return text;
}

std::string hexDump(std::span<const uint8_t> content, int base)
{
    // Four figures for where a byte is, more when the file needs them.
    const int digits = base + static_cast<int>(content.size()) > 0x10000 ? 6 : 4;
    std::string dump;
    char field[16];
    for (size_t at = 0; at < content.size(); at += 16) {
        std::snprintf(field, sizeof field, "#%0*X: ", digits, base + static_cast<int>(at));
        dump += field;
        std::string characters;
        for (size_t column = 0; column < 16; ++column) {
            if (at + column >= content.size()) {
                dump += "   ";
                continue;
            }
            const uint8_t byte = content[at + column];
            std::snprintf(field, sizeof field, "%02X ", byte);
            dump += field;
            characters += byte > ' ' && byte < 0x7F ? static_cast<char>(byte) : '.';
        }
        dump += "| " + characters + '\n';
    }
    return dump;
}

}  // namespace tuxdsk
