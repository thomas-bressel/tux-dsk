// The discs made through the library against those the real ManageDsk made
// of the same files (tests/golden, tools/managedsk-goldens/run.sh): the same
// bytes, except where ManageDsk loses some (notes/ecarts.md).

#include "check.h"
#include "disc/amsdos.h"
#include "disc/disc.h"
#include "golden.h"

using golden::Bytes;
using tuxdsk::AddResult;
using tuxdsk::CpcDisc;

namespace {

// As ManageDsk's command line does: files go up to the last block.
tuxdsk::AddOptions anywhere()
{
    tuxdsk::AddOptions options;
    options.blocks = tuxdsk::kAllBlocks;
    return options;
}

}  // namespace

int main()
{
    // ---- An empty disc ----
    CHECK(CpcDisc().image().bytes() == golden::disc("empty"));

    // ---- Files added as they are ----
    {
        CpcDisc disc;
        for (const char* name : {"hello.bas", "code.bin", "withhdr.bin", "k17.dat"})
            CHECK(disc.add(name, golden::input(name), anywhere()) == AddResult::Done);
        CHECK(disc.image().bytes() == golden::disc("files"));

        // And ManageDsk's disc read: the same files.
        const CpcDisc theirs(*tuxdsk::DiscImage::parse(golden::disc("files")));
        const auto files = theirs.files();
        CHECK_EQ(files.size(), 4);
        CHECK(files.size() == 4 && files[0].name == "HELLO.BAS" && files[0].type == tuxdsk::FileType::Ascii);
        CHECK(files.size() == 4 && files[2].name == "WITHHDR.BIN" && files[2].type == tuxdsk::FileType::Binary);
        CHECK(files.size() == 4 && files[2].loadAddress == 0x8000 && files[2].entryAddress == 0x8010);
        CHECK(files.size() == 4 && files[3].name == "K17.DAT" && files[3].entries.size() == 2);
        CHECK(files.size() == 4 && files[3].records == 133 && files[3].kilobytes() == 17);
        // A file comes back whole, to the end of its last record.
        Bytes k17 = golden::input("k17.dat");
        k17.resize(133 * 128, 0);
        CHECK(files.size() == 4 && theirs.read(files[3]) == k17);
        CHECK_EQ(theirs.usedBlocks(), 1 + 1 + 1 + 17);
    }

    // ---- What names become, and a file added twice ----
    {
        CpcDisc disc;
        for (const char* name : {"my.file.name.txt", "lowercase.bas", "verylongfilename.binary", "a#b$c&.x1", "noext",
                                 "UPPER.BIN", "d1g1t5.b4s"})
            CHECK(disc.add(name, golden::input(name), anywhere()) == AddResult::Done);
        CHECK(disc.add("noext", golden::input("noext"), anywhere()) == AddResult::Exists);
        CHECK(disc.image().bytes() == golden::disc("names"));
        const char* expected[] = {"MY.FIL", "LOWERCAS.BAS", "VERYLONG.BIN", "A#B$C&.X1", "NOEXT", "UPPER.BIN", "D1G1T5.B4S"};
        const auto files = disc.files();
        CHECK_EQ(files.size(), 7);
        for (size_t at = 0; at < files.size() && at < 7; ++at)
            CHECK(files[at].name == expected[at]);
    }

    // ---- Files given a header: ManageDsk's -I ----
    {
        const CpcDisc theirs(*tuxdsk::DiscImage::parse(golden::disc("import")));
        const Bytes code = golden::input("code.bin");
        struct Case {
            const char* name;
            uint8_t type;
            uint16_t load, entry;
        };
        for (const Case& each : {Case{"CODE.BIN", tuxdsk::kAmsdosBinary, 32768, 32784},
                                 Case{"PROG.BAS", tuxdsk::kAmsdosBasic, 368, 0}}) {
            const auto file = theirs.find(each.name);
            CHECK(file.has_value());
            if (!file)
                continue;
            Bytes mine = tuxdsk::makeAmsdosHeader(each.name, each.type, each.load, each.entry, code.size());
            mine.insert(mine.end(), code.begin(), code.end());
            // ManageDsk counts a record too many: the bytes are compared as
            // far as the file goes.
            Bytes real = theirs.read(*file);
            CHECK_EQ(file->records, 5);
            real.resize(mine.size());
            CHECK(real == mine);
            CHECK(file->loadAddress == each.load && file->entryAddress == each.entry && file->length == 300);
        }
    }

    // ---- A disc that grows past its last track ----
    {
        CpcDisc disc;
        for (int index = 0; index < 12; ++index) {
            char name[16];
            std::snprintf(name, sizeof name, "fill%02d.dat", index);
            CHECK(disc.add(name, golden::fill(index), anywhere()) == AddResult::Done);
        }
        // ManageDsk leaves out the track that holds the second half of the
        // last block: the end of the last file is lost. (When it adds more
        // tracks it also formats over the first sector of every other one:
        // see notes/ecarts.md.) The tracks its file has are the same.
        const auto theirs = tuxdsk::DiscImage::parse(golden::disc("grown"));
        CHECK(theirs && theirs->tracks() == 43);
        CHECK_EQ(disc.image().tracks(), 44);
        const auto written = tuxdsk::DiscImage::parse(disc.image().bytes());
        for (int number = 0; theirs && written && number < theirs->tracks(); ++number) {
            const tuxdsk::Track& real = theirs->track(number);
            const tuxdsk::Track& mine = written->track(number);
            CHECK(real.header == mine.header);
            CHECK_EQ(real.sectors.size(), mine.sectors.size());
            for (size_t place = 0; place < real.sectors.size() && place < mine.sectors.size(); ++place)
                CHECK(real.sectors[place].data == mine.sectors[place].data);
        }
        CHECK(disc.image().track(43).sector(0xC1)->data == Bytes(512, 'L'));
        // Here the files are whole.
        for (int index = 0; index < 12; ++index) {
            const auto files = disc.files();
            CHECK(files.size() == 12 && disc.read(files[static_cast<size_t>(index)]) == golden::fill(index));
        }
        const auto files = CpcDisc(*theirs).files();
        CHECK(files.size() == 12 && files[11].name == "FILL11.DAT");
    }

    return checkSummary("disc_golden");
}
