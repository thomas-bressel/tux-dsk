#include "disc/amsdos.h"

#include <algorithm>

#include "disc/names.h"

namespace tuxdsk {

namespace {

constexpr size_t kSummed = 67;

void putWord(std::span<uint8_t> header, size_t at, uint16_t value)
{
    header[at] = static_cast<uint8_t>(value & 0xFF);
    header[at + 1] = static_cast<uint8_t>(value >> 8);
}

uint16_t word(std::span<const uint8_t> header, size_t at)
{
    return static_cast<uint16_t>(header[at] | header[at + 1] << 8);
}

}  // namespace

uint16_t amsdosChecksum(std::span<const uint8_t> header)
{
    unsigned sum = 0;
    for (size_t at = 0; at < kSummed && at < header.size(); ++at)
        sum += header[at];
    return static_cast<uint16_t>(sum);
}

bool hasAmsdosHeader(std::span<const uint8_t> file)
{
    if (file.size() < kAmsdosHeaderSize)
        return false;
    const uint16_t sum = amsdosChecksum(file);
    return sum != 0 && sum == word(file, 67);
}

std::optional<AmsdosHeader> amsdosHeader(std::span<const uint8_t> file)
{
    if (!hasAmsdosHeader(file))
        return std::nullopt;
    AmsdosHeader header;
    header.user = file[0];
    header.name = shownName(file.subspan(1, 11));
    header.type = file[18];
    header.loadAddress = word(file, 21);
    header.entryAddress = word(file, 26);
    header.length = static_cast<uint32_t>(file[64] | file[65] << 8 | file[66] << 16);
    if (header.length == 0)
        header.length = word(file, 24);
    return header;
}

std::vector<uint8_t> makeAmsdosHeader(const std::string& name, uint8_t type, uint16_t loadAddress,
                                      uint16_t entryAddress, size_t length)
{
    std::vector<uint8_t> header(kAmsdosHeaderSize, 0);
    const std::array<uint8_t, 11> padded = directoryName(name);
    std::copy(padded.begin(), padded.end(), header.begin() + 1);
    header[18] = type;
    const uint16_t shortLength = static_cast<uint16_t>(length);
    putWord(header, 19, shortLength);
    putWord(header, 21, loadAddress);
    putWord(header, 24, shortLength);
    putWord(header, 26, entryAddress);
    putWord(header, 64, shortLength);
    header[66] = static_cast<uint8_t>(length >> 16);
    putWord(header, 67, amsdosChecksum(header));
    return header;
}

void setAmsdosHeader(std::span<uint8_t> file, uint8_t type, uint16_t loadAddress, uint16_t entryAddress)
{
    if (file.size() < kAmsdosHeaderSize)
        return;
    file[18] = type;
    putWord(file, 21, loadAddress);
    putWord(file, 26, entryAddress);
    putWord(file, 67, amsdosChecksum(file));
}

void cleanAmsdosHeader(std::span<uint8_t> file)
{
    if (!hasAmsdosHeader(file))
        return;
    std::fill(file.begin() + 28, file.begin() + 64, uint8_t{0});
    std::fill(file.begin() + 69, file.begin() + kAmsdosHeaderSize, uint8_t{0});
    putWord(file, 67, amsdosChecksum(file));
}

std::span<const uint8_t> withoutAmsdosHeader(std::span<const uint8_t> file)
{
    return hasAmsdosHeader(file) ? file.subspan(kAmsdosHeaderSize) : file;
}

}  // namespace tuxdsk
