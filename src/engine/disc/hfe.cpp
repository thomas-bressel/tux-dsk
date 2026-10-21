#include "disc/hfe.h"

#include <algorithm>
#include <cstring>

namespace tuxdsk {

namespace {

constexpr size_t kBlock = 512;
constexpr size_t kHalf = kBlock / 2;  // a side's part of a block
const char kSignature[] = "HXCPICFE";
constexpr int kBitRate = 250;        // kbit/s: what a CPC's controller reads and writes
constexpr uint8_t kCpcDrive = 6;     // the interface the emulator is to be: a CPC's drive
constexpr size_t kLongestSide = 0x7FFF;  // a track's length, both sides, is said on 16 bits

// A field starts with three bytes no data can be mistaken for: &A1 written
// with one clock cell left out. The index mark's are &C2, likewise.
constexpr uint16_t kSyncA1 = 0x4489;
constexpr uint16_t kSyncC2 = 0x5224;
constexpr uint8_t kIndexMark = 0xFC;
constexpr uint8_t kIdMark = 0xFE;
constexpr uint8_t kDataMark = 0xFB;
constexpr uint8_t kDeletedMark = 0xF8;
// A CRC that is to be wrong is the right one with these bits turned over:
// the ones HxC's software turns, for an ID and for data.
constexpr uint16_t kWrongIdCrc = 0x1317;
constexpr uint16_t kWrongDataCrc = 0x2120;

// A track as a CPC's controller formats one, in bytes: a gap, the index
// mark, a gap; then for each sector its ID, a gap, its data and the gap the
// format command was given; and gap bytes to the end.
constexpr size_t kGap4a = 80;
constexpr size_t kSync = 12;
constexpr size_t kGap1 = 50;
constexpr size_t kGap2 = 22;
constexpr uint8_t kGapByte = 0x4E;
constexpr size_t kPreamble = kGap4a + kSync + 4 + kGap1;
// What a sector takes besides its data and the gap after it: two syncs, two
// marks of four bytes, the four bytes of its ID, two CRCs and the gap
// between ID and data.
constexpr size_t kSectorFrame = kSync + 4 + 4 + 2 + kGap2 + kSync + 4 + 2;

uint8_t turned(uint8_t byte)
{
    uint8_t other = 0;
    for (int bit = 0; bit < 8; ++bit, byte >>= 1)
        other = static_cast<uint8_t>(other << 1 | (byte & 1));
    return other;
}

uint16_t crcWith(uint16_t crc, uint8_t byte)
{
    crc ^= static_cast<uint16_t>(byte << 8);
    for (int bit = 0; bit < 8; ++bit)
        crc = static_cast<uint16_t>(crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1);
    return crc;
}

size_t dataSize(const Sector& sector)
{
    return size_t{128} << (sector.size & 7);
}

// ---- Writing ---------------------------------------------------------------------

// One side of a track being written: its bit cells, the first in the top
// bit of each byte. A bit of data is a clock cell and the bit; the clock is
// set between two zeros.
class MfmTrack {
public:
    void byte(uint8_t value)
    {
        unsigned cells = 0;
        for (int bit = 7; bit >= 0; --bit) {
            const unsigned data = value >> bit & 1u;
            cells = cells << 2 | (!last_ && !data ? 2u : 0u) | data;
            last_ = data;
        }
        push(static_cast<uint16_t>(cells));
        crc_ = crcWith(crc_, value);
    }
    void bytes(uint8_t value, size_t count)
    {
        for (; count > 0; --count)
            byte(value);
    }
    // A sync byte: its cells as they are, a clock missing.
    void sync(uint16_t cells, uint8_t value)
    {
        push(cells);
        last_ = value & 1u;
        crc_ = crcWith(crc_, value);
    }
    // A field: twelve bytes of 0 for the controller to lock on, the three
    // sync bytes and the mark. Its CRC counts from the first sync byte.
    void field(uint16_t sync3, uint8_t syncValue, uint8_t mark)
    {
        bytes(0x00, kSync);
        crc_ = 0xFFFF;
        for (int i = 0; i < 3; ++i)
            sync(sync3, syncValue);
        byte(mark);
    }
    // The CRC of the field so far; or a wrong one, the right one with some
    // of its bits turned over.
    void crc(uint16_t turnedOver = 0)
    {
        const uint16_t value = static_cast<uint16_t>(crc_ ^ turnedOver);
        byte(static_cast<uint8_t>(value >> 8));
        byte(static_cast<uint8_t>(value & 0xFF));
    }
    size_t size() const { return cells_.size(); }
    Bytes take() { return std::move(cells_); }

private:
    void push(uint16_t cells)
    {
        cells_.push_back(static_cast<uint8_t>(cells >> 8));
        cells_.push_back(static_cast<uint8_t>(cells & 0xFF));
    }
    Bytes cells_;
    unsigned last_ = 0;
    uint16_t crc_ = 0xFFFF;
};

// The gap after each sector of a track: the one its header says. A header
// that says &FF leaves it to be worked out: the usual one for so many
// sectors of that size, or else what room a turn of the disc leaves.
size_t gapOf(const Track& track)
{
    if (track.gap() != 0xFF)
        return track.gap();
    struct Usual {
        size_t size;
        size_t count;
        size_t gap;
    };
    static const Usual kUsual[] = {{256, 18, 0x0C}, {256, 16, 0x32}, {512, 8, 0x50},  {512, 9, 0x50},  {1024, 4, 0xF0},
                                   {2048, 2, 0xF0}, {4096, 1, 0xF0}, {256, 26, 0x36}, {512, 15, 0x54}, {512, 18, 0x6C},
                                   {1024, 8, 0x74}, {2048, 4, 0xF0}, {4096, 2, 0xF0}, {8192, 1, 0xF0}};
    size_t content = kPreamble;
    bool alike = true;
    for (const Sector& sector : track.sectors) {
        content += kSectorFrame + dataSize(sector);
        alike = alike && dataSize(sector) == dataSize(track.sectors.front());
    }
    for (const Usual& usual : kUsual)
        if (alike && !track.sectors.empty() && usual.size == dataSize(track.sectors.front())
            && usual.count == track.sectors.size())
            return usual.gap;
    const size_t turn = 6250;  // bytes in a fifth of a second at 250 kbit/s
    if (track.sectors.empty() || content >= turn)
        return 0;
    return std::min<size_t>((turn - content) / track.sectors.size(), 200);
}

// How many cells a track is given, reckoned as HxC's software reckons it,
// so that a track is as long as the one it makes: its content, made up to
// a fifth of a second (a turn at 300 rpm) with time counted in hundredths
// of a millisecond, each part's rounded down, and to a multiple of 32
// cells. The content is counted with two bytes more a sector than it has.
size_t cellsOf(const Track& track, size_t gap)
{
    constexpr int kTurn = 20000;
    size_t bytes = kPreamble * 2;
    int period = static_cast<int>(100000.0f * (static_cast<float>(bytes * 4) / 250000.0f));
    for (const Sector& sector : track.sectors) {
        const size_t part = (kSectorFrame + 2 + gap + dataSize(sector)) * 2;
        bytes += part;
        period += static_cast<int>(10000000.0f * (static_cast<float>(part * 4) / 25000000.0f));
    }
    size_t cells = bytes * 8;
    if (period < kTurn)
        cells += static_cast<size_t>(kTurn - period) * 5;
    return (cells + 31) / 32 * 32;
}

// One side of a track as cells.
Bytes sideCells(const Track& track)
{
    // A track that was never formatted is a turn of the disc with nothing a
    // controller can find on it: no mark, gap bytes all round. (HxC's
    // software puts random cells there, as many.)
    if (!track.formatted()) {
        MfmTrack blank;
        blank.bytes(kGapByte, 6250);
        return blank.take();
    }
    const size_t gap = gapOf(track);
    const size_t length = cellsOf(track, gap) / 8;
    MfmTrack mfm;
    // One that was formatted with no sector has no index mark either: HxC's
    // software makes it gap bytes all round too.
    if (track.sectors.empty()) {
        while (mfm.size() < length)
            mfm.byte(kGapByte);
        return mfm.take();
    }
    // What the image holds for the track, sector after sector. An image
    // may hold less for a sector than its ID says: the dump of a disc whose
    // sectors run into one another, as copy protections make them. Its
    // data then goes on with what follows it in the image, as HxC's
    // software has it, and with the track's filler after that.
    Bytes held;
    std::vector<size_t> heldAt;
    for (const Sector& sector : track.sectors) {
        heldAt.push_back(held.size());
        held.insert(held.end(), sector.data.begin(), sector.data.end());
    }
    held.insert(held.end(), track.tail.begin(), track.tail.end());

    mfm.bytes(kGapByte, kGap4a);
    mfm.field(kSyncC2, 0xC2, kIndexMark);
    mfm.bytes(kGapByte, kGap1);
    for (size_t place = 0; place < track.sectors.size(); ++place) {
        const Sector& sector = track.sectors[place];
        // What the controller said when the sector was read: an error it
        // found in the ID, or in the data; data marked as deleted.
        const bool error = sector.status1 & 0x20;
        const bool inData = sector.status2 & 0x20;
        mfm.field(kSyncA1, 0xA1, kIdMark);
        mfm.byte(sector.track);
        mfm.byte(sector.side);
        mfm.byte(sector.id);
        mfm.byte(sector.size);
        mfm.crc(error && !inData ? kWrongIdCrc : 0);
        mfm.bytes(kGapByte, kGap2);
        mfm.field(kSyncA1, 0xA1, sector.status2 & 0x40 ? kDeletedMark : kDataMark);
        // As many bytes as the ID says. An image that has more has the
        // same sector read several times: the first is the one written.
        const size_t size = dataSize(sector);
        for (size_t at = heldAt[place]; at < heldAt[place] + size; ++at)
            mfm.byte(at < held.size() ? held[at] : track.filler());
        mfm.crc(error && inData ? kWrongDataCrc : 0);
        mfm.bytes(kGapByte, gap);
    }
    while (mfm.size() < length)
        mfm.byte(kGapByte);
    Bytes cells = mfm.take();
    cells.resize(std::min(cells.size(), kLongestSide));
    return cells;
}

// A side's cells in the room its blocks give it: what is left after them
// goes on as their last two bytes do, the gap that ends the track.
Bytes inRoom(Bytes cells, size_t room)
{
    cells.resize(room, 0);
    size_t last = room;
    for (size_t at = room; at-- > 1 && cells[at] == 0;)
        last = at;
    for (size_t at = last; last > 0 && at < room;) {
        if (last >= 2)
            cells[at++] = cells[last - 2];
        if (at < room)
            cells[at++] = cells[last - 1];
    }
    return cells;
}

void putWord(Bytes& bytes, size_t at, size_t value)
{
    bytes[at] = static_cast<uint8_t>(value & 0xFF);
    bytes[at + 1] = static_cast<uint8_t>(value >> 8);
}

// ---- Reading ---------------------------------------------------------------------

// One side of a track as it is read: cells, the track going round.
class Cells {
public:
    explicit Cells(Bytes cells)
        : cells_(std::move(cells))
    {
    }
    size_t count() const { return cells_.size() * 8; }
    // Sixteen cells from a place.
    uint16_t word(size_t at) const
    {
        uint16_t value = 0;
        for (int i = 0; i < 16; ++i, ++at)
            value = static_cast<uint16_t>(value << 1 | cell(at));
        return value;
    }
    // The byte of data in those sixteen cells: every second one.
    uint8_t byte(size_t at) const
    {
        uint8_t value = 0;
        for (size_t i = 0; i < 8; ++i)
            value = static_cast<uint8_t>(value << 1 | cell(at + 1 + i * 2));
        return value;
    }

private:
    unsigned cell(size_t at) const
    {
        at %= count();
        return cells_[at >> 3] >> (7 - (at & 7)) & 1u;
    }
    Bytes cells_;
};

// The sectors of a side: each ID found, with the data that follows it.
std::vector<Sector> sectorsOf(const Cells& cells)
{
    std::vector<Sector> sectors;
    const size_t count = cells.count();
    if (count < 16 * 64)
        return sectors;
    bool haveId = false;
    bool idGood = false;
    Sector id;
    for (size_t at = 0; at < count;) {
        if (cells.word(at) != kSyncA1) {
            ++at;
            continue;
        }
        while (cells.word(at) == kSyncA1 && at < 2 * count)
            at += 16;
        const uint8_t mark = cells.byte(at);
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < 3; ++i)
            crc = crcWith(crc, 0xA1);
        crc = crcWith(crc, mark);
        if (mark == kIdMark) {
            uint8_t field[6];
            for (size_t i = 0; i < 6; ++i) {
                field[i] = cells.byte(at + 16 * (1 + i));
                crc = crcWith(crc, field[i]);
            }
            id = Sector();
            id.track = field[0];
            id.side = field[1];
            id.id = field[2];
            id.size = field[3];
            idGood = crc == 0;
            haveId = true;
            at += 16 * 7;
        } else if ((mark == kDataMark || mark == kDeletedMark) && haveId) {
            const size_t size = dataSize(id);
            Sector sector = id;
            sector.data.resize(size);
            for (size_t i = 0; i < size; ++i) {
                sector.data[i] = cells.byte(at + 16 * (1 + i));
                crc = crcWith(crc, sector.data[i]);
            }
            for (size_t i = 0; i < 2; ++i)
                crc = crcWith(crc, cells.byte(at + 16 * (1 + size + i)));
            // As the controller would report them: a bad CRC in the ID or
            // in the data, a deleted data mark.
            const bool dataGood = crc == 0;
            sector.status1 = static_cast<uint8_t>(!idGood || !dataGood ? 0x20 : 0);
            sector.status2 = static_cast<uint8_t>((idGood && !dataGood ? 0x20 : 0) | (mark == kDeletedMark ? 0x40 : 0));
            sectors.push_back(std::move(sector));
            haveId = false;
            at += 16 * (1 + size);
        } else {
            at += 16;
        }
    }
    return sectors;
}

}  // namespace

