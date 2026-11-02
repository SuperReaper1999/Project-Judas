#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "PhysicsWorld.h"

class GravityField;
class RadialTerrain;

// Bounded, CPU-side particle liquid. Every particle is authoritative matter;
// the renderer may interpolate its poses but never drives this state.
struct FluidParticle {
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    float mass = 0.0f;
    glm::vec3 acceleration{0.0f}; // actual velocity change over latest executed fluid step
};

// Geometric water cell observed being cut by an actual solid contact. The
// cell stays anchored in the fluid chart, not attached to the moving body.
// Current material/surface witnesses verify that its owner still contacts water.
// It contributes no mass, velocity, pressure or particles.
struct FluidDisplacementMarker {
    BodyHandle owner{};
    glm::vec3 position{0};
    glm::quat rotation{1,0,0,0};
    float halfWidth=0;
    glm::vec3 materialPoint{0}, contactPoint{0};
};

struct FluidSettings {
    float updateRateHz = 30.0f; // production cadence; direct Step still advances its supplied interval
    float hydrostaticDragRate = 2.0f; // s^-1 at full immersion
    float restDensity = 1000.0f;       // kg/m^3
    float particleRadius = 0.018f;    // preferred soft clearance; physical centre exclusion is hard
    float smoothingRadius = 0.105f;   // density-kernel support, metres
    int substeps = 3;
    int densityIterations = 2;
    float maxDensityCorrection = 0.025f; // per iteration, metres
    float velocitySmoothing = 0.15f; // XSPH-style numerical damping at this resolution
};

// One ordinary oriented box from the rigid-body world. Compound bodies pass
// one collider per child box, with the same owner; there is no cup concept.
// Direct callers supply endpoints of their fluid interval. Production also
// supplies owned, resolved rigid segments accumulated over that interval.
// FluidWorld samples them at its existing substeps, including rotation.
struct FluidBoxCollider {
    BodyHandle owner;
    BodyTransform previousPose;
    BodyTransform currentPose;
    glm::vec3 halfExtents{0.0f};
    // Optional copied authoritative rigid path, with seconds relative to this
    // liquid update. Existing direct fixtures retain the endpoint convention.
    std::vector<PhysicsWorld::BodyMotionSegment> resolvedMotion{};
    double motionDuration = 0;
    glm::vec3 localCenter{0}; // compound child offset on the parent path
    glm::quat localRotation{1,0,0,0};
};

// Ordinary solid sphere geometry (for example a planet or dynamic ball).
// As with boxes, a moving sphere is sampled over the enclosing fixed step.
struct FluidSphereCollider {
    BodyHandle owner;
    BodyTransform previousPose;
    BodyTransform currentPose;
    float radius = 0.0f;
    std::vector<PhysicsWorld::BodyMotionSegment> resolvedMotion{};
    double motionDuration = 0;
};

// A generic solid with a locally defined radial terrain surface. The
// previous/current rigid poses describe actual solid motion over this fixed
// step; fluid samples the same geometric terrain used by other collision
// consumers. There is no knowledge of planets, basins, or lakes here.
struct FluidTerrainCollider {
    BodyHandle owner;
    BodyTransform previousPose;
    BodyTransform currentPose;
    const RadialTerrain* surface = nullptr;
    std::vector<PhysicsWorld::BodyMotionSegment> resolvedMotion{};
    double motionDuration = 0;
};

// Measured equal-and-opposite impulse from a particle velocity contact.
// Production finite-mass cavity response applies it inside the callback; this
// record is diagnostic and must not be applied again. Exterior response is
// intentionally one-way. FluidWorld owns no PhysicsWorld instance.
struct FluidContactImpulse {
    BodyHandle owner;
    glm::vec3 point{0.0f};
    glm::vec3 impulse{0.0f};
    std::size_t particleIndex = static_cast<std::size_t>(-1);
};

struct FluidVelocityContact {
    std::size_t particleIndex;
    BodyHandle owner;
    glm::vec3 point, normal, wallVelocity, particlePosition, particleVelocity;
    float particleMass, stepFraction;
    // Lower bound on n dot(v_particle - v_wall). A yielding preferred skin
    // allows the actual physical gap to close over this fluid substep.
    float minimumNormalVelocity = 0;
};
// Resolves all normals together after each PBF substep. The callback updates
// particle/rigid velocities and returns the impulse ON each particle contact.
// Batching lets a container's actual supporting contacts share the same solve.
using FluidVelocityBatchResponse = std::function<void(std::vector<FluidParticle>&,
    const std::vector<FluidVelocityContact>&, std::vector<glm::vec3>&)>;

