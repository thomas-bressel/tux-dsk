// What the viewer shows as text: a BASIC program made a listing again, one
// that was protected, a Dams source, Z80 instructions, bytes in hexadecimal,
// a text file, and lines cut as the CPC cuts them.

#include <initializer_list>

#include "check.h"
#include "view/basic.h"
#include "view/dams.h"
#include "view/disasm.h"
#include "view/text.h"

using Bytes = std::vector<uint8_t>;

namespace {

// A line of a program: its length, its number, its tokens and a 0.
void line(Bytes& program, int number, std::initializer_list<int> tokens)
{
    const size_t length = tokens.size() + 5;
    program.push_back(static_cast<uint8_t>(length & 0xFF));
    program.push_back(static_cast<uint8_t>(length >> 8));
    program.push_back(static_cast<uint8_t>(number & 0xFF));
    program.push_back(static_cast<uint8_t>(number >> 8));
    for (const int token : tokens)
        program.push_back(static_cast<uint8_t>(token));
    program.push_back(0);
}

Bytes program()
{
    Bytes bytes;
    // 10 MODE 1:PRINT "HELLO":GOTO 10
    line(bytes, 10, {0xAD, ' ', 0x0F, 0x01, 0xBF, ' ', '"', 'H', 'E', 'L', 'L', 'O', '"', 0x01, 0xA0, ' ', 0x1E, 10, 0});
    // 20 a%=&BB18:b$="x":c!=1.5:d=255
    line(bytes, 20, {0x02, 0, 0, 'a' | 0x80, 0xEF, 0x1C, 0x18, 0xBB, 0x01, 0x03, 0, 0, 'b' | 0x80, 0xEF, '"', 'x', '"',
                     0x01, 0x04, 0, 0, 'c' | 0x80, 0xEF, 0x1F, 0x00, 0x00, 0x00, 0x40, 0x81, 0x01, 0x0D, 0, 0,
                     'd' | 0x80, 0xEF, 0x19, 255});
    // 30 IF a%>1 THEN PRINT 1 ELSE CALL &BB18' note : x
    line(bytes, 30, {0xA1, ' ', 0x02, 0, 0, 'a' | 0x80, 0xEE, 0x0F, ' ', 0xEB, ' ', 0xBF, ' ', 0x0F, ' ', 0x01, 0x97, ' ',
                     0x83, ' ', 0x1C, 0x18, 0xBB, 0x01, 0xC0, ' ', 'n', 'o', 't', 'e', ' ', ':', ' ', 'x'});
    // 40 DATA 1,"a:b",c:REM x:y
    line(bytes, 40, {0x8C, ' ', '1', ',', '"', 'a', ':', 'b', '"', ',', 'c', 0x01, 0xC5, ' ', 'x', ':', 'y'});
    // 50 lo=PEEK(&X101)+SIN(2)-|DISC
    line(bytes, 50, {0x0D, 0, 0, 'l', 'o' | 0x80, 0xEF, 0xFF, 0x12, '(', 0x1B, 0x05, 0x00, ')', 0xF4, 0xFF, 0x15, '(', 0x10,
                     ')', 0xF5, 0x7C, 0, 'D', 'I', 'S', 'C' | 0x80});
    // 60 x=-2.5+0+1E+10+0.1+10+32000
    line(bytes, 60, {0x0D, 0, 0, 'x' | 0x80, 0xEF, 0x1F, 0x00, 0x00, 0x00, 0xA0, 0x82, 0xF4, 0x1F, 0, 0, 0, 0, 0, 0xF4,
                     0x1F, 0x00, 0xF9, 0x02, 0x15, 0xA2, 0xF4, 0x1F, 0xCD, 0xCC, 0xCC, 0x4C, 0x7D, 0xF4, 0x18, 0xF4,
                     0x1A, 0x00, 0x7D});
    bytes.push_back(0);
    bytes.push_back(0);
    return bytes;
}

}  // namespace

