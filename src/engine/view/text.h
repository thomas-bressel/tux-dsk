#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// What ManageDsk's viewer shows of a file as text. The listings are made of
// the CPC's characters, a byte each, with '\n' between the lines; cpcText()
// makes them what a window or a text file takes.

namespace tuxdsk {

// A listing in UTF-8. What a PC cannot print is shown as '?'. On a French
// CPC five signs are accented letters (@ is à, \ is ç, { is é, | is ù and }
// is è): ManageDsk turns them into those letters, and so does this if asked.
std::string cpcText(std::string_view listing, bool french = false);

// A file that is text: its lines up to the end-of-file mark (&1A), or to
// the end if it has none.
std::string asciiListing(std::span<const uint8_t> content);

// ManageDsk's "line wrap like CPC mode 2": lines cut every `width`
// characters, as the CPC shows them.
std::string wrapped(std::string_view listing, int width = 80);

// A file's bytes, sixteen a line, with where they are and what they are as
// characters:
//
//   #0000: 31 30 20 50 52 49 4E 54 20 22 48 45 4C 4C 4F 22 | 10.PRINT."HELLO"
//
// `base` is where the first byte counts from.
std::string hexDump(std::span<const uint8_t> content, int base = 0);

}  // namespace tuxdsk
