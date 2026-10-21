// Disc images: the empty disc against the one ManageDsk makes, files read
// and written back without a change, of both kinds and with all that a
// .DSK file may hold, tracks added, sectors put in the order a CPC formats
// them, and files that are not disc images.
//
//   engine_disc_image [folder]   reads and writes back every .dsk of the
//                                folder (TUXDSK_DISC_DIR when none is named
//                                and it is set) and counts those that come
//                                back changed

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

#include "check.h"
#include "disc/compare.h"
#include "disc/image.h"
#include "golden.h"

using golden::Bytes;
using tuxdsk::DiscImage;

namespace {

void put(Bytes& file, size_t at, const char* text)
{
    std::memcpy(&file[at], text, std::strlen(text));
}

// An extended image made by hand: two sides, a track never formatted,
// sectors of several sizes, one with an error, room left after the sectors
// and bytes after the last track.
Bytes extendedImage()
{
    Bytes file(0x100, 0);
    put(file, 0, "EXTENDED CPC DSK File\r\nDisk-Info\r\n");
    put(file, 0x22, "TuxDSK test");
    file[0x30] = 2;  // tracks
    file[0x31] = 2;  // sides
    const auto track = [&file](int number, int side, std::vector<std::pair<uint8_t, int>> sectors, size_t room) {
        Bytes bytes(0x100, 0);
        put(bytes, 0, "Track-Info\r\n");
        bytes[0x10] = static_cast<uint8_t>(number);
        bytes[0x11] = static_cast<uint8_t>(side);
        bytes[0x14] = 2;
        bytes[0x15] = static_cast<uint8_t>(sectors.size());
        bytes[0x16] = 0x4E;
        bytes[0x17] = 0xE5;
        for (size_t place = 0; place < sectors.size(); ++place) {
            uint8_t* entry = &bytes[0x18 + place * 8];
            entry[0] = static_cast<uint8_t>(number);
            entry[1] = static_cast<uint8_t>(side);
            entry[2] = sectors[place].first;
            entry[3] = 2;
            entry[4] = place == 1 ? 0x20 : 0;  // a sector read with an error
            entry[5] = place == 1 ? 0x20 : 0;
            entry[6] = static_cast<uint8_t>(sectors[place].second & 0xFF);
            entry[7] = static_cast<uint8_t>(sectors[place].second >> 8);
            for (int at = 0; at < sectors[place].second; ++at)
                bytes.push_back(static_cast<uint8_t>(sectors[place].first + at));
        }
        bytes.resize(room, 0x77);
        file[0x34 + static_cast<size_t>(number * 2 + side)] = static_cast<uint8_t>(room / 256);
        file.insert(file.end(), bytes.begin(), bytes.end());
    };
    track(0, 0, {{0xC1, 512}, {0xC2, 512}, {0xC3, 512}}, 0x800);
    track(0, 1, {{0x01, 256}, {0x02, 1024}, {0x03, 0}}, 0x700);
    track(1, 0, {{0x41, 512}}, 0x300);
    // Track 1 of side 1 was never formatted: its size is 0.
    const char after[] = "something after the tracks";
    file.insert(file.end(), after, after + sizeof after);
    return file;
}

int checkFolder(const std::string& folder)
{
    namespace fs = std::filesystem;
    int read = 0;
    int changed = 0;
    int refused = 0;
    std::error_code error;
    for (fs::recursive_directory_iterator each(folder, fs::directory_options::skip_permission_denied, error), end;
         each != end; each.increment(error)) {
        std::string extension = each->path().extension().string();
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (extension != ".dsk" || !each->is_regular_file(error))
            continue;
        const Bytes file = golden::read(each->path().string());
        const auto image = DiscImage::parse(file);
        if (!image) {
            ++refused;
            continue;
        }
        ++read;
        if (image->bytes() != file) {
            if (++changed <= 10)
                std::printf("  comes back changed: %s\n", each->path().string().c_str());
        }
    }
    std::printf("%s: %d discs read and written back, %d changed, %d files that are not discs\n", folder.c_str(), read,
                changed, refused);
    return changed;
}

}  // namespace

