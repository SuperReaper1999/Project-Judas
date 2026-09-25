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

void StepCoarseEntities(RuntimeWorld& world, float dt) {
    const PhysicsWorld& physics = world.Physics();
    const GravityField& gravity = world.Gravity();
    std::vector<EntityRecord>& entities = world.MutableEntities();

    // Celestial participants' pre-step positions/masses, so pairwise
    // forces are symmetric and order-independent within the step.
    struct Mass { glm::vec3 position; float mass; EntityId id; };
    std::vector<Mass> masses;
    for (const EntityRecord& e : entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed || !e.definition.celestial || !e.definition.body) continue;
        if (e.definition.body->motion != SceneBodyMotion::Dynamic) continue;
        if (e.fidelity == SimulationFidelity::Full) {
            masses.push_back({physics.GetTransform(world.DynamicBodies()[e.slot].Handle()).position,
                              e.definition.body->mass, e.id});
        } else if (e.fidelity == SimulationFidelity::Coarse) {
            masses.push_back({e.state.position, e.definition.body->mass, e.id});
        }
    }

    for (EntityRecord& e : entities) {
        if (e.lifecycle != EntityLifecycle::Active || e.fidelity != SimulationFidelity::Coarse) continue;
        DynamicBody& presentation = world.DynamicBodies()[e.slot];
        if (e.coarseMotion == CoarseMotion::Settled) {
            presentation.SetPoseFromState(e.state.position, e.state.rotation);
            continue;
        }
        glm::vec3 acceleration = gravity.Sample(e.state.position);
        for (const RuntimeWorld::PointMassSource& source : world.PointMassSources()) {
            acceleration += CelestialGravity::AccelerationFromPointMass(
                source.position, source.gravitationalParameter, e.state.position);
        }
        if (e.definition.celestial && e.definition.body) {
            for (const Mass& other : masses) {
                if (other.id == e.id) continue;
                acceleration += CelestialGravity::ForceOnB(other.position, other.mass, e.state.position,
                                                           e.definition.body->mass) /
                                e.definition.body->mass;
            }
        }
        e.state.linearVelocity += acceleration * dt;
        e.state.position += e.state.linearVelocity * dt;
        e.state.rotation = IntegrateOrientation(e.state.rotation, e.state.angularVelocity, dt);
        presentation.SetPoseFromState(e.state.position, e.state.rotation);
        e.coarseStepsSimulated += 1;
    }
}
