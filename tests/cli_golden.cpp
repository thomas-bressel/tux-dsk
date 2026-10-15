// The program run as a user runs it, with the command lines the real
// ManageDsk was given (tools/managedsk-goldens/run.sh): the discs it writes
// are ManageDsk's, byte for byte, except where ManageDsk loses bytes
// (notes/ecarts.md).

#include "cli_test.h"
#include "disc/disc.h"

using golden::Bytes;

namespace {

// Runs the program in the scratch folder; its exit status.
int program(const cli::Scratch& scratch, const std::string& arguments, const std::string& output = "out.txt")
{
    const std::string line = "cd '" + scratch.path().string() + "' && '" TUXDSK_PROGRAM "' " + arguments + " > " + output
                             + " 2> err.txt";
    const int status = std::system(line.c_str());
    return WEXITSTATUS(status);
}

std::string text(const Bytes& bytes)
{
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace

int main()
{
    cli::Scratch scratch("golden");
    scratch.addInputs(true);

    CHECK_EQ(program(scratch, "-C -Sempty.dsk"), 0);
    CHECK(scratch.read("empty.dsk") == golden::disc("empty"));
    CHECK(text(scratch.read("out.txt")) == "Disc created.\nSaved empty.dsk.\n");

    CHECK_EQ(program(scratch, "-C -Ahello.bas -Acode.bin -Awithhdr.bin -Ak17.dat -Sfiles.dsk"), 0);
    CHECK(scratch.read("files.dsk") == golden::disc("files"));

    CHECK_EQ(program(scratch, "-C -Amy.file.name.txt -Alowercase.bas -Averylongfilename.binary '-Aa#b$c&.x1' -Anoext "
                             "-AUPPER.BIN -Ad1g1t5.b4s -Anoext -Snames.dsk"),
             0);
    CHECK(scratch.read("names.dsk") == golden::disc("names"));

    // -I: ManageDsk counts a record too many for each file, and puts an
    // ASCII file twice in what it counts: the files are compared, not the
    // directory.
    CHECK_EQ(program(scratch, "-C -Icode.bin/CODE.BIN/BIN/32768/32784 -Icode.bin/PROG.BAS/BAS/368 "
                             "-Ihello.bas/HELLO.TXT/ASCII -Simport.dsk"),
             0);
    {
        const tuxdsk::CpcDisc mine(*tuxdsk::DiscImage::parse(scratch.read("import.dsk")));
        const tuxdsk::CpcDisc theirs(*tuxdsk::DiscImage::parse(golden::disc("import")));
        const int lengths[] = {428, 428, 18};
        const char* names[] = {"CODE.BIN", "PROG.BAS", "HELLO.TXT"};
        for (int at = 0; at < 3; ++at) {
            const auto file = mine.find(names[at]);
            const auto real = theirs.find(names[at]);
            CHECK(file && real);
            if (!file || !real)
                continue;
            CHECK_EQ(file->records, (lengths[at] + 127) / 128);
            CHECK_EQ(real->records, file->records + 1);
            Bytes content = mine.read(*file);
            Bytes golden = theirs.read(*real);
            content.resize(static_cast<size_t>(lengths[at]));
            golden.resize(static_cast<size_t>(lengths[at]));
            CHECK(content == golden);
        }
    }

    // A disc that grows: ManageDsk's file stops a track short, and the end
    // of the last file with it. What it has is the same.
    CHECK_EQ(program(scratch, "-C -Afill00.dat -Afill01.dat -Afill02.dat -Afill03.dat -Afill04.dat -Afill05.dat "
                             "-Afill06.dat -Afill07.dat -Afill08.dat -Afill09.dat -Afill10.dat -Afill11.dat -Sgrown.dsk"),
             0);
    {
        const Bytes mine = scratch.read("grown.dsk");
        const Bytes theirs = golden::disc("grown");
        CHECK_EQ(theirs.size(), 0x100 + 43 * 0x1300);
        CHECK_EQ(mine.size(), 0x100 + 44 * 0x1300);
        CHECK_EQ(mine[0x30], 44);
        CHECK(mine.size() > theirs.size() && std::equal(theirs.begin() + 0x31, theirs.end(), mine.begin() + 0x31));
        CHECK(std::equal(theirs.begin(), theirs.begin() + 0x30, mine.begin()));
    }

    // A disc read and saved is the same file.
    CHECK_EQ(program(scratch, "-Lfiles.dsk -Scopy.dsk"), 0);
    CHECK(scratch.read("copy.dsk") == golden::disc("files"));

    // A name that ends in .hfe: the disc as an HxC Floppy Emulator's file,
    // the one its own converter makes; and read from it, the same disc.
    scratch.write("hfe_data.dsk", golden::read(std::string(TUXDSK_GOLDEN_DIR) + "/hfe_data.dsk"));
    CHECK_EQ(program(scratch, "-Lhfe_data.dsk -Shfe_data.HFE"), 0);
    CHECK(scratch.read("hfe_data.HFE") == golden::read(std::string(TUXDSK_GOLDEN_DIR) + "/hfe_data.hfe"));
    CHECK_EQ(program(scratch, "-C -Ahello.bas -Acode.bin -Awithhdr.bin -Ak17.dat -Sgotek.hfe"), 0);
    CHECK(text(scratch.read("out.txt")).find("Saved gotek.hfe.") != std::string::npos);
    CHECK_EQ(scratch.read("gotek.hfe").size(), 512 * (2 + 42 * 49));
    CHECK_EQ(program(scratch, "-Lgotek.hfe -Ad1g1t5.b4s -Sback.dsk"), 0);
    {
        const tuxdsk::CpcDisc back(*tuxdsk::DiscImage::parse(scratch.read("back.dsk")));
        const tuxdsk::CpcDisc files(*tuxdsk::DiscImage::parse(golden::disc("files")));
        CHECK_EQ(back.files().size(), 5);
        for (const char* name : {"HELLO.BAS", "CODE.BIN", "WITHHDR.BIN", "K17.DAT"})
            CHECK(back.find(name) && files.find(name) && back.read(*back.find(name)) == files.read(*files.find(name)));
        CHECK(back.find("D1G1T5.B4S").has_value());
    }

    // The program says what is wrong, and how it ends.
    CHECK_EQ(program(scratch, "-Lnothere.dsk -Sx.dsk"), 1);
    CHECK(text(scratch.read("err.txt")) == "tuxdsk: nothere.dsk cannot be read\n");
    CHECK_EQ(program(scratch, "-Q"), 2);
    CHECK(text(scratch.read("err.txt")).find("-help lists the commands") != std::string::npos);
    CHECK_EQ(program(scratch, "-help"), 0);
    CHECK(text(scratch.read("out.txt")).find("-I<file>/<name>/<type>/<start>/<entry>") != std::string::npos);
    CHECK_EQ(program(scratch, ""), 0);
    CHECK(text(scratch.read("out.txt")).find("ManageDsk") != std::string::npos);

    return checkSummary("cli_golden");
}
