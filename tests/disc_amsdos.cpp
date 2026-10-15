// AMSDOS headers: found, read, made as ManageDsk makes them, changed and
// cleaned; and the names files take on a disc.

#include "check.h"
#include "disc/amsdos.h"
#include "disc/names.h"
#include "golden.h"

using golden::Bytes;

int main()
{
    using namespace tuxdsk;

    // ---- A header is known by its sum ----
    const Bytes withHeader = golden::input("withhdr.bin");
    CHECK(hasAmsdosHeader(withHeader));
    CHECK(!hasAmsdosHeader(golden::input("code.bin")));
    CHECK(!hasAmsdosHeader(golden::input("hello.bas")));
    // 128 bytes of 0 add up to the 0 they hold: not a header.
    CHECK(!hasAmsdosHeader(Bytes(300, 0)));
    const auto header = amsdosHeader(withHeader);
    CHECK(header.has_value());
    CHECK(header && header->name == "WITHHDR.BIN" && header->user == 0 && header->type == kAmsdosBinary);
    CHECK(header && header->loadAddress == 0x8000 && header->entryAddress == 0x8010 && header->length == 300);
    CHECK_EQ(withoutAmsdosHeader(withHeader).size(), 300);
    CHECK_EQ(withoutAmsdosHeader(golden::input("code.bin")).size(), 300);

    // ---- One made ----
    const Bytes made = makeAmsdosHeader("screen.scr", kAmsdosBinary, 0xC000, 0x1234, 0x4000);
    CHECK_EQ(made.size(), 128);
    CHECK(hasAmsdosHeader(made));
    CHECK(std::string(made.begin() + 1, made.begin() + 12) == "SCREEN  SCR");
    CHECK_EQ(made[0], 0);
    CHECK_EQ(made[18], 2);
    CHECK_EQ(made[19] | made[20] << 8, 0x4000);
    CHECK_EQ(made[21] | made[22] << 8, 0xC000);
    CHECK_EQ(made[24] | made[25] << 8, 0x4000);
    CHECK_EQ(made[26] | made[27] << 8, 0x1234);
    CHECK_EQ(made[64] | made[65] << 8 | made[66] << 16, 0x4000);
    // A file longer than 64K says so on its three bytes.
    const auto longOne = amsdosHeader(makeAmsdosHeader("BIG.BIN", kAmsdosBinary, 0, 0, 0x12345));
    CHECK(longOne && longOne->length == 0x12345);

    // ---- One changed ----
    Bytes changed = withHeader;
    setAmsdosHeader(changed, kAmsdosBasic, 0x170, 0);
    const auto after = amsdosHeader(changed);
    CHECK(after && after->type == kAmsdosBasic && after->loadAddress == 0x170 && after->entryAddress == 0);
    CHECK(after && after->length == 300 && after->name == "WITHHDR.BIN");

    // ---- One cleaned: the bytes AMSDOS does not use are 0 ----
    Bytes cleaned = withHeader;
    CHECK_EQ(cleaned[0x1C], 0xAA);
    CHECK_EQ(cleaned[0x7F], 0xBB);
    cleanAmsdosHeader(cleaned);
    CHECK(hasAmsdosHeader(cleaned));
    for (size_t at = 28; at < 64; ++at)
        CHECK_EQ(cleaned[at], 0);
    for (size_t at = 69; at < 128; ++at)
        CHECK_EQ(cleaned[at], 0);
    CHECK(std::equal(cleaned.begin(), cleaned.begin() + 28, withHeader.begin()));
    CHECK(std::equal(cleaned.begin() + 128, cleaned.end(), withHeader.begin() + 128));
    const auto clean = amsdosHeader(cleaned);
    CHECK(clean && clean->loadAddress == 0x8000 && clean->length == 300);
    // A file without a header is left alone.
    Bytes plain = golden::input("code.bin");
    cleanAmsdosHeader(plain);
    CHECK(plain == golden::input("code.bin"));

    // ---- Names ----
    CHECK(cpcName("hello.bas") == "HELLO.BAS");
    CHECK(cpcName("/home/user/My Files/hello.bas") == "HELLO.BAS");
    CHECK(cpcName("C:\\dos\\Game.Bin") == "GAME.BIN");
    CHECK(cpcName("my.file.name.txt") == "MY.FIL");
    CHECK(cpcName("verylongfilename.binary") == "VERYLONG.BIN");
    CHECK(cpcName("noext") == "NOEXT");
    CHECK(cpcName("a#b$c&.x1") == "A#B$C&.X1");
    // What a directory cannot hold is left out.
    CHECK(cpcName("my file.t x") == "MYFILE.TX");
    CHECK(cpcName("été*?.s:r") == "T.SR");
    CHECK(cpcName(".hidden").empty());
    CHECK(cpcName("").empty());
    const auto padded = directoryName("ab.c");
    CHECK(std::string(padded.begin(), padded.end()) == "AB      C  ");
    const uint8_t raw[11] = {'D', 'I', 'S', 'C', ' ', ' ', ' ', ' ', 'B' | 0x80, 'A' | 0x80, 'S'};
    CHECK(shownName(raw) == "DISC.BAS");
    const uint8_t odd[11] = {'A', 0x01, 'B', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    CHECK(shownName(odd) == "A?B");

    return checkSummary("disc_amsdos");
}
