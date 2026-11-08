#include "NavigationRasterBins.h"
#include <algorithm>
#include <cmath>
#include <limits>

bool NavigationRasterBins::Build(const NavigationGeometry &geometry,
                                 const std::vector<unsigned char> &areas, glm::vec3 minimum,
                                 float cellSize, int tileSize, int border, int xCount, int zCount,
                                 std::string &error, size_t referenceBudget,
                                 const std::function<bool()> &cancelled) {
    if (xCount <= 0 || zCount <= 0 || size_t(xCount) * zCount > 4096 || !(cellSize > 0) ||
        !std::isfinite(cellSize) || tileSize <= 0 || border < 0 || geometry.triangles.size() % 3 ||
        areas.size() != geometry.triangles.size() / 3) {
        error = "invalid navigation raster index inputs";
        return false;
    }
    nx = xCount;
    fullScan = false;
    references = 0;
    tiles.assign(size_t(xCount) * zCount, {});
    struct Bounds {
        float minimum, maximum;
    };
    std::vector<Bounds> xs, zs;
    // Keep the arithmetic identical to NavigationGeometryBake's rcConfig.
    for (int x = 0; x < xCount; ++x)
        xs.push_back({minimum.x + (x * tileSize - border) * cellSize,
                      minimum.x + ((x + 1) * tileSize + border) * cellSize});
    for (int z = 0; z < zCount; ++z)
        zs.push_back({minimum.z + (z * tileSize - border) * cellSize,
                      minimum.z + ((z + 1) * tileSize + border) * cellSize});
    for (size_t face = 0; face < areas.size(); ++face) {
        if ((face & 1023) == 0 && cancelled && cancelled()) {
            error = "cancelled";
            return false;
        }
        glm::vec2 low(std::numeric_limits<float>::max()), high(-low);
        for (unsigned corner = 0; corner < 3; ++corner) {
            const auto index = geometry.triangles[face * 3 + corner];
            if (index < 0 || size_t(index) >= geometry.vertices.size() / 3) {
                error = "navigation raster triangle index is outside its vertices";
                return false;
            }
            const glm::vec2 point(geometry.vertices[size_t(index) * 3],
                                  geometry.vertices[size_t(index) * 3 + 2]);
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                error = "navigation raster geometry is nonfinite";
                return false;
            }
            low = glm::min(low, point);
            high = glm::max(high, point);
        }
        // Sorted exact float intervals avoid rounded coordinate-to-cell casts.
        // Include tangent faces. Recast still performs its original clipping.
        const auto firstX =
            std::lower_bound(xs.begin(), xs.end(), low.x,
                             [](const Bounds &b, float value) { return b.maximum < value; });
        const auto lastX =
            std::upper_bound(xs.begin(), xs.end(), high.x,
                             [](float value, const Bounds &b) { return value < b.minimum; });
        const auto firstZ =
            std::lower_bound(zs.begin(), zs.end(), low.y,
                             [](const Bounds &b, float value) { return b.maximum < value; });
        const auto lastZ =
            std::upper_bound(zs.begin(), zs.end(), high.y,
                             [](float value, const Bounds &b) { return value < b.minimum; });
        for (auto z = firstZ; z != lastZ; ++z)
            for (auto x = firstX; x != lastX; ++x) {
                if (++references > referenceBudget) {
                    // Pathological huge overlapping faces can defeat indexing.
                    // Release the bounded index and retain full original work;
                    // never drop triangles or reject otherwise valid geometry.
                    fullScan = true;
                    tiles.clear();
                    error.clear();
                    return true;
                }
                auto &tile = tiles[size_t(z - zs.begin()) * xCount + size_t(x - xs.begin())];
                tile.indices.insert(tile.indices.end(), geometry.triangles.begin() + face * 3,
                                    geometry.triangles.begin() + face * 3 + 3);
                tile.areas.push_back(areas[face]);
            }
    }
    error.clear();
    return true;
}
