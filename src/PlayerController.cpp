#include "PlayerController.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "GravityField.h"
#include "PhysicsWorld.h"
#include "Window.h"

namespace {

// Physical tuning. Explicit and small in number, per the brief — no
// acceleration curves, no sprint, no crouch.
constexpr float kCapsuleRadius = 0.3f;
constexpr float kCapsuleHalfHeight = 0.6f;  // total capsule height: 1.8m
constexpr float kMass = 70.0f;              // kg; also CharacterVirtualSettings' own default
constexpr float kEyeHeight = 1.6f;          // above the feet position
constexpr float kMoveSpeed = 4.0f;          // m/s, walking pace
constexpr float kJumpSpeed = 5.0f;          // m/s, imparted opposite the sampled gravity direction

// Mouse look, matching Milestones 2-3's free camera exactly.
constexpr float kMouseSensitivity = 0.12f;  // degrees per pixel of mouse motion
constexpr float kMaxPitchDegrees = 89.0f;
constexpr float kFovDegrees = 70.0f;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 500.0f;

// Third-person follow offset. Fixed, not smoothed or collision-checked —
// see docs/ARCHITECTURE.md, "Camera and look controls."
constexpr float kCameraFollowDistance = 4.0f;
constexpr float kCameraHeightOffset = 1.0f;

}  // namespace

PlayerController::PlayerController(const glm::vec3& spawnFeetPosition, float spawnYawDegrees)
    : m_spawnFeetPosition(spawnFeetPosition),
      m_spawnYawDegrees(spawnYawDegrees),
      m_yaw(spawnYawDegrees),
      m_pitch(0.0f) {}

bool PlayerController::Spawn(PhysicsWorld& physics, const GravityField& gravity) {
    const glm::vec3 up = -glm::normalize(gravity.Sample(m_spawnFeetPosition));
    return physics.CreatePlayer(m_spawnFeetPosition, up, kCapsuleRadius, kCapsuleHalfHeight, kMass);
}

void PlayerController::Destroy(PhysicsWorld& physics) {
    physics.DestroyPlayer();
}

void PlayerController::UpdateFrameInput(Window& window) {
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
    window.GetMouseDelta(mouseDeltaX, mouseDeltaY);

    // Not scaled by deltaTime — see Milestone 2/3's Camera for why relative
    // mouse deltas are already frame-rate independent as-is.
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

glm::vec3 PlayerController::Front() const {
    const float yawRad = glm::radians(m_yaw);
    const float pitchRad = glm::radians(m_pitch);
    glm::vec3 front;
    front.x = std::cos(pitchRad) * std::cos(yawRad);
    front.y = std::sin(pitchRad);
    front.z = std::cos(pitchRad) * std::sin(yawRad);
    return glm::normalize(front);
}

glm::vec3 PlayerController::ComputeHorizontalVelocity(const Window& window) const {
    // This horizontal movement basis assumes a Y-up ground plane — this
    // milestone's convention, matching FaithfulGravity's current constant
    // direction, NOT a claim that "gravity direction" and "ground plane
    // orientation" are the same concept in general. See
    // docs/ARCHITECTURE.md, "Ground/support semantics." Vertical motion
    // (gravity integration, jump direction) is computed separately in
    // FixedUpdate from the actual sampled gravity, never from this axis.
    const float yawRad = glm::radians(m_yaw);
    const glm::vec3 frontFlat(std::cos(yawRad), 0.0f, std::sin(yawRad));
    const glm::vec3 rightFlat = glm::normalize(glm::cross(frontFlat, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 direction(0.0f);
    if (window.IsActionActive(Action::MoveForward)) direction += frontFlat;
    if (window.IsActionActive(Action::MoveBackward)) direction -= frontFlat;
    if (window.IsActionActive(Action::StrafeRight)) direction += rightFlat;
    if (window.IsActionActive(Action::StrafeLeft)) direction -= rightFlat;

    if (glm::length(direction) > 0.0f) {
        direction = glm::normalize(direction);
    }
    return direction * kMoveSpeed;
}

void PlayerController::FixedUpdate(const Window& window, PhysicsWorld& physics,
                                    const GravityField& gravity, float fixedDeltaTime) {
    const glm::vec3 position = physics.GetPlayerPosition();
    const glm::vec3 acceleration = gravity.Sample(position);

    // Direction away from gravity, resampled fresh every fixed step (never
    // cached), so a jump always matches whatever the active GravityField
    // currently reports at this position — never a hard-coded world axis.
    const glm::vec3 up = -glm::normalize(acceleration);

    const PlayerGroundContact ground = physics.GetPlayerGroundContact();

    // Vertical speed along `up`: start from whatever it already was
    // (ground velocity if supported, the player's own velocity if not),
    // then integrate this step's gravity — the same
    // velocity-+= acceleration*dt pattern used for every other body in
    // PhysicsWorld, just projected onto `up` so it never fights the
    // horizontal intent below.
    const glm::vec3 baseVelocity = ground.isGrounded ? ground.velocity : physics.GetPlayerVelocity();
    float verticalSpeed = glm::dot(baseVelocity, up) + glm::dot(acceleration, up) * fixedDeltaTime;

    // A jump only ever begins while actually supported, per the physics
    // controller's own contact state — never a height comparison. Once
    // consumed, the request is cleared unconditionally: a jump attempt
    // made while airborne is discarded, not buffered until landing.
    if (ground.isGrounded && m_jumpRequested) {
        verticalSpeed = kJumpSpeed;
    }
    m_jumpRequested = false;

    const glm::vec3 horizontalVelocity = ComputeHorizontalVelocity(window);
    physics.SetPlayerVelocity(horizontalVelocity + up * verticalSpeed);
    physics.UpdatePlayer(fixedDeltaTime, acceleration);
}

void PlayerController::Reset(PhysicsWorld& physics) {
    physics.ResetPlayer(m_spawnFeetPosition);
    m_yaw = m_spawnYawDegrees;
    m_pitch = 0.0f;
    m_jumpRequested = false;
}

glm::mat4 PlayerController::GetViewMatrix(const PhysicsWorld& physics) const {
    const glm::vec3 eyePosition = physics.GetPlayerPosition() + glm::vec3(0.0f, kEyeHeight, 0.0f);
    const glm::vec3 front = Front();
    const glm::vec3 cameraPosition =
        eyePosition - front * kCameraFollowDistance + glm::vec3(0.0f, kCameraHeightOffset, 0.0f);
    return glm::lookAt(cameraPosition, cameraPosition + front, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 PlayerController::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(kFovDegrees), aspectRatio, kNearPlane, kFarPlane);
}

glm::vec3 PlayerController::GetRenderCenter(const PhysicsWorld& physics) const {
    return physics.GetPlayerPosition() + glm::vec3(0.0f, kCapsuleHalfHeight + kCapsuleRadius, 0.0f);
}

glm::vec3 PlayerController::GetRenderHalfExtents() const {
    return glm::vec3(kCapsuleRadius, kCapsuleHalfHeight + kCapsuleRadius, kCapsuleRadius);
}
