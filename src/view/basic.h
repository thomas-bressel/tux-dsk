#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// A BASIC program as the CPC keeps it, made a listing again: ManageDsk's
// viewer in "listing" mode.
//
// A line is its length and its number, two bytes each, then its tokens and a
// 0; a length of 0 ends the program. Keywords are a byte from &80 up, the
// functions two (&FF and theirs); numbers are kept as the CPC works with
// them, and variables as a place for their address and their name.

namespace tuxdsk {

// The listing of a program: `content` is the file without its header.
// A line with its number, a space and its text; '\n' after each.
std::string basicListing(std::span<const uint8_t> content);

// A program saved with ",P" has its bytes scrambled: this is the program
// they stand for.
std::vector<uint8_t> unprotectedBasic(std::span<const uint8_t> content);

}  // namespace tuxdsk
