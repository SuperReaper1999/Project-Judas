#include "TerrainLibrary.h"

#include "RadialTerrain.h"
#include "TerrainDemo.h"

std::shared_ptr<const RadialTerrain> CreateTerrainSurface(const std::string& identifier) {
    if (identifier == "m25-radial") return TerrainDemo::CreateSurface();
    return nullptr;
}

const std::vector<std::string>& KnownTerrainSurfaces() {
    static const std::vector<std::string> kSurfaces{"m25-radial"};
    return kSurfaces;
}
