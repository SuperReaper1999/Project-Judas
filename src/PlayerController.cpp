#include "PlayerController.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "GravityField.h"
#include "PhysicsWorld.h"
#include "Window.h"

namespace {

// Physical tuning. Explicit and small in number, per the brief — no
// acceleration curves, no sprint, no crouch.
constexpr float kCapsuleRadius = 0.3f;
constexpr float kCapsuleHalfHeight = 0.6f;    // total capsule height: 1.8m
constexpr float kEyeHeightAboveCenter = 0.7f;
constexpr float kMoveSpeed = 4.0f;            // m/s, walking pace
constexpr float kJumpSpeed = 5.0f;            // m/s, imparted opposite the sampled gravity direction

// Mouse look, matching Milestones 2-4.
constexpr float kMouseSensitivity = 0.12f;  // degrees per pixel of mouse motion
constexpr float kMaxPitchDegrees = 89.0f;
constexpr float kFovDegrees = 70.0f;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 500.0f;

// Third-person follow offset. Fixed, not smoothed or collision-checked —
// see docs/ARCHITECTURE.md, "Camera and look controls."
constexpr float kCameraFollowDistance = 4.0f;
constexpr float kCameraHeightOffset = 1.0f;

// Judas's own minimal movement resolution: sweep, stop just short of a hit,
// slide the remainder along the surface, repeat a few times. Not a general
// physics solver — just enough iterations to handle "hit one surface, then
// slide into a second" without visibly getting stuck.
constexpr int kMaxSlideIterations = 4;
constexpr float kSkinMargin = 0.02f;           // stay this far from a surface after moving
constexpr float kGroundProbeDistance = 0.15f;  // how far past the capsule to look for support
constexpr float kMinGroundDot = 0.643f;        // cos(~50 degrees): matches Milestone 4's slope limit

// Returns the shortest-arc rotation that takes unit vector `from` to unit
// vector `to`. Used once per fixed step to keep the player's local frame
// tracking a changing gravity direction — see UpdateFrameOrientation. Not a
// general quaternion-math utility; this is the one rotation this class
// needs.
glm::quat RotationBetweenUnitVectors(const glm::vec3& from, const glm::vec3& to) {
    const float d = std::clamp(glm::dot(from, to), -1.0f, 1.0f);
    if (d > 0.9999f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // already aligned
    }
    if (d < -0.9999f) {
        // Exactly opposite: any axis perpendicular to `from` works.
        const glm::vec3 axis = std::abs(from.x) < 0.9f
                                    ? glm::cross(from, glm::vec3(1.0f, 0.0f, 0.0f))
                                    : glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::normalize(glm::cross(from, to));
    return glm::angleAxis(std::acos(d), axis);
}

}  // namespace

PlayerController::PlayerController(const glm::vec3& spawnCenterPosition, float spawnYawDegrees)
    : m_position(spawnCenterPosition),
      m_frameOrientation(1.0f, 0.0f, 0.0f, 0.0f),
      m_previousPosition(spawnCenterPosition),
      m_previousOrientation(1.0f, 0.0f, 0.0f, 0.0f),
      m_yaw(spawnYawDegrees),
      m_spawnPosition(spawnCenterPosition),
      m_spawnYawDegrees(spawnYawDegrees) {}

bool PlayerController::Spawn(PhysicsWorld& physics) {
    return physics.CreatePlayerShape(kCapsuleRadius, kCapsuleHalfHeight);
}

void PlayerController::Destroy(PhysicsWorld& physics) {
    physics.DestroyPlayerShape();
}

void PlayerController::UpdateFrameInput(Window& window) {
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
    window.GetMouseDelta(mouseDeltaX, mouseDeltaY);

    // Not scaled by deltaTime — relative mouse deltas already represent
    // motion since the last poll (see Milestone 2/3's Camera for the full
    // reasoning), independent of frame rate as-is.
    m_yaw += static_cast<float>(mouseDeltaX) * kMouseSensitivity;
    m_pitch -= static_cast<float>(mouseDeltaY) * kMouseSensitivity;
    m_pitch = std::clamp(m_pitch, -kMaxPitchDegrees, kMaxPitchDegrees);

    // Latch the jump request until a fixed step consumes it, so a short
    // press during a render frame with zero fixed steps isn't lost — see
    // docs/ARCHITECTURE.md, "Simulation timing."
    if (window.ConsumeJumpRequest()) {
        m_jumpRequested = true;
    }
}

glm::vec3 PlayerController::ComputeLocalUp(const glm::vec3& gravityAcceleration) const {
    const float length = glm::length(gravityAcceleration);
    if (length < 1.0e-6f) {
        // Degenerate GravityField sample (no defined direction) — hold the
        // existing frame's up rather than producing a NaN.
        return m_frameOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return -(gravityAcceleration / length);
}

void PlayerController::UpdateFrameOrientation(const glm::vec3& localUp) {
    const glm::vec3 currentUp = m_frameOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::quat delta = RotationBetweenUnitVectors(currentUp, localUp);
    m_frameOrientation = glm::normalize(delta * m_frameOrientation);
}

glm::vec3 PlayerController::ComputeTangentVelocity(const Window& window,
                                                    const glm::vec3& localUp) const {
    // The look-relative reference frame, yawed by mouse input but not
    // pitched — so looking up/down doesn't tilt ground movement off the
    // tangent plane. This mirrors Milestone 4's "flat forward," now
    // expressed relative to the current LOCAL frame instead of world Y, so
    // it stays correct as that frame rotates around the sphere.
    const glm::quat yawedFrame =
        m_frameOrientation * glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 forward = glm::normalize(yawedFrame * glm::vec3(0.0f, 0.0f, -1.0f));
    const glm::vec3 right = glm::normalize(glm::cross(forward, localUp));

    glm::vec3 direction(0.0f);
    if (window.IsActionActive(Action::MoveForward)) direction += forward;
    if (window.IsActionActive(Action::MoveBackward)) direction -= forward;
    if (window.IsActionActive(Action::StrafeRight)) direction += right;
    if (window.IsActionActive(Action::StrafeLeft)) direction -= right;

    if (glm::length(direction) > 0.0f) {
        direction = glm::normalize(direction);
    }
    return direction * kMoveSpeed;
}

void PlayerController::FixedUpdate(const Window& window, PhysicsWorld& physics,
                                    const GravityField& gravity, float fixedDeltaTime) {
    // Presentation history: snapshot the state as of the END of the
    // PREVIOUS step, before this step changes it. This is bookkeeping for
    // rendering only — see GetPresentedPosition/Orientation — and reads
    // nothing that affects the authoritative computation below.
    m_previousPosition = m_position;
    m_previousOrientation = m_frameOrientation;

    const glm::vec3 acceleration = gravity.Sample(m_position);
    const glm::vec3 localUp = ComputeLocalUp(acceleration);
    UpdateFrameOrientation(localUp);

    // Support comes only from an actual geometry query, never a height or
    // distance-from-center comparison: sweep a short distance opposite
    // localUp and see what's there. See docs/ARCHITECTURE.md, "Support."
    //
    // The probe distance (kGroundProbeDistance) is deliberately generous
    // enough to keep catching the surface while standing still or walking,
    // which means a single fixed step's jump departure (kJumpSpeed *
    // fixedDeltaTime, a few centimeters) doesn't move the player outside
    // its reach — so the probe alone would immediately "catch" the very
    // next step after a jump and cancel it before any real arc happened.
    // The fix is the standard one: a hit only counts as support if the
    // player wasn't already moving away from the surface as of last
    // step's velocity — once ascending, the probe is ignored until that
    // stops being true, i.e. until gravity has actually turned the jump
    // around.
    const bool wasAscending = glm::dot(m_velocity, localUp) > 0.0f;
    const ShapeSweepHit groundHit =
        physics.SweepPlayerShape(m_position, m_frameOrientation, -localUp * kGroundProbeDistance);
    const bool isGrounded =
        !wasAscending && groundHit.hit && glm::dot(groundHit.normal, localUp) > kMinGroundDot;
    m_lastGrounded = isGrounded;

    // Vertical speed along `localUp`: held at zero while supported (so
    // standing still doesn't accumulate fall speed into the ground every
    // step), otherwise carried over from last step, then gravity is
    // integrated in either case — same velocity += acceleration*dt pattern
    // PhysicsWorld uses for ordinary bodies, just projected onto localUp so
    // it never fights the horizontal intent below.
    float verticalSpeed = (isGrounded ? 0.0f : glm::dot(m_velocity, localUp)) +
                          glm::dot(acceleration, localUp) * fixedDeltaTime;

    // A jump only ever begins while actually supported, per this step's own
    // geometry query — never a height comparison. Once consumed, the
    // request is cleared unconditionally: a jump attempt made while
    // airborne is discarded, not buffered until landing.
    if (isGrounded && m_jumpRequested) {
        verticalSpeed = kJumpSpeed;
    }
    m_jumpRequested = false;

    const glm::vec3 horizontalVelocity = ComputeTangentVelocity(window, localUp);
    m_velocity = horizontalVelocity + localUp * verticalSpeed;

    // Judas's own minimal move-and-slide: never trust a raw transform
    // write, always resolve displacement against Jolt's collision query.
    glm::vec3 remaining = m_velocity * fixedDeltaTime;
    for (int i = 0; i < kMaxSlideIterations; ++i) {
        const float remainingLength = glm::length(remaining);
        if (remainingLength < 1.0e-6f) break;

        const ShapeSweepHit hit =
            physics.SweepPlayerShape(m_position, m_frameOrientation, remaining);
        if (!hit.hit) {
            m_position += remaining;
            break;
        }

        const float travelDistance = std::max(hit.distance - kSkinMargin, 0.0f);
        const float travelFraction = travelDistance / remainingLength;
        m_position += remaining * travelFraction;

        glm::vec3 leftover = remaining * (1.0f - travelFraction);
        const float intoSurface = glm::dot(leftover, hit.normal);
        if (intoSurface < 0.0f) {
            leftover -= hit.normal * intoSurface;
        }
        remaining = leftover;
    }
}

void PlayerController::Reset() {
    m_position = m_spawnPosition;
    m_velocity = glm::vec3(0.0f);
    m_frameOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    // Synchronize presentation history to the same pose: with both
    // endpoints identical, GetPresentedPosition/Orientation return exactly
    // the reset pose regardless of `alpha`, so the very next render
    // presents the reset position directly — never an interpolation
    // across the world from wherever the player was. See
    // docs/ARCHITECTURE.md, "Reset and discontinuities."
    m_previousPosition = m_position;
    m_previousOrientation = m_frameOrientation;
    m_yaw = m_spawnYawDegrees;
    m_pitch = 0.0f;
    m_jumpRequested = false;
}

glm::vec3 PlayerController::GetPresentedPosition(float alpha) const {
    return glm::mix(m_previousPosition, m_position, std::clamp(alpha, 0.0f, 1.0f));
}

glm::quat PlayerController::GetPresentedOrientation(float alpha) const {
    return glm::slerp(m_previousOrientation, m_frameOrientation, std::clamp(alpha, 0.0f, 1.0f));
}

glm::mat4 PlayerController::GetViewMatrix(float presentationAlpha) const {
    const glm::vec3 presentedPosition = GetPresentedPosition(presentationAlpha);
    const glm::quat presentedOrientation = GetPresentedOrientation(presentationAlpha);
    const glm::vec3 localUp = presentedOrientation * glm::vec3(0.0f, 1.0f, 0.0f);

    // Mouse look (yaw/pitch) is applied on top of the presented orientation
    // unmodified — it already updates every render frame in
    // UpdateFrameInput, so it's already as responsive as rendering itself
    // and needs no interpolation of its own. See docs/ARCHITECTURE.md,
    // "Input responsiveness."
    const glm::quat lookOrientation =
        presentedOrientation * glm::angleAxis(glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::angleAxis(glm::radians(m_pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 front = glm::normalize(lookOrientation * glm::vec3(0.0f, 0.0f, -1.0f));

    const glm::vec3 eyePosition = presentedPosition + localUp * kEyeHeightAboveCenter;
    const glm::vec3 cameraPosition =
        eyePosition - front * kCameraFollowDistance + localUp * kCameraHeightOffset;
    return glm::lookAt(cameraPosition, cameraPosition + front, localUp);
}

glm::mat4 PlayerController::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(kFovDegrees), aspectRatio, kNearPlane, kFarPlane);
}

glm::vec3 PlayerController::GetRenderHalfExtents() const {
    return glm::vec3(kCapsuleRadius, kCapsuleHalfHeight + kCapsuleRadius, kCapsuleRadius);
}
