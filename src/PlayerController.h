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
//
// Milestone 6 adds a presentation boundary: `m_position`/`m_frameOrientation`
// remain the sole authoritative state, updated only by FixedUpdate exactly
// as before, but every render needs to display something *between* two
// fixed-step states rather than the same one repeated or jumping wholesale
// — see docs/ARCHITECTURE.md, "Diagnosis" and "Simulation/presentation
// boundary," for why and GetPresentedPosition/GetPresentedOrientation
// below for what's interpolated (rendering only — never gameplay).
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
    // Also records the pre-step position/orientation as the interpolation
    // baseline for presentation — see GetPresentedPosition/Orientation.
    void FixedUpdate(const Window& window, PhysicsWorld& physics, const GravityField& gravity,
                      float fixedDeltaTime);

    // All player state lives in this class (position, velocity,
    // orientation, pending jump) — there is no physics-side state to reset
    // separately, unlike Milestone 4's Jolt-backed player. Also
    // synchronizes presentation history to the reset pose (see
    // GetPresentedPosition/Orientation) so the very next render presents
    // the reset position directly, not an interpolation across the world
    // from wherever the player was.
    void Reset();

    // `presentationAlpha` blends the camera's position/orientation inputs
    // exactly as GetPresentedPosition/Orientation do (and must be the same
    // value passed to those for the player's own rendered box, so camera
    // and player move in visual lockstep) — see docs/ARCHITECTURE.md,
    // "Camera."  Mouse look (yaw/pitch) is unaffected by this parameter:
    // it already updates every render frame in UpdateFrameInput, so it's
    // already as responsive as rendering itself.
    glm::mat4 GetViewMatrix(float presentationAlpha) const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    // Presentation-only interpolated transform — see docs/ARCHITECTURE.md,
    // "Simulation/presentation boundary." `alpha` in [0,1]: 0 is the state
    // as of the end of the PREVIOUS fixed step, 1 is the state as of the
    // end of the MOST RECENT one (typically
    // physicsAccumulator / fixedTimestep — how far into an as-yet-
    // unsimulated step real time has progressed). Never authoritative:
    // nothing in FixedUpdate, ComputeLocalUp, collision queries, or any
    // gameplay decision ever reads these — only rendering does.
    glm::vec3 GetPresentedPosition(float alpha) const;
    glm::quat GetPresentedOrientation(float alpha) const;

    // A simple box standing in for the player's actual capsule collider —
    // see docs/ARCHITECTURE.md, "Player visual representation" (unchanged
    // reasoning from Milestone 4).
    glm::vec3 GetRenderHalfExtents() const;

    // Authoritative accessors — the real simulation state, with no
    // presentation interpolation applied. Used by the test harness to log
    // ground truth, and internally as the interpolation endpoints above.
    glm::vec3 GetPosition() const { return m_position; }
    glm::quat GetOrientation() const { return m_frameOrientation; }
    glm::vec3 GetVelocity() const { return m_velocity; }
    bool IsGrounded() const { return m_lastGrounded; }

    // Mouse-look state and the resulting authoritative view/look direction
    // (the same yaw/pitch-on-top-of-frame-orientation composition
    // GetViewMatrix uses, but built from m_frameOrientation directly rather
    // than a presented/interpolated one) — for diagnostics (live telemetry,
    // the test harness) that need to know where the player is actually
    // looking, not just which way its body is oriented.
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    glm::vec3 GetLookDirection() const;

private:
    glm::vec3 ComputeLocalUp(const glm::vec3& gravityAcceleration) const;
    void UpdateFrameOrientation(const glm::vec3& localUp, float fixedDeltaTime);
    glm::vec3 ComputeTangentVelocity(const Window& window, const glm::vec3& localUp) const;

    // Judas-owned player state. None of this is a Jolt body.
    glm::vec3 m_position;          // capsule center, world space
    glm::vec3 m_velocity{0.0f};
    glm::quat m_frameOrientation;  // local frame: (m_frameOrientation * +Y) is the current local up
    bool m_lastGrounded = false;   // support state as of the most recent FixedUpdate

    // Presentation-only: position/orientation as of the end of the
    // PREVIOUS fixed step, i.e. the interpolation start point for whatever
    // FixedUpdate most recently produced. Written only by FixedUpdate
    // (snapshotted before that step's authoritative state changes) and by
    // Reset (synchronized to the reset pose) — never read by gameplay.
    glm::vec3 m_previousPosition;
    glm::quat m_previousOrientation;

    float m_yaw;
    float m_pitch = 0.0f;
    bool m_jumpRequested = false;

    glm::vec3 m_spawnPosition;
    float m_spawnYawDegrees;
};