struct FluidDiagnostics {
    std::size_t particleCount = 0;
    std::size_t geometryEscapeCount = 0; // last executed step's hard physical-box union repairs
    std::size_t unresolvedGeometryCount = 0; // no verified feasible candidate; never silently resolved
    std::size_t geometryEscapeCandidates = 0; // all candidate validation work is counted
    float maximumGeometryEscapeDistance = 0; // rare real-solid repair displacement, metres
    std::size_t skinClippedProposals = 0; // preferred radius yielded to real geometry
    std::size_t skinPreventedReversals = 0; // deterministic blocked-clearance tie ordering
    std::size_t surfaceRoundingSteps = 0; // strict hard-surface nextafter construction
    std::size_t fallbackContactIterationCaps = 0; // direct prescribed-wall path, no callback
    float maximumFallbackContactResidual = 0; // excess normal closing speed, m/s
    std::size_t pressureReconstructionSamples = 0; // particle/substep observations in last executed step
    double maximumPressureReconstructionVelocityError = 0; // passive subtraction-versus-increment residual, m/s
    double rmsPressureReconstructionVelocityError = 0; // no correction is applied to authoritative velocity
    double densityMilliseconds = 0, boundaryMilliseconds = 0, velocityMilliseconds = 0; // passive last-step phase timings
    float totalMass = 0.0f;
    glm::vec3 centerOfMass{0.0f};
    glm::vec3 totalMomentum{0.0f};
    float kineticEnergy = 0.0f;
    float meanDensity = 0.0f;        // kg/m^3; free surfaces naturally read low
    float meanPositiveDensityError = 0.0f; // (rho-rho0)/rho0, clamped at zero
    float maxPositiveDensityError = 0.0f;
};

class FluidWorld {
public:
    FluidWorld();
    explicit FluidWorld(const FluidSettings& settings);

    void AddParticle(const glm::vec3& position, const glm::vec3& velocity, float mass);
    void Clear();

    // Advances exactly one supplied fixed step. Gravity is sampled at each
    // particle's own local simulation position. No direction is privileged.
    void Step(float fixedDeltaTime, const GravityField& gravity,
              const std::vector<FluidBoxCollider>& boxes,
              std::vector<FluidContactImpulse>* contactImpulses = nullptr);
    void Step(float fixedDeltaTime, const GravityField& gravity,
              const std::vector<FluidBoxCollider>& boxes,
              const std::vector<FluidSphereCollider>& spheres,
              std::vector<FluidContactImpulse>* contactImpulses = nullptr);
    void Step(float fixedDeltaTime, const GravityField& gravity,
              const std::vector<FluidBoxCollider>& boxes,
              const std::vector<FluidSphereCollider>& spheres,
              const std::vector<FluidTerrainCollider>& terrains,
              std::vector<FluidContactImpulse>* contactImpulses = nullptr,
              const FluidVelocityBatchResponse& velocityResponse = {});

    const std::vector<FluidParticle>& Particles() const { return m_particles; }
    const std::vector<FluidDisplacementMarker>& DisplacementMarkers() const { return m_boundaryMarkers; }
    std::uint64_t Revision() const { return m_revision; }
    const FluidSettings& Settings() const { return m_settings; }
    glm::vec3 PresentedPosition(std::size_t index, float alpha) const;
    FluidDiagnostics GetDiagnostics() const;

private:
    double m_densityMilliseconds=0, m_boundaryMilliseconds=0, m_velocityMilliseconds=0;
    std::uint64_t m_revision = 0; // presentation invalidation, never saved
    FluidSettings m_settings;
    std::vector<FluidParticle> m_particles;
    std::vector<FluidDisplacementMarker> m_boundaryMarkers;
    std::vector<float> m_lastDensities;
    std::size_t m_geometryEscapes=0,m_unresolvedGeometry=0,m_geometryEscapeCandidates=0;
    float m_maximumGeometryEscapeDistance=0;
    std::size_t m_skinClippedProposals=0,m_skinPreventedReversals=0;
    std::size_t m_surfaceRoundingSteps=0;
    std::size_t m_fallbackContactIterationCaps=0;
    float m_maximumFallbackContactResidual=0;
    std::size_t m_pressureReconstructionSamples=0;
    double m_maximumPressureReconstructionVelocityError=0;
    double m_pressureReconstructionVelocityErrorSquared=0;
};
