#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// A disc image as a .DSK file holds it: tracks, and in each the sectors in
// the order they pass under the head, each with the numbers the controller
// reads in front of it.
//
// Two kinds of file. "MV - CPCEMU Disk-File": every track takes the same
// room, and every sector of a track the size the track says. "EXTENDED CPC
// DSK File": each track has its own size, which may be none (a track never
// formatted), and each sector its own length.
//
// What is read is kept as it was read: an image written back without a
// change is the file it came from, byte for byte, whatever its tracks hold.
// ManageDsk writes an extended image as 9 sectors of 512 bytes a track,
// which loses whatever is not that; see notes/ecarts.md.

namespace tuxdsk {

using Bytes = std::vector<uint8_t>;

constexpr int kSectorSize = 512;
// The number of the first sector of a track says what a disc is for AMSDOS.
constexpr uint8_t kDataFirstId = 0xC1;
constexpr uint8_t kVendorFirstId = 0x41;
constexpr uint8_t kPcwFirstId = 0x01;

struct Sector {
    uint8_t track = 0;    // C, R, H and N: what the sector says it is
    uint8_t side = 0;
    uint8_t id = 0;
    uint8_t size = 2;     // 128 << size bytes
    uint8_t status1 = 0;  // the controller's ST1 and ST2 when it was read
    uint8_t status2 = 0;
    uint16_t declared = 0;  // its length as the file states it
    Bytes data;
};

struct Track {
    // The 256 bytes the file has in front of the track's sectors; none for a
    // track that was never formatted. They list the sectors as the file had
    // them: the list is made again from `sectors` when the image is written.
    Bytes header;
    std::vector<Sector> sectors;
    // What the file holds after the last sector, to the end of the track.
    Bytes tail;
    // The bytes the file had for the track, and the size its header gave
    // it (an extended file's, in units of 256 bytes): a file that stops
    // short, or says a size it does not have, is written back as it was.
    size_t stored = 0;
    uint8_t listed = 0;

    bool formatted() const { return !header.empty(); }
    uint8_t sizeCode() const { return formatted() ? header[0x14] : 0; }
    uint8_t gap() const { return formatted() ? header[0x16] : 0; }
    uint8_t filler() const { return formatted() ? header[0x17] : 0; }
    // The smallest of its sectors' numbers; -1 if it has no sector.
    int firstId() const;
    const Sector* sector(uint8_t id) const;
    Sector* sector(uint8_t id);
};

// Why bytes are not a disc image that can be read: what the windows and the
// command line say, each in its words.
enum class ImageError {
    None,
    TooShort,       // too short to be a disc image
    NotDsk,         // not a .DSK file
    NoTrack,        // a .DSK file without a track
    TooManyTracks,  // more tracks than an extended .DSK file's header can list
    NotHfe,         // not an .HFE file
    HfeKind,        // an .HFE file of a kind that is not read: only the first, in MFM, is
    HfeNoTrack,     // an .HFE file without a track
};

class DiscImage {
public:
    // An empty disc as ManageDsk formats one: one side, nine sectors of 512
    // bytes a track, numbered from `firstId` and interleaved, filled with
    // &E5, in a "MV - CPCEMU" file.
    static DiscImage formatted(int tracks, uint8_t firstId = kDataFirstId);

    // An image made of sectors that come from elsewhere than a .DSK file:
    // those of each track and side, in the order track 0 side 0, track 0
    // side 1, track 1 side 0... A track without a sector is one that was
    // never formatted. The image is of the extended kind, which keeps each
    // sector's length and what the controller said of it.
    static DiscImage fromSectors(int tracks, int sides, std::vector<std::vector<Sector>> sectors);

    // The image a .DSK file holds. Nothing, with the reason, if the bytes
    // are not such a file.
    static std::optional<DiscImage> parse(std::span<const uint8_t> file, ImageError* error = nullptr);
    // The image as a .DSK file, of the kind it was read as.
    Bytes bytes() const;

    bool extended() const { return extended_; }
    int tracks() const { return tracks_; }
    int sides() const { return sides_; }
    const Track& track(int number, int side = 0) const;
    Track& track(int number, int side = 0);
    // A sector by its number, on the track the head is over; null if the
    // track has none of that number.
    const Sector* sector(int track, int side, uint8_t id) const;
    Sector* sector(int track, int side, uint8_t id);

    // One more track, on each side, formatted as formatted() does: what
    // ManageDsk does when a file goes past the last track.
    void addTrack(uint8_t firstId);

    // Whether the sectors pass under the head in the order a CPC formats
    // them (&C1, &C6, &C2, &C7...) wherever their numbers follow one
    // another; interleave() puts them in that order.
    bool interleaved() const;
    void interleave();

    bool operator==(const DiscImage& other) const;

private:
    bool extended_ = false;
    int tracks_ = 0;
    int sides_ = 1;
    Bytes header_;                // the file's first 256 bytes
    std::vector<Track> content_;  // track 0 side 0, track 0 side 1, track 1 side 0...
    Bytes trailer_;               // what the file holds after its last track
};

}  // namespace tuxdsk
