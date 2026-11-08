#include "ParticleSettings.h"

#include <cmath>
#include <tuple>

namespace {
template <class Vector> bool Finite(const Vector &value) {
    for (int axis = 0; axis < value.length(); ++axis)
        if (!std::isfinite(value[axis]))
            return false;
    return true;
}
bool Range(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
} // namespace

bool ValidParticleSettings(const ParticleEmitterSettings &s) {
    return Range(s.rate, 0, 100000) && Range(s.lifetime, 0, 3600) && s.lifetime > 0 &&
           std::isfinite(s.size) && s.size >= 0 && std::isfinite(s.endSize) && s.endSize >= 0 &&
           s.maxParticles > 0 && s.maxParticles <= 100000 && s.burst >= 0 &&
           s.burst <= s.maxParticles && Finite(s.spread) && Finite(s.velocity) &&
           Finite(s.velocityVariation) && Finite(s.acceleration) && Finite(s.color) &&
           Finite(s.endColor) && glm::all(glm::greaterThanEqual(s.spread, glm::vec3(0))) &&
           glm::all(glm::greaterThanEqual(s.velocityVariation, glm::vec3(0))) &&
           Range(s.color.a, 0, 1) && Range(s.endColor.a, 0, 1) &&
           (s.backend == ParticleBackend::Cpu || s.backend == ParticleBackend::Auto ||
            s.backend == ParticleBackend::Gpu) &&
           (s.response == ParticleResponse::Bounce || s.response == ParticleResponse::Slide ||
            s.response == ParticleResponse::Stick || s.response == ParticleResponse::Remove) &&
           (s.impacts == ParticleImpactDelivery::Off ||
            s.impacts == ParticleImpactDelivery::Cosmetic ||
            s.impacts == ParticleImpactDelivery::Reliable) &&
           Range(s.collisionRadius, 0, 1000) && s.collisionRadius > 0 &&
           Finite(s.collisionRegionOffset) && Finite(s.collisionRegionHalfExtents) &&
           glm::all(glm::greaterThan(s.collisionRegionHalfExtents, glm::vec3(0))) &&
           glm::all(glm::lessThanEqual(s.collisionRegionHalfExtents, glm::vec3(8192))) &&
           Range(s.restitution, 0, 1) && Range(s.friction, 0, 1) && s.maxContacts >= 1 &&
           s.maxContacts <= 32 && Range(s.impactCellSize, .000001f, 10000) &&
           Range(s.impactWeight, 0, 1000000) && s.maxImpactCells >= 1 && s.maxImpactCells <= 65536;
}

bool ParticleSettingsEqual(const ParticleEmitterSettings &a, const ParticleEmitterSettings &b) {
    const auto fields = [](const ParticleEmitterSettings &s) {
        return std::tie(s.enabled, s.loop, s.localSpace, s.useGravity, s.rate, s.lifetime, s.size,
                        s.endSize, s.burst, s.maxParticles, s.spread, s.velocity,
                        s.velocityVariation, s.acceleration, s.color, s.endColor, s.textureAsset,
                        s.seed, s.backend, s.collision, s.useWind, s.collisionRadius, s.restitution,
                        s.friction, s.response, s.collisionLayers, s.collisionRegion,
                        s.collisionRegionOffset, s.collisionRegionHalfExtents, s.maxContacts,
                        s.impacts, s.impactCellSize, s.impactWeight, s.maxImpactCells);
    };
    return fields(a) == fields(b);
}
