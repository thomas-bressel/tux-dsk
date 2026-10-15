#pragma once

#include <cstdint>
#include <span>
#include <string>

// A source of the Dams assembler, made text again: ManageDsk's viewer in
// "Dams" mode, after the DamsConverter of Thierry Jouin (Ramlaid).
//
// Dams keeps a line as its label, a byte for its instruction (&80 and up),
// its operands, and a &0D; &FF starts a comment.

namespace tuxdsk {

// The source as text: label, instruction and operands with a tab between
// them, '\n' after each line. `content` is the file without its header.
std::string damsListing(std::span<const uint8_t> content);

}  // namespace tuxdsk
