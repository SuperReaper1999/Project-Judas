#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Scene.h"

// Milestone 29: existence is not the same thing as active simulation.
//
// A persistent entity is something the world says exists. At any moment it
// is represented at one SIMULATION FIDELITY:
//
//   Full     — an ordinary live PhysicsWorld body: contacts, gravity,
//              forces, presentation, interactions. What every Judas entity
//              was through M28.
//   Coarse   — no physics body. The entity's physical state (pose, linear
//              and angular velocity) is engine data that CoarseSimulation
//              advances cheaply each fixed step: settled entities stay put,
//              inertial ones integrate under the same gravity fields with
//              no contacts (see src/CoarseSimulation.h for the exact
//              semantics and limits).
//   Dormant  — no physics body and no per-step work at all. State is
//              retained exactly as it was when the entity went dormant;
//              time does not advance for it (explicit semantics, not an
//              omission — see docs/ARCHITECTURE.md, "Milestone 29, Time").
//
// and in one LIFECYCLE state:
//
//   Active    — loaded: Full or Coarse.
//   Unloaded  — Dormant. The entity can be reconstructed; it is not gone.
//   Destroyed — permanently gone. It stays gone when the world is rebuilt
//               from its baseline plus persisted deltas.
//
// Unload != destroy. Reconstruction (any state -> Full) recreates the live
// body from the retained state with the retained velocities: no impulse,
// no duplicate, and the previous incarnation's handle is invalid.
//
// Which fidelities an entity MAY take is a capability question answered by
// its components (RuntimeWorld::EntityRequiresFull); WHEN it moves between
// them is a policy question (src/FidelityPolicy.h). A simple game that
// never sets `managed` on anything keeps every entity Full forever and
// touches none of this.

enum class SimulationFidelity { Full, Coarse, Dormant };
enum class EntityLifecycle { Active, Unloaded, Destroyed };

// How a Coarse entity evolves. Captured at demotion from the live body.
enum class CoarseMotion {
    Settled,   // at rest on something: state frozen, zero work
    Inertial,  // free flight: integrated under gravity, no contacts
};

using EntityId = SceneObjectId;
// Persistent entities created at runtime take ids from this range so they
// can never collide with an authored scene object's id, whatever the
// baseline's own counter does later.
constexpr EntityId kRuntimeEntityIdBase = static_cast<EntityId>(1) << 62;

struct EntityPhysicalState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
};

// The engine's record of one persistent entity: everything RuntimeWorld
// needs to represent it at any fidelity and to reconstruct it.
struct EntityRecord {
    EntityId id = kInvalidSceneObjectId;
    std::string name;
    // The authored (or, for a runtime-created entity, creation-time)
    // definition: what the entity IS. Its transform/initial velocity are
    // the baseline state the R reset and the world-state delta compare to.
    SceneObject definition;
    bool authored = true;   // from the baseline scene (else created at runtime)
    bool managed = false;   // the scene's policy may change its fidelity
    bool requiresFull = false;  // capability: this entity has no reduced form
    EntityLifecycle lifecycle = EntityLifecycle::Active;
    SimulationFidelity fidelity = SimulationFidelity::Full;
    // Authoritative while not Full; refreshed from the live body on
    // demotion and pushed into the new body on reconstruction.
    EntityPhysicalState state;
    CoarseMotion coarseMotion = CoarseMotion::Settled;
    std::optional<SimulationFidelity> forcedFidelity;  // debug override
    std::size_t slot = 0;   // index into RuntimeWorld::DynamicBodies()
    // Bookkeeping for tests and the editor's debug view.
    unsigned int reconstructions = 0;
    unsigned long long coarseStepsSimulated = 0;
    double dormantSinceSeconds = -1.0;
};

const char* FidelityName(SimulationFidelity fidelity);
const char* LifecycleName(EntityLifecycle lifecycle);
