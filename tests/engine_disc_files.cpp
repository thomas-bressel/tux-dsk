// AMSDOS's files on a disc: added, read, erased and brought back, renamed,
// given attributes and other contents; the three formats; a disc that
// fills up, in its directory and in its blocks; and discs AMSDOS's files
// cannot be put on.

#include <numeric>

#include "check.h"
#include "disc/amsdos.h"
#include "disc/disc.h"
#include "golden.h"

using golden::Bytes;
using tuxdsk::AddOptions;
using tuxdsk::AddResult;
using tuxdsk::CpcDisc;
using tuxdsk::DiscFormat;

namespace {

Bytes pattern(size_t size, int step)
{
    Bytes bytes(size);
    for (size_t at = 0; at < size; ++at)
        bytes[at] = static_cast<uint8_t>(at * static_cast<size_t>(step) + 1);
    return bytes;
}

// A file as read back: its bytes, then 0 to the end of its last record.
Bytes padded(Bytes bytes)
{
    bytes.resize((bytes.size() + 127) / 128 * 128, 0);
    return bytes;
}

}  // namespace

int main()
{
    // ---- An empty disc ----
    {
        const CpcDisc disc;
        CHECK(disc.format() == DiscFormat::Data);
        CHECK(disc.hasDirectory());
        CHECK(disc.files().empty() && disc.files(true).empty());
        CHECK_EQ(disc.usedBlocks(), 0);
        CHECK_EQ(disc.freeBlocks(), 187);  // 42 tracks, less the directory
        CHECK_EQ(CpcDisc::formatted(40).freeBlocks(), 178);
        for (const tuxdsk::DirEntry& entry : disc.directory())
            CHECK(entry.free());
        CHECK(std::string(tuxdsk::formatName(disc.format())) == "DATA");
    }

    // ---- Files added and read ----
    CpcDisc disc = CpcDisc::formatted(40);
    const Bytes small = pattern(300, 7);
    const Bytes large = pattern(40000, 11);
    CHECK(disc.add("small.bin", small) == AddResult::Done);
    CHECK(disc.add("large.dat", large) == AddResult::Done);
    CHECK(disc.add("empty.txt", {}) == AddResult::Done);
    {
        const auto files = disc.files();
        CHECK_EQ(files.size(), 3);
        CHECK(files[0].name == "SMALL.BIN" && files[0].records == 3 && files[0].kilobytes() == 1);
        CHECK(files[1].name == "LARGE.DAT" && files[1].records == 313 && files[1].entries.size() == 3);
        CHECK(files[1].kilobytes() == 40 && files[1].length == 313 * 128);
        // A file of nothing is an entry that holds nothing.
        CHECK(files[2].name == "EMPTY.TXT" && files[2].records == 0 && files[2].entries.size() == 1);
        CHECK(disc.read(files[0]) == padded(small));
        CHECK(disc.read(files[1]) == padded(large));
        CHECK(disc.read(files[2]).empty());
        CHECK_EQ(disc.usedBlocks(), 1 + 40);
        CHECK_EQ(disc.freeBlocks(), 178 - 41);
        // The entries, as AMSDOS wants them: 16K each, numbered from 0.
        const auto entries = disc.directory();
        CHECK_EQ(entries[1].extent, 0);
        CHECK_EQ(entries[1].records, 128);
        CHECK_EQ(entries[2].extent, 1);
        CHECK_EQ(entries[3].extent, 2);
        CHECK_EQ(entries[3].records, 313 - 256);
        CHECK_EQ(entries[1].blocks[0], 3);
        CHECK_EQ(entries[3].blocks[7], 42);
        CHECK_EQ(entries[3].blocks[8], 0);
        CHECK(entries[4].records == 0 && entries[4].blocks[0] == 0 && !entries[4].free());
        CHECK(entries[5].free());
    }
    // The same name again, whatever its letters; other users have their own.
    CHECK(disc.add("Small.BIN", small) == AddResult::Exists);
    AddOptions other;
    other.user = 3;
    other.readOnly = true;
    other.hidden = true;
    CHECK(disc.add("small.bin", pattern(10, 1), other) == AddResult::Done);
    CHECK(disc.add("???", small) == AddResult::BadName);
    {
        const auto mine = disc.find("small.bin", 3);
        CHECK(mine && mine->user == 3 && mine->readOnly && mine->hidden && mine->records == 1);
        const auto first = disc.find("SMALL.BIN");
        CHECK(first && first->user == 0 && !first->readOnly && !first->hidden && first->records == 3);
        CHECK(!disc.find("nothere.bin").has_value());
        // The attributes are the high bits of the extension's letters.
        CHECK(mine && disc.directory()[static_cast<size_t>(mine->entries[0])].name[8] == ('B' | 0x80));
        CHECK(mine && disc.directory()[static_cast<size_t>(mine->entries[0])].name[9] == ('I' | 0x80));
    }

    // ---- What a file's first bytes say of it ----
    {
        Bytes program = tuxdsk::makeAmsdosHeader("PROG.BAS", tuxdsk::kAmsdosBasic, 0x170, 0, 20);
        program.resize(128 + 20, 0x55);
        CHECK(disc.add("prog.bas", program) == AddResult::Done);
        const auto file = disc.find("prog.bas");
        CHECK(file && file->type == tuxdsk::FileType::Basic && file->hasHeader());
        CHECK(file && file->loadAddress == 0x170 && file->length == 20 && file->records == 2);
        const auto plain = disc.find("small.bin");
        CHECK(plain && plain->type == tuxdsk::FileType::Ascii && !plain->hasHeader() && plain->length == 3 * 128);
        CHECK(std::string(tuxdsk::typeName(tuxdsk::FileType::ProtectedBinary)) == "BINARY (P)");
        Bytes odd = tuxdsk::makeAmsdosHeader("ODD.XXX", 0x16, 0, 0, 0);
        CHECK(tuxdsk::fileType(odd) == tuxdsk::FileType::Unknown);
    }

    // ---- Attributes, names and users ----
    {
        auto file = *disc.find("large.dat");
        disc.setAttributes(file, true, false);
        file = *disc.find("large.dat");
        CHECK(file.readOnly && !file.hidden && file.entries.size() == 3);
        CHECK(disc.rename(file, "huge.bin", 0) == AddResult::Done);
        CHECK(!disc.find("large.dat").has_value());
        file = *disc.find("huge.bin");
        // The attributes stay, and so does what it holds.
        CHECK(file.readOnly && file.records == 313 && disc.read(file) == padded(large));
        CHECK(disc.rename(file, "small.bin", 0) == AddResult::Exists);
        CHECK(disc.rename(file, "*", 0) == AddResult::BadName);
        CHECK(disc.rename(file, "huge.bin", 0) == AddResult::Done);
        CHECK(disc.rename(file, "huge.bin", 7) == AddResult::Done);
        CHECK(disc.find("huge.bin", 7).has_value() && !disc.find("huge.bin", 0).has_value());
        CHECK(disc.rename(*disc.find("huge.bin", 7), "large.dat", 0) == AddResult::Done);
        disc.setAttributes(*disc.find("large.dat"), false, false);
    }

    // ---- Erased, seen as erased, and brought back ----
    {
        const int used = disc.usedBlocks();
        disc.erase(*disc.find("large.dat"));
        CHECK(!disc.find("large.dat").has_value());
        CHECK_EQ(disc.usedBlocks(), used - 40);
        CHECK_EQ(disc.files().size(), 4);
        const auto all = disc.files(true);
        CHECK_EQ(all.size(), 5);
        const auto erased = disc.find("large.dat", tuxdsk::kErasedUser);
        CHECK(erased && erased->erased && erased->user == 229 && erased->entries.size() == 3);
        // Its blocks were not taken since: it is all there.
        CHECK(erased && disc.read(*erased) == padded(large));
        CHECK(erased && disc.rename(*erased, "large.dat", 0) == AddResult::Done);
        const auto back = disc.find("large.dat");
        CHECK(back && !back->erased && disc.read(*back) == padded(large));
        CHECK_EQ(disc.usedBlocks(), used);
        // For good, nothing is left of it.
        disc.erase(*back, true);
        CHECK_EQ(disc.files(true).size(), 4);
        CHECK(!disc.find("large.dat", tuxdsk::kErasedUser).has_value());
        // The room is taken by the next file, from the first block free.
        CHECK(disc.add("next.dat", pattern(2000, 3)) == AddResult::Done);
        const auto next = disc.find("next.dat");
        CHECK(next && next->entries[0] == 1 && disc.directory()[1].blocks[0] == 3 && disc.directory()[1].blocks[1] == 4);
    }

    // ---- Other contents ----
    {
        // As long: in the same blocks.
        const auto before = *disc.find("small.bin");
        const uint8_t block = disc.directory()[static_cast<size_t>(before.entries[0])].blocks[0];
        CHECK(disc.replace(before, pattern(290, 5)) == AddResult::Done);
        auto after = *disc.find("small.bin");
        CHECK(after.entries == before.entries && after.records == 3);
        CHECK(disc.directory()[static_cast<size_t>(after.entries[0])].blocks[0] == block);
        CHECK(disc.read(after) == padded(pattern(290, 5)));
        // Longer: as a file added in its place, with its user and attributes.
        const auto third = *disc.find("small.bin", 3);
        CHECK(disc.replace(third, pattern(5000, 9)) == AddResult::Done);
        after = *disc.find("small.bin", 3);
        CHECK(after.user == 3 && after.readOnly && after.hidden && after.records == 40);
        CHECK(disc.read(after) == padded(pattern(5000, 9)));
        // Nothing of the one it took the place of is left to be seen.
        CHECK(!disc.find("small.bin", tuxdsk::kErasedUser).has_value());
        // Too long for the disc: it stays as it was.
        CHECK(disc.replace(after, Bytes(200 * 1024, 1)) == AddResult::NoSpace);
        after = *disc.find("small.bin", 3);
        CHECK(after.records == 40 && disc.read(after) == padded(pattern(5000, 9)));
    }

    // ---- The disc fills up ----
    {
        // The window stops at block 188: 187K on a disc of 40 tracks, which
        // grows to 42 for them.
        CpcDisc full = CpcDisc::formatted(40);
        CHECK(full.add("a.dat", Bytes(100 * 1024, 1)) == AddResult::Done);
        CHECK_EQ(full.image().tracks(), 40);
        CHECK(full.add("b.dat", Bytes(87 * 1024, 2)) == AddResult::Done);
        CHECK_EQ(full.image().tracks(), 42);
        CHECK(full.add("c.dat", Bytes(1, 3)) == AddResult::NoSpace);
        CHECK_EQ(full.freeBlocks(), 0);
        CHECK_EQ(full.usedBlocks(), 187);
        const auto b = full.find("b.dat");
        CHECK(b && full.read(*b) == Bytes(87 * 1024, 2));
        // With more than 42 tracks allowed, to block 255.
        AddOptions options;
        options.blocks = tuxdsk::kAllBlocks;
        CHECK(full.add("c.dat", Bytes(67 * 1024, 3), options) == AddResult::Done);
        CHECK_EQ(full.image().tracks(), 57);
        CHECK(full.add("d.dat", Bytes(1, 4), options) == AddResult::NoSpace);
        const auto c = full.find("c.dat");
        CHECK(c && full.read(*c) == Bytes(67 * 1024, 3));
        // What does not go in leaves the disc as it was.
        CHECK(full.image().bytes().size() == 0x100 + 57 * 0x1300);

        // 64 entries: the 65th file does not go in.
        CpcDisc many = CpcDisc::formatted(40);
        for (int index = 0; index < 64; ++index)
            CHECK(many.add("f" + std::to_string(index) + ".x", Bytes(1, 1)) == AddResult::Done);
        CHECK(many.add("last.x", Bytes(1, 1)) == AddResult::NoEntry);
        CHECK_EQ(many.files().size(), 64);
        // A file that needs two entries when one is left.
        many.erase(*many.find("f10.x"), true);
        const Bytes before = many.image().bytes();
        CHECK(many.add("two.x", Bytes(17000, 1)) == AddResult::NoEntry);
        CHECK(many.image().bytes() == before);
    }

    // ---- The VENDOR and PCW formats: tracks kept for the system ----
    for (const DiscFormat format : {DiscFormat::Vendor, DiscFormat::Pcw}) {
        CpcDisc system = CpcDisc::formatted(40, format);
        const int reserved = format == DiscFormat::Vendor ? 2 : 1;
        const uint8_t first = format == DiscFormat::Vendor ? 0x41 : 0x01;
        CHECK(system.format() == format && system.hasDirectory());
        CHECK_EQ(system.freeBlocks(), (40 - reserved) * 9 / 2 - 2);
        CHECK(system.add("file.bin", pattern(1500, 3)) == AddResult::Done);
        // The directory is on the first track after those kept, and the
        // file's first block (the third) on that track's fifth sector.
        const tuxdsk::Sector* directory = system.image().sector(reserved, 0, first);
        CHECK(directory && directory->data[0] == 0 && directory->data[1] == 'F' && directory->data[16] == 2);
        const tuxdsk::Sector* data = system.image().sector(reserved, 0, static_cast<uint8_t>(first + 4));
        CHECK(data && data->data[0] == 1 && data->data[1] == 4);
        for (int track = 0; track < reserved; ++track)
            for (const tuxdsk::Sector& sector : system.image().track(track).sectors)
                CHECK(sector.data == Bytes(512, 0xE5));
        const auto file = system.find("file.bin");
        CHECK(file && system.read(*file) == padded(pattern(1500, 3)));
    }

    // ---- Discs AMSDOS's files cannot be put on ----
    {
        CpcDisc odd(tuxdsk::DiscImage::formatted(40, 0x11));
        CHECK(odd.format() == DiscFormat::Unknown && !odd.hasDirectory());
        CHECK(odd.files().empty());
        CHECK_EQ(odd.freeBlocks(), 0);
        CHECK(odd.add("a.bin", Bytes(10, 1)) == AddResult::NoDisc);
        // A DATA disc whose directory track lost a sector.
        tuxdsk::DiscImage image = tuxdsk::DiscImage::formatted(40);
        image.track(0).sectors.erase(image.track(0).sectors.begin() + 2);  // &C2
        CpcDisc broken(image);
        CHECK(broken.format() == DiscFormat::Data && !broken.hasDirectory());
        CHECK(broken.add("a.bin", Bytes(10, 1)) == AddResult::NoDisc);
        // One that lost a sector further on: the block it was half of is
        // passed over.
        image = tuxdsk::DiscImage::formatted(40);
        image.track(0).sectors.erase(image.track(0).sectors.begin() + 1);  // &C6: block 2's second half
        CpcDisc holed(image);
        CHECK(holed.hasDirectory());
        CHECK(holed.add("a.bin", pattern(1024, 3)) == AddResult::Done);
        CHECK_EQ(holed.directory()[0].blocks[0], 3);
        CHECK(holed.read(*holed.find("a.bin")) == pattern(1024, 3));
    }

    return checkSummary("engine_disc_files");
}
