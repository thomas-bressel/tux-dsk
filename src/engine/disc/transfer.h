#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "disc/amsdos.h"

// What a file becomes when it goes from the computer to a disc and back, in
// the modes ManageDsk's window has for it.
//
// To a disc: as an ASCII file, without the header it may have come with; as
// a binary, with the header it has or one made for it; or as it is. From a
// disc: with its header or without, and no longer than its header says, the
// rest of its last record left behind.

namespace tuxdsk {

enum class ImportKind { AsIs, Ascii, Binary };

struct ImportOptions {
    ImportKind kind = ImportKind::AsIs;
    // For a binary: the type of the header made for a file that has none,
    // and the addresses of the header, made or not, if they are given.
    uint8_t type = kAmsdosBinary;
    bool setAddresses = false;
    int loadAddress = 0;
    int entryAddress = 0;
    // ManageDsk's "auto clean-up" of the header.
    bool cleanHeader = false;
};

// The bytes that go on the disc for a file of the computer; `name` is the
// name it will have there, for a header made.
std::vector<uint8_t> importedFile(const std::string& name, std::span<const uint8_t> file, const ImportOptions& options);

// The bytes that go to the computer for a file of a disc, given as all its
// records.
std::vector<uint8_t> exportedFile(std::span<const uint8_t> records, bool withHeader, bool cleanHeader = false);

}  // namespace tuxdsk
