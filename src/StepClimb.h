#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"

// Milestone 10: the player's automatic step-up/step-down primitive,
// expressed purely in terms of PhysicsWorld::SweepPlayerShape queries
// relative to a caller-supplied `localUp` — never a world-Y assumption, and
// never a "what am I standing on" identity check. See
// docs/ARCHITECTURE.md, "Milestone 10," for the full design and the
// rotate-the-scenario tests (tests/StepClimbTests.cpp) that verify these
// two functions produce identical results under an arbitrarily rotated
// scenario.
//
// Deliberately two small free functions, not a class or a "character
// controller" abstraction — `PlayerController::FixedUpdate` is still the
// only place that owns WHEN to call these and what to do with the result.

// Attempts to carry the player's capsule up and over a short obstruction
// while moving horizontally. `horizontalDisplacement` is the desired
// tangential (perpendicular to `localUp`) displacement for this step —
// typically the player's full remaining move-and-slide displacement before
// any sliding has reduced it. The technique is the standard "step up, move
// forward, step back down" three-sweep pattern:
//
//   1. Would a flat sweep from `position` already succeed, or is it
//      blocked by something not already walkable? If already walkable or
//      unobstructed, there is nothing to step over — return false and let
//      the caller's own ordinary move-and-slide handle it (slopes are
//      never routed through this function).
//   2. Sweep up by up to `maxStepHeight`.
//   3. Sweep forward by `horizontalDisplacement` from the raised position.
//      If this makes no more progress than the flat sweep did, stepping
//      didn't help — return false.
//   4. Sweep back down by up to `maxStepHeight` (plus a small margin) to
//      reacquire a walkable floor. If none is found, the "step" would have
//      landed in open air — not a real step — return false.
//
// A real bug hit and fixed while building this, worth knowing before
// touching the down-sweep again: sweeping straight down from EXACTLY where
// the forward sweep stopped can catch the obstruction's own front edge (a
// box's top-vs-front-face corner) rather than its flat top, since a
// forward-blocked step's landing point is very often right at that edge —
// closest-point-on-OBB blends the two faces' normals there, which can read
// as just barely too steep to count as walkable even though the surface
// itself is flat. Fixed with a small forward look-ahead on the down-probe
// origin only (see StepClimb.cpp's kDownProbeLookAhead) so it samples
// safely past the lip.
//
// Returns true and fills `outNewPosition` only when all of the above
// succeed, i.e. stepping produced genuine forward progress a flat sweep
// from `position` could not have achieved on its own.
bool TryStepMove(const PhysicsWorld& physics, const glm::vec3& position,
                  const glm::quat& orientation, const glm::vec3& localUp,
                  const glm::vec3& horizontalDisplacement, float maxStepHeight, float minGroundDot,
                  float skinMargin, glm::vec3& outNewPosition);

// Attempts to catch a small drop-off directly below the player (a step
// down, or walking off a low ledge) by reaching further than the caller's
// own ordinary ground probe — up to `maxStepHeight` along `-localUp` —
// without an intermediate free-fall frame. Returns true and fills
// `outNewPosition`/`outNormal`/`outHitBody` only if a walkable surface
// (per `minGroundDot`) is found within that reach; false otherwise,
// leaving the caller's own airborne-integration fallback to handle a
// genuine drop taller than `maxStepHeight`. `outHitBody` identifies what
// was landed on (mirroring `ShapeSweepHit::hitBody`'s own convention),
// meaningful only when this returns true — callers that need to know what
// the player is now standing on (moving-support velocity carry, the
// flying primitive's take-control gating) need this, not just a position.
bool TryStepDown(const PhysicsWorld& physics, const glm::vec3& position,
                  const glm::quat& orientation, const glm::vec3& localUp, float maxStepHeight,
                  float minGroundDot, float skinMargin, glm::vec3& outNewPosition,
                  glm::vec3& outNormal, BodyHandle& outHitBody);
