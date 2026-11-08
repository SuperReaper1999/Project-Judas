#pragma once
#include "NavigationAsset.h"
#include <glm/glm.hpp>

// Conservative triangle/tile broadphase for the existing Recast operator.
// Membership uses the exact same float bounds as rasterization, including
// erosion borders. Triangle order within each tile remains source order.
class NavigationRasterBins {
  public:
    struct Tile {
        std::vector<int> indices;
        std::vector<unsigned char> areas;
    };
    bool Build(const NavigationGeometry &, const std::vector<unsigned char> &areas,
               glm::vec3 minimum, float cellSize, int tileSize, int border, int xCount, int zCount,
               std::string &error, size_t referenceBudget = 4 * 1024 * 1024,
               const std::function<bool()> &cancelled = {});
    const Tile &At(int x, int z) const { return tiles.at(size_t(z) * nx + x); }
    bool FullScan() const { return fullScan; }
    size_t References() const { return references; }

  private:
    int nx = 0;
    bool fullScan = false;
    size_t references = 0;
    std::vector<Tile> tiles;
};
