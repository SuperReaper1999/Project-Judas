#include "CoarseSimulation.h"

#include <glm/gtc/quaternion.hpp>

#include "CelestialGravity.h"
#include "RuntimeWorld.h"

namespace {
// The same orientation update as IntegrateRigidBody (src/RigidBody.cpp),
// so a coarse tumble matches what the live body would have done.
glm::quat IntegrateOrientation(const glm::quat& orientation, const glm::vec3& angularVelocity, float dt) {
    const glm::quat omega(0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z);
    return glm::normalize(orientation + (omega * orientation) * (0.5f * dt));
}
}  // namespace

void ApplyCoarseCelestialForces(RuntimeWorld& world, float dt) {
    const auto& entities = world.Entities();
    const auto participates = [](const EntityRecord& e) {
        return e.lifecycle == EntityLifecycle::Active && e.fidelity == SimulationFidelity::Coarse &&
               e.definition.celestial && e.definition.body &&
               e.definition.body->motion == SceneBodyMotion::Dynamic;
    };
    // Ordinary compact games and Full-only celestial scenes pay no scratch
    // allocation or second pair traversal.
    std::size_t coarseCount = 0;
    for (const EntityRecord& e : entities) coarseCount += participates(e) ? 1 : 0;
    if (coarseCount == 0) return;

    PhysicsWorld& physics = world.Physics();
    struct Participant {
        glm::vec3 position;
        float mass;
        BodyHandle body;
        EntityRecord* coarse;
        glm::vec3 force{0.0f};
    };
    std::vector<Participant> participants;
    participants.reserve(world.CelestialParticipants().size() + coarseCount);
    // This is the runtime's actual Full participant set, including a Local
    // vehicle even when that vehicle has no explicit celestial component.
    for (BodyHandle handle : world.CelestialParticipants()) {
        if (physics.IsDynamicBody(handle))
            participants.push_back({physics.GetTransform(handle).position, physics.GetMass(handle), handle, nullptr});
    }
    for (const EntityRecord& e : entities) {
        if (participates(e)) participants.push_back({e.state.position, e.definition.body->mass, {}, world.FindEntity(e.id)});
    }
    // All geometry is sampled before either fidelity advances. Evaluate each
    // cross-fidelity/coarse pair once and distribute the very same vector.
    // Full/Full pairs remain exclusively owned by CelestialGravity.
    for (std::size_t i = 0; i < participants.size(); ++i) {
        for (std::size_t j = i + 1; j < participants.size(); ++j) {
            Participant& a = participants[i];
            Participant& b = participants[j];
            if (!a.coarse && !b.coarse) continue;
            const glm::vec3 onB = CelestialGravity::ForceOnB(a.position, a.mass, b.position, b.mass);
            a.force -= onB;
            b.force += onB;
        }
    }
    for (Participant& p : participants) {
        if (p.coarse) {
            p.coarse->state.linearVelocity += p.force * (dt / p.mass);
            // A nonzero mutual force invalidates the reduced model's settled
            // assumption. Ordinary supported Coarse props remain untouched.
            if (p.force != glm::vec3(0.0f)) p.coarse->coarseMotion = CoarseMotion::Inertial;
        } else {
            physics.ApplyForce(p.body, p.force);
        }
    }
}

void StepCoarseEntities(RuntimeWorld& world, float dt) {
    const GravityField& gravity = world.Gravity();
    // Motion changes neither IDs nor vector membership. MutableEntities() is
    // reserved for structural edits which really invalidate the identity index.
    for (const EntityRecord& current : world.Entities()) {
        if (current.lifecycle != EntityLifecycle::Active || current.fidelity != SimulationFidelity::Coarse) continue;
        EntityRecord& e=*world.FindEntity(current.id);
        DynamicBody& presentation = world.DynamicBodies()[e.slot];
        if (e.coarseMotion == CoarseMotion::Settled) {
            presentation.SetPoseFromState(e.state.position, e.state.rotation);
            continue;
        }
        // Local fields apply to ordinary Full and Coarse entities alike.
        // Static point-mass sources are selected only by Celestial-mode
        // vehicles, whose capability requires Full fidelity (Simulation).
        e.state.linearVelocity += gravity.Sample(e.state.position) * dt;
        e.state.position += e.state.linearVelocity * dt;
        e.state.rotation = IntegrateOrientation(e.state.rotation, e.state.angularVelocity, dt);
        presentation.SetPoseFromState(e.state.position, e.state.rotation);
        e.coarseStepsSimulated += 1;
    }
}
