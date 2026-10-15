// Discs as HxC Floppy Emulator's .HFE files, which a Gotek or an HxC drive
// gives a real CPC to read: written against the files the HxC software's
// own converter, hxcfe, made of the same discs (tests/golden/hfe_*, see
// tools/hxc-goldens/run.sh), and read back.

#include <cstring>

#include "check.h"
#include "disc/disc.h"
#include "disc/hfe.h"
#include "golden.h"

using golden::Bytes;
using tuxdsk::DiscImage;

namespace {

Bytes goldenFile(const char* name)
{
    return golden::read(std::string(TUXDSK_GOLDEN_DIR) + "/" + name);
}

int word(const Bytes& file, size_t at)
{
    return file[at] | file[at + 1] << 8;
}

// Whether two images have the same sectors: numbers, bytes and what the
// controller says of them, as far as an .HFE file keeps it.
bool sameSectors(const DiscImage& one, const DiscImage& other)
{
    if (one.tracks() != other.tracks() || one.sides() != other.sides())
        return false;
    for (int track = 0; track < one.tracks(); ++track) {
        for (int side = 0; side < one.sides(); ++side) {
            const auto& mine = one.track(track, side).sectors;
            const auto& theirs = other.track(track, side).sectors;
            if (mine.size() != theirs.size())
                return false;
            for (size_t place = 0; place < mine.size(); ++place) {
                const tuxdsk::Sector& a = mine[place];
                const tuxdsk::Sector& b = theirs[place];
                if (a.track != b.track || a.side != b.side || a.id != b.id || a.size != b.size || a.data != b.data
                    || (a.status1 & 0x20) != (b.status1 & 0x20) || (a.status2 & 0x60) != (b.status2 & 0x60))
                    return false;
            }
        }
    }
    return true;
}

}  // namespace

