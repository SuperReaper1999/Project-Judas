#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"

// Milestone 11's central new capability: the explicit, authoritative
// kinematic relationship that keeps a piloting player's body glued to the
// spacecraft's own pose through arbitrary translation AND rotation —
// including upside down. This is NOT ordinary grounding (see
// docs/ARCHITECTURE.md, "Support") and NOT a render-only parent transform:
// it exists in the fixed-step simulation, produced fresh every step from
// the spacecraft's own real authoritative pose, never predicted or
// accumulated incrementally (so there is nothing for per-step error to
// accumulate INTO — see ApplyPilotAttachment).
//
// Deliberately three small free functions plus one plain struct, not a
// class, a joint type, or a general attachment/reference-frame framework —
// the same "two free functions, not a character-controller abstraction"
// shape StepClimb.h already established for a different, smaller problem.
// This models exactly one relationship (the player, secured to the one
// spacecraft it's currently piloting); see src/PilotControl.h for the
// "when do these get called" policy shared by the interactive game loop and
// the JUDAS_TEST_SCRIPT harness.
struct PilotAttachment {
    bool attached = false;

    // The player's pose relative to the spacecraft's own body frame,
    // captured once at the instant control is acquired (see
    // BeginPilotAttachment) and held fixed for the ENTIRE duration of one
    // piloting session — never recomputed or drifted while attached, so a
    // long flight with many rotations accumulates exactly zero attachment
    // error (see ApplyPilotAttachment: it is a pure function of the ship's
    // CURRENT transform and these two fields, not an incremental update).
    glm::vec3 localOffset{0.0f};
    glm::quat localOrientation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Captures the player's CURRENT world pose relative to the spacecraft's
// CURRENT authoritative transform — its actual resolved pose as of the end
// of the most recent PhysicsWorld::Step, never a predicted or presented
// one (see docs/ARCHITECTURE.md, "Milestone 11," for why that distinction
// matters here). Called exactly once, at the instant piloting control is
// acquired; see src/PilotControl.h.
void BeginPilotAttachment(PilotAttachment& attachment, const BodyTransform& shipTransform,
                           const glm::vec3& playerPosition, const glm::quat& playerOrientation);

// Reconstructs a world pose from the spacecraft's CURRENT transform and the
// attachment's stored local offset/orientation — the inverse of
// BeginPilotAttachment's own capture. Used for two distinct purposes with
// the exact same math (see docs/ARCHITECTURE.md, "Milestone 11"):
//   1. Once per fixed step while attached, from the spacecraft's fresh
//      AUTHORITATIVE transform, to drive the player's own authoritative
//      pose (see PlayerController::FixedUpdateAttached).
//   2. Once per render, from the spacecraft's PRESENTED (interpolated)
//      transform, so the rendered pilot and camera stay coherent with the
//      exact same presented spacecraft pose rather than interpolating the
//      player's own authoritative snapshots separately — see
//      docs/ARCHITECTURE.md, "Milestone 11, Camera and presentation."
void ApplyPilotAttachment(const PilotAttachment& attachment, const BodyTransform& shipTransform,
                           glm::vec3& outPosition, glm::quat& outOrientation);

// The world-space velocity the player should inherit at the instant of
// release: the spacecraft's own linear velocity PLUS the velocity
// contributed by its angular velocity at the player's own offset from the
// spacecraft's center of mass — ordinary rigid-body point velocity,
// v + omega x r, the exact same formula PlayerController's own
// moving-support carry already uses for a rotating support (see
// docs/ARCHITECTURE.md, "Milestone 8," "The moving-support bug"). Takes the
// player's CURRENT world position (not the stored local offset) as `r`'s
// basis, since that's the actual point being released.
glm::vec3 ComputePilotReleaseVelocity(const BodyTransform& shipTransform,
                                       const glm::vec3& shipLinearVelocity,
                                       const glm::vec3& shipAngularVelocity,
                                       const glm::vec3& playerPosition);
