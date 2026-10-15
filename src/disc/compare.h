#pragma once

#include <optional>
#include <string>

#include "disc/image.h"

// ManageDsk's "Compare": two discs are the same when they have the same
// tracks, with the same sectors in the same order, holding the same bytes.

namespace tuxdsk {

// Where two discs first differ, in a few words ("track 3, sector &C4");
// nothing if they are the same.
std::optional<std::string> firstDifference(const DiscImage& one, const DiscImage& other);

}  // namespace tuxdsk