bool isHfe(std::span<const uint8_t> file)
{
    return file.size() >= sizeof kSignature - 1 && std::memcmp(file.data(), kSignature, sizeof kSignature - 1) == 0;
}

Bytes hfeFile(const DiscImage& image)
{
    const size_t tracks = static_cast<size_t>(image.tracks());
    const int sides = image.sides();
    // What the disc is: unused bytes are &FF.
    Bytes file(kBlock, 0xFF);
    std::memcpy(file.data(), kSignature, sizeof kSignature - 1);
    file[8] = 0;  // the first kind of .HFE file
    file[9] = static_cast<uint8_t>(tracks);
    file[10] = static_cast<uint8_t>(sides);
    file[11] = 0;  // MFM, as a PC's
    putWord(file, 12, kBitRate);
    putWord(file, 14, 0);  // the speed of the drive: not said
    file[16] = kCpcDrive;
    file[17] = 1;
    putWord(file, 18, 1);  // the list of tracks is the next block

    // Where each track is, in blocks, and how long, both sides counted.
    const size_t listBlocks = (tracks + 1) * 4 / kBlock + 1;
    Bytes list(listBlocks * kBlock, 0xFF);
    Bytes body;
    size_t block = 1 + listBlocks;
    for (size_t number = 0; number < tracks; ++number) {
        Bytes side[2];
        for (int each = 0; each < sides; ++each)
            side[each] = sideCells(image.track(static_cast<int>(number), each));
        const size_t length = std::max(side[0].size(), side[1].size()) * 2;
        putWord(list, number * 4, block);
        putWord(list, number * 4 + 2, length);
        const size_t blocks = (length + kBlock - 1) / kBlock;
        // The two sides take turns, half a block each; a byte's first cell
        // goes in its lowest bit. A side the disc does not have is 0.
        for (int each = 0; each < 2; ++each)
            side[each] = inRoom(std::move(side[each]), blocks * kHalf);
        for (size_t at = 0; at < blocks; ++at)
            for (int each = 0; each < 2; ++each)
                for (size_t i = 0; i < kHalf; ++i)
                    body.push_back(turned(side[each][at * kHalf + i]));
        block += blocks;
    }
    file.insert(file.end(), list.begin(), list.end());
    file.insert(file.end(), body.begin(), body.end());
    return file;
}

