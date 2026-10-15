#include "disc/compare.h"

#include <cstdio>

namespace tuxdsk {

std::optional<std::string> firstDifference(const DiscImage& one, const DiscImage& other)
{
    if (one.tracks() != other.tracks())
        return std::to_string(one.tracks()) + " tracks and " + std::to_string(other.tracks());
    if (one.sides() != other.sides())
        return std::to_string(one.sides()) + " sides and " + std::to_string(other.sides());
    for (int number = 0; number < one.tracks(); ++number) {
        for (int side = 0; side < one.sides(); ++side) {
            const Track& mine = one.track(number, side);
            const Track& theirs = other.track(number, side);
            const std::string where =
                "track " + std::to_string(number) + (one.sides() > 1 ? ", side " + std::to_string(side) : "");
            if (mine.sectors.size() != theirs.sectors.size())
                return where + ": " + std::to_string(mine.sectors.size()) + " sectors and "
                       + std::to_string(theirs.sectors.size());
            for (size_t place = 0; place < mine.sectors.size(); ++place) {
                const Sector& a = mine.sectors[place];
                const Sector& b = theirs.sectors[place];
                char id[8];
                std::snprintf(id, sizeof id, "&%02X", a.id);
                if (a.id != b.id || a.track != b.track || a.side != b.side || a.size != b.size)
                    return where + ": the sectors are not numbered alike";
                if (a.data != b.data)
                    return where + ", sector " + id;
            }
        }
    }
    return std::nullopt;
}

}  // namespace tuxdsk
