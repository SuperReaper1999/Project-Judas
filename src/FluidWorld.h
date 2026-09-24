#pragma once

#include <cstddef>
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
};

struct FluidSettings {
    float restDensity = 1000.0f;       // kg/m^3
    float particleRadius = 0.018f;    // collision radius, metres
    float smoothingRadius = 0.105f;   // density-kernel support, metres
    int substeps = 3;
    int densityIterations = 2;
    float maxDensityCorrection = 0.025f; // per iteration, metres
    float velocitySmoothing = 0.15f; // XSPH-style numerical damping at this resolution
};

// One ordinary oriented box from the rigid-body world. Compound bodies pass
// one collider per child box, with the same owner; there is no cup concept.
// The two poses span the enclosing fixed rigid-body step. FluidWorld samples
// them continuously across its own substeps, including rotation.
struct FluidBoxCollider {
    BodyHandle owner;
    BodyTransform previousPose;
    BodyTransform currentPose;
    glm::vec3 halfExtents{0.0f};
};

// Ordinary solid sphere geometry (for example a planet or dynamic ball).
// As with boxes, a moving sphere is sampled over the enclosing fixed step.
struct FluidSphereCollider {
    BodyHandle owner;
    BodyTransform previousPose;
    BodyTransform currentPose;
    float radius = 0.0f;
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
};

// Equal-and-opposite impulse due to a particle/solid contact. The caller may
// hand this to its rigid-body world after the fluid step. Static owners ignore
// it; FluidWorld itself has no dependency on a physics-world instance.
struct FluidContactImpulse {
    BodyHandle owner;
    glm::vec3 point{0.0f};
    glm::vec3 impulse{0.0f};
};

struct FluidDiagnostics {
    std::size_t particleCount = 0;
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
              std::vector<FluidContactImpulse>* contactImpulses = nullptr);

    const std::vector<FluidParticle>& Particles() const { return m_particles; }
    const FluidSettings& Settings() const { return m_settings; }
    glm::vec3 PresentedPosition(std::size_t index, float alpha) const;
    FluidDiagnostics GetDiagnostics() const;

private:
    FluidSettings m_settings;
    std::vector<FluidParticle> m_particles;
    std::vector<float> m_lastDensities;
};
