#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>

// Judas-owned rigid body state. No physics middleware backs this — mass,
// orientation, velocity, and the force/torque accumulators are plain data
// this engine integrates itself. Nothing here assumes a world-space "up":
// orientation is a free quaternion, inertia is a full 3x3 tensor (never
// just a scalar or a Y-axis-only value), and every operation is expressed
// in terms of this body's own state and whatever forces/torques are handed
// to it — never a fixed axis.
//
// `inverseMass == 0` means infinite mass (static or kinematic — never
// moved by force/impulse). `inverseInertiaLocal` being the zero matrix
// carries the same meaning for rotation. This mirrors the inverse-mass
// convention used throughout ordinary impulse-based rigid body dynamics
// (multiplying by inverse mass/inertia is what actually appears in the
// integration and impulse-resolution math, and correctly represents
// "infinite mass" as zero rather than needing a division-by-infinity
// special case).
struct RigidBody {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};

    float inverseMass = 0.0f;
    glm::mat3 inverseInertiaLocal{0.0f};  // body-space; rotated into world space per use.

    glm::vec3 forceAccumulator{0.0f};
    glm::vec3 torqueAccumulator{0.0f};

    bool IsStatic() const { return inverseMass <= 0.0f; }

    void ApplyForce(const glm::vec3& force) { forceAccumulator += force; }
    void ApplyTorque(const glm::vec3& torque) { torqueAccumulator += torque; }

    // A force applied at a specific world-space point, decomposed into the
    // net force plus the torque it produces about the body's center of
    // mass (position). Ordinary rigid-body mechanics — r x F — with no
    // axis assumption.
    void ApplyForceAtPoint(const glm::vec3& force, const glm::vec3& worldPoint) {
        forceAccumulator += force;
        torqueAccumulator += glm::cross(worldPoint - position, force);
    }

    // Impulses change velocity directly (not through the force
    // accumulator/integration) — the standard shape for contact/collision
    // resolution, which this engine's contact solver will use.
    void ApplyLinearImpulse(const glm::vec3& impulse) {
        if (IsStatic()) return;
        linearVelocity += impulse * inverseMass;
    }
    void ApplyAngularImpulse(const glm::vec3& angularImpulse) {
        if (IsStatic()) return;
        angularVelocity += InverseInertiaWorld() * angularImpulse;
    }
    void ApplyImpulseAtPoint(const glm::vec3& impulse, const glm::vec3& worldPoint) {
        ApplyLinearImpulse(impulse);
        ApplyAngularImpulse(glm::cross(worldPoint - position, impulse));
    }

    void ClearAccumulators() {
        forceAccumulator = glm::vec3(0.0f);
        torqueAccumulator = glm::vec3(0.0f);
    }

    // Rotates the body-space inverse inertia tensor into world space:
    // R * I^-1_local * R^T. Recomputed from the current orientation every
    // time it's needed rather than cached, since a rotating body's world
    // inertia genuinely changes every step — correctness over the (small,
    // 3x3) cost of recomputing it.
    glm::mat3 InverseInertiaWorld() const {
        const glm::mat3 rotation = glm::mat3_cast(orientation);
        return rotation * inverseInertiaLocal * glm::transpose(rotation);
    }
};

// Solid-sphere and solid-box body-space inverse inertia tensors — the only
// two shapes Milestone 7-Final's dynamic bodies use. Both are diagonal in
// body space (no cross terms) for these primitive shapes, so InverseInertiaWorld's
// general R * I^-1 * R^T still does the right thing once the body rotates
// away from its own local axes — nothing here assumes it stays
// axis-aligned in world space.
glm::mat3 SolidSphereInverseInertia(float mass, float radius);
glm::mat3 SolidBoxInverseInertia(float mass, const glm::vec3& halfExtents);

// Force/torque kicks use the current world inertia. Linear drift uses the
// kicked velocity; free angular drift solves the body-momentum implicit midpoint
// and uses its anchored Cayley quaternion step, then transports world angular
// momentum into the endpoint inertia. Free rotation is second order and
// conserves quadratic kinetic energy to solve/rounding error. Torque kicks
// retain first-order splitting.
// Both force accumulators are cleared by the velocity phase.
void IntegrateRigidBody(RigidBody& body, float fixedDeltaTime);

// Milestone 32: the same integration split at the point a contact solver
// needs to act — velocity from this step's force/torque (accumulators
// cleared), then pose from the (possibly contact-corrected) velocity.
// Calling both in order is exactly IntegrateRigidBody.
void IntegrateRigidBodyVelocity(RigidBody& body, float fixedDeltaTime);
void IntegrateRigidBodyPosition(RigidBody& body, float fixedDeltaTime);

// Recover physical world angular velocity after a force-free orientation drift.
// The old orientation/velocity define L = R I_body R^T omega. Singular/locked
// rotational inertia retains its prior representation; no inverse of zero.
glm::vec3 TransportAngularVelocity(const glm::mat3& inverseInertiaLocal,
    const glm::quat& from,const glm::quat& to,const glm::vec3& velocity);

// Bounded implicit body-momentum midpoint for the Cayley drift generator.
// Retains an anchored trajectory and second-order free rotation. Nonconvergence
// is an explicit error rather than a nonconservative fallback.
glm::vec3 AngularDriftGenerator(const glm::mat3& inverseInertiaLocal,
    const glm::quat& orientation,const glm::vec3& velocity,float duration);
