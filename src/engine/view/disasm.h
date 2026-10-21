#pragma once

#include <cstdint>
#include <span>
#include <string>

// A file's bytes as Z80 instructions: ManageDsk's viewer in "disassembly"
// mode. A line is where the instruction is, its bytes and the instruction:
//
//   8000 21 00 C0       LD HL,C000
//
// Numbers are hexadecimal without a sign in front, as ManageDsk writes
// them; a relative jump shows where it goes.

namespace tuxdsk {

// `base` is where the first byte is in memory: where the file loads.
std::string disassembly(std::span<const uint8_t> content, int base = 0);

}  // namespace tuxdsk
