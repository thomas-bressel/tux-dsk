#include "disc/disc.h"

#include <algorithm>

#include "disc/amsdos.h"
#include "disc/names.h"

namespace tuxdsk {

namespace {

constexpr int kTrackSectors = 9;
constexpr int kEntrySize = 32;
constexpr int kEntriesInSector = kSectorSize / kEntrySize;
constexpr int kExtentRecords = 128;  // 16K

DirEntry entryFrom(const uint8_t* bytes)
{
    DirEntry entry;
    entry.user = bytes[0];
    std::copy(bytes + 1, bytes + 12, entry.name.begin());
    entry.extent = bytes[12];
    entry.reserved1 = bytes[13];
    entry.reserved2 = bytes[14];
    entry.records = bytes[15];
    std::copy(bytes + 16, bytes + 32, entry.blocks.begin());
    return entry;
}

void entryTo(const DirEntry& entry, uint8_t* bytes)
{
    bytes[0] = entry.user;
    std::copy(entry.name.begin(), entry.name.end(), bytes + 1);
    bytes[12] = entry.extent;
    bytes[13] = entry.reserved1;
    bytes[14] = entry.reserved2;
    bytes[15] = entry.records;
    std::copy(entry.blocks.begin(), entry.blocks.end(), bytes + 16);
}

std::array<uint8_t, 11> plainName(const DirEntry& entry)
{
    std::array<uint8_t, 11> name = entry.name;
    for (uint8_t& c : name)
        c &= 0x7F;
    return name;
}

// An entry nothing was ever written in, as formatting leaves it.
bool neverUsed(const DirEntry& entry)
{
    return entry.user == kErasedUser && entry.extent == kErasedUser
           && std::all_of(entry.name.begin(), entry.name.end(), [](uint8_t c) { return c == kErasedUser; });
}

// Where an entry comes among those of its file.
int extentNumber(const DirEntry& entry)
{
    return entry.reserved2 * 32 + entry.extent;
}

int blocksOf(const DirEntry& entry)
{
    return std::min((entry.records + 7) / 8, 16);
}

}  // namespace

const char* formatName(DiscFormat format)
{
    switch (format) {
    case DiscFormat::Data: return "DATA";
    case DiscFormat::Vendor: return "VENDOR";
    case DiscFormat::Pcw: return "PCW";
    case DiscFormat::Unknown: break;
    }
    return "unknown";
}

const char* typeName(FileType type)
{
    switch (type) {
    case FileType::Basic: return "BASIC";
    case FileType::ProtectedBasic: return "BASIC (P)";
    case FileType::Binary: return "BINARY";
    case FileType::ProtectedBinary: return "BINARY (P)";
    case FileType::Unknown: return "UNKNOWN";
    case FileType::Ascii: break;
    }
    return "ASCII";
}

FileType fileType(std::span<const uint8_t> content)
{
    const std::optional<AmsdosHeader> header = amsdosHeader(content);
    if (!header)
        return FileType::Ascii;
    switch (header->type) {
    case kAmsdosBasic: return FileType::Basic;
    case kAmsdosProtectedBasic: return FileType::ProtectedBasic;
    case kAmsdosBinary: return FileType::Binary;
    case kAmsdosProtectedBinary: return FileType::ProtectedBinary;
    }
    return FileType::Unknown;
}

CpcDisc::CpcDisc()
    : image_(DiscImage::formatted(42))
{
}

CpcDisc::CpcDisc(DiscImage image)
    : image_(std::move(image))
{
}

CpcDisc CpcDisc::formatted(int tracks, DiscFormat format)
{
    const uint8_t first = format == DiscFormat::Vendor ? kVendorFirstId
                          : format == DiscFormat::Pcw  ? kPcwFirstId
                                                       : kDataFirstId;
    return CpcDisc(DiscImage::formatted(tracks, first));
}

int CpcDisc::firstId() const
{
    return image_.track(0).firstId();
}

DiscFormat CpcDisc::format() const
{
    switch (firstId()) {
    case kDataFirstId: return DiscFormat::Data;
    case kVendorFirstId: return DiscFormat::Vendor;
    case kPcwFirstId: return DiscFormat::Pcw;
    }
    return DiscFormat::Unknown;
}

int CpcDisc::reservedTracks() const
{
    switch (format()) {
    case DiscFormat::Vendor: return 2;
    case DiscFormat::Pcw: return 1;
    default: return 0;
    }
}

bool CpcDisc::hasDirectory() const
{
    if (format() == DiscFormat::Unknown)
        return false;
    for (int half = 0; half < 4; ++half)
        if (!blockSector(half / 2, half % 2))
            return false;
    return true;
}

int CpcDisc::blocksOnDisc() const
{
    const int tracks = image_.tracks() - reservedTracks();
    return std::clamp(tracks * kTrackSectors / 2, 0, kAllBlocks);
}

// One of the two sectors of a block: null if the disc does not have it, or
// has it with another size.
const Sector* CpcDisc::blockSector(int block, int half) const
{
    const int first = firstId();
    if (first < 0)
        return nullptr;
    const int place = block * 2 + half;
    const Sector* sector =
        image_.sector(place / kTrackSectors + reservedTracks(), 0, static_cast<uint8_t>(first + place % kTrackSectors));
    return sector && sector->data.size() >= kSectorSize ? sector : nullptr;
}

Bytes CpcDisc::readBlock(int block) const
{
    Bytes data(kBlockSize, 0);
    for (int half = 0; half < 2; ++half)
        if (const Sector* sector = blockSector(block, half))
            std::copy_n(sector->data.begin(), kSectorSize, data.begin() + half * kSectorSize);
    return data;
}

void CpcDisc::writeBlock(int block, std::span<const uint8_t> data)
{
    // A file that goes past the last track has the disc grow, as
    // ManageDsk's does.
    const int last = (block * 2 + 1) / kTrackSectors + reservedTracks();
    while (image_.tracks() <= last && image_.tracks() < 255)
        image_.addTrack(static_cast<uint8_t>(firstId()));
    Bytes padded(kBlockSize, 0);
    std::copy_n(data.begin(), std::min<size_t>(data.size(), kBlockSize), padded.begin());
    for (int half = 0; half < 2; ++half)
        if (const Sector* sector = blockSector(block, half))
            std::copy_n(padded.begin() + half * kSectorSize, kSectorSize, const_cast<Sector*>(sector)->data.begin());
}

std::array<DirEntry, kDirectoryEntries> CpcDisc::directory() const
{
    std::array<DirEntry, kDirectoryEntries> entries;
    if (!hasDirectory())
        return entries;
    for (int index = 0; index < kDirectoryEntries; ++index) {
        const Sector* sector = blockSector(index / kEntriesInSector / 2, index / kEntriesInSector % 2);
        entries[static_cast<size_t>(index)] = entryFrom(&sector->data[static_cast<size_t>(index % kEntriesInSector) * kEntrySize]);
    }
    return entries;
}

void CpcDisc::setEntry(int index, const DirEntry& entry)
{
    const Sector* sector = blockSector(index / kEntriesInSector / 2, index / kEntriesInSector % 2);
    if (sector)
        entryTo(entry, &const_cast<Sector*>(sector)->data[static_cast<size_t>(index % kEntriesInSector) * kEntrySize]);
}

std::vector<bool> CpcDisc::blocksInUse() const
{
    std::vector<bool> used(kAllBlocks, false);
    used[0] = used[1] = true;  // the directory
    for (const DirEntry& entry : directory())
        if (!entry.free())
            for (const uint8_t block : entry.blocks)
                used[block] = used[block] || block > 1;
    return used;
}

int CpcDisc::usedBlocks() const
{
    const std::vector<bool> used = blocksInUse();
    return static_cast<int>(std::count(used.begin() + 2, used.end(), true));
}

int CpcDisc::freeBlocks() const
{
    if (!hasDirectory())
        return 0;
    const std::vector<bool> used = blocksInUse();
    return static_cast<int>(std::count(used.begin() + 2, used.begin() + std::max(blocksOnDisc(), 2), false));
}

std::vector<DiscFile> CpcDisc::files(bool withErased) const
{
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    std::vector<DiscFile> files;
    std::vector<std::array<uint8_t, 11>> names;
    for (int index = 0; index < kDirectoryEntries; ++index) {
        const DirEntry& entry = entries[static_cast<size_t>(index)];
        if (entry.free() && (!withErased || neverUsed(entry)))
            continue;
        const std::array<uint8_t, 11> name = plainName(entry);
        size_t at = 0;
        while (at < files.size() && (files[at].user != entry.user || names[at] != name))
            ++at;
        if (at == files.size()) {
            DiscFile file;
            file.name = shownName(name);
            file.user = entry.user;
            file.erased = entry.free();
            files.push_back(std::move(file));
            names.push_back(name);
        }
        files[at].entries.push_back(index);
    }
    for (DiscFile& file : files) {
        std::stable_sort(file.entries.begin(), file.entries.end(), [&entries](int a, int b) {
            return extentNumber(entries[static_cast<size_t>(a)]) < extentNumber(entries[static_cast<size_t>(b)]);
        });
        for (const int index : file.entries)
            file.records += entries[static_cast<size_t>(index)].records;
        const DirEntry& first = entries[static_cast<size_t>(file.entries.front())];
        file.readOnly = first.name[8] & 0x80;
        file.hidden = first.name[9] & 0x80;
        file.length = file.records * kRecordSize;
        if (first.records == 0)
            continue;
        const Bytes start = readBlock(first.blocks[0]);
        file.type = fileType(start);
        if (const std::optional<AmsdosHeader> header = amsdosHeader(start)) {
            file.loadAddress = header->loadAddress;
            file.entryAddress = header->entryAddress;
            file.length = static_cast<int>(header->length);
        }
    }
    return files;
}

std::optional<DiscFile> CpcDisc::find(const std::string& name, int user) const
{
    const std::string wanted = cpcName(name);
    for (DiscFile& file : files(user == kErasedUser))
        if (file.user == user && file.name == wanted)
            return std::move(file);
    return std::nullopt;
}

Bytes CpcDisc::read(const DiscFile& file) const
{
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    Bytes content;
    for (const int index : file.entries) {
        const DirEntry& entry = entries[static_cast<size_t>(index)];
        size_t left = std::min<size_t>(static_cast<size_t>(entry.records) * kRecordSize,
                                       static_cast<size_t>(blocksOf(entry)) * kBlockSize);
        for (int at = 0; at < blocksOf(entry) && left > 0; ++at) {
            const Bytes block = readBlock(entry.blocks[static_cast<size_t>(at)]);
            const size_t taken = std::min<size_t>(left, kBlockSize);
            content.insert(content.end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(taken));
            left -= taken;
        }
    }
    return content;
}

AddResult CpcDisc::add(const std::string& name, std::span<const uint8_t> content, const AddOptions& options)
{
    if (!hasDirectory())
        return AddResult::NoDisc;
    const std::string clean = cpcName(name);
    if (clean.empty())
        return AddResult::BadName;
    if (find(clean, options.user))
        return AddResult::Exists;

    const int records = static_cast<int>((content.size() + kRecordSize - 1) / kRecordSize);
    const int extents = std::max(1, (records + kExtentRecords - 1) / kExtentRecords);
    const int needed = static_cast<int>((content.size() + kBlockSize - 1) / kBlockSize);

    // The first entries that are free, and the first blocks: all of them
    // are found before anything is written.
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    std::vector<int> places;
    for (int index = 0; index < kDirectoryEntries && static_cast<int>(places.size()) < extents; ++index)
        if (entries[static_cast<size_t>(index)].free())
            places.push_back(index);
    if (static_cast<int>(places.size()) < extents)
        return AddResult::NoEntry;

    const std::vector<bool> used = blocksInUse();
    const int limit = std::clamp(options.blocks, 2, kAllBlocks);
    std::vector<int> blocks;
    for (int block = 2; block < limit && static_cast<int>(blocks.size()) < needed; ++block) {
        if (used[static_cast<size_t>(block)])
            continue;
        // A block of a track the disc has must have its two sectors; one of
        // a track still to come will.
        bool there = true;
        for (int half = 0; half < 2; ++half) {
            const int track = (block * 2 + half) / kTrackSectors + reservedTracks();
            there = there && (track < image_.tracks() ? blockSector(block, half) != nullptr : track < 255);
        }
        if (there)
            blocks.push_back(block);
    }
    if (static_cast<int>(blocks.size()) < needed)
        return AddResult::NoSpace;

    DirEntry entry;
    entry.user = static_cast<uint8_t>(options.user);
    entry.name = directoryName(clean);
    if (options.readOnly)
        entry.name[8] |= 0x80;
    if (options.hidden)
        entry.name[9] |= 0x80;
    size_t written = 0;
    size_t nextBlock = 0;
    int left = records;
    for (int extent = 0; extent < extents; ++extent) {
        entry.extent = static_cast<uint8_t>(extent & 0x1F);
        entry.reserved2 = static_cast<uint8_t>(extent >> 5);
        entry.records = static_cast<uint8_t>(std::min(left, kExtentRecords));
        left -= entry.records;
        entry.blocks.fill(0);
        for (int at = 0; at < blocksOf(entry); ++at) {
            const int block = blocks[nextBlock++];
            entry.blocks[static_cast<size_t>(at)] = static_cast<uint8_t>(block);
            writeBlock(block, content.subspan(written, std::min<size_t>(kBlockSize, content.size() - written)));
            written = std::min(written + kBlockSize, content.size());
        }
        setEntry(places[static_cast<size_t>(extent)], entry);
    }
    return AddResult::Done;
}

AddResult CpcDisc::replace(const DiscFile& file, std::span<const uint8_t> content, int blocks)
{
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    const int records = static_cast<int>((content.size() + kRecordSize - 1) / kRecordSize);
    if (records == file.records && !file.entries.empty()) {
        // As long as it was: it stays where it is.
        size_t written = 0;
        for (const int index : file.entries) {
            const DirEntry& entry = entries[static_cast<size_t>(index)];
            for (int at = 0; at < blocksOf(entry) && written < content.size(); ++at) {
                writeBlock(entry.blocks[static_cast<size_t>(at)],
                           content.subspan(written, std::min<size_t>(kBlockSize, content.size() - written)));
                written = std::min(written + kBlockSize, content.size());
            }
        }
        return AddResult::Done;
    }
    // Longer or shorter: it is taken off, with nothing left of it in the
    // directory, and put back; it stays as it was if the new one does not
    // go in.
    DirEntry blank;
    blank.name.fill(kErasedUser);
    blank.blocks.fill(kErasedUser);
    blank.extent = blank.reserved1 = blank.reserved2 = blank.records = kErasedUser;
    for (const int index : file.entries)
        setEntry(index, blank);
    AddOptions options;
    options.user = file.user;
    options.readOnly = file.readOnly;
    options.hidden = file.hidden;
    options.blocks = blocks;
    const AddResult result = add(file.name, content, options);
    if (result != AddResult::Done)
        for (const int index : file.entries)
            setEntry(index, entries[static_cast<size_t>(index)]);
    return result;
}

void CpcDisc::erase(const DiscFile& file, bool forGood)
{
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    for (const int index : file.entries) {
        DirEntry entry = entries[static_cast<size_t>(index)];
        if (forGood) {
            // As formatting leaves an entry.
            entry.name.fill(kErasedUser);
            entry.blocks.fill(kErasedUser);
            entry.extent = entry.reserved1 = entry.reserved2 = entry.records = kErasedUser;
        }
        entry.user = kErasedUser;
        setEntry(index, entry);
    }
}

AddResult CpcDisc::rename(const DiscFile& file, const std::string& name, int user)
{
    const std::string clean = cpcName(name);
    if (clean.empty())
        return AddResult::BadName;
    if (clean == file.name && user == file.user)
        return AddResult::Done;
    if (user != kErasedUser && find(clean, user))
        return AddResult::Exists;
    const std::array<uint8_t, 11> padded = directoryName(clean);
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    for (const int index : file.entries) {
        DirEntry entry = entries[static_cast<size_t>(index)];
        entry.user = static_cast<uint8_t>(user);
        // The attributes stay with the place they are at.
        for (size_t at = 0; at < padded.size(); ++at)
            entry.name[at] = static_cast<uint8_t>(padded[at] | (entry.name[at] & 0x80));
        setEntry(index, entry);
    }
    return AddResult::Done;
}

void CpcDisc::setAttributes(const DiscFile& file, bool readOnly, bool hidden)
{
    const std::array<DirEntry, kDirectoryEntries> entries = directory();
    for (const int index : file.entries) {
        DirEntry entry = entries[static_cast<size_t>(index)];
        entry.name[8] = static_cast<uint8_t>((entry.name[8] & 0x7F) | (readOnly ? 0x80 : 0));
        entry.name[9] = static_cast<uint8_t>((entry.name[9] & 0x7F) | (hidden ? 0x80 : 0));
        setEntry(index, entry);
    }
}

}  // namespace tuxdsk
