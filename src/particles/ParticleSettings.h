#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

enum class ParticleBackend : unsigned { Cpu, Auto, Gpu };
enum class ParticleResponse : unsigned { Bounce, Slide, Stick, Remove };
enum class ParticleImpactDelivery : unsigned { Off, Cosmetic, Reliable };

// One authored definition for portable and compute implementations. Existing
// projects retain their CPU visual-only behaviour unless explicitly extended.
struct ParticleEmitterSettings {
    bool enabled=true, loop=true, localSpace=false, useGravity=false;
    float rate=20, lifetime=2, size=.2f, endSize=.6f;
    int burst=0, maxParticles=256;
    glm::vec3 spread{.1f}, velocity{0,1,0}, velocityVariation{.2f}, acceleration{0};
    glm::vec4 color{.7f,.7f,.7f,.6f}, endColor{.4f,.4f,.4f,0};
    std::string textureAsset;
    std::uint32_t seed=1;

    ParticleBackend backend=ParticleBackend::Cpu;
    bool collision=false, useWind=false;
    float collisionRadius=.025f, restitution=.35f, friction=.2f;
    ParticleResponse response=ParticleResponse::Bounce;
    std::uint64_t collisionLayers=~std::uint64_t(0);
    unsigned maxContacts=8;
    ParticleImpactDelivery impacts=ParticleImpactDelivery::Off;
    float impactCellSize=.5f, impactWeight=1;
    unsigned maxImpactCells=1024;
};

bool ValidParticleSettings(const ParticleEmitterSettings&);
bool ParticleSettingsEqual(const ParticleEmitterSettings&,const ParticleEmitterSettings&);
