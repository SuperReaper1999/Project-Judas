#include "RadialTerrain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <glm/gtc/constants.hpp>

namespace {

constexpr float kMinimumRadius = 0.01f;
constexpr float kDirectionEpsilon = 1.0e-6f;
constexpr float kNormalDifference = 2.0e-4f;
const glm::vec3 kLocalPole(0.0f, 1.0f, 0.0f);

bool IsFinite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::vec3 UnitOrPole(const glm::vec3& value) {
    if (!IsFinite(value)) return kLocalPole;
    const float length = glm::length(value);
    return length > kDirectionEpsilon ? value / length : kLocalPole;
}

// Fixed, feature-focused asset tessellation. Sixty percent of latitude
// intervals cover the first 0.2 radian from the model's north pole. The
// remaining intervals expand smoothly over the rest of the globe; the
// first interval on either side of the cap has the same derivative, so
// the ring distribution itself has no abrupt density seam. This is not
// camera-dependent LOD and has no bearing on collision queries.
float LatitudeAngle(int ring, int intervals) {
    const int capIntervals = std::clamp(static_cast<int>(std::lround(intervals * 0.6f)),
                                        1, intervals - 1);
    constexpr float capAngle = 0.2f;
    if (ring <= capIntervals) {
        return capAngle * static_cast<float>(ring) / static_cast<float>(capIntervals);
    }
    const int outerIntervals = intervals - capIntervals;
    const float u = static_cast<float>(ring - capIntervals) /
                    static_cast<float>(outerIntervals);
    const float initialSlope = capAngle * static_cast<float>(outerIntervals) /
                               static_cast<float>(capIntervals);
    const float quadratic = glm::pi<float>() - capAngle - initialSlope;
    return capAngle + initialSlope * u + quadratic * u * u;
}

}  // namespace

RadialTerrain::RadialTerrain(float baseRadius, Elevation elevation, float maxAbsElevation)
    : m_baseRadius(std::isfinite(baseRadius) ? std::max(baseRadius, kMinimumRadius)
                                          : kMinimumRadius),
      m_elevation(std::move(elevation)),
      m_maxAbsElevation(std::isfinite(maxAbsElevation)
                            ? std::clamp(maxAbsElevation, 0.0f,
                                         m_baseRadius - kMinimumRadius)
                            : 0.0f) {}

float RadialTerrain::ElevationAt(const glm::vec3& localUnitDirection) const {
    if (!m_elevation) return 0.0f;
    const float height = m_elevation(localUnitDirection);
    if (!std::isfinite(height)) return 0.0f;
    return std::clamp(height, -m_maxAbsElevation, m_maxAbsElevation);
}

float RadialTerrain::RadiusAt(const glm::vec3& localUnitDirection) const {
    return m_baseRadius + ElevationAt(UnitOrPole(localUnitDirection));
}

float RadialTerrain::BoundRadius() const {
    return m_baseRadius + m_maxAbsElevation;
}

TerrainSample RadialTerrain::Sample(const glm::vec3& localPoint) const {
    const glm::vec3 point = IsFinite(localPoint) ? localPoint : glm::vec3(0.0f);
    const float radialDistance = glm::length(point);
    // The exact centre has no unique radial direction or surface normal.
    // A fixed MODEL-local convention makes the query finite; ordinary
    // physical contact occurs near the surface, where the direction is
    // defined by geometry instead of this fallback.
    const glm::vec3 direction = radialDistance > kDirectionEpsilon
                                    ? point / radialDistance : kLocalPole;
    const float surfaceRadius = RadiusAt(direction);

    // For F(p)=|p|-R(p/|p|), grad F is radial direction minus the
    // tangential derivative of the height field divided by radius.
    // Only two independent derivatives exist on the unit sphere. Form an
    // orthonormal tangent basis and central-difference along it, needing
    // four elevation calls rather than six Cartesian perturbations. The
    // reconstructed vector is basis-independent: switching the helper
    // axis near a pole changes tangent coordinates, not the normal's
    // physical direction. The sampled function is smooth, so truncation
    // differences at that switch stay far below contact tolerances.
    const glm::vec3 helper = std::abs(direction.y) < 0.9f
                                 ? glm::vec3(0.0f, 1.0f, 0.0f)
                                 : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 tangentA = glm::normalize(glm::cross(helper, direction));
    const glm::vec3 tangentB = glm::cross(direction, tangentA);
    auto tangentDerivative = [&](const glm::vec3& tangent) {
        const glm::vec3 perturbation = tangent * kNormalDifference;
        const float above = ElevationAt(glm::normalize(direction + perturbation));
        const float below = ElevationAt(glm::normalize(direction - perturbation));
        return (above - below) / (2.0f * kNormalDifference);
    };
    const glm::vec3 angularHeightGradient =
        tangentDerivative(tangentA) * tangentA +
        tangentDerivative(tangentB) * tangentB;
    const glm::vec3 gradient = direction - angularHeightGradient / surfaceRadius;
    const float gradientLength = glm::length(gradient);
    const glm::vec3 normal = gradientLength > kDirectionEpsilon
                                 ? gradient / gradientLength : direction;
    const float signedDistance = (radialDistance - surfaceRadius) /
                                 std::max(gradientLength, kDirectionEpsilon);
    return TerrainSample{signedDistance, normal, point - normal * signedDistance};
}

