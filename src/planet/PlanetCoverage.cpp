#include "PlanetCoverage.h"
#include <set>
#include <stdexcept>
namespace {
bool descendant(PlanetTileKey k, PlanetTileKey p) {
    return k.face == p.face && k.level >= p.level && (k.x >> (k.level - p.level)) == p.x &&
           (k.y >> (k.level - p.level)) == p.y;
}
} // namespace
bool PreparePlanetCoverage(const PlanetSurface &surface,
                           const std::vector<PlanetTerrainCutout> &cuts,
                           std::vector<PlanetTileKey> &output, std::string &error) {
    try {
        if (cuts.size() > 64)
            throw std::invalid_argument("planet terrain cutout count exceeds64");
        std::set<PlanetTileKey> fixed;
        const auto collisionLevel = PlanetCollisionLevel(surface.settings);
        for (size_t i = 0; i < cuts.size(); ++i) {
            const auto &c = cuts[i];
            if (c.tile.face >= 6 || c.tile.level < 3 || c.tile.level > collisionLevel ||
                c.boundaryLevel < c.tile.level || c.boundaryLevel > surface.settings.maxLevel ||
                c.boundaryLevel - c.tile.level > 7 || c.tile.x == 0 || c.tile.y == 0 ||
                uint64_t(c.tile.x) + 1 >= (uint64_t(1) << c.tile.level) ||
                uint64_t(c.tile.y) + 1 >= (uint64_t(1) << c.tile.level))
                throw std::invalid_argument("planet excavation requires an interior cube-face "
                                            "patch within collision/visual depth");
            for (size_t j = 0; j < i; ++j)
                if (descendant(c.tile, cuts[j].tile) || descendant(cuts[j].tile, c.tile))
                    throw std::invalid_argument("planet terrain replacement footprints overlap");
            const int scale = 1 << int(c.boundaryLevel - c.tile.level);
            const int x0 = int(c.tile.x) * scale, y0 = int(c.tile.y) * scale;
            auto add = [&](int x, int y) {
                fixed.insert({c.tile.face, c.boundaryLevel, unsigned(x0 + x), unsigned(y0 + y)});
            };
            for (int x = -1; x <= scale; ++x)
                for (int y : {-1, 0, scale - 1, scale})
                    add(x, y);
            for (int y = 1; y < scale - 1; ++y)
                for (int x : {-1, 0, scale - 1, scale})
                    add(x, y);
        }
        std::vector<PlanetTileKey> pins(fixed.begin(), fixed.end());
        // Validate the entire protected topology before changing either rendering or
        // physical support. No budget limit may leave a partial unprepared hole.
        SelectPlanetTiles(surface, {{surface.settings.center, 1., 1}}, pins);
        output = std::move(pins);
        error.clear();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}
