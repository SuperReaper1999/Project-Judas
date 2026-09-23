#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"

class Window;
class GravityField;

// Judas-owned player. Unlike Milestone 4 (which delegated locomotion to
// Jolt's CharacterVirtual, back when Jolt was still this project's physics
// middleware — see docs/ARCHITECTURE.md, "Physics ownership"), this class
// itself decides desired locomotion, gravity response, local up/down, jump
// behavior, support interpretation, and orientation — every piece of state
// here (position, velocity, orientation) is plain data owned by Judas, not
// a physics-engine body or character controller of any kind. The physics
// engine is consulted only for the raw geometric question "how far can
// this shape move, and what does it touch?" via
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
    // displacement against collision via a minimal move-and-slide loop —
    // as of Milestone 10, with a small acceleration/deceleration model on
    // the ground, modest momentum-preserving air control while airborne,
    // and an automatic step-up/step-down pass (src/StepClimb.h) tried
    // before ordinary sliding. Also records the pre-step position/
    // orientation as the interpolation baseline for presentation — see
    // GetPresentedPosition/Orientation.
    // `inputEnabled` (Milestone 8, default true): when false, WASD/jump are
    // ignored — the player still runs gravity, support detection,
    // moving-support velocity carry (see below), and collision-aware
    // movement exactly as normal, it simply isn't given fresh locomotion
    // intent that step. Used while the player is controlling the flying
    // primitive instead of themselves (see src/FlyingPrimitiveControl.h) —
    // the player remains a real, physically simulated participant the
    // entire time, per docs/ARCHITECTURE.md's Milestone 8 section, rather
    // than being frozen or detached.
    void FixedUpdate(const Window& window, PhysicsWorld& physics, const GravityField& gravity,
                      float fixedDeltaTime, bool inputEnabled = true);

    // Milestone 11: advances the player by exactly one fixed step while
    // SECURED to a spacecraft (see src/PilotAttachment.h) — called INSTEAD
    // OF FixedUpdate for a step where the player is attached (see
    // src/PilotControl.h). Snapshots presentation history exactly like
    // FixedUpdate, then overwrites position/orientation directly from the
    // attachment's own computation rather than running gravity/support/
    // locomotion at all — ordinary grounding and move-and-slide must not
    // fight the secured pose (see docs/ARCHITECTURE.md, "Milestone 11").
    // Clears support/velocity/jump bookkeeping to a neutral state (never
    // grounded, zero ground-carry velocity, no buffered jump) so ordinary
    // FixedUpdate resumes cleanly the moment attachment ends — see
    // GetVelocity's own note below for why authoritative velocity itself
    // reads zero throughout the attached period.
    void FixedUpdateAttached(const glm::vec3& newPosition, const glm::quat& newOrientation);

    // Milestone 11: injects the player's inherited world-space velocity at
    // the exact instant piloting control is released (see
    // src/PilotAttachment.h's ComputePilotReleaseVelocity and
    // src/PilotControl.h's HandlePilotToggleRequest) — the ONLY place
    // outside FixedUpdate/FixedUpdateAttached that ever writes m_velocity
    // directly. The very next ordinary FixedUpdate call integrates gravity
    // and collision on top of this starting velocity exactly as it would
    // for any other airborne player.
    void SetVelocityAfterRelease(const glm::vec3& velocity) { m_velocity = velocity; }

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

    // Milestone 8: builds the identical camera (same fixed offset/eye
    // height/look composition, same player-controlled m_yaw/m_pitch free
    // look) anchored to an EXTERNAL position/orientation instead of the
    // player's own presented pose. Used only while controlling the flying
    // primitive (see src/FlyingPrimitiveControl.h, Application::Run) so
    // flying it has a working viewpoint without a second camera system —
    // "keep the existing camera architecture intact as practical." Mouse
    // look still comes from this class; only the anchor changes.
    glm::mat4 GetViewMatrix(const glm::vec3& anchorPosition, const glm::quat& anchorOrientation) const;

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
    // Milestone 11: while attached to a spacecraft (see FixedUpdateAttached),
    // this reads exactly zero — there is no independent "player velocity"
    // concept while secured; the spacecraft's own velocity is queryable
    // separately via PhysicsWorld::GetLinearVelocity(shipHandle). A fresh,
    // meaningful value is written the instant control is released (see
    // SetVelocityAfterRelease).
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

    // Milestone 14: the player torch's world-space origin/direction for a
    // given presentation alpha — same eye position and yaw/pitch-composed
    // look direction BuildViewMatrix uses for the camera's own `front`
    // vector (see PlayerController.cpp), but carried at the player's own
    // eye position rather than the third-person camera position behind
    // it, so the torch visually originates from the player, not from
    // empty space behind them. Uses PRESENTED position/orientation (see
    // GetPresentedPosition/Orientation above), matching every other
    // Milestone 14 dynamic light's "follow the rendered pose, not a
    // stale/baked one" requirement — see docs/ARCHITECTURE.md, "Milestone
    // 14, Dynamic means dynamic." Never derives from gravity or assumes a
    // world axis: entirely a function of this player's own frame
    // orientation plus free-look yaw/pitch, exactly like GetLookDirection.
    void GetTorchTransform(float presentationAlpha, glm::vec3& outPosition,
                            glm::vec3& outDirection) const;

    // Milestone 8: the physics body the player is currently standing on,
    // meaningful only when IsGrounded() is true (same "meaningful only
    // when hit is true" convention as ShapeSweepHit::hitBody, which this
    // is taken directly from). Lets a caller (Application, gating F's
    // take-control request) check "is the player on THIS specific object"
    // without PlayerController needing to know what a flying primitive is.
    BodyHandle GetSupportBodyHandle() const { return m_lastGroundHitBody; }

private:
    glm::vec3 ComputeLocalUp(const glm::vec3& gravityAcceleration) const;
    void UpdateFrameOrientation(const glm::vec3& localUp, float fixedDeltaTime);
    // Milestone 10: renamed from ComputeTangentVelocity — now returns a
    // normalized (or zero) DIRECTION only, not a speed-scaled velocity, so
    // both the grounded acceleration model and the airborne air-control
    // addition can each apply their own speed/acceleration constant to the
    // same underlying input direction. See FixedUpdate.
    glm::vec3 ComputeInputDirection(const Window& window, const glm::vec3& localUp) const;
    glm::mat4 BuildViewMatrix(const glm::vec3& position, const glm::quat& orientation) const;

    // Judas-owned player state. None of this is a physics-engine body.
    glm::vec3 m_position;          // capsule center, world space
    glm::vec3 m_velocity{0.0f};
    glm::quat m_frameOrientation;  // local frame: (m_frameOrientation * +Y) is the current local up
    bool m_lastGrounded = false;   // support state as of the most recent FixedUpdate
    BodyHandle m_lastGroundHitBody;  // see GetSupportBodyHandle(); meaningful only if m_lastGrounded
    // Milestone 8: the moving-support velocity carried into m_velocity as
    // of the most recent grounded step (zero on static ground or while
    // airborne) — see FixedUpdate's wasAscending computation for why this
    // has to be tracked separately from m_velocity itself.
    glm::vec3 m_lastGroundVelocity{0.0f};

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
