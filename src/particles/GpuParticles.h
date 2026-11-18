#pragma once

#include "ParticleImpacts.h"
#include "../render/ComputeDevice.h"
#include <array>
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace JudasRender {class GpuParticlePass;}
namespace JudasParticles {
class CollisionSnapshot;
enum class GpuGravityKind : unsigned { None, Uniform, Radial };
struct GpuParticleStepInputs {
    float dt = 0;
    unsigned spawnCount = 0;
    std::uint64_t tick = 0;
    glm::vec3 position{0}, scale{1};
    glm::quat rotation{1,0,0,0};
    GpuGravityKind gravityKind = GpuGravityKind::None;
    glm::vec3 gravity{0}, gravityCenter{0}, wind{0};
    float gravityMagnitude = 0;
    glm::dvec3 worldOrigin{0};
};
// Explicit std430 diagnostic layout. Diagnostic state reads are deliberately
// blocking and never called by the normal simulation/render/event paths.
struct alignas(16) GpuParticleState {
    std::array<float,4> positionAge{}, velocityLife{};
    std::array<std::uint32_t,4> identity{}; // live, stable birth id, seed, stuck
};
static_assert(sizeof(GpuParticleState)==48);
struct GpuParticleDiagnostics {
    std::uint64_t dispatches=0, draws=0, readbackBytes=0, skippedReadbacks=0;
    std::uint64_t parameterUploadBytes=0, geometryUploadBytes=0, geometrySharedBytes=0;
    std::uint64_t sampleTick=0, emitted=0;
    unsigned capacity=0, sortCapacity=0;
};

// Resident visual simulation; reliable gameplay contacts deliberately select
// the CPU backend. No world queries, full-state readback or simulation-per-view.
// Explicit Shutdown runs on the owner context before world/context teardown.
class GpuParticles {
public:
    GpuParticles();
    ~GpuParticles();
    GpuParticles(const GpuParticles&)=delete;
    GpuParticles& operator=(const GpuParticles&)=delete;
    bool Initialize(const GraphicsCapabilities&, const ParticleEmitterSettings&,
                    JudasRender::ComputeBudget, std::string& error);
    bool Step(const ParticleEmitterSettings&, const GpuParticleStepInputs&,
              const CollisionSnapshot*, std::string& error, GpuParticles *geometryPeer=nullptr);
    bool PrepareView(const glm::mat4& view,const glm::mat4& projection,bool sorted,std::string& error);
    bool BindDrawBuffers(std::string& error);
    // Presentation records as shader storage, for draws that fetch them by index.
    bool BindDrawStorage(unsigned binding,std::string& error);
    bool CanMutate(std::string& error) const;
    bool CanShiftOrigin(glm::dvec3 delta,std::string& error) const;
    bool Clear(std::string& error);
    bool ShiftOrigin(glm::dvec3 delta,std::string& error);
    bool PollEvents(std::string& error);
    std::vector<ParticleImpact> TakeImpacts();
    bool ReadStateForDiagnostics(std::vector<GpuParticleState>&,std::string& error);
    bool Shutdown(std::string& error);
    bool Initialized() const;
    const ParticleRuntimeStats& Stats() const;
    GpuParticleDiagnostics Diagnostics() const;
private:
    friend class JudasRender::GpuParticlePass;
    void RecordDraw();
    struct Storage;
    std::unique_ptr<Storage> storage;
};
} // namespace JudasParticles
