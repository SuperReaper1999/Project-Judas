#pragma once
#include "PlanetTerrain.h"
struct PlanetTerrainCutout {
    PlanetTileKey tile;
    unsigned boundaryLevel = 0;
    bool operator==(const PlanetTerrainCutout &b) const {
        return tile == b.tile && boundaryLevel == b.boundaryLevel;
    }
};
// Pure preparation shared by active coverage and private region reservations.
// Failed preparation never changes the published topology or physical support.
bool PreparePlanetCoverage(const PlanetSurface &, const std::vector<PlanetTerrainCutout> &,
                           std::vector<PlanetTileKey> &, std::string &);
