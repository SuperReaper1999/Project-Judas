#include "InteractionSystem.h"

#include "Interactable.h"

namespace {
// The facing cone's half-angle, expressed as a cosine (computed once,
// not a runtime radians->cos conversion per candidate per frame) — 60
// degrees is generous enough that a player doesn't have to aim dead-on
// (this is a "look roughly toward it" check, not a precision reticle),
// while still refusing to select something behind or far to the side of
// the player. Not exposed as a per-interactable tunable — this milestone
// needs one consistent detection feel across every interactable kind, not
// per-object aiming tolerances.
constexpr float kFacingCosineThreshold = 0.5f;  // cos(60 degrees)
}  // namespace

Interactable* SelectInteractable(const glm::vec3& playerPosition, const glm::vec3& lookDirection,
                                  const std::vector<Interactable*>& candidates) {
    const glm::vec3 look =
        glm::length(lookDirection) > 1.0e-6f ? glm::normalize(lookDirection) : glm::vec3(0.0f, 0.0f, -1.0f);

    Interactable* best = nullptr;
    float bestDistance = 0.0f;

    for (Interactable* candidate : candidates) {
        if (!candidate || !candidate->CanInteract()) continue;

        const glm::vec3 toCandidate = candidate->GetInteractionPoint() - playerPosition;
        const float distance = glm::length(toCandidate);
        if (distance > candidate->GetInteractionRadius()) continue;

        // A candidate essentially at the player's own position (distance
        // near zero) has no meaningful direction to face toward — accept
        // it on proximity alone rather than dividing by ~zero.
        if (distance > 1.0e-4f) {
            const float facing = glm::dot(toCandidate / distance, look);
            if (facing < kFacingCosineThreshold) continue;
        }

        if (!best || distance < bestDistance) {
            best = candidate;
            bestDistance = distance;
        }
    }

    return best;
}
