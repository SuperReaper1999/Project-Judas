#pragma once

#include "particles/GpuParticles.h"

// Process-local state needed to restore world-owned GPU simulation after
// graphics retirement. Presentation-only caches are recreated from the world.
class WorldGraphicsState {
    friend class RuntimeWorld;
    struct Emitter {
        std::uint64_t id;
        JudasParticles::GpuParticles::DeviceState state;
    };
    std::vector<Emitter> m_emitters;
};