std::optional<DiscImage> parseHfe(std::span<const uint8_t> file, ImageError* error)
{
    const auto no = [error](ImageError why) {
        if (error)
            *error = why;
        return std::nullopt;
    };
    if (!isHfe(file) || file.size() < 2 * kBlock)
        return no(ImageError::NotHfe);
    const int tracks = file[9];
    const int sides = file[10];
    const size_t list = static_cast<size_t>(file[18] | file[19] << 8) * kBlock;
    // Its encoding byte: 0 is the PC's MFM, which is all a CPC's controller
    // writes; &FF is "not said".
    if (file[8] != 0 || (file[11] != 0 && file[11] != 0xFF))
        return no(ImageError::HfeKind);
    if (tracks == 0 || sides < 1 || sides > 2 || list + static_cast<size_t>(tracks) * 4 > file.size())
        return no(ImageError::HfeNoTrack);

    std::vector<std::vector<Sector>> sectors;
    for (int number = 0; number < tracks; ++number) {
        const uint8_t* entry = &file[list + static_cast<size_t>(number) * 4];
        const size_t start = static_cast<size_t>(entry[0] | entry[1] << 8) * kBlock;
        const size_t length =
            std::min<size_t>(static_cast<size_t>(entry[2] | entry[3] << 8), start < file.size() ? file.size() - start : 0);
        Bytes cells[2];
        for (size_t at = 0; at < length; at += kBlock)
            for (size_t side = 0; side < 2; ++side)
                for (size_t i = at + side * kHalf; i < std::min(at + (side + 1) * kHalf, length); ++i)
                    cells[side].push_back(turned(file[start + i]));
        for (int side = 0; side < sides; ++side)
            sectors.push_back(sectorsOf(Cells(std::move(cells[side]))));
    }
    return DiscImage::fromSectors(tracks, sides, std::move(sectors));
}

std::optional<DiscImage> parseDisc(std::span<const uint8_t> file, ImageError* error)
{
    return isHfe(file) ? parseHfe(file, error) : DiscImage::parse(file, error);
}

}  // namespace tuxdsk
