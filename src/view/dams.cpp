#include "view/dams.h"

#include <iterator>

namespace tuxdsk {

namespace {

const char* const kInstructions[] = {
    "LD", "INC", "DEC", "ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP", "PUSH", "POP",
    "BIT", "RES", "SET", "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SRL", "IN", "OUT", "RST",
    "DJNZ", "EX", "IM", "JR", "CALL", "RET", "JP", "NOP", "RLCA", "RRCA", "RLA", "RRA", "DAA",
    "CPL", "SCF", "CCF", "HALT", "EXX", "DI", "EI", "NEG", "RETN", "RETI", "RRD", "RLD", "LDI",
    "CPI", "INI", "OUTI", "LDD", "CPD", "IND", "OUTD", "LDIR", "CPIR", "INIR", "OTIR", "LDDR", "CPDR",
    "INDR", "OTDR", "DB", "DW", "DM", "DS", "EQU", "ORG", "ENT", "IF", "ELSE", "END"};

constexpr uint8_t kEndOfLine = 0x0D;
constexpr uint8_t kComment = 0xFF;
constexpr uint8_t kDs = 0xC6;
constexpr uint8_t kEnt = 0xC9;

const char* instruction(uint8_t code)
{
    const size_t index = code & 0x7F;
    return index < std::size(kInstructions) ? kInstructions[index] : "";
}

}  // namespace

std::string damsListing(std::span<const uint8_t> content)
{
    std::string listing;
    size_t at = 0;
    const auto next = [&]() -> uint8_t { return at < content.size() ? content[at++] : kEndOfLine; };

    // The operands, to the end of the line. Of a DS only the size is kept:
    // what Dams fills it with is left out.
    const auto operands = [&](uint8_t code, const char* comment) {
        uint8_t c = next();
        while (c != kEndOfLine && at <= content.size()) {
            if (code == kDs && c == ',')
                while (c != kEndOfLine && c != kComment && at < content.size())
                    c = next();
            if (c == kEndOfLine)
                break;
            if (c == kComment)
                listing += comment;
            else
                listing += static_cast<char>(c);
            if (at >= content.size())
                break;
            c = next();
        }
    };

    while (at < content.size()) {
        uint8_t c = next();
        if (c == 0)
            break;
        if (c == kComment) {
            // A line that is a comment.
            listing += ';';
            for (c = next(); c != kEndOfLine; c = next()) {
                listing += static_cast<char>(c);
                if (at >= content.size())
                    break;
            }
        } else if (c >= 0x80) {
            // An instruction without a label. ENT is not one an assembler
            // of a PC knows: it is made a comment.
            if (c == kEnt)
                listing += ';';
            listing += '\t';
            listing += instruction(c);
            listing += '\t';
            operands(c, "\t");
        } else {
            // A label, then perhaps an instruction.
            while (c < 0x80 && c != kEndOfLine) {
                listing += static_cast<char>(c);
                if (at >= content.size()) {
                    c = kEndOfLine;
                    break;
                }
                c = next();
            }
            if (c != kEndOfLine) {
                if (c == kEnt)
                    listing += ';';
                if (c != kComment) {
                    listing += '\t';
                    listing += instruction(c);
                    listing += '\t';
                } else {
                    // A label with nothing but a comment after it.
                    listing += "\t\t\t;";
                }
                operands(c, "\t;");
            }
        }
        listing += '\n';
    }
    return listing;
}

}  // namespace tuxdsk
