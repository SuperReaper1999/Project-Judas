#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Window;
class PhysicsWorld;
class GravityField;

// Judas-owned player. Unlike Milestone 4 (which delegated locomotion to
// Jolt's CharacterVirtual), this class itself decides desired locomotion,
// gravity response, local up/down, jump behavior, support interpretation,
// and orientation — every piece of state here (position, velocity,
// orientation) is plain data owned by Judas, not a Jolt body or character
// controller of any kind. Jolt is consulted only for the raw geometric
// question "how far can this shape move, and what does it touch?" via
// PhysicsWorld::SweepPlayerShape. See docs/ARCHITECTURE.md,
// "Player/controller ownership."
//
// This class also computes the player's camera transform for rendering —
// unchanged in spirit from Milestone 4 (a fixed third-person offset along
// the current look direction), but now built relative to the player's own
// local frame instead of world +Y, so it keeps working as that frame
// rotates while walking around a curved surface.
class PlayerController {
public:
    PlayerController(const glm::vec3& spawnCenterPosition, float spawnYawDegrees);

    // Creates the underlying collision shape (a capsule, used only for
    // queries — see PhysicsWorld::CreatePlayerShape). No body, no character
    // controller.
    bool Spawn(PhysicsWorld& physics);
    void Destroy(PhysicsWorld& physics);

    // Reads mouse-look delta and latches jump key presses. Call exactly
    // once per render frame, regardless of how many (or how few) fixed
    // physics steps happen that frame.
    void UpdateFrameInput(Window& window);

    // Advances the player by exactly one fixed physics step: samples
    // gravity at the player's own current position, derives local up from
    // it, reorients the player's local frame to match, determines support
    // via a geometry query (never a height comparison), integrates
    // gravity/jump into vertical velocity, combines it with WASD's
    // tangent-plane movement intent, and resolves the resulting
    // displacement against collision via a minimal move-and-slide loop.
    void FixedUpdate(const Window& window, PhysicsWorld& physics, const GravityField& gravity,
                      float fixedDeltaTime);

    // All player state lives in this class (position, velocity,
    // orientation, pending jump) — there is no physics-side state to reset
    // separately, unlike Milestone 4's Jolt-backed player.
    void Reset();

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    // A simple box standing in for the player's actual capsule collider —
    // see docs/ARCHITECTURE.md, "Player visual representation" (unchanged
    // reasoning from Milestone 4).
    glm::vec3 GetRenderCenter() const;
    glm::quat GetRenderOrientation() const;
    glm::vec3 GetRenderHalfExtents() const;

private:
    glm::vec3 ComputeLocalUp(const glm::vec3& gravityAcceleration) const;
    void UpdateFrameOrientation(const glm::vec3& localUp);
    glm::vec3 ComputeTangentVelocity(const Window& window, const glm::vec3& localUp) const;

    // Judas-owned player state. None of this is a Jolt body.
    glm::vec3 m_position;          // capsule center, world space
    glm::vec3 m_velocity{0.0f};
    glm::quat m_frameOrientation;  // local frame: (m_frameOrientation * +Y) is the current local up

    float m_yaw;
    float m_pitch = 0.0f;
    bool m_jumpRequested = false;

    glm::vec3 m_spawnPosition;
    float m_spawnYawDegrees;
};
