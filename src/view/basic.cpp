#include "view/basic.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace tuxdsk {

namespace {

// The keywords, from token &80.
const char* const kKeywords[0x80] = {
    "AFTER", "AUTO", "BORDER", "CALL", "CAT", "CHAIN", "CLEAR", "CLG", "CLOSEIN", "CLOSEOUT", "CLS", "CONT",
    "DATA", "DEF", "DEFINT", "DEFREAL", "DEFSTR", "DEG", "DELETE", "DIM", "DRAW", "DRAWR", "EDIT", "ELSE",
    "END", "ENT", "ENV", "ERASE", "ERROR", "EVERY", "FOR", "GOSUB", "GOTO", "IF", "INK", "INPUT",
    "KEY", "LET", "LINE", "LIST", "LOAD", "LOCATE", "MEMORY", "MERGE", "MID$", "MODE", "MOVE", "MOVER",
    "NEXT", "NEW", "ON", "ON BREAK", "ON ERROR GOTO", "SQ", "OPENIN", "OPENOUT", "ORIGIN", "OUT", "PAPER", "PEN",
    "PLOT", "PLOTR", "POKE", "PRINT", "'", "RAD", "RANDOMIZE", "READ", "RELEASE", "REM", "RENUM", "RESTORE",
    "RESUME", "RETURN", "RUN", "SAVE", "SOUND", "SPEED", "STOP", "SYMBOL", "TAG", "TAGOFF", "TROFF", "TRON",
    "WAIT", "WEND", "WHILE", "WIDTH", "WINDOW", "WRITE", "ZONE", "DI", "EI", "FILL", "GRAPHICS", "MASK",
    "FRAME", "CURSOR", "#E2", "ERL", "FN", "SPC", "STEP", "SWAP", "#E8", "#E9", "TAB", "THEN",
    "TO", "USING", ">", "=", ">=", "<", "<>", "<=", "+", "-", "*", "/",
    "^", "\\", "AND", "MOD", "OR", "XOR", "NOT", "#FF"};

constexpr uint8_t kElse = 0x97;
constexpr uint8_t kQuoteRemark = 0xC0;
constexpr uint8_t kRem = 0xC5;
constexpr uint8_t kData = 0x8C;

// The functions, after token &FF: three runs of them.
const char* function(uint8_t token)
{
    static const char* const first[] = {"ABS", "ASC", "ATN", "CHR$", "CINT", "COS", "CREAL", "EXP",
                                        "FIX", "FRE", "INKEY", "INP", "INT", "JOY", "LEN", "LOG",
                                        "LOG10", "LOWER$", "PEEK", "REMAIN", "SGN", "SIN", "SPACE$", "SQ",
                                        "SQR", "STR$", "TAN", "UNT", "UPPER$", "VAL"};
    static const char* const second[] = {"EOF", "ERR", "HIMEM", "INKEY$", "PI", "RND", "TIME", "XPOS", "YPOS", "DERR"};
    static const char* const third[] = {"BIN$", "DEC$", "HEX$", "INSTR", "LEFT$", "MAX", "MIN", "POS",
                                        "RIGHT$", "ROUND", "STRING$", "TEST", "TESTR", "COPYCHR$", "VPOS"};
    if (token < std::size(first))
        return first[token];
    if (token >= 0x40 && token < 0x40 + std::size(second))
        return second[token - 0x40];
    if (token >= 0x71 && token < 0x71 + std::size(third))
        return third[token - 0x71];
    return "";
}

// A program's bytes, read one after the other; past the end they are 0.
class Reader {
public:
    explicit Reader(std::span<const uint8_t> bytes)
        : bytes_(bytes)
    {
    }

    bool done() const { return at_ >= bytes_.size(); }
    size_t position() const { return at_; }
    void seek(size_t at) { at_ = at; }
    uint8_t byte() { return at_ < bytes_.size() ? bytes_[at_++] : (++at_, uint8_t{0}); }
    int word()
    {
        const int low = byte();
        return low | byte() << 8;
    }

    // A name: its characters up to the one with its high bit set.
    std::string name()
    {
        std::string text;
        for (int count = 0; count < 256 && !done(); ++count) {
            const uint8_t c = byte();
            text += static_cast<char>(c & 0x7F);
            if (c & 0x80)
                break;
        }
        return text;
    }

private:
    std::span<const uint8_t> bytes_;
    size_t at_ = 0;
};

// A number as the CPC keeps one that is not whole: four bytes of mantissa,
// the sign in place of its top bit, and the power of two it is times.
std::string real(Reader& reader)
{
    uint32_t mantissa = 0;
    for (int at = 0; at < 4; ++at)
        mantissa |= static_cast<uint32_t>(reader.byte()) << (8 * at);
    const int exponent = reader.byte();
    if (exponent == 0)
        return "0";
    const bool negative = mantissa & 0x80000000u;
    const double value = std::ldexp(static_cast<double>(mantissa | 0x80000000u) / 4294967296.0, exponent - 128);
    char text[32];
    // Nine figures, as the CPC prints them.
    std::snprintf(text, sizeof text, "%.9G", negative ? -value : value);
    return text;
}

std::string binary(int value)
{
    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + (value & 1)));
        value >>= 1;
    } while (value);
    return digits;
}

}  // namespace

