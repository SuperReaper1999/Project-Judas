#include "ContactSolver.h"

#include <algorithm>
#include <cmath>

#include "RigidBody.h"

namespace {
constexpr float kEpsilon = 1.0e-6f;

// How much of the remaining penetration to correct per step (not all of
// it at once, which would overshoot/jitter), and the small allowed
// overlap ("slop") left uncorrected so resting contacts don't fight the
// solver every single step trying to reach exactly zero penetration —
// both ordinary, well-established constants for this class of simple
// positional-correction scheme.
constexpr float kPositionalCorrectionPercent = 0.2f;
constexpr float kPenetrationSlop = 0.005f;

// The effective mass along `direction` for an impulse applied at `point`
// to both bodies — the standard rigid-body contact formula: linear inverse
// mass plus the angular contribution each body's own inertia tensor
// produces from that same impulse. No axis assumption: `direction` is
// whatever the caller (normal or tangent) hands in.
float EffectiveMass(const RigidBody& bodyA, const RigidBody& bodyB, const glm::vec3& point,
                     const glm::vec3& direction) {
    const glm::vec3 rA = point - bodyA.position;
    const glm::vec3 rB = point - bodyB.position;
    const glm::vec3 angularTermA =
        glm::cross(bodyA.InverseInertiaWorld() * glm::cross(rA, direction), rA);
    const glm::vec3 angularTermB =
        glm::cross(bodyB.InverseInertiaWorld() * glm::cross(rB, direction), rB);
    return bodyA.inverseMass + bodyB.inverseMass + glm::dot(direction, angularTermA + angularTermB);
}

glm::vec3 PointVelocity(const RigidBody& body, const glm::vec3& point) {
    return body.linearVelocity + glm::cross(body.angularVelocity, point - body.position);
}

}  // namespace

void ResolveContact(RigidBody& bodyA, RigidBody& bodyB, const Contact& contact, float friction,
                     float restitution) {
    if (!contact.hit) return;
    if (bodyA.IsStatic() && bodyB.IsStatic()) return;

    const glm::vec3& normal = contact.normal;
    const glm::vec3& point = contact.point;

    // --- Normal impulse (non-penetration + restitution) ---
    const glm::vec3 relativeVelocity = PointVelocity(bodyA, point) - PointVelocity(bodyB, point);
    const float velocityAlongNormal = glm::dot(relativeVelocity, normal);

    const float normalEffectiveMass = EffectiveMass(bodyA, bodyB, point, normal);
    float normalImpulseMagnitude = 0.0f;
    if (velocityAlongNormal < 0.0f && normalEffectiveMass > kEpsilon) {
        // Only apply restitution to a meaningfully fast closing speed —
        // otherwise a resting contact's own tiny per-step gravity nudge
        // would "bounce" forever at ever-smaller amplitude and never
        // settle. A small fixed threshold (not zero) is the standard fix.
        constexpr float kRestitutionVelocityThreshold = 0.5f;
        const float effectiveRestitution =
            (-velocityAlongNormal > kRestitutionVelocityThreshold) ? restitution : 0.0f;
        normalImpulseMagnitude =
            -(1.0f + effectiveRestitution) * velocityAlongNormal / normalEffectiveMass;
        normalImpulseMagnitude = std::max(normalImpulseMagnitude, 0.0f);
        const glm::vec3 normalImpulse = normal * normalImpulseMagnitude;
        bodyA.ApplyImpulseAtPoint(normalImpulse, point);
        bodyB.ApplyImpulseAtPoint(-normalImpulse, point);
    }

    // --- Friction impulse (Coulomb, clamped by the normal impulse just
    // applied) --- recompute relative velocity, since the normal impulse
    // above may have changed it.
    const glm::vec3 relativeVelocityAfterNormal =
        PointVelocity(bodyA, point) - PointVelocity(bodyB, point);
    const glm::vec3 tangentVelocity =
        relativeVelocityAfterNormal - normal * glm::dot(relativeVelocityAfterNormal, normal);
    const float tangentSpeed = glm::length(tangentVelocity);
    if (tangentSpeed > kEpsilon) {
        const glm::vec3 tangent = tangentVelocity / tangentSpeed;
        const float tangentEffectiveMass = EffectiveMass(bodyA, bodyB, point, tangent);
        if (tangentEffectiveMass > kEpsilon) {
            float frictionImpulseMagnitude = -tangentSpeed / tangentEffectiveMass;
            const float maxFriction = friction * normalImpulseMagnitude;
            frictionImpulseMagnitude =
                std::clamp(frictionImpulseMagnitude, -maxFriction, maxFriction);
            const glm::vec3 frictionImpulse = tangent * frictionImpulseMagnitude;
            bodyA.ApplyImpulseAtPoint(frictionImpulse, point);
            bodyB.ApplyImpulseAtPoint(-frictionImpulse, point);
        }
    }

    // --- Positional correction --- directly separates overlapping bodies
    // along the normal, proportional to how far they interpenetrate,
    // distributed by inverse mass (a body with more inverse mass — less
    // actual mass — yields more). Deliberately NOT run through velocity:
    // a velocity-only (pure Baumgarte) correction couples into restitution
    // and can add energy; nudging position directly avoids that while
    // still preventing bodies from sinking into each other over many
    // steps.
    const float correctionMagnitude =
        std::max(contact.penetration - kPenetrationSlop, 0.0f) * kPositionalCorrectionPercent;
    const float inverseMassSum = bodyA.inverseMass + bodyB.inverseMass;
    if (correctionMagnitude > 0.0f && inverseMassSum > kEpsilon) {
        const glm::vec3 correction = normal * (correctionMagnitude / inverseMassSum);
        if (!bodyA.IsStatic()) bodyA.position += correction * bodyA.inverseMass;
        if (!bodyB.IsStatic()) bodyB.position -= correction * bodyB.inverseMass;
    }
}
