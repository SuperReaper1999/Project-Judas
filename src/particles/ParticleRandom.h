#pragma once
#include "ParticleSettings.h"

namespace JudasParticles {
// V2 births are keyed by authored seed and a stable nonzero birth id. Integer
// arithmetic and six explicitly ordered draws match GpuParticleShaders.cpp.
// This intentionally does not replace legacy V1's existing sequence.
inline std::uint32_t ParticleBirthSeed(std::uint32_t seed,std::uint32_t birth){
    auto value=seed^birth;
    value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;
    return value;
}
inline float ParticleRandomUnit(std::uint32_t& state){
    state=state*1664525u+1013904223u;
    return static_cast<float>(state>>8)*(1.0f/16777216.0f);
}
inline glm::vec3 ParticleRandomVariation(std::uint32_t& state,glm::vec3 extent){
    const auto x=ParticleRandomUnit(state),y=ParticleRandomUnit(state),z=ParticleRandomUnit(state);
    return (glm::vec3(x,y,z)*2.0f-1.0f)*extent;
}
struct ParticleSpawnSample {glm::vec3 position,velocity;std::uint32_t state;};
inline ParticleSpawnSample SampleParticleBirth(const ParticleEmitterSettings& settings,std::uint32_t birth){
    auto state=ParticleBirthSeed(settings.seed,birth);
    const auto position=ParticleRandomVariation(state,settings.spread);
    const auto velocity=settings.velocity+ParticleRandomVariation(state,settings.velocityVariation);
    return {position,velocity,state};
}
}
