#include "NavigationTileReuse.h"
#include <DetourTileCacheBuilder.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <tuple>

namespace {
bool Compatible(const NavigationData &old, const NavigationSurfaceSettings &s,
                const NavigationProfile &p) {
    const auto &a = old.settings;
    const auto &b = old.profile;
    return a.enabled == s.enabled && a.includeDynamic == s.includeDynamic &&
           a.profile == s.profile && a.sources == s.sources && a.halfExtents == s.halfExtents &&
           a.cellSize == s.cellSize && a.cellHeight == s.cellHeight &&
           a.simplification == s.simplification && a.tileSize == s.tileSize &&
           a.minRegion == s.minRegion && a.asset == s.asset && b.radius == p.radius &&
           b.height == p.height && b.slope == p.slope && b.climb == p.climb;
}
} // namespace
bool NavigationTileReuse::Prepare(const NavigationBakeUpdate *update,
                                  const NavigationSurfaceSettings &settings,
                                  const NavigationProfile &profile, int xCount, int zCount,
                                  int border, std::string &error) {
    previous = nullptr;
    dirty.clear();
    layers.clear();
    if (!update || !update->previous || !Compatible(*update->previous, settings, profile))
        return true;
    if (xCount <= 0 || zCount <= 0 || size_t(xCount) * zCount > 4096) {
        error = "invalid incremental navigation tile dimensions";
        return false;
    }
    for (const auto &box : update->changed)
        for (unsigned axis = 0; axis < 3; ++axis)
            if (!std::isfinite(box.minimum[axis]) || !std::isfinite(box.maximum[axis]) ||
                box.minimum[axis] > box.maximum[axis]) {
                error = "incremental navigation requires finite ordered changed bounds";
                return false;
            }
    nx = xCount;
    dirty.resize(size_t(nx) * zCount);
    layers.resize(dirty.size());
    for (int z = 0; z < zCount; ++z)
        for (int x = 0; x < nx; ++x) {
            // Identical binary32 arithmetic and inclusive tangencies to the
            // baker's raster bounds, including the entire erosion/filter border.
            const float x0 =
                -settings.halfExtents.x + (x * settings.tileSize - border) * settings.cellSize;
            const float x1 = -settings.halfExtents.x +
                             ((x + 1) * settings.tileSize + border) * settings.cellSize;
            const float z0 =
                -settings.halfExtents.z + (z * settings.tileSize - border) * settings.cellSize;
            const float z1 = -settings.halfExtents.z +
                             ((z + 1) * settings.tileSize + border) * settings.cellSize;
            for (const auto &box : update->changed)
                if (box.minimum.x <= x1 && box.maximum.x >= x0 && box.minimum.z <= z1 &&
                    box.maximum.z >= z0) {
                    dirty[size_t(z) * nx + x] = true;
                    break;
                }
        }
    std::set<std::tuple<int, int, int>> seen;
    for (size_t i = 0; i < update->previous->layers.size(); ++i) {
        const auto &bytes = update->previous->layers[i];
        dtTileCacheLayerHeader header{};
        if (bytes.size() < sizeof(header)) {
            error = "incremental navigation previous tile header is truncated";
            return false;
        }
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (header.magic != DT_TILECACHE_MAGIC || header.version != DT_TILECACHE_VERSION ||
            header.tx < 0 || header.tx >= nx || header.ty < 0 || header.ty >= zCount ||
            header.tlayer < 0 || !seen.emplace(header.tx, header.ty, header.tlayer).second) {
            error = "incremental navigation previous tile identity is invalid";
            return false;
        }
        layers[size_t(header.ty) * nx + header.tx].push_back(i);
    }
    previous = update->previous;
    return true;
}
bool NavigationTileReuse::Append(int x, int z,
                                 std::vector<std::vector<unsigned char>> &output) const {
    if (!previous || dirty.at(size_t(z) * nx + x))
        return false;
    for (auto index : layers[size_t(z) * nx + x])
        output.push_back(previous->layers[index]);
    return true;
}