MeshData RadialTerrain::BuildMesh(int latitudeSegments, int longitudeSegments) const {
    const int latitudes = std::max(latitudeSegments, 4);
    const int longitudes = std::max(longitudeSegments, 3);
    const std::uint32_t ringStride = static_cast<std::uint32_t>(longitudes + 1);
    MeshData mesh;
    mesh.vertices.reserve(2 + static_cast<std::size_t>(latitudes - 1) * ringStride);
    mesh.indices.reserve(static_cast<std::size_t>(latitudes - 1) * longitudes * 6);

    auto addVertex = [&](const glm::vec3& direction, const glm::vec2& uv) {
        const glm::vec3 position = direction * RadiusAt(direction);
        mesh.vertices.push_back(MeshVertex{position, Sample(position).outwardNormal, uv});
    };

    const std::uint32_t north = 0;
    addVertex(kLocalPole, glm::vec2(0.5f, 0.0f));
    for (int ring = 1; ring < latitudes; ++ring) {
        const float theta = LatitudeAngle(ring, latitudes);
        const float sine = std::sin(theta);
        const float cosine = std::cos(theta);
        for (int longitude = 0; longitude <= longitudes; ++longitude) {
            const float u = static_cast<float>(longitude) /
                            static_cast<float>(longitudes);
            const float phi = glm::two_pi<float>() * u;
            const glm::vec3 direction(sine * std::cos(phi), cosine,
                                       sine * std::sin(phi));
            addVertex(direction, glm::vec2(u, theta / glm::pi<float>()));
        }
    }
    const std::uint32_t south = static_cast<std::uint32_t>(mesh.vertices.size());
    addVertex(-kLocalPole, glm::vec2(0.5f, 1.0f));

    // Winding order follows d(position)/d(phi) cross d(position)/d(theta),
    // which points outward for this latitude/longitude parameterization.
    for (int longitude = 0; longitude < longitudes; ++longitude) {
        const std::uint32_t a = 1 + static_cast<std::uint32_t>(longitude);
        mesh.indices.push_back(north);
        mesh.indices.push_back(a + 1);
        mesh.indices.push_back(a);
    }
    for (int ring = 0; ring < latitudes - 2; ++ring) {
        const std::uint32_t top = 1 + static_cast<std::uint32_t>(ring) * ringStride;
        const std::uint32_t bottom = top + ringStride;
        for (int longitude = 0; longitude < longitudes; ++longitude) {
            const std::uint32_t a = top + static_cast<std::uint32_t>(longitude);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = bottom + static_cast<std::uint32_t>(longitude);
            const std::uint32_t d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    }
    const std::uint32_t lastRing = south - ringStride;
    for (int longitude = 0; longitude < longitudes; ++longitude) {
        const std::uint32_t a = lastRing + static_cast<std::uint32_t>(longitude);
        mesh.indices.push_back(a);
        mesh.indices.push_back(a + 1);
        mesh.indices.push_back(south);
    }
    return mesh;
}
