#pragma once

#include <optional>

#include "disc/image.h"

// ManageDsk's "Compare": two discs are the same when they have the same
// tracks, with the same sectors in the same order, holding the same bytes.

namespace tuxdsk {

// Where two discs first differ: the windows and the command line say it in
// their words.
struct Difference {
    enum Kind {
        Tracks,       // not as many tracks: `one` and `other`
        Sides,        // not as many sides: `one` and `other`
        SectorCount,  // a track without as many sectors: `one` and `other`
        Numbering,    // a track whose sectors are not numbered alike
        Bytes,        // a sector whose bytes differ: `sector`
    };
    Kind kind = Tracks;
    int one = 0;
    int other = 0;
    int track = 0;
    int side = -1;   // of a disc of two sides; -1 for a disc of one
    int sector = 0;  // its number, as its ID says (&C1...)

    bool operator==(const Difference&) const = default;
};

// Nothing if they are the same.
std::optional<Difference> firstDifference(const DiscImage& one, const DiscImage& other);

}  // namespace tuxdsk
