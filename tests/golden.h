#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// What the tests share of tests/golden: the discs the real ManageDsk made
// (tools/managedsk-goldens/run.sh) and the files it made them of.

namespace golden {

using Bytes = std::vector<uint8_t>;

inline Bytes read(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        std::printf("cannot read %s\n", path.c_str());
    return Bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// A disc ManageDsk made: "empty", "files", "names", "import" or "grown".
inline Bytes disc(const std::string& name)
{
    return read(std::string(TUXDSK_GOLDEN_DIR) + "/" + name + ".dsk");
}

inline std::string inputPath(const std::string& name)
{
    return std::string(TUXDSK_GOLDEN_DIR) + "/inputs/" + name;
}

// One of the files the discs were made of.
inline Bytes input(const std::string& name)
{
    return read(inputPath(name));
}

// The twelve files "grown" is made of, which are not kept: 16K of one
// letter each, from 'A'.
inline Bytes fill(int index)
{
    return Bytes(16384, static_cast<uint8_t>('A' + index));
}

}  // namespace golden
