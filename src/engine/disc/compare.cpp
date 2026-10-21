#include "disc/compare.h"

namespace tuxdsk {

std::optional<Difference> firstDifference(const DiscImage& one, const DiscImage& other)
{
    if (one.tracks() != other.tracks())
        return Difference{Difference::Tracks, one.tracks(), other.tracks()};
    if (one.sides() != other.sides())
        return Difference{Difference::Sides, one.sides(), other.sides()};
    for (int number = 0; number < one.tracks(); ++number) {
        for (int side = 0; side < one.sides(); ++side) {
            const Track& mine = one.track(number, side);
            const Track& theirs = other.track(number, side);
            Difference where;
            where.track = number;
            where.side = one.sides() > 1 ? side : -1;
            if (mine.sectors.size() != theirs.sectors.size()) {
                where.kind = Difference::SectorCount;
                where.one = static_cast<int>(mine.sectors.size());
                where.other = static_cast<int>(theirs.sectors.size());
                return where;
            }
            for (size_t place = 0; place < mine.sectors.size(); ++place) {
                const Sector& a = mine.sectors[place];
                const Sector& b = theirs.sectors[place];
                if (a.id != b.id || a.track != b.track || a.side != b.side || a.size != b.size) {
                    where.kind = Difference::Numbering;
                    return where;
                }
                if (a.data != b.data) {
                    where.kind = Difference::Bytes;
                    where.sector = a.id;
                    return where;
                }
            }
        }
    }
    return std::nullopt;
}

}  // namespace tuxdsk
