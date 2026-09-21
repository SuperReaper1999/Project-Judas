#include "GravityResolver.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

namespace {

// Smooth (C1-continuous, zero derivative at both ends) falloff from full
// weight at innerRadius to zero weight at outerRadius — an ordinary
// smoothstep, chosen so a zone's contribution never has a slope
// discontinuity as a consumer crosses either boundary (see
// docs/ARCHITECTURE.md, "Transition semantics" — acceleration continuity
// is a stated requirement).
float ZoneWeight(float distance, float innerRadius, float outerRadius) {
    if (distance <= innerRadius) return 1.0f;
    if (distance >= outerRadius) return 0.0f;
    const float t = (distance - innerRadius) / (outerRadius - innerRadius);
    return 1.0f - (t * t * (3.0f - 2.0f * t));
}

// Spherical (great-circle) interpolation between two UNIT vectors — the
// same mathematically-correct tool PlayerController already uses for
// orientation (glm::slerp on quaternions), just applied directly to a
// direction vector instead of a rotation, and self-contained here rather
// than reaching for GLM's less-commonly-used vector-slerp extension.
// Guards the same two degenerate cases
// PlayerController::RotationBetweenUnitVectors already guards, for the
// same underlying reason: near-identical directions (dividing by a
// near-zero sin(theta) below would be numerically unstable) and exactly
// opposite directions (no unique great-circle path — pick an arbitrary
// perpendicular axis, same trick).
glm::vec3 SlerpUnitVectors(const glm::vec3& from, const glm::vec3& to, float t) {
    const float cosTheta = std::clamp(glm::dot(from, to), -1.0f, 1.0f);
    if (cosTheta > 0.9995f) {
        return glm::normalize(glm::mix(from, to, t));
    }
    if (cosTheta < -0.9995f) {
        const glm::vec3 axis = std::abs(from.x) < 0.9f
                                    ? glm::normalize(glm::cross(from, glm::vec3(1.0f, 0.0f, 0.0f)))
                                    : glm::normalize(glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f)));
        const float angle = glm::pi<float>() * t;
        return glm::normalize(std::cos(angle) * from + std::sin(angle) * axis);
    }
    const float theta = std::acos(cosTheta);
    const float sinTheta = std::sin(theta);
    return (std::sin((1.0f - t) * theta) / sinTheta) * from + (std::sin(t * theta) / sinTheta) * to;
}

}  // namespace

void GravityResolver::AddZone(GravityField& field, const glm::vec3& falloffCenter,
                               float innerRadius, float outerRadius) {
    m_zones.push_back(Zone{&field, falloffCenter, innerRadius, outerRadius});
}

glm::vec3 GravityResolver::Sample(const glm::vec3& worldPosition) const {
    glm::vec3 blendedDirection(0.0f);
    float weightedAvgMagnitude = 0.0f;
    float totalWeight = 0.0f;

    for (const Zone& zone : m_zones) {
        const float distance = glm::length(worldPosition - zone.falloffCenter);
        const float weight = ZoneWeight(distance, zone.innerRadius, zone.outerRadius);
        if (weight <= 0.0f) continue;

        const glm::vec3 acceleration = zone.field->Sample(worldPosition);
        const float magnitude = glm::length(acceleration);
        if (magnitude < 1.0e-6f) {
            // This zone's own source returned a degenerate (near-zero)
            // sample at this exact position (e.g. RadicalGravity sampled
            // exactly at its center) — exclude it from the blend rather
            // than folding an undefined direction in. See
            // docs/ARCHITECTURE.md, "Degenerate gravity handling."
            continue;
        }
        const glm::vec3 direction = acceleration / magnitude;

        if (totalWeight < 1.0e-6f) {
            blendedDirection = direction;
            weightedAvgMagnitude = magnitude;
        } else {
            const float t = weight / (totalWeight + weight);
            blendedDirection = SlerpUnitVectors(blendedDirection, direction, t);
            weightedAvgMagnitude += t * (magnitude - weightedAvgMagnitude);
        }
        totalWeight += weight;
    }

    if (totalWeight < 1.0e-6f) {
        // No zone has meaningful influence here — the same "no defined
        // gravity" signal a lone GravityField already returns in its own
        // degenerate case (e.g. RadicalGravity exactly at its center).
        return glm::vec3(0.0f);
    }

    // `weightedAvgMagnitude` alone only answers "how strong is gravity
    // among the zone(s) currently contributing" — it says nothing about
    // whether those zones are contributing weakly or strongly overall. A
    // single zone fading from weight 1.0 to 0.0 would otherwise still
    // report its full magnitude at weight 0.001, then snap straight to
    // zero the instant weight actually reaches 0 — a hard cliff, not the
    // smooth fade ZoneWeight's falloff shape is meant to produce.
    // Multiplying by the combined weight (clamped to 1, so two
    // simultaneously full-strength zones average rather than sum) fixes
    // that: the result now genuinely tends to zero as every contributing
    // zone's own influence does. See docs/ARCHITECTURE.md, "Transition
    // semantics."
    const float overallIntensity = std::min(totalWeight, 1.0f);
    return blendedDirection * (weightedAvgMagnitude * overallIntensity);
}
