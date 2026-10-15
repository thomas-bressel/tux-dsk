#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "disc/image.h"

// A disc image as an HxC Floppy Emulator's .HFE file: what a Gotek or an
// HxC drive, put in place of a real one, gives a real CPC to read.
//
// Where a .DSK file holds sectors, an .HFE file holds each track as the
// head of a drive reads it: bit cells, two to a bit of data (MFM), with the
// gaps, the sync bytes, the marks and the CRCs a floppy controller looks
// for. Its first 512 bytes say what the disc is (tracks, sides, 250 kbit/s,
// a CPC's drive); the next 512 list where each track is and how long; then
// come the tracks, in blocks of 512 bytes of which each side has half, a
// byte's first cell in its lowest bit.
//
// A track is written as HxC's own software writes one for a CPC's disc:
// for a disc of nine sectors of 512 bytes a track, the file is the one its
// converter makes of the same .DSK, byte for byte. This is the first kind
// of .HFE file ("HXCPICFE", revision 0), the one every firmware takes.

namespace tuxdsk {

// Whether a file is one: it starts with "HXCPICFE".
bool isHfe(std::span<const uint8_t> file);

// The image as an .HFE file.
Bytes hfeFile(const DiscImage& image);

// The image an .HFE file holds: the sectors found on its tracks. Nothing,
// with the reason, if the file is not one, or not of the first kind in MFM.
std::optional<DiscImage> parseHfe(std::span<const uint8_t> file, std::string* error = nullptr);

// Either kind of image, told by what the file starts with.
std::optional<DiscImage> parseDisc(std::span<const uint8_t> file, std::string* error = nullptr);

}  // namespace tuxdsk
