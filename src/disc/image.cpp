#include "disc/image.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace tuxdsk {

namespace {

constexpr size_t kHeaderSize = 0x100;
constexpr size_t kSectorTable = 0x18;  // where a track's header lists its sectors
constexpr size_t kSectorEntry = 8;
constexpr int kMostSectors = 29;       // what the list has room for
constexpr int kFormattedSectors = 9;

const char kStandardMark[] = "MV - CPCEMU Disk-File\r\nDisk-Info\r\n";
const char kTrackMark[] = "Track-Info\r\n";

// The order nine sectors numbered from `first` are formatted in: every
// other one, so that the next in number has gone by when the first is done.
uint8_t interleavedId(int first, int count, int place)
{
    const int step = place % 2 == 0 ? place / 2 : (count + 1) / 2 + place / 2;
    return static_cast<uint8_t>(first + step);
}

Track formattedTrack(int number, int side, uint8_t firstId)
{
    Track track;
    track.header.assign(kHeaderSize, 0);
    std::memcpy(track.header.data(), kTrackMark, sizeof kTrackMark - 1);
    track.header[0x10] = static_cast<uint8_t>(number);
    track.header[0x11] = static_cast<uint8_t>(side);
    track.header[0x14] = 2;
    track.header[0x15] = kFormattedSectors;
    track.header[0x16] = 0x4E;
    track.header[0x17] = 0xE5;
    for (int place = 0; place < kFormattedSectors; ++place) {
        Sector sector;
        sector.track = static_cast<uint8_t>(number);
        sector.side = static_cast<uint8_t>(side);
        sector.id = interleavedId(firstId, kFormattedSectors, place);
        sector.size = 2;
        sector.declared = kSectorSize;
        sector.data.assign(kSectorSize, 0xE5);
        track.sectors.push_back(std::move(sector));
    }
    return track;
}

// A track's bytes: its header with its sectors listed as they are now, the
// sectors, and what followed them.
Bytes trackBytes(const Track& track)
{
    Bytes bytes = track.header;
    const int listed = std::min(static_cast<int>(track.sectors.size()), kMostSectors);
    // The count is the header's own unless the sectors are no longer those
    // it listed.
    if (std::min<int>(bytes[0x15], kMostSectors) != listed)
        bytes[0x15] = static_cast<uint8_t>(listed);
    for (int place = 0; place < listed; ++place) {
        const Sector& sector = track.sectors[static_cast<size_t>(place)];
        uint8_t* entry = &bytes[kSectorTable + static_cast<size_t>(place) * kSectorEntry];
        entry[0] = sector.track;
        entry[1] = sector.side;
        entry[2] = sector.id;
        entry[3] = sector.size;
        entry[4] = sector.status1;
        entry[5] = sector.status2;
        entry[6] = static_cast<uint8_t>(sector.declared & 0xFF);
        entry[7] = static_cast<uint8_t>(sector.declared >> 8);
    }
    for (const Sector& sector : track.sectors)
        bytes.insert(bytes.end(), sector.data.begin(), sector.data.end());
    bytes.insert(bytes.end(), track.tail.begin(), track.tail.end());
    return bytes;
}

// A track out of the `room` bytes the file gives it.
Track parseTrack(std::span<const uint8_t> room, bool extended)
{
    Track track;
    if (room.size() < kHeaderSize)
        return track;
    track.header.assign(room.begin(), room.begin() + kHeaderSize);
    const int count = std::min<int>(track.header[0x15], kMostSectors);
    size_t at = kHeaderSize;
    for (int place = 0; place < count; ++place) {
        const uint8_t* entry = &track.header[kSectorTable + static_cast<size_t>(place) * kSectorEntry];
        Sector sector;
        sector.track = entry[0];
        sector.side = entry[1];
        sector.id = entry[2];
        sector.size = entry[3];
        sector.status1 = entry[4];
        sector.status2 = entry[5];
        sector.declared = static_cast<uint16_t>(entry[6] | entry[7] << 8);
        // An extended file says how long each sector is; the other gives
        // every sector the size its track says.
        size_t length = extended ? sector.declared : size_t{128} << std::min<int>(track.header[0x14], 6);
        length = std::min(length, room.size() - at);
        sector.data.assign(room.begin() + static_cast<std::ptrdiff_t>(at),
                           room.begin() + static_cast<std::ptrdiff_t>(at + length));
        at += length;
        track.sectors.push_back(std::move(sector));
    }
    // The header counts the sectors it lists: it is written back with the
    // count it had, whatever that was.
    track.tail.assign(room.begin() + static_cast<std::ptrdiff_t>(at), room.end());
    return track;
}

bool fail(std::string* error, const char* why)
{
    if (error)
        *error = why;
    return false;
}

}  // namespace