int main()
{
    // ---- A disc as AMSDOS formats it: hxcfe's file, byte for byte ----
    {
        const auto image = DiscImage::parse(goldenFile("hfe_data.dsk"));
        CHECK(image.has_value());
        const Bytes theirs = goldenFile("hfe_data.hfe");
        const Bytes mine = image ? tuxdsk::hfeFile(*image) : Bytes();
        CHECK_EQ(mine.size(), theirs.size());
        CHECK(mine == theirs);
        // What the file says the disc is.
        CHECK(tuxdsk::isHfe(mine) && std::memcmp(mine.data(), "HXCPICFE", 8) == 0);
        CHECK_EQ(mine[8], 0);            // the first kind of .HFE file
        CHECK_EQ(mine[9], 3);            // tracks
        CHECK_EQ(mine[10], 1);           // sides
        CHECK_EQ(mine[11], 0);           // MFM
        CHECK_EQ(word(mine, 12), 250);   // kbit/s
        CHECK_EQ(mine[16], 6);           // a CPC's drive
        CHECK_EQ(word(mine, 18), 1);     // the list of tracks is the second block
        CHECK_EQ(mine[20], 0xFF);        // may be written to
        // Each track: where it is, in blocks of 512 bytes, and how long.
        // Nine sectors of 512 bytes make 6254 bytes, sixteen cells each,
        // and the other side takes as much room.
        CHECK(word(mine, 512) == 2 && word(mine, 514) == 25016);
        CHECK(word(mine, 516) == 2 + 49 && word(mine, 518) == 25016);
        CHECK_EQ(mine.size(), 512 * (2 + 3 * 49));
        // A side the disc does not have is 0.
        CHECK(mine[1024] != 0 && mine[1024 + 256] == 0 && mine[1024 + 511] == 0);

        // Read back: the sectors the disc had, in their order.
        const auto back = tuxdsk::parseHfe(mine);
        CHECK(back.has_value());
        CHECK(image && back && sameSectors(*image, *back));
        CHECK(back && back->extended() && back->track(2).sectors.size() == 9 && back->track(2).sectors[1].id == 0xC6);
    }

    // ---- What else a disc may hold: hxcfe's file again ----
    {
        const auto image = DiscImage::parse(goldenFile("hfe_odd.dsk"));
        CHECK(image.has_value());
        const Bytes theirs = goldenFile("hfe_odd.hfe");
        const Bytes mine = image ? tuxdsk::hfeFile(*image) : Bytes();
        CHECK(mine == theirs);
        CHECK(mine.size() > 1024 && mine[9] == 2 && mine[10] == 2);

        const auto back = tuxdsk::parseHfe(theirs);
        CHECK(back.has_value());
        if (image && back) {
            CHECK(back->tracks() == 2 && back->sides() == 2);
            // Ten sectors on a track, with the small gap they need.
            CHECK_EQ(back->track(0, 1).sectors.size(), 10);
            // Sectors of 256, 1024 and 128 bytes.
            const auto& mixed = back->track(1, 0).sectors;
            CHECK_EQ(mixed.size(), 6);
            if (mixed.size() == 6) {
                CHECK(mixed[0].data.size() == 256 && mixed[1].data.size() == 1024 && mixed[5].data.size() == 128);
                // Data marked as deleted; a CRC wrong in the data; one wrong in the ID.
                CHECK(mixed[2].status2 == 0x40 && mixed[2].status1 == 0);
                CHECK(mixed[3].status1 == 0x20 && mixed[3].status2 == 0x20);
                CHECK(mixed[4].status1 == 0x20 && mixed[4].status2 == 0x00);
                CHECK(mixed[0].status1 == 0 && mixed[0].status2 == 0);
            }
            // A track formatted with no sector has none to be found.
            CHECK(back->track(1, 1).sectors.empty());
            CHECK(sameSectors(*image, *back));
        }
    }

    // ---- ManageDsk's disc of files: to a real CPC and back ----
    {
        const auto image = DiscImage::parse(golden::disc("files"));
        const Bytes file = tuxdsk::hfeFile(*image);
        CHECK_EQ(file.size(), 512 * (2 + 42 * 49));
        CHECK(file[9] == 42 && file[10] == 1);
        std::string error;
        const auto back = tuxdsk::parseDisc(file, &error);
        CHECK(back.has_value() && error.empty());
        if (back) {
            CHECK(sameSectors(*image, *back));
            const tuxdsk::CpcDisc disc(*back);
            CHECK(disc.format() == tuxdsk::DiscFormat::Data && disc.hasDirectory());
            const auto files = disc.files();
            CHECK_EQ(files.size(), 4);
            CHECK(files.size() == 4 && files[3].name == "K17.DAT" && files[3].records == 133);
            Bytes k17 = golden::input("k17.dat");
            k17.resize(133 * 128, 0);
            CHECK(files.size() == 4 && disc.read(files[3]) == k17);
            // Written again as an .HFE file it is the same file: what was
            // read is all that was written.
            CHECK(tuxdsk::hfeFile(*back) == file);
            // And as a .DSK file it reads as the disc it was.
            const auto again = DiscImage::parse(back->bytes());
            CHECK(again && sameSectors(*image, *again));
        }
        // parseDisc takes a .DSK file too.
        const auto plain = tuxdsk::parseDisc(golden::disc("files"));
        CHECK(plain && !plain->extended() && plain->tracks() == 42);
    }

    // ---- Tracks that were never formatted, a sector the image has less of ----
    {
        std::vector<std::vector<tuxdsk::Sector>> sectors(3);
        tuxdsk::Sector sector;
        sector.track = 0;
        sector.id = 0xC1;
        sector.size = 2;
        sector.data.assign(512, 0x11);
        sectors[0].push_back(sector);
        sector.track = 2;
        sector.data.assign(100, 0x22);  // less than the 512 bytes its ID says
        sectors[2].push_back(sector);
        const DiscImage image = DiscImage::fromSectors(3, 1, sectors);
        CHECK(image.track(0).formatted() && !image.track(1).formatted() && image.extended());
        CHECK(image.track(2).sectors[0].declared == 100);
        const Bytes file = tuxdsk::hfeFile(image);
        // A track never formatted: a turn of the disc, 6250 bytes, with
        // nothing to be found on it.
        CHECK_EQ(word(file, 512 + 4 + 2), 25000);
        const auto back = tuxdsk::parseHfe(file);
        CHECK(back && back->tracks() == 3 && back->track(1).sectors.empty() && !back->track(1).formatted());
        CHECK(back && back->track(0).sectors.size() == 1 && back->track(0).sectors[0].data == Bytes(512, 0x11));
        // The sector is as long as its ID says, made up with the filler.
        CHECK(back && back->track(2).sectors.size() == 1 && back->track(2).sectors[0].data.size() == 512);
        CHECK(back && back->track(2).sectors[0].data[99] == 0x22 && back->track(2).sectors[0].data[100] == 0xE5);
        CHECK(back && back->track(2).sectors[0].status1 == 0);
    }

    // ---- What is not an .HFE file ----
    {
        std::string error;
        CHECK(!tuxdsk::isHfe(golden::disc("empty")) && !tuxdsk::isHfe(Bytes{}));
        CHECK(!tuxdsk::parseHfe(golden::disc("empty"), &error).has_value() && !error.empty());
        Bytes file = goldenFile("hfe_data.hfe");
        // Another kind of .HFE file (version 3 calls itself HXCHFEV3).
        Bytes other = file;
        other[8] = 1;
        error.clear();
        CHECK(!tuxdsk::parseHfe(other, &error).has_value() && error.find("kind") != std::string::npos);
        other = file;
        other[9] = 0;
        CHECK(!tuxdsk::parseHfe(other).has_value());
        // Cut short, it is read as far as it goes.
        file.resize(1024 + 49 * 512 + 3000);
        const auto cut = tuxdsk::parseHfe(file);
        CHECK(cut && cut->tracks() == 3 && cut->track(0).sectors.size() == 9 && cut->track(2).sectors.empty());
        CHECK(!tuxdsk::parseHfe(Bytes(file.begin(), file.begin() + 600)).has_value());
        // Nothing but the signature and noise.
        Bytes noise(4096, 0xA5);
        std::memcpy(noise.data(), "HXCPICFE", 8);
        noise[8] = 0;
        noise[9] = 2;
        noise[10] = 1;
        noise[11] = 0;
        noise[18] = 1;
        noise[19] = 0;
        const auto read = tuxdsk::parseHfe(noise);
        CHECK(!read || read->track(0).sectors.empty());
    }

    return checkSummary("disc_hfe");
}
