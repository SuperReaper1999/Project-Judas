#include "TerrainDemo.h"

#include <cmath>
#include <memory>

#include "RadialTerrain.h"

namespace {
float Gaussian(float x, float z, float centerX, float centerZ, float sigma) {
    const float dx = x - centerX;
    const float dz = z - centerZ;
    return std::exp(-(dx * dx + dz * dz) / (2.0f * sigma * sigma));
}

float Smooth01(float value) {
    const float t = glm::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

namespace TerrainDemo {
float Elevation(const glm::vec3& localUnitDirection) {
    // x/z are tangent coordinates near this authored patch, not world axes.
    // The smooth cap makes the far hemisphere an ordinary sphere while the
    // accessible patch contains hills, two depressions, and a low channel.
    const float x = kBaseRadius * localUnitDirection.x;
    const float z = kBaseRadius * localUnitDirection.z;
    const float cap = Smooth01(localUnitDirection.y / 0.4f);
    const float distanceSquared = x * x + z * z;
    const float broadMask = 1.0f - std::exp(-distanceSquared / (2.0f * 12.0f * 12.0f));
    const float rolling = broadMask * (0.85f * std::sin(x / 10.0f) * std::sin(z / 11.0f) +
                                        0.55f * std::cos((x + z) / 16.0f));
    const float hills = 3.8f * Gaussian(x, z, -17.0f, 8.0f, 7.0f) +
                        3.0f * Gaussian(x, z, 16.0f, -7.0f, 8.0f);
    const float basins = -2.8f * Gaussian(x, z, kBasinAX, kBasinZ, 2.7f) -
                         4.0f * Gaussian(x, z, kBasinBX, kBasinZ, 2.7f);
    const float channel = -0.4f * std::exp(-((z - kBasinZ) * (z - kBasinZ)) /
                                          (2.0f * 1.2f * 1.2f)) *
                          std::exp(-(x * x) / (2.0f * 5.5f * 5.5f));
    return cap * (rolling + hills + basins + channel);
}

std::shared_ptr<const RadialTerrain> CreateSurface() {
    // 12 m conservatively bounds the sum of every signed authored term.
    return std::make_shared<const RadialTerrain>(kBaseRadius, Elevation, 12.0f);
}

glm::vec3 LocalPointAbove(const RadialTerrain& terrain, float tangentX,
                          float tangentZ, float clearance) {
    const glm::vec3 direction = glm::normalize(
        glm::vec3(tangentX, kBaseRadius, tangentZ));
    return direction * (terrain.RadiusAt(direction) + clearance);
}
}  // namespace TerrainDemo
