#pragma once

#include <functional>

#include <glm/glm.hpp>

#include "MeshData.h"

// A continuous, planet-local radial surface. The owning body supplies the
// centre and orientation; this type has no world origin or gravity direction.
struct TerrainSample {
    float signedDistance = 0.0f;  // approximate Euclidean distance; positive outside
    glm::vec3 outwardNormal{0.0f, 1.0f, 0.0f};
    glm::vec3 surfacePoint{0.0f};  // in the same terrain-local coordinates
};

class RadialTerrain {
public:
    using Elevation = std::function<float(const glm::vec3& localUnitDirection)>;

    RadialTerrain(float baseRadius, Elevation elevation, float maxAbsElevation);

    float RadiusAt(const glm::vec3& localUnitDirection) const;
    TerrainSample Sample(const glm::vec3& localPoint) const;
    float BoundRadius() const;

    // Indexed, full-sphere mesh. Latitude rings are statically concentrated
    // near this model's +Y pole, where the authored M25 demo features live;
    // this is an asset-tessellation choice, never simulation or runtime LOD.
    MeshData BuildMesh(int latitudeSegments, int longitudeSegments) const;

private:
    float ElevationAt(const glm::vec3& localUnitDirection) const;

    float m_baseRadius = 1.0f;
    Elevation m_elevation;
    float m_maxAbsElevation = 0.0f;
};