std::string basicListing(std::span<const uint8_t> content)
{
    Reader reader(content);
    std::string listing;
    char number[16];
    while (!reader.done()) {
        const size_t start = reader.position();
        const int length = reader.word();
        if (length == 0)
            break;
        std::snprintf(number, sizeof number, "%d ", reader.word());
        listing += number;

        bool inString = false;
        bool literal = false;  // after REM or ': the rest of the line is as typed
        bool data = false;     // after DATA: as typed, to the end of the statement
        for (;;) {
            const uint8_t token = reader.byte();
            if (token == 0 || reader.done())
                break;
            if (literal || inString || (data && token != 0x01 && token != '"')) {
                listing += static_cast<char>(token);
                if (token == '"')
                    inString = !inString;
                continue;
            }
            if (token >= 0x80 && token < 0xFF) {
                // ELSE and ' come with a ':' in front, which is not shown.
                if ((token == kElse || token == kQuoteRemark) && !listing.empty() && listing.back() == ':')
                    listing.pop_back();
                listing += kKeywords[token & 0x7F];
                literal = token == kRem || token == kQuoteRemark;
                data = token == kData;
            } else if (token >= 0x0E && token <= 0x18) {
                listing += std::to_string(token - 0x0E);
            } else if (token >= 0x20 && token < 0x7C) {
                listing += static_cast<char>(token);
                inString = token == '"';
            } else {
                switch (token) {
                case 0x01:
                    listing += ':';
                    data = false;
                    break;
                case 0x02:  // a variable with its type after its name
                case 0x03:
                case 0x04:
                    reader.word();
                    listing += reader.name();
                    listing += token == 0x02 ? '%' : token == 0x03 ? '$' : '!';
                    break;
                case 0x0B:  // a variable without
                case 0x0C:
                case 0x0D:
                    reader.word();
                    listing += reader.name();
                    break;
                case 0x19:
                    listing += std::to_string(reader.byte());
                    break;
                case 0x1A:
                case 0x1D:  // a line by where it is, and by its number
                case 0x1E:
                    listing += std::to_string(reader.word());
                    break;
                case 0x1B:
                    listing += "&X" + binary(reader.word());
                    break;
                case 0x1C:
                    std::snprintf(number, sizeof number, "&%X", reader.word());
                    listing += number;
                    break;
                case 0x1F:
                    listing += real(reader);
                    break;
                case 0x7C:  // a command of a ROM or of a program
                    reader.byte();
                    listing += '|' + reader.name();
                    break;
                case 0xFF: {
                    const uint8_t which = reader.byte();
                    if (which < 0x80)
                        listing += function(which);
                    else
                        listing += static_cast<char>(which & 0x7F);
                    break;
                }
                default:
                    if (token >= 0x7C && token < 0x80)
                        listing += static_cast<char>(token);
                    break;
                }
            }
        }
        listing += '\n';
        // The next line is where the length says, whatever the tokens were
        // taken for.
        if (start + static_cast<size_t>(length) > reader.position() || length >= 4)
            reader.seek(start + static_cast<size_t>(length));
    }
    return listing;
}

std::vector<uint8_t> unprotectedBasic(std::span<const uint8_t> content)
{
    // AMSDOS mixes each byte with one of each of these, taken in turn.
    static const std::array<uint8_t, 13> kFirst = {0xE2, 0x9D, 0xDB, 0x1A, 0x42, 0x29, 0x39,
                                                   0xC6, 0xB3, 0xC6, 0x90, 0x45, 0x8A};
    static const std::array<uint8_t, 11> kSecond = {0x49, 0xB1, 0x36, 0xF0, 0x2E, 0x1E, 0x06, 0x2A, 0x28, 0x19, 0xEA};
    std::vector<uint8_t> program(content.begin(), content.end());
    for (size_t at = 0; at < program.size(); ++at)
        program[at] ^= static_cast<uint8_t>(kFirst[at % kFirst.size()] ^ kSecond[at % kSecond.size()]);
    return program;
}

}  // namespace tuxdsk
