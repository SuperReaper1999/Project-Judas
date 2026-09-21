#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Window;
class PhysicsWorld;
class GravityField;

// Judas-owned boundary between four distinct concerns:
//   - input intent        (read from Window: WASD state, mouse delta, jump key)
//   - locomotion decisions (this class: what velocity should the player have?)
//   - gravity              (never known here as a constant — always sampled
//                            from whichever GravityField is passed in)
//   - physical representation (a Jolt CharacterVirtual, touched only through
//                            PhysicsWorld — no Jolt type appears here)
//
// This class also computes the player's camera transform for rendering — a
// fixed third-person offset behind the player along the same look direction
// used for movement. Looking around only ever changes that offset; it never
// writes back to the physical player's position.
class PlayerController {
public:
    PlayerController(const glm::vec3& spawnFeetPosition, float spawnYawDegrees);

    // Creates the underlying physics representation. `gravity` is sampled
    // once, at the spawn position, purely to seed the character
    // controller's own internal "up" reference (used only for its slope
    // classification, not for gravity) — see PhysicsWorld::CreatePlayer.
    bool Spawn(PhysicsWorld& physics, const GravityField& gravity);
    void Destroy(PhysicsWorld& physics);

    // Reads mouse-look delta and latches jump key presses. Call exactly
    // once per render frame, regardless of how many (or how few) fixed
    // physics steps happen that frame.
    void UpdateFrameInput(Window& window);

    // Advances the player by exactly one fixed physics step: computes
    // grounded/airborne horizontal + vertical velocity (integrating
    // gravity sampled fresh from `gravity`, and consuming any latched jump
    // request) and hands it to `physics`.
    void FixedUpdate(const Window& window, PhysicsWorld& physics, const GravityField& gravity,
                      float fixedDeltaTime);

    void Reset(PhysicsWorld& physics);

    glm::mat4 GetViewMatrix(const PhysicsWorld& physics) const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    // A simple box standing in for the player's actual capsule collider,
    // for the sole purpose of being able to see it — not a faithful
    // capsule mesh. See docs/ARCHITECTURE.md, "Player visual representation."
    glm::vec3 GetRenderCenter(const PhysicsWorld& physics) const;
    glm::vec3 GetRenderHalfExtents() const;

private:
    glm::vec3 Front() const;
    glm::vec3 ComputeHorizontalVelocity(const Window& window) const;

    glm::vec3 m_spawnFeetPosition;
    float m_spawnYawDegrees;

    float m_yaw;
    float m_pitch;
    bool m_jumpRequested = false;
};
