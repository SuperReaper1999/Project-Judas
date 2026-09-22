#include "StepClimb.h"

#include <algorithm>

namespace {
// A tiny threshold so a step whose forward sweep makes only numerically-
// noisy "progress" over the flat attempt isn't treated as a real step —
// not a tuning knob, just floating-point hygiene.
constexpr float kMinStepImprovement = 0.01f;
constexpr float kDownSweepMargin = 0.05f;

// When the forward sweep lands right at the FRONT EDGE of the step being
// climbed (a very common case: the forward displacement is often just
// barely enough to clear the edge, not walk deep onto the step), a
// straight-down sweep from exactly that point can catch the edge itself —
// where the box's own closest-point-on-OBB normal blends the top face and
// the front face together, reading as slightly too steep to count as
// walkable even though the player is plainly standing on an ordinary flat
// top. Nudging the down-probe a little further onto the step first (never
// more than half this step's own forward travel, so a very small step
// doesn't get a disproportionate nudge) reliably samples the flat top
// instead of its lip — the same category of small, deliberate approximation
// kSkinMargin already is throughout this engine's own move-and-slide code.
constexpr float kDownProbeLookAhead = 0.08f;
}  // namespace

bool TryStepMove(const PhysicsWorld& physics, const glm::vec3& position,
                  const glm::quat& orientation, const glm::vec3& localUp,
                  const glm::vec3& horizontalDisplacement, float maxStepHeight, float minGroundDot,
                  float skinMargin, glm::vec3& outNewPosition) {
    const float horizontalLength = glm::length(horizontalDisplacement);
    if (horizontalLength < 1.0e-6f) return false;

    // The caller only ever calls this while grounded, i.e. already resting
    // exactly on a walkable surface — a raw sweep from `position` itself
    // would immediately report that same surface as an "already touching"
    // hit at distance 0 (see SweepPlayerShape's own t=0 check),
    // REGARDLESS of the sweep direction, since it's the single closest
    // body from that exact point. That would make every flat/up/forward
    // sweep below see only the floor and never anything ahead. Lifting
    // the query origin by `skinMargin` first — the same margin-restore
    // idiom the ordinary move-and-slide loop already uses, just applied
    // once up front instead of discovered iteratively — clears that
    // immediate contact so these sweeps can actually see a wall ahead.
    const glm::vec3 probeOrigin = position + localUp * skinMargin;

    // 1) How far does an ordinary flat sweep get? If it's already
    // unobstructed, or blocked by something that's already walkable ground
    // (a slope, not a step-shaped wall), there is nothing to step over —
    // slopes are handled entirely by the caller's own existing move-and-
    // slide loop, never routed through here.
    const ShapeSweepHit flatHit =
        physics.SweepPlayerShape(probeOrigin, orientation, horizontalDisplacement);
    if (!flatHit.hit) return false;
    if (glm::dot(flatHit.normal, localUp) > minGroundDot) return false;
    const float flatDistance = flatHit.distance;

    // 2) Sweep up by up to maxStepHeight.
    const ShapeSweepHit upHit =
        physics.SweepPlayerShape(probeOrigin, orientation, localUp * maxStepHeight);
    const float upDistance = upHit.hit ? upHit.distance : maxStepHeight;
    if (upDistance < 1.0e-4f) return false;  // nothing to rise into, e.g. a low ceiling right here
    const glm::vec3 raisedPosition = probeOrigin + localUp * upDistance;

    // 3) Sweep forward from the raised position. If this makes no more
    // progress than the flat attempt already did, stepping didn't help.
    //
    // Milestone 12 bugfix: a raised sweep that reaches the FULL requested
    // displacement completely unobstructed (`!forwardHit.hit`) is always a
    // genuine, complete success, regardless of how close the flat sweep
    // happened to get on its own — the `kMinStepImprovement` margin below
    // exists to reject floating-point noise between two BLOCKED sweeps
    // (flat vs. raised-then-forward both stopped by something), not to
    // second-guess a sweep that cleared entirely. Skipping the margin
    // check in the fully-clear case matters most for a SMALL per-step
    // displacement (an ordinary walking step, a few centimeters) against
    // an obstruction the flat sweep already happened to reach 90%+ of the
    // way into. Verified this changes nothing observable for a STATIC
    // obstruction (its distance only ever shrinks step over step as the
    // player keeps closing in — see tests/StepClimbTests.cpp, still
    // passing); it matters for a PUSHABLE dynamic body specifically (see
    // docs/ARCHITECTURE.md, "Milestone 12," for the walking-into-the-
    // spacecraft scenario this was found in).
    const ShapeSweepHit forwardHit =
        physics.SweepPlayerShape(raisedPosition, orientation, horizontalDisplacement);
    if (forwardHit.hit && forwardHit.distance <= flatDistance + kMinStepImprovement) return false;
    const float forwardDistance = forwardHit.hit ? forwardHit.distance : horizontalLength;

    // Stop just short of anything the forward sweep hit, same skin-margin
    // convention the ordinary move-and-slide loop already uses.
    const float travelDistance = forwardHit.hit ? std::max(0.0f, forwardDistance - skinMargin)
                                                 : forwardDistance;
    const glm::vec3 steppedForwardPosition =
        raisedPosition + horizontalDisplacement * (travelDistance / horizontalLength);

    // 4) Sweep back down to reacquire a walkable floor. A step that lands
    // in open air, or on something too steep to stand on, isn't a real
    // step — abandon it and let the caller's ordinary handling take over
    // (which will very likely just block on the same obstruction it always
    // would have, rather than silently launching the player up onto
    // whatever the up-sweep happened to clear).
    const float lookAhead = std::min(kDownProbeLookAhead, travelDistance * 0.5f);
    const glm::vec3 downProbeOrigin =
        steppedForwardPosition + (horizontalDisplacement / horizontalLength) * lookAhead;
    const float downSweepDistance = maxStepHeight + kDownSweepMargin;
    const ShapeSweepHit downHit =
        physics.SweepPlayerShape(downProbeOrigin, orientation, -localUp * downSweepDistance);
    if (!downHit.hit) return false;
    if (glm::dot(downHit.normal, localUp) <= minGroundDot) return false;

    outNewPosition = downProbeOrigin - localUp * std::max(0.0f, downHit.distance - skinMargin);
    return true;
}

bool TryStepDown(const PhysicsWorld& physics, const glm::vec3& position,
                  const glm::quat& orientation, const glm::vec3& localUp, float maxStepHeight,
                  float minGroundDot, float skinMargin, glm::vec3& outNewPosition,
                  glm::vec3& outNormal, BodyHandle& outHitBody) {
    const ShapeSweepHit downHit =
        physics.SweepPlayerShape(position, orientation, -localUp * maxStepHeight);
    if (!downHit.hit) return false;
    if (glm::dot(downHit.normal, localUp) <= minGroundDot) return false;

    outNewPosition = position - localUp * std::max(0.0f, downHit.distance - skinMargin);
    outNormal = downHit.normal;
    outHitBody = downHit.hitBody;
    return true;
}