int main()
{
    using namespace tuxdsk;

    // ---- BASIC ----
    const char* listing = "10 MODE 1:PRINT \"HELLO\":GOTO 10\n"
                          "20 a%=&BB18:b$=\"x\":c!=1.5:d=255\n"
                          "30 IF a%>1 THEN PRINT 1 ELSE CALL &BB18' note : x\n"
                          "40 DATA 1,\"a:b\",c:REM x:y\n"
                          "50 lo=PEEK(&X101)+SIN(2)-|DISC\n"
                          "60 x=-2.5+0+1E+10+0.1+10+32000\n";
    const std::string made = basicListing(program());
    CHECK(made == listing);
    if (made != listing)
        std::printf("%s", made.c_str());
    // A protected program is the same one, scrambled.
    const Bytes scrambled = unprotectedBasic(program());
    CHECK(scrambled != program());
    CHECK_EQ(scrambled[0], program()[0] ^ 0xAB);
    CHECK_EQ(scrambled[1], program()[1] ^ 0x2C);
    CHECK(unprotectedBasic(scrambled) == program());
    CHECK(basicListing(unprotectedBasic(scrambled)) == listing);
    // What is not a program does not go on for ever, nor past its end.
    CHECK(basicListing({}).empty());
    CHECK(basicListing(Bytes{0, 0}).empty());
    CHECK(!basicListing(Bytes(300, 0x02)).empty());
    CHECK(!basicListing(Bytes{9, 0, 10, 0, 0x1F}).empty());
    Bytes cut = program();
    cut.resize(40);
    CHECK(basicListing(cut).find("10 MODE 1") == 0);

    // ---- Dams ----
    {
        Bytes source;
        const auto put = [&source](std::initializer_list<int> bytes) {
            for (const int byte : bytes)
                source.push_back(static_cast<uint8_t>(byte));
        };
        put({0xFF, 'h', 'e', 'l', 'l', 'o', 0x0D});                          // a comment
        put({0xC8, '#', '8', '0', '0', '0', 0x0D});                          // ORG
        put({0xC9, 'S', 'T', 'A', 'R', 'T', 0x0D});                          // ENT
        put({'S', 'T', 'A', 'R', 'T', 0x80, 'A', ',', '5', 0xFF, 'f', 'i', 'v', 'e', 0x0D});
        put({0xA1, 0x0D});                                                   // NOP
        put({'L', 'O', 'O', 'P', 0x0D});                                     // a label by itself
        put({'T', 'A', 'B', 0xC6, '1', '0', ',', '0', 0x0D});                // DS
        put({'E', 'N', 'D', '_', 0xFF, 'd', 'o', 'n', 'e', 0x0D});           // a label and a comment
        put({0x9F, 0x0D, 0});                                                // RET
        const char* expected = ";hello\n"
                               "\tORG\t#8000\n"
                               ";\tENT\tSTART\n"
                               "START\tLD\tA,5\t;five\n"
                               "\tNOP\t\n"
                               "LOOP\n"
                               "TAB\tDS\t10\n"
                               "END_\t\t\t;done\n"
                               "\tRET\t\n";
        const std::string text = damsListing(source);
        CHECK(text == expected);
        if (text != expected)
            std::printf("%s", text.c_str());
        CHECK(damsListing({}).empty());
        // A source that stops in the middle of a line.
        source.resize(30);
        CHECK(!damsListing(source).empty());
        CHECK(!damsListing(Bytes(200, 0xFF)).empty());
    }

    // ---- Z80 instructions ----
    {
        const Bytes code = {0x21, 0x00, 0xC0, 0x3E, 0x05, 0x18, 0xFE, 0xCB, 0x47, 0xED, 0xB0, 0xDD, 0x36, 0x05, 0x12,
                            0xDD, 0xCB, 0xFE, 0x46, 0xFD, 0xCB, 0x03, 0xC6, 0xED, 0x00, 0xCD, 0x5A, 0xBB, 0x10, 0xE2,
                            0xFD, 0x21, 0x34, 0x12, 0xC9};
        const char* expected = "8000 21 00 C0       LD HL,C000\n"
                               "8003 3E 05          LD A,05\n"
                               "8005 18 FE          JR 8005\n"
                               "8007 CB 47          BIT 0,A\n"
                               "8009 ED B0          LDIR\n"
                               "800B DD 36 05 12    LD (IX+05),12\n"
                               "800F DD CB FE 46    BIT 0,(IX-02)\n"
                               "8013 FD CB 03 C6    SET 0,(IY+03)\n"
                               "8017 ED 00          ED 00 00 ????\n"
                               "8019 CD 5A BB       CALL BB5A\n"
                               "801C 10 E2          DJNZ 8000\n"
                               "801E FD 21 34 12    LD IY,1234\n"
                               "8022 C9             RET\n";
        const std::string text = disassembly(code, 0x8000);
        CHECK(text == expected);
        if (text != expected)
            std::printf("%s", text.c_str());
        CHECK(disassembly({}, 0).empty());
        // An instruction the file stops in the middle of.
        CHECK(disassembly(Bytes{0xCD, 0x5A}, 0) == "0000 CD 5A 00       CALL 005A\n");
        // Every instruction there is, with each prefix, is a line.
        for (const int prefix : {-1, 0xCB, 0xED, 0xDD, 0xFD}) {
            for (int code2 = 0; code2 < 256; ++code2) {
                Bytes one;
                if (prefix >= 0)
                    one.push_back(static_cast<uint8_t>(prefix));
                one.push_back(static_cast<uint8_t>(code2));
                one.insert(one.end(), {0x12, 0x34, 0x56});
                const std::string made2 = disassembly(one, 0);
                CHECK(made2.size() > 20 && made2.find("nn") == std::string::npos && made2.find("eeee") == std::string::npos);
            }
        }
        for (const int prefix : {0xDD, 0xFD})
            for (int code2 = 0; code2 < 256; ++code2)
                CHECK(disassembly(Bytes{static_cast<uint8_t>(prefix), 0xCB, 0x85, static_cast<uint8_t>(code2)}, 0)
                          .find("nn")
                      == std::string::npos);
    }

    // ---- Bytes ----
    {
        Bytes bytes;
        for (int at = 0; at < 20; ++at)
            bytes.push_back(static_cast<uint8_t>(0x30 + at));
        bytes[1] = 0x00;
        bytes[2] = 0x7F;
        bytes[3] = ' ';
        const char* expected = "#0100: 30 00 7F 20 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F | 0...456789:;<=>?\n"
                               "#0110: 40 41 42 43                                     | @ABC\n";
        CHECK(hexDump(bytes, 0x100) == expected);
        CHECK(hexDump({}).empty());
        CHECK(hexDump(Bytes(0x10001, 0)).find("#000000: ") == 0);
    }

    // ---- Text ----
    CHECK(asciiListing(Bytes{'a', '\r', '\n', 'b', '\r', '\n', 0x1A, 'z'}) == "a\nb\n");
    CHECK(asciiListing(Bytes{'n', 'o', ' ', 'e', 'n', 'd'}) == "no end");
    CHECK(cpcText("10 PRINT \"\x01\xA0|@\"\n") == "10 PRINT \"??|@\"\n");
    CHECK(cpcText("@\\{|}~", true) == "àçéùè~");
    CHECK(cpcText("@\\{|}~") == "@\\{|}~");
    CHECK(wrapped("abcdefgh\nij\n", 3) == "abc\ndef\ngh\nij\n");
    CHECK(wrapped("abc\ndef", 3) == "abc\ndef");
    CHECK(wrapped(std::string(100, 'x')).find('\n') == 80);

    return checkSummary("engine_view_listings");
}
