#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

// File names between a computer's and a CPC's: on a disc a name is eight
// characters and three, in capitals, padded with spaces.

namespace tuxdsk {

// The name a file of the computer takes on a disc, as ManageDsk gives it:
// what is before the first dot, cut to eight characters, and the three
// characters after that dot, in capitals: "my.file.name.txt" is "MY.FIL".
// The folders in front are left out, and so is what a CPC's directory
// cannot hold: spaces, the signs CP/M keeps for itself and anything that is
// not plain ASCII. Empty if nothing is left of the name.
std::string cpcName(const std::string& hostName);

// "NAME.EXT" as the directory has it: eleven characters. A name given
// without care is cut and put in capitals as cpcName() does.
std::array<uint8_t, 11> directoryName(const std::string& name);

// The eleven characters of a directory entry as "NAME.EXT", their attribute
// bits left out; what cannot be printed shows as '?'.
std::string shownName(std::span<const uint8_t> entryName);

}  // namespace tuxdsk
