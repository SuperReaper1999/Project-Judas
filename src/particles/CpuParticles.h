#pragma once
#include "ParticleImpacts.h"
#include <memory>
#include <glm/gtc/quaternion.hpp>

class GravityField;
struct VisualParticle;
namespace JudasParticles {
class CollisionSnapshot;
struct ParticlePose {glm::vec3 position{0},scale{1};glm::quat rotation{1,0,0,0};};

// Optional CPU collision/lifecycle state. Pure legacy visual emitters do not
// allocate it. A blocked reliable impact retains one immutable step and cursor;
// retry never recomputes a historical contact against a newer world.
class CpuParticles {
public:
    CpuParticles();
    ~CpuParticles();
    CpuParticles(const CpuParticles&);
    CpuParticles& operator=(const CpuParticles&);
    bool Update(std::vector<VisualParticle>&,const ParticleEmitterSettings&,float dt,
                const ParticlePose&,const GravityField&,
                std::shared_ptr<const CollisionSnapshot>,glm::dvec3 origin,
                std::uint64_t tick,glm::vec3 wind={});
    bool Control(const std::string&,std::vector<VisualParticle>&);
    // Keep transient per-particle attachments and actual birth frames aligned
    // when the owning pool compacts or replaces its population.
    void RemapParticles(const std::vector<std::size_t>& retained,bool resetPose=false);
    void RecordBirth(std::size_t index,const ParticlePose&);
    void ShiftOrigin(glm::dvec3);
    bool PendingStep() const;
    bool Emitting() const;
    bool Paused() const;
    float CompletedSeconds() const;
    ParticleRuntimeStats Stats() const;
    ParticleImpacts& Impacts();
    const ParticleImpacts& Impacts() const;
private:
    struct State;
    std::unique_ptr<State> state;
};
}
