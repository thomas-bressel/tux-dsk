#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "disc/image.h"

// A disc as ManageDsk has it: an image, and AMSDOS's files on its first
// side.
//
// What a disc is for AMSDOS is told by the number of the first sector of
// its first track: &C1 for the DATA format, &41 for the VENDOR (or SYSTEM)
// one, whose first two tracks are kept for CP/M, and 1 for the PCW's, which
// keeps one. In all three a track is nine sectors of 512 bytes, a block is
// two sectors, and the directory is the first four sectors after the tracks
// kept: 64 entries, each naming up to 16 blocks, 16K of a file.
//
// ManageDsk lets a file go past the last track: tracks are added as they
// are needed, up to the last block an entry can name. Its window stops at
// block 188, the end of a 42nd track; its command line does not.

namespace tuxdsk {

enum class DiscFormat { Unknown, Data, Vendor, Pcw };

// "DATA", "VENDOR", "PCW", or "unknown".
const char* formatName(DiscFormat format);

constexpr int kDirectoryEntries = 64;
constexpr int kBlockSize = 1024;
constexpr int kRecordSize = 128;
constexpr uint8_t kErasedUser = 0xE5;
// Where files stop: ManageDsk's window, and its command line or its window
// with "more than 42 tracks" allowed.
constexpr int kWindowBlocks = 189;
constexpr int kAllBlocks = 256;

// One of the 64 entries of the directory, as it is on the disc.
struct DirEntry {
    uint8_t user = kErasedUser;
    std::array<uint8_t, 11> name{};  // with the attributes in their high bits
    uint8_t extent = 0;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
    uint8_t records = 0;
    std::array<uint8_t, 16> blocks{};

    bool free() const { return user == kErasedUser; }
};

enum class FileType { Basic, ProtectedBasic, Binary, ProtectedBinary, Unknown, Ascii };

// "BASIC", "BASIC (P)", "BINARY", "BINARY (P)", "UNKNOWN" or "ASCII", as
// ManageDsk's list has them.
const char* typeName(FileType type);

// A file of the disc: the entries of the directory that have its user and
// its name.
struct DiscFile {
    std::string name;  // "NAME.EXT"
    int user = 0;      // 229 for a file that was erased and is still there
    bool erased = false;
    bool readOnly = false;
    bool hidden = false;
    int records = 0;           // of 128 bytes, as the directory counts them
    std::vector<int> entries;  // its entries, in the order of what they hold
    // What its first bytes say of it: ASCII if they are not a header.
    FileType type = FileType::Ascii;
    int loadAddress = 0;
    int entryAddress = 0;
    int length = 0;  // what follows the header, or all of a file without one

    bool hasHeader() const { return type != FileType::Ascii; }
    // Its size as ManageDsk's list shows it.
    int kilobytes() const { return (records + 7) / 8; }
};

enum class AddResult {
    Done,
    BadName,    // nothing of the name can go in a directory
    Exists,     // the disc has a file of that name and user
    NoEntry,    // the directory is full
    NoSpace,    // no block left for it
    NoDisc,     // the disc is not one AMSDOS's files can be put on
};

struct AddOptions {
    int user = 0;
    bool readOnly = false;
    bool hidden = false;
    int blocks = kWindowBlocks;  // files stop before this block
};

class CpcDisc {
public:
    // An empty disc in the DATA format, of the 42 tracks ManageDsk gives it.
    CpcDisc();
    explicit CpcDisc(DiscImage image);
    static CpcDisc formatted(int tracks, DiscFormat format = DiscFormat::Data);

    DiscImage& image() { return image_; }
    const DiscImage& image() const { return image_; }

    DiscFormat format() const;
    // Whether the disc is of a format that is known and has its directory.
    bool hasDirectory() const;

    // The 64 entries as they are; all free if there is no directory.
    std::array<DirEntry, kDirectoryEntries> directory() const;
    // The files, in the order of the directory; with those that were erased
    // and still have a name, if asked.
    std::vector<DiscFile> files(bool withErased = false) const;
    std::optional<DiscFile> find(const std::string& name, int user = 0) const;

    // All the records of a file, in order.
    Bytes read(const DiscFile& file) const;

    // Blocks that files hold, and blocks left for them on the tracks the
    // disc has now: a block is a kilobyte.
    int usedBlocks() const;
    int freeBlocks() const;

    // Puts a file on the disc, as it is given. The disc is not changed if
    // it cannot be done.
    AddResult add(const std::string& name, std::span<const uint8_t> content, const AddOptions& options = {});
    // Gives a file other contents, in the blocks it has when it needs as
    // many, or else as a file added in its place.
    AddResult replace(const DiscFile& file, std::span<const uint8_t> content, int blocks = kWindowBlocks);

    // Erases a file: its entries are marked free and keep their name, so
    // that it can be brought back; for good, they keep nothing.
    void erase(const DiscFile& file, bool forGood = false);
    // Another name, another user: user 229 erases a file, and any other
    // brings an erased one back.
    AddResult rename(const DiscFile& file, const std::string& name, int user);
    void setAttributes(const DiscFile& file, bool readOnly, bool hidden);

private:
    DiscImage image_;

    int reservedTracks() const;
    int firstId() const;
    int blocksOnDisc() const;
    const Sector* blockSector(int block, int half) const;
    Bytes readBlock(int block) const;
    void writeBlock(int block, std::span<const uint8_t> data);
    void setEntry(int index, const DirEntry& entry);
    std::vector<bool> blocksInUse() const;
};

// The bytes of a file as a list shows them: the name, the user and the
// header of ManageDsk's "information about a file".
FileType fileType(std::span<const uint8_t> content);

}  // namespace tuxdsk
