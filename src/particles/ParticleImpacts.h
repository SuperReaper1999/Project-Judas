#pragma once

#include "ParticleSettings.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace JudasParticles {
// Historical contact, expressed both absolutely and in the captured body's COM
// frame. Runtime geometry/body tokens are diagnostic, never persistent receiver
// identities. The scene entity and child key identify the receiving system.
struct ParticleImpact {
    std::uint64_t sequence=0, tick=0, receiver=0, geometryRevision=0;
    std::uint32_t childKey=0, feature=~0u;
    glm::dvec3 position{0}, localPosition{0};
    glm::vec3 normal{0}, localNormal{0}, velocity{0}, surfaceVelocity{0};
    glm::vec4 color{1};
    float size=0, age=0;
    std::uint64_t count=0;
    double weight=0;
};

struct ParticleRuntimeStats {
    ParticleBackend requested=ParticleBackend::Cpu, active=ParticleBackend::Cpu;
    std::uint64_t live=0, simulated=0, contacts=0, queryFailures=0;
    std::uint64_t queuedImpacts=0, deliveredImpacts=0, cosmeticDropped=0;
    std::uint64_t snapshotBodies=0, snapshotColliders=0, snapshotTriangles=0;
    std::uint64_t gpuBytes=0, geometryUploads=0, stateTransferBytes=0;
    bool paused=false, backpressure=false, pendingStep=false;
    double simulationMs=0, snapshotMs=0;
    std::string diagnostic;
};

// Receiver-local spatial cells; output is bounded and drains in stable insertion
// order. Reliable insertion failure applies backpressure BEFORE the response is
// committed, so the pool can resume the exact captured step after a drain.
// Restored records form a sealed historical batch: drain it before adding new
// reliable contacts, since their original aggregation settings are not saved.
class ParticleImpacts {
public:
    bool Add(const ParticleImpact&,const ParticleEmitterSettings&);
    std::vector<ParticleImpact> Take();
    void Clear();
    const std::vector<ParticleImpact>& Pending() const {return records;}
    std::uint64_t Dropped() const {return dropped;}
    std::uint64_t Delivered() const {return delivered;}
    std::uint64_t NextSequence() const {return nextSequence;}
    bool Restore(std::vector<ParticleImpact>,std::uint64_t next,std::string& error);
private:
    struct Cell {
        std::uint64_t receiver,revision;
        std::uint32_t child;
        std::int64_t x,y,z;
        int normal;
        bool operator==(const Cell&) const;
    };
    struct CellHash {std::size_t operator()(const Cell&) const;};
    std::unordered_map<Cell,std::size_t,CellHash> cells;
    std::vector<ParticleImpact> records;
    std::uint64_t nextSequence=1,dropped=0,delivered=0;
    bool sealed=false;
};
}
