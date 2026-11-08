#pragma once
#include "NavigationAsset.h"

// Per-bake view over a complete immutable previous result. Owns only bounded
// tile membership/index arrays; never a world, Detour runtime or geometry cache.
class NavigationTileReuse {
  public:
    bool Prepare(const NavigationBakeUpdate *, const NavigationSurfaceSettings &,
                 const NavigationProfile &, int xCount, int zCount, int border, std::string &error);
    // True means this exact tile's compressed layers (possibly none) were reused.
    bool Append(int x, int z, std::vector<std::vector<unsigned char>> &) const;

  private:
    const NavigationData *previous = nullptr;
    int nx = 0;
    std::vector<bool> dirty;
    std::vector<std::vector<size_t>> layers;
};
