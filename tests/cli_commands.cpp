// The command line: read, with what is wrong in it said; what -I is given,
// taken apart; the masks of -A; and the commands done, in a folder of the
// test's own.

#include "cli/commands.h"
#include "cli/files.h"
#include "cli_test.h"
#include "disc/amsdos.h"
#include "disc/disc.h"

using namespace tuxdsk::cli;
using golden::Bytes;

namespace {

Outcome done(const std::vector<std::string>& arguments)
{
    const ParsedLine line = parseCommandLine(arguments);
    CHECK(line.ok());
    return run(line.commands);
}

tuxdsk::CpcDisc discOf(const cli::Scratch& scratch, const std::string& name)
{
    const auto image = tuxdsk::DiscImage::parse(scratch.read(name));
    CHECK(image.has_value());
    return image ? tuxdsk::CpcDisc(*image) : tuxdsk::CpcDisc();
}

}  // namespace

int main()
{
    // ---- The line ----
    {
        const ParsedLine line = parseCommandLine({"-C", "-A*.BIN", "-Stoto.dsk"});
        CHECK(line.ok() && !line.usageAsked);
        CHECK_EQ(line.commands.size(), 3);
        CHECK(line.commands[0].letter == 'C' && line.commands[0].argument.empty());
        CHECK(line.commands[1].letter == 'A' && line.commands[1].argument == "*.BIN");
        CHECK(line.commands[2].letter == 'S' && line.commands[2].argument == "toto.dsk");
        // Small letters are the same commands; what follows them is kept as typed.
        const ParsedLine small = parseCommandLine({"-lOld.dsk", "-a*.bas", "-i/tmp/a/b.bin/B.BIN/BIN/1/2", "-sNew.dsk"});
        CHECK(small.ok() && small.commands.size() == 4);
        CHECK(small.commands[0].letter == 'L' && small.commands[0].argument == "Old.dsk");
        CHECK(small.commands[2].letter == 'I' && small.commands[2].argument == "/tmp/a/b.bin/B.BIN/BIN/1/2");
        CHECK(parseCommandLine({"-help"}).usageAsked);
        CHECK(parseCommandLine({"-C", "-?"}).usageAsked);
        CHECK(!parseCommandLine({"-X"}).ok());
        CHECK(!parseCommandLine({"toto.dsk"}).ok());
        CHECK(!parseCommandLine({"-L"}).ok());
        CHECK(!parseCommandLine({"-S"}).ok());
        CHECK(!parseCommandLine({"-Cx"}).ok());
        CHECK(!parseCommandLine({"-"}).ok());
        const std::string usage = usageText();
        for (const char* command : {"-C", "-L<disc>", "-A<mask>", "-I<file>/<name>/<type>/<start>/<entry>", "-S<disc>"})
            CHECK(usage.find(command) != std::string::npos);
        CHECK(usage.find("ManageDsk 0.20g") != std::string::npos);
    }

    // ---- What -I is given ----
    {
        ImportRequest request = parseImport("C:\\MONFICHIER/TOTO.BIN/BIN/16384/16385");
        CHECK(request.ok() && request.file == "C:\\MONFICHIER" && request.name == "TOTO.BIN");
        CHECK(request.type == ImportRequest::Type::Binary && request.start == 16384 && request.entry == 16385);
        // The computer's folders are parted with '/' too.
        request = parseImport("/home/me/my screen.src/ECRAN.SCR/BIN/49152");
        CHECK(request.ok() && request.file == "/home/me/my screen.src" && request.name == "ECRAN.SCR");
        CHECK(request.start == 49152 && request.entry == 0);
        request = parseImport("testbasic/PROG.BAS/BAS/368");
        CHECK(request.ok() && request.type == ImportRequest::Type::Basic && request.start == 368);
        // A BASIC program loads at &170 if nothing is said.
        request = parseImport("testbasic/PROG.BAS/BAS");
        CHECK(request.ok() && request.start == 0x170 && request.entry == 0);
        request = parseImport("dir/fichierbidon/test/ASCII");
        CHECK(request.ok() && request.type == ImportRequest::Type::Ascii && request.file == "dir/fichierbidon");
        CHECK(request.name == "test");
        // Types in small letters, addresses in hexadecimal.
        request = parseImport("a.bin/A.BIN/bin/&8000/#8010");
        CHECK(request.ok() && request.type == ImportRequest::Type::Binary && request.start == 0x8000);
        CHECK(request.entry == 0x8010);
        request = parseImport("a.bin/A.BIN/BINARY/0xC000");
        CHECK(request.ok() && request.start == 0xC000);
        // A folder called as a type is, further on, is a folder.
        request = parseImport("BIN/a.bin/A.BIN/BAS");
        CHECK(request.ok() && request.file == "BIN/a.bin" && request.type == ImportRequest::Type::Basic);
        CHECK(!parseImport("a.bin").ok());
        CHECK(!parseImport("a.bin/A.BIN").ok());
        CHECK(!parseImport("a.bin/A.BIN/XYZ/1/2").ok());
        CHECK(!parseImport("a.bin/A.BIN/BIN/12ab").ok());
        CHECK(!parseImport("a.bin/*.*/BIN").ok());
    }

    // ---- Masks ----
    CHECK(matches("*.BIN", "code.bin") && matches("*.bin", "CODE.BIN") && !matches("*.BIN", "code.bas"));
    CHECK(matches("c?de.*", "code.bin") && !matches("c?de.*", "cde.bin"));
    CHECK(matches("*", "noext") && matches("*.*", "noext") && matches("*.*", "a.b.c"));
    CHECK(matches("f*0*.dat", "fill00.dat") && !matches("f*1*.dat", "fill00.dat"));
    CHECK(!matches("", "a") && matches("", ""));

    // ---- The commands done ----
    cli::Scratch scratch("commands");
    scratch.addInputs();
    {
        // The files of a mask, by name whatever their letters.
        const std::vector<std::string> found = filesOf(scratch.file("*.bin"));
        CHECK_EQ(found.size(), 3);
        CHECK(found.size() == 3 && found[0] == scratch.file("code.bin") && found[1] == scratch.file("UPPER.BIN"));
        CHECK(found.size() == 3 && found[2] == scratch.file("withhdr.bin"));
        CHECK(filesOf(scratch.file("nothing.*")).empty());
        CHECK_EQ(filesOf(scratch.file("code.bin")).size(), 1);
        CHECK(filesOf(scratch.file("nothere.bin")).empty());
    }
    {
        const Outcome outcome = done({"-C", "-A" + scratch.file("*.bin"), "-A" + scratch.file("hello.bas"),
                                      "-S" + scratch.file("one.dsk")});
        CHECK(outcome.status == kStatusDone && outcome.error.empty());
        CHECK_EQ(outcome.said.size(), 6);
        CHECK(outcome.said.size() == 6 && outcome.said[0] == "Disc created.");
        CHECK(outcome.said.size() == 6 && outcome.said[1] == "Added CODE.BIN (300 bytes).");
        CHECK(outcome.said.size() == 6 && outcome.said[5] == "Saved " + scratch.file("one.dsk") + ".");
        const tuxdsk::CpcDisc disc = discOf(scratch, "one.dsk");
        const auto files = disc.files();
        CHECK_EQ(files.size(), 4);
        CHECK(files.size() == 4 && files[0].name == "CODE.BIN" && files[1].name == "UPPER.BIN");
        CHECK(files.size() == 4 && files[2].name == "WITHHDR.BIN" && files[3].name == "HELLO.BAS");
    }
    {
        // A disc read, files added to it, saved under another name.
        const Outcome outcome = done({"-L" + scratch.file("one.dsk"), "-A" + scratch.file("k17.dat"),
                                      "-A" + scratch.file("code.bin"), "-S" + scratch.file("two.dsk")});
        CHECK(outcome.status == kStatusDone);
        // A file the disc has is left as it is, and the line goes on.
        CHECK_EQ(outcome.said.size(), 4);
        CHECK(outcome.said.size() == 4 && outcome.said[2].find("already on the disc") != std::string::npos);
        CHECK_EQ(discOf(scratch, "two.dsk").files().size(), 5);
        CHECK_EQ(discOf(scratch, "one.dsk").files().size(), 4);
    }
    {
        // -I: the header of the type, whatever header the file came with.
        const Outcome outcome = done({"-C", "-I" + scratch.file("code.bin") + "/Loader.Bin/BIN/&8000/&8010",
                                      "-I" + scratch.file("withhdr.bin") + "/W.BIN/BIN/16384/16385",
                                      "-I" + scratch.file("withhdr.bin") + "/W.TXT/ASCII",
                                      "-I" + scratch.file("code.bin") + "/P.BAS/BAS", "-S" + scratch.file("i.dsk")});
        CHECK(outcome.status == kStatusDone);
        const tuxdsk::CpcDisc disc = discOf(scratch, "i.dsk");
        const auto loader = disc.find("LOADER.BIN");
        CHECK(loader && loader->type == tuxdsk::FileType::Binary && loader->loadAddress == 0x8000);
        CHECK(loader && loader->entryAddress == 0x8010 && loader->length == 300 && loader->records == 4);
        const Bytes whole = golden::input("withhdr.bin");
        const Bytes data(whole.begin() + 128, whole.end());
        const auto w = disc.find("W.BIN");
        CHECK(w && w->loadAddress == 16384 && w->entryAddress == 16385 && w->length == 300 && w->records == 4);
        if (w) {
            const Bytes content = disc.read(*w);
            const auto header = tuxdsk::amsdosHeader(content);
            CHECK(header && header->name == "W.BIN");
            CHECK(std::equal(data.begin(), data.end(), content.begin() + 128));
        }
        const auto text = disc.find("W.TXT");
        CHECK(text && text->type == tuxdsk::FileType::Ascii && text->records == 3);
        CHECK(text && std::equal(data.begin(), data.end(), disc.read(*text).begin()));
        const auto program = disc.find("P.BAS");
        CHECK(program && program->type == tuxdsk::FileType::Basic && program->loadAddress == 0x170);
    }
    {
        // What cannot be done stops the line, and says why.
        Outcome outcome = done({"-A" + scratch.file("code.bin")});
        CHECK(outcome.status == kStatusFailed && outcome.error.find("no disc yet") != std::string::npos);
        outcome = done({"-L" + scratch.file("nothere.dsk"), "-S" + scratch.file("never.dsk")});
        CHECK(outcome.status == kStatusFailed && outcome.error.find("cannot be read") != std::string::npos);
        CHECK(!cli::fs::exists(scratch.file("never.dsk")));
        outcome = done({"-L" + scratch.file("code.bin")});
        CHECK(outcome.status == kStatusFailed && outcome.error.find("not a .DSK disc image") != std::string::npos);
        outcome = done({"-C", "-A" + scratch.file("nothere.bin")});
        CHECK(outcome.status == kStatusFailed);
        outcome = done({"-C", "-S" + scratch.file("no/such/folder/a.dsk")});
        CHECK(outcome.status == kStatusFailed && outcome.error.find("cannot be written") != std::string::npos);
        // A mask no file is called by is said, and is not a failure.
        outcome = done({"-C", "-A" + scratch.file("*.xyz"), "-S" + scratch.file("none.dsk")});
        CHECK(outcome.status == kStatusDone && outcome.said.size() == 3);
        // A disc that is not saved is said so.
        outcome = done({"-C", "-A" + scratch.file("code.bin")});
        CHECK(outcome.status == kStatusDone && outcome.said.back().find("not saved") != std::string::npos);
        // A disc with no room left.
        scratch.write("big.dat", Bytes(200 * 1024, 1));
        outcome = done({"-C", "-A" + scratch.file("big.dat"), "-A" + scratch.file("big.dat"), "-A" + scratch.file("k17.dat"),
                        "-A" + scratch.file("big.dat")});
        CHECK(outcome.status == kStatusDone);
        scratch.write("big2.dat", Bytes(60 * 1024, 1));
        outcome = done({"-C", "-A" + scratch.file("big.dat"), "-A" + scratch.file("big2.dat")});
        CHECK(outcome.status == kStatusFailed && outcome.error.find("no room left") != std::string::npos);
    }

    return checkSummary("cli_commands");
}
