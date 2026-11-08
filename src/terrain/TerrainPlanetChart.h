#pragma once
#include "PlanetTerrain.h"
#include <glm/gtc/quaternion.hpp>

namespace TerrainV2 {
struct Source;
struct Chunk;
// A bounded cube-face chart, using the same double-precision rays as PlanetTiles.
// The existing heightfield/volume source stays in chart metres; presentation and
// collision share this mapping. Placement in an absolute world is separate.
struct PlanetChart {
    double radius = 1;
    PlanetTileKey tile;
    double span = 1;
    double originHeight = 0;
    // Native-source ownership evidence. Geometry-only charts may leave these
    // empty; attached source3 assets require the serializable provider and rim.
    std::string surfaceFingerprint{};
    std::vector<float> rim{};
    bool operator==(const PlanetChart &b) const {
        return radius == b.radius && tile == b.tile && span == b.span &&
               originHeight == b.originHeight && surfaceFingerprint == b.surfaceFingerprint &&
               rim == b.rim;
    }
    bool operator!=(const PlanetChart &b) const { return !(*this == b); }
    glm::dvec3 ModelOrigin() const;
    glm::dquat ModelRotation() const;
    glm::dvec3 ModelPoint(glm::dvec3 chartPoint) const;
    glm::dvec3 LocalPoint(glm::dvec3 chartPoint) const;
    glm::dvec3 ChartPoint(glm::dvec3 localPoint) const;
    glm::dmat3 Jacobian(glm::dvec3 chartPoint) const;
};
std::string PlanetGeometryFingerprint(const PlanetSettings &);
std::vector<float> PlanetSourceRim(const Source &);
bool ValidatePlanetChart(const PlanetChart &, std::string &);
// Does not mutate the planet/provider. Samples exactly the selected geographic
// footprint, with a practical mean metre pitch and a local radial height axis.
bool CreatePlanetChart(const PlanetSurface &, PlanetTileKey, unsigned cells, Source &,
                       PlanetChart &, std::string &);
// Worker-only mapping after source extraction and topology conformity. The same
// mapped mesh is used for collision and rendering, never a visual-only cave.
bool ProjectPlanetChunk(const PlanetChart &, Chunk &, std::string &);
} // namespace TerrainV2