int Track::firstId() const
{
    int first = -1;
    for (const Sector& sector : sectors)
        if (first < 0 || sector.id < first)
            first = sector.id;
    return first;
}

const Sector* Track::sector(uint8_t id) const
{
    for (const Sector& sector : sectors)
        if (sector.id == id)
            return &sector;
    return nullptr;
}

Sector* Track::sector(uint8_t id)
{
    return const_cast<Sector*>(std::as_const(*this).sector(id));
}

DiscImage DiscImage::formatted(int tracks, uint8_t firstId)
{
    DiscImage image;
    image.tracks_ = std::clamp(tracks, 1, 255);
    image.sides_ = 1;
    image.header_.assign(kHeaderSize, 0);
    std::memcpy(image.header_.data(), kStandardMark, sizeof kStandardMark - 1);
    // The room a track takes: nine sectors and their header.
    const size_t room = kHeaderSize + kFormattedSectors * kSectorSize;
    image.header_[0x32] = static_cast<uint8_t>(room & 0xFF);
    image.header_[0x33] = static_cast<uint8_t>(room >> 8);
    for (int number = 0; number < image.tracks_; ++number)
        image.content_.push_back(formattedTrack(number, 0, firstId));
    return image;
}

DiscImage DiscImage::fromSectors(int tracks, int sides, std::vector<std::vector<Sector>> sectors)
{
    static const char kExtendedMark[] = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";
    static const char kCreator[] = "TuxDSK";
    DiscImage image;
    image.extended_ = true;
    image.tracks_ = std::clamp(tracks, 1, 255);
    image.sides_ = std::clamp(sides, 1, 2);
    image.header_.assign(kHeaderSize, 0);
    std::memcpy(image.header_.data(), kExtendedMark, sizeof kExtendedMark - 1);
    std::memcpy(&image.header_[0x22], kCreator, sizeof kCreator - 1);
    sectors.resize(static_cast<size_t>(image.tracks_) * static_cast<size_t>(image.sides_));
    for (size_t index = 0; index < sectors.size(); ++index) {
        Track track;
        if (!sectors[index].empty()) {
            track.header.assign(kHeaderSize, 0);
            std::memcpy(track.header.data(), kTrackMark, sizeof kTrackMark - 1);
            track.header[0x10] = static_cast<uint8_t>(index / static_cast<size_t>(image.sides_));
            track.header[0x11] = static_cast<uint8_t>(index % static_cast<size_t>(image.sides_));
            track.header[0x14] = sectors[index].front().size;
            track.header[0x16] = 0x4E;
            track.header[0x17] = 0xE5;
            track.sectors = std::move(sectors[index]);
            for (Sector& sector : track.sectors)
                sector.declared = static_cast<uint16_t>(sector.data.size());
        }
        image.content_.push_back(std::move(track));
    }
    return image;
}

std::optional<DiscImage> DiscImage::parse(std::span<const uint8_t> file, std::string* error)
{
    const auto no = [error](const char* why) {
        fail(error, why);
        return std::nullopt;
    };
    if (file.size() < kHeaderSize)
        return no("the file is too short to be a disc image");
    const bool extended = std::memcmp(file.data(), "EXTENDED", 8) == 0;
    // ManageDsk knows the other kind by its first four characters.
    if (!extended && std::memcmp(file.data(), "MV -", 4) != 0)
        return no("the file is not a .DSK disc image");

    DiscImage image;
    image.extended_ = extended;
    image.header_.assign(file.begin(), file.begin() + kHeaderSize);
    image.tracks_ = file[0x30];
    image.sides_ = file[0x31];
    if (image.tracks_ == 0 || image.sides_ < 1 || image.sides_ > 2)
        return no("the disc image has no track");
    const size_t count = static_cast<size_t>(image.tracks_) * static_cast<size_t>(image.sides_);
    // An extended file lists a size for each track, on a byte: there is
    // room for 204 of them.
    if (extended && count > kHeaderSize - 0x34)
        return no("the disc image has more tracks than its header can list");
    const size_t fixed = static_cast<size_t>(file[0x32] | file[0x33] << 8);

    size_t at = kHeaderSize;
    for (size_t index = 0; index < count; ++index) {
        const size_t size = extended ? static_cast<size_t>(file[0x34 + index]) * 256 : fixed;
        const size_t there = std::min(size, file.size() - at);
        // A track the file stops in the middle of is what is left of it; one
        // the file has nothing of is one that was never formatted.
        Track track = there >= kHeaderSize ? parseTrack(file.subspan(at, there), extended) : Track{};
        track.stored = track.formatted() ? there : 0;
        track.listed = extended ? file[0x34 + index] : 0;
        at += track.stored;
        image.content_.push_back(std::move(track));
    }
    image.trailer_.assign(file.begin() + static_cast<std::ptrdiff_t>(at), file.end());
    return image;
}

