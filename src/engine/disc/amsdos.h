#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The 128 bytes AMSDOS puts ahead of the files it writes (all but ASCII
// ones), as ManageDsk reads and makes them:
//
//   0      user            1-11   the name, eight characters and three
//   18     type            19-20  length
//   21-22  where it loads  24-25  length again
//   26-27  where it is entered
//   64-66  length, a third time, on three bytes
//   67-68  the sum of bytes 0 to 66
//
// A file has a header if the sum is right and is not 0: 128 bytes of 0 add
// up to the 0 they hold, and are not one.

namespace tuxdsk {

constexpr size_t kAmsdosHeaderSize = 128;

// A file's type, in its header.
constexpr uint8_t kAmsdosBasic = 0;
constexpr uint8_t kAmsdosProtectedBasic = 1;
constexpr uint8_t kAmsdosBinary = 2;
constexpr uint8_t kAmsdosProtectedBinary = 3;

struct AmsdosHeader {
    uint8_t user = 0;
    std::string name;  // "NAME.EXT"
    uint8_t type = kAmsdosBinary;
    uint16_t loadAddress = 0;
    uint16_t entryAddress = 0;
    // What the file holds after its header: the length on three bytes if it
    // says one, or else the one AMSDOS loads (bytes 24-25).
    uint32_t length = 0;
};

uint16_t amsdosChecksum(std::span<const uint8_t> header);
bool hasAmsdosHeader(std::span<const uint8_t> file);
std::optional<AmsdosHeader> amsdosHeader(std::span<const uint8_t> file);

// A header as ManageDsk makes one for a file that has none: `name` is
// "NAME.EXT", the user is 0, the length is said three times.
std::vector<uint8_t> makeAmsdosHeader(const std::string& name, uint8_t type, uint16_t loadAddress,
                                      uint16_t entryAddress, size_t length);

// A file's header says something else: its type and its addresses are
// changed where they are, and the sum made right again. `file` must start
// with a header.
void setAmsdosHeader(std::span<uint8_t> file, uint8_t type, uint16_t loadAddress, uint16_t entryAddress);

// ManageDsk's "clean-up" of a header: the bytes AMSDOS does not use (28 to
// 63 and 69 to 127) are made 0, so that the image packs better. A file
// without a header is left as it is.
void cleanAmsdosHeader(std::span<uint8_t> file);

// The file without its header, or the whole of it if it has none.
std::span<const uint8_t> withoutAmsdosHeader(std::span<const uint8_t> file);

}  // namespace tuxdsk