int main(int argc, char* argv[])
{
    // ---- The empty disc: ManageDsk's, byte for byte ----
    const Bytes empty = golden::disc("empty");
    CHECK_EQ(empty.size(), 0x100 + 42 * 0x1300);
    const DiscImage made = DiscImage::formatted(42);
    CHECK(made.bytes() == empty);
    CHECK(!made.extended());
    CHECK_EQ(made.tracks(), 42);
    CHECK_EQ(made.sides(), 1);
    CHECK_EQ(made.track(0).sectors.size(), 9);
    CHECK_EQ(made.track(0).firstId(), 0xC1);
    CHECK_EQ(made.track(0).gap(), 0x4E);
    CHECK_EQ(made.track(0).filler(), 0xE5);
    // Interleaved: &C1, &C6, &C2, &C7...
    const uint8_t order[9] = {0xC1, 0xC6, 0xC2, 0xC7, 0xC3, 0xC8, 0xC4, 0xC9, 0xC5};
    for (size_t place = 0; place < 9; ++place)
        CHECK_EQ(made.track(41).sectors[place].id, order[place]);
    CHECK(made.interleaved());
    CHECK(made.sector(7, 0, 0xC4) && made.sector(7, 0, 0xC4)->data.size() == 512);
    CHECK(made.sector(7, 0, 0x41) == nullptr);
    CHECK(made.sector(42, 0, 0xC1) == nullptr);
    CHECK(made.sector(0, 1, 0xC1) == nullptr);
    CHECK_EQ(DiscImage::formatted(40, tuxdsk::kVendorFirstId).track(39).firstId(), 0x41);

    // ---- ManageDsk's discs come back as they were ----
    for (const char* name : {"empty", "files", "names", "import", "grown"}) {
        const Bytes file = golden::disc(name);
        const auto image = DiscImage::parse(file);
        CHECK(image.has_value());
        CHECK(image && image->bytes() == file);
    }

    // ---- An extended image, with all it may hold ----
    {
        const Bytes file = extendedImage();
        tuxdsk::ImageError error = tuxdsk::ImageError::None;
        const auto image = DiscImage::parse(file, &error);
        CHECK(image.has_value() && error == tuxdsk::ImageError::None);
        if (image) {
            CHECK(image->extended());
            CHECK_EQ(image->tracks(), 2);
            CHECK_EQ(image->sides(), 2);
            CHECK(image->bytes() == file);
            CHECK(image->track(0, 0).formatted() && image->track(1, 0).formatted());
            CHECK(!image->track(1, 1).formatted());
            CHECK_EQ(image->track(1, 1).firstId(), -1);
            // Sectors of 256, 1024 and no bytes.
            const tuxdsk::Track& odd = image->track(0, 1);
            CHECK_EQ(odd.sectors.size(), 3);
            CHECK_EQ(odd.sectors[0].data.size(), 256);
            CHECK_EQ(odd.sectors[1].data.size(), 1024);
            CHECK_EQ(odd.sectors[2].data.size(), 0);
            CHECK_EQ(odd.sectors[1].status1, 0x20);
            CHECK_EQ(odd.sectors[1].data[5], 0x02 + 5);
            CHECK_EQ(odd.tail.size(), 0x700 - 0x100 - 256 - 1024);

            // A sector changed is the only thing that changes in the file.
            DiscImage changed = *image;
            changed.sector(0, 0, 0xC2)->data[10] ^= 0xFF;
            const Bytes after = changed.bytes();
            CHECK_EQ(after.size(), file.size());
            size_t different = 0;
            for (size_t at = 0; at < file.size(); ++at)
                different += after[at] != file[at];
            CHECK_EQ(different, 1);
            CHECK(after[0x100 + 0x100 + 512 + 10] != file[0x100 + 0x100 + 512 + 10]);
            const auto difference = tuxdsk::firstDifference(*image, changed);
            CHECK(difference && difference->kind == tuxdsk::Difference::Bytes && difference->track == 0
                  && difference->side == 0 && difference->sector == 0xC2);
            CHECK(!tuxdsk::firstDifference(*image, *image).has_value());

            // A track more, on both sides, with the size it needs.
            DiscImage grown = *image;
            grown.addTrack(0xC1);
            CHECK_EQ(grown.tracks(), 3);
            const auto again = DiscImage::parse(grown.bytes());
            CHECK(again && again->tracks() == 3 && again->track(2, 1).sectors.size() == 9);
            CHECK(again && again->bytes()[0x34 + 4] == 0x13 && again->bytes()[0x34 + 5] == 0x13);
            CHECK(again && again->track(2, 1).sectors[1].id == 0xC6 && again->track(2, 1).sectors[1].side == 1);
        }
    }

    // ---- A "MV - CPCEMU" image grows with tracks of its own size ----
    {
        DiscImage image = DiscImage::formatted(3);
        image.addTrack(0xC1);
        const Bytes file = image.bytes();
        CHECK_EQ(file.size(), 0x100 + 4 * 0x1300);
        CHECK_EQ(file[0x30], 4);
        CHECK(file == DiscImage::formatted(4).bytes());
    }

    // ---- Sectors in the order of their numbers are put in a CPC's order ----
    {
        DiscImage image = DiscImage::formatted(2);
        tuxdsk::Track& track = image.track(1);
        std::sort(track.sectors.begin(), track.sectors.end(),
                  [](const tuxdsk::Sector& a, const tuxdsk::Sector& b) { return a.id < b.id; });
        track.sector(0xC3)->data[0] = 0x33;
        CHECK(!image.interleaved());
        image.interleave();
        CHECK(image.interleaved());
        CHECK_EQ(image.track(1).sectors[4].id, 0xC3);
        CHECK_EQ(image.track(1).sectors[4].data[0], 0x33);
        CHECK(image.track(0).sectors[1].id == 0xC6);
    }

    // ---- What is not a disc image ----
    {
        tuxdsk::ImageError error = tuxdsk::ImageError::None;
        CHECK(!DiscImage::parse(Bytes(10, 0), &error).has_value() && error == tuxdsk::ImageError::TooShort);
        CHECK(!DiscImage::parse(Bytes(0x2000, 0), &error).has_value() && error == tuxdsk::ImageError::NotDsk);
        Bytes noTrack(0x100, 0);
        put(noTrack, 0, "MV - CPCEMU Disk-File\r\nDisk-Info\r\n");
        CHECK(!DiscImage::parse(noTrack).has_value());
        // A file that stops short is read as far as it goes.
        Bytes cut = golden::disc("files");
        cut.resize(0x100 + 3 * 0x1300 + 0x500);
        const auto image = DiscImage::parse(cut);
        CHECK(image && image->tracks() == 42);
        CHECK(image && image->track(2).sectors.size() == 9 && image->track(3).sectors.size() == 9);
        CHECK(image && image->track(3).sectors[2].data.size() == 0 && !image->track(4).formatted());
    }

    // ---- A folder of discs, when there is one ----
    const std::string folder = argc > 1 ? argv[1] : TUXDSK_DISC_DIR;
    if (std::filesystem::is_directory(folder))
        CHECK_EQ(checkFolder(folder), 0);

    return checkSummary("engine_disc_image");
}
