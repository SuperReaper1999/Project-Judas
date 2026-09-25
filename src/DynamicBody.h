#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"

class GravityField;

// A single Judas-owned handle onto one physically simulated test object —
// the minimum shared representation Milestone 7-A's dynamic cubes and
// spheres both need, and nothing more. This is deliberately NOT an
// entity/component system (see docs/ARCHITECTURE.md, "Dynamic bodies"): it
// exists only so several test bodies can be driven by one small loop
// instead of copy-pasted per-object code.
//
// Unlike PlayerController, this class does not own movement resolution
// itself — the physics engine (Judas's own, see docs/ARCHITECTURE.md,
// "Physics ownership") fully owns this body's rigid-body dynamics and
// collision (see PhysicsWorld::CreateDynamicBox/CreateDynamicSphere). All this class
// adds on top is exactly the same presentation boundary Milestone 6 gave
// the player: a previous/current pose pair so rendering can interpolate
// between fixed steps without touching authoritative state. See
// docs/ARCHITECTURE.md, "Simulation/presentation boundary."
class DynamicBody {
public:
    enum class Shape { Box, Sphere };

    // Rendering-only description — PhysicsWorld already knows the real
    // collision shape; this is what Renderer needs to draw a stand-in for
    // it. `halfExtents` is meaningful only for Shape::Box, `radius` only
    // for Shape::Sphere.
    struct Visual {
        Shape shape = Shape::Box;
        glm::vec3 halfExtents{0.5f};
        float radius = 0.5f;
        glm::vec3 color{1.0f};
    };

    DynamicBody(BodyHandle handle, const Visual& visual, const glm::vec3& spawnPosition,
                const glm::quat& spawnRotation);

    BodyHandle Handle() const { return m_handle; }
    const Visual& GetVisual() const { return m_visual; }

    glm::vec3 GetPosition() const { return m_position; }
    glm::quat GetOrientation() const { return m_orientation; }

    // Presentation-only interpolated transform — same contract as
    // PlayerController::GetPresentedPosition/Orientation (`alpha` in
    // [0,1], 0 = end of the previous fixed step, 1 = end of the most
    // recent one). Never authoritative; nothing but rendering reads these.
    glm::vec3 GetPresentedPosition(float alpha) const;
    glm::quat GetPresentedOrientation(float alpha) const;

    // Call once per fixed step, BEFORE PhysicsWorld::Step: records the
    // pose as of the end of the previous step as this step's interpolation
    // baseline. Unlike PlayerController::FixedUpdate (which owns its own
    // integration and can snapshot internally), a dynamic body's actual
    // motion happens inside PhysicsWorld::Step(), so snapshotting has to
    // happen from the outside, before that call — see
    // PrepareDynamicBodiesForStep below.
    void SnapshotPrevious();

    // Call once per fixed step, AFTER PhysicsWorld::Step: reads back the
    // fresh authoritative transform the physics engine just produced.
    void SyncFromPhysics(const PhysicsWorld& physics);

    // Restores the body to its spawn pose with zero velocity and
    // synchronizes presentation history to match, so the very next render
    // presents the reset pose directly rather than interpolating across
    // the discontinuity — same reasoning as PlayerController::Reset.
    void ResetToSpawn(PhysicsWorld& physics);

    // Milestone 29: a presentation slot outlives its physics body. Rebind
    // attaches a new (or no) body handle; SetPoseFromState lets a Coarse or
    // Dormant entity present its retained state; IsLive says whether a
    // PhysicsWorld body currently backs this slot.
    void Rebind(BodyHandle handle) { m_handle = handle; }
    void SetPoseFromState(const glm::vec3& position, const glm::quat& orientation);
    // Snaps presentation history to the current pose so the next frame
    // shows a reconstructed pose directly rather than interpolating to it.
    void SnapPresentation();
    bool IsLive() const { return m_handle.IsValid(); }

private:
    BodyHandle m_handle;
    Visual m_visual;
    glm::vec3 m_spawnPosition;
    glm::quat m_spawnRotation;

    glm::vec3 m_position;
    glm::quat m_orientation;
    glm::vec3 m_previousPosition;
    glm::quat m_previousOrientation;
};

// Called once per fixed step, before PhysicsWorld::Step: snapshots each
// body's presentation history, then samples the active GravityField at
// that body's own current position and hands the result to
// PhysicsWorld::ApplyLinearAcceleration — the exact same "Judas samples,
// physics obeys" pattern PlayerController uses, just applied to a list
// instead of one player. A body never learns which GravityField
// implementation is active or anything about world geometry; it only ever
// sees the acceleration value itself. See docs/ARCHITECTURE.md, "Multiple
// gravity consumers." An optional body can be excluded when the composition
// root assigns it a different physical gravity source in the same space;
// its presentation history is still captured normally.
void PrepareDynamicBodiesForStep(std::vector<DynamicBody>& bodies, const GravityField& gravity,
                                  PhysicsWorld& physics, float fixedDeltaTime,
                                  BodyHandle excludedFromLocalGravity = BodyHandle{});

// Called once per fixed step, after PhysicsWorld::Step: reads back each
// body's fresh authoritative transform.
void SyncDynamicBodiesFromPhysics(std::vector<DynamicBody>& bodies, const PhysicsWorld& physics);