Bytes DiscImage::bytes() const
{
    Bytes file = header_;
    file[0x30] = static_cast<uint8_t>(tracks_);
    file[0x31] = static_cast<uint8_t>(sides_);

    std::vector<Bytes> tracks;
    tracks.reserve(content_.size());
    size_t longest = 0;
    // The tracks from this one on are as the file had them, to the byte.
    size_t asReadFrom = content_.size();
    for (size_t index = 0; index < content_.size(); ++index) {
        const Track& track = content_[index];
        tracks.push_back(track.formatted() ? trackBytes(track) : Bytes{});
        longest = std::max(longest, tracks.back().size());
    }
    while (asReadFrom > 0 && tracks[asReadFrom - 1].size() == content_[asReadFrom - 1].stored)
        --asReadFrom;

    if (extended_) {
        // Each track with its own size, in units of 256 bytes: the size the
        // file gave it if it still has the bytes it had.
        for (size_t index = 0; index < tracks.size(); ++index) {
            Bytes& track = tracks[index];
            if (track.size() == content_[index].stored) {
                file[0x34 + index] = content_[index].listed;
            } else {
                track.resize((track.size() + 255) / 256 * 256, 0);
                file[0x34 + index] = static_cast<uint8_t>(std::min<size_t>(track.size() / 256, 255));
            }
            file.insert(file.end(), track.begin(), track.end());
        }
    } else {
        // Every track in the same room: the one the file had, or the room
        // the longest track now needs. A file that stopped short still does.
        size_t room = static_cast<size_t>(header_[0x32] | header_[0x33] << 8);
        if (room < longest) {
            room = longest;
            file[0x32] = static_cast<uint8_t>(room & 0xFF);
            file[0x33] = static_cast<uint8_t>(room >> 8);
        }
        for (size_t index = 0; index < tracks.size(); ++index) {
            Bytes& track = tracks[index];
            if (index < asReadFrom)
                track.resize(room, 0);
            file.insert(file.end(), track.begin(), track.end());
        }
    }
    file.insert(file.end(), trailer_.begin(), trailer_.end());
    return file;
}

const Track& DiscImage::track(int number, int side) const
{
    static const Track none;
    if (number < 0 || number >= tracks_ || side < 0 || side >= sides_)
        return none;
    return content_[static_cast<size_t>(number * sides_ + side)];
}

Track& DiscImage::track(int number, int side)
{
    return const_cast<Track&>(std::as_const(*this).track(number, side));
}

const Sector* DiscImage::sector(int track, int side, uint8_t id) const
{
    return this->track(track, side).sector(id);
}

Sector* DiscImage::sector(int track, int side, uint8_t id)
{
    return const_cast<Sector*>(std::as_const(*this).sector(track, side, id));
}

void DiscImage::addTrack(uint8_t firstId)
{
    if (tracks_ >= 255)
        return;
    for (int side = 0; side < sides_; ++side)
        content_.push_back(formattedTrack(tracks_, side, firstId));
    ++tracks_;
}

namespace {

// Whether a track's sectors are numbered one after the other, whatever the
// order they are in: the tracks interleaving is about.
bool numberedInARow(const Track& track)
{
    if (track.sectors.size() < 3)
        return false;
    const int first = track.firstId();
    std::vector<bool> seen(track.sectors.size(), false);
    for (const Sector& sector : track.sectors) {
        const size_t rank = static_cast<size_t>(sector.id - first);
        if (rank >= seen.size() || seen[rank])
            return false;
        seen[rank] = true;
    }
    return true;
}

bool inOrder(const Track& track)
{
    const int first = track.firstId();
    const int count = static_cast<int>(track.sectors.size());
    for (int place = 0; place < count; ++place)
        if (track.sectors[static_cast<size_t>(place)].id != interleavedId(first, count, place))
            return false;
    return true;
}

}  // namespace

bool DiscImage::interleaved() const
{
    for (const Track& track : content_)
        if (numberedInARow(track) && !inOrder(track))
            return false;
    return true;
}

void DiscImage::interleave()
{
    for (Track& track : content_) {
        if (!numberedInARow(track) || inOrder(track))
            continue;
        const int first = track.firstId();
        const int count = static_cast<int>(track.sectors.size());
        std::vector<Sector> ordered;
        for (int place = 0; place < count; ++place)
            ordered.push_back(*track.sector(interleavedId(first, count, place)));
        track.sectors = std::move(ordered);
    }
}

bool DiscImage::operator==(const DiscImage& other) const
{
    return bytes() == other.bytes();
}

}  // namespace tuxdsk
