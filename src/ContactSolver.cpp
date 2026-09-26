#include "ContactSolver.h"

#include <algorithm>
#include <cmath>

#include "RigidBody.h"

namespace {
constexpr float kEpsilon = 1.0e-6f;

// How much of the remaining penetration to correct per position iteration,
// and the small allowed overlap ("slop") left uncorrected so resting
// contacts do not fight the solver every step trying to reach exactly zero
// — unchanged from the pre-M32 solver.
constexpr float kPositionalCorrectionPercent = 0.2f;
constexpr float kPenetrationSlop = 0.005f;

// Restitution only above a meaningful closing speed — otherwise a resting
// contact's own per-step gravity nudge would "bounce" forever at
// ever-smaller amplitude. Unchanged from the pre-M32 solver.
constexpr float kRestitutionVelocityThreshold = 0.5f;

glm::vec3 PointVelocity(const RigidBody& body, const glm::vec3& r) {
    return body.linearVelocity + glm::cross(body.angularVelocity, r);
}

// Linear inverse masses plus the angular contribution each body's inertia
// produces from an impulse along `direction` at offsets rA/rB. No axis
// assumption: `direction` is the contact normal or the current slip.
float InverseEffectiveMass(const ContactConstraint& c, const glm::vec3& rA, const glm::vec3& rB,
                           const glm::vec3& direction) {
    const glm::vec3 angularA = glm::cross(c.inverseInertiaA * glm::cross(rA, direction), rA);
    const glm::vec3 angularB = glm::cross(c.inverseInertiaB * glm::cross(rB, direction), rB);
    return c.bodyA->inverseMass + c.bodyB->inverseMass + glm::dot(direction, angularA + angularB);
}

void ApplyImpulse(ContactConstraint& c, const glm::vec3& rA, const glm::vec3& rB,
                  const glm::vec3& impulse) {
    RigidBody& a = *c.bodyA;
    RigidBody& b = *c.bodyB;
    if (!a.IsStatic()) {
        a.linearVelocity += impulse * a.inverseMass;
        a.angularVelocity += c.inverseInertiaA * glm::cross(rA, impulse);
    }
    if (!b.IsStatic()) {
        b.linearVelocity -= impulse * b.inverseMass;
        b.angularVelocity -= c.inverseInertiaB * glm::cross(rB, impulse);
    }
}
}  // namespace

void ContactSolver::AddContact(RigidBody& bodyA, RigidBody& bodyB, const Contact& contact,
                               float friction, float restitution, float warmNormalImpulse,
                               const glm::vec3& warmTangentImpulse) {
    if (!contact.hit) return;
    if (bodyA.IsStatic() && bodyB.IsStatic()) return;
    ContactConstraint c;
    c.bodyA = &bodyA;
    c.bodyB = &bodyB;
    c.point = contact.point;
    c.normal = contact.normal;
    c.penetration = contact.penetration;
    c.friction = friction;
    m_constraints.push_back(c);
    m_pending.push_back(Pending{restitution, std::max(warmNormalImpulse, 0.0f), warmTangentImpulse});
}

void ContactSolver::Prepare(float fixedDeltaTime) {
    for (std::size_t i = 0; i < m_constraints.size(); ++i) {
        ContactConstraint& c = m_constraints[i];
        const RigidBody& a = *c.bodyA;
        const RigidBody& b = *c.bodyB;
        c.inverseInertiaA = a.IsStatic() ? glm::mat3(0.0f) : a.InverseInertiaWorld();
        c.inverseInertiaB = b.IsStatic() ? glm::mat3(0.0f) : b.InverseInertiaWorld();
        const glm::vec3 rA = c.point - a.position;
        const glm::vec3 rB = c.point - b.position;
        c.localAnchorA = glm::conjugate(a.orientation) * rA;
        c.localAnchorB = glm::conjugate(b.orientation) * rB;
        const float k = InverseEffectiveMass(c, rA, rB, c.normal);
        c.normalMass = k > kEpsilon ? 1.0f / k : 0.0f;
        c.normalImpulse = 0.0f;
        c.tangentImpulse = glm::vec3(0.0f);
        const float closingSpeed = glm::dot(PointVelocity(a, rA) - PointVelocity(b, rB), c.normal);
        const float restitution = m_pending[i].restitution;
        const bool bounces = -closingSpeed > kRestitutionVelocityThreshold;
        const float gap = std::max(-c.penetration, 0.0f);
        if (gap <= 0.0f) {
            c.restitutionBias = bounces ? -restitution * closingSpeed : 0.0f;
        } else {
            // Speculative contact (Milestone 32): the pair is `gap` apart.
            // It may close exactly that gap this step (v_n >= -gap/dt) but
            // not penetrate. Restitution applies only if this step's
            // approach actually reaches the surface.
            const bool reaches = fixedDeltaTime > 0.0f && -closingSpeed * fixedDeltaTime > gap;
            if (reaches && bounces) {
                c.restitutionBias = -restitution * closingSpeed;
            } else {
                c.restitutionBias = fixedDeltaTime > 0.0f ? -gap / fixedDeltaTime : 0.0f;
            }
        }
    }
    // Warm start after every restitution target has been read from the
    // untouched velocities. The friction part is re-projected onto this
    // step's tangent plane and kept inside this step's Coulomb disc.
    for (std::size_t i = 0; i < m_constraints.size(); ++i) {
        ContactConstraint& c = m_constraints[i];
        if (c.normalMass <= 0.0f) continue;
        c.normalImpulse = m_pending[i].warmNormal;
        glm::vec3 tangent = m_pending[i].warmTangent - c.normal * glm::dot(m_pending[i].warmTangent, c.normal);
        const float limit = c.friction * c.normalImpulse;
        const float magnitude = glm::length(tangent);
        if (magnitude > limit) tangent = magnitude > kEpsilon ? tangent * (limit / magnitude) : glm::vec3(0.0f);
        c.tangentImpulse = tangent;
        const glm::vec3 impulse = c.normal * c.normalImpulse + c.tangentImpulse;
        if (glm::dot(impulse, impulse) > 0.0f) {
            ApplyImpulse(c, c.point - c.bodyA->position, c.point - c.bodyB->position, impulse);
        }
    }
    m_pending.clear();
}

