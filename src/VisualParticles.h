#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Visibility.h"
#include "particles/ParticleSettings.h"
#include "particles/ParticleImpacts.h"
class GravityField;
namespace JudasParticles {class CpuParticles;class CollisionSnapshot;}
struct VisualParticle {glm::vec3 position{0},velocity{0};float age=0;};
struct ParticleBillboard {glm::vec3 position;float size;glm::vec4 color;};
class VisualParticlePool {
public:
    explicit VisualParticlePool(const ParticleEmitterSettings& s={});
    ~VisualParticlePool();
    VisualParticlePool(const VisualParticlePool&);
    VisualParticlePool& operator=(const VisualParticlePool&);
    VisualParticlePool(VisualParticlePool&&) noexcept;
    VisualParticlePool& operator=(VisualParticlePool&&) noexcept;
    void Update(float dt,glm::vec3 position,glm::quat rotation,glm::vec3 scale,const GravityField& gravity);
    void UpdateWorld(float dt,glm::vec3 position,glm::quat rotation,glm::vec3 scale,
                     const GravityField&,std::shared_ptr<const JudasParticles::CollisionSnapshot>,
                     glm::dvec3 origin,std::uint64_t tick,glm::vec3 wind={});
    void Burst(unsigned count,glm::vec3 position,glm::quat rotation,glm::vec3 scale);
    void SetSettings(const ParticleEmitterSettings&);
    void ShiftOrigin(glm::dvec3 delta);
    const std::vector<ParticleBillboard>& Presentation(glm::vec3 position,glm::quat rotation,glm::vec3 scale);
    const std::vector<VisualParticle>& Particles()const{return particles;}
    const VisualBounds& Bounds()const{return bounds;}
    bool Control(const std::string&);
    bool PendingStep()const;
    bool Paused()const;
    bool Emitting()const;
    JudasParticles::ParticleRuntimeStats RuntimeStats()const;
    const JudasParticles::ParticleImpacts* ImpactState()const;
    JudasParticles::ParticleImpacts& ImpactState();
    void ImportTransient(std::vector<VisualParticle>,double emissionCredit,bool emissionStarted,std::uint64_t nextBirth);
    ParticleEmitterSettings settings;
private:
    float Random();
    glm::vec3 Variation(glm::vec3 extent);
    void Spawn(glm::vec3 position,glm::quat rotation,glm::vec3 scale);
    std::uint32_t random;double credit=0;bool started=false;
    std::uint64_t nextBirth=1;
    std::unique_ptr<JudasParticles::CpuParticles> runtime;
    std::vector<VisualParticle> particles;
    std::vector<ParticleBillboard> billboards;
    VisualBounds bounds;
};