void ContactSolver::SolveVelocities(int iterations) {
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (ContactConstraint& c : m_constraints) {
            if (c.normalMass <= 0.0f) continue;
            const glm::vec3 rA = c.point - c.bodyA->position;
            const glm::vec3 rB = c.point - c.bodyB->position;

            // --- Normal: accumulate, clamp the total at zero, apply delta.
            const float vn = glm::dot(PointVelocity(*c.bodyA, rA) - PointVelocity(*c.bodyB, rB), c.normal);
            const float lambda = c.normalMass * (c.restitutionBias - vn);
            const float newNormal = std::max(c.normalImpulse + lambda, 0.0f);
            const float appliedNormal = newNormal - c.normalImpulse;
            c.normalImpulse = newNormal;
            if (appliedNormal != 0.0f) ApplyImpulse(c, rA, rB, c.normal * appliedNormal);

            // --- Friction: Coulomb disc against the ACCUMULATED normal
            // impulse of this point.
            const glm::vec3 relative = PointVelocity(*c.bodyA, rA) - PointVelocity(*c.bodyB, rB);
            const glm::vec3 slip = relative - c.normal * glm::dot(relative, c.normal);
            const float slipSpeed = glm::length(slip);
            const float limit = c.friction * c.normalImpulse;
            glm::vec3 newTangent = c.tangentImpulse;
            if (slipSpeed > kEpsilon) {
                const glm::vec3 direction = slip / slipSpeed;
                const float k = InverseEffectiveMass(c, rA, rB, direction);
                if (k > kEpsilon) newTangent -= direction * (slipSpeed / k);
            }
            const float magnitude = glm::length(newTangent);
            if (magnitude > limit) {
                newTangent = magnitude > kEpsilon ? newTangent * (limit / magnitude) : glm::vec3(0.0f);
            }
            const glm::vec3 appliedTangent = newTangent - c.tangentImpulse;
            c.tangentImpulse = newTangent;
            if (glm::dot(appliedTangent, appliedTangent) > 0.0f) ApplyImpulse(c, rA, rB, appliedTangent);
        }
    }
}

void ContactSolver::SolvePositions(int iterations) {
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (ContactConstraint& c : m_constraints) {
            RigidBody& a = *c.bodyA;
            RigidBody& b = *c.bodyB;
            const float inverseMassSum = a.inverseMass + b.inverseMass;
            if (inverseMassSum <= kEpsilon) continue;
            // Both anchors coincided with the contact point at detection;
            // their current separation along the normal is how much the
            // bodies have moved apart (positive) or together since.
            const glm::vec3 worldA = a.position + a.orientation * c.localAnchorA;
            const glm::vec3 worldB = b.position + b.orientation * c.localAnchorB;
            const float penetration = c.penetration - glm::dot(worldA - worldB, c.normal);
            const float correctionMagnitude =
                std::max(penetration - kPenetrationSlop, 0.0f) * kPositionalCorrectionPercent;
            if (correctionMagnitude <= 0.0f) continue;
            const glm::vec3 correction = c.normal * (correctionMagnitude / inverseMassSum);
            if (!a.IsStatic()) a.position += correction * a.inverseMass;
            if (!b.IsStatic()) b.position -= correction * b.inverseMass;
        }
    }
}
