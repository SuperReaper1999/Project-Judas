#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Interactable.h"
#include "PhysicsWorld.h"

class Renderer;

// Milestone 16: a hinged, physically-collidable, interactable door — the
// first concrete `Interactable` (see src/Interactable.h). Real collision
// in both states (a single STATIC `PhysicsWorld` body whose pose this
// class drives directly every fixed step via `PhysicsWorld::ResetBody` —
// see docs/ARCHITECTURE.md, "Milestone 16, Door," for why a static body
// driven kinematically was judged sufficient and no joint/constraint
// system was needed: `ResetBody` already lets any body's transform be set
// directly, and every existing collision query in this engine reads a
// body's position/orientation live, with no cached/baked broadphase state
// to go stale when it moves). Swings about its own authored hinge edge
// (see src/HingeTransform.h) rather than assuming any fixed world axis.
class Door : public Interactable {
public:
    // `hingeWorldPosition`/`baseOrientation`: the door's CLOSED pose,
    // authored exactly like every other piece of static demo geometry
    // (see Application.cpp's `RotationAligningUpTo` convention) — the
    // hinge EDGE's own world position (not the box's center) and the
    // orientation whose local axes define the door's own "up"/"forward"/
    // "hinge-side" directions. `halfExtents` is the closed box's own
    // half-extents; the box's own local -X face is always the hinge edge
    // (an authoring convention, not a physical requirement — consistent
    // across every door this engine has, since there's only ever one).
    // `localHingeAxis` is expressed in the door's OWN local space (not
    // world space — see src/HingeTransform.h) — typically (0,1,0), the
    // door's own local "up," which is only world +Y when the door happens
    // to be placed at a bearing where `baseOrientation` makes that true;
    // it is never assumed to be world +Y in general.
    Door(PhysicsWorld& physics, const glm::vec3& hingeWorldPosition, const glm::quat& baseOrientation,
         const glm::vec3& halfExtents, const glm::vec3& localHingeAxis, float openAngleRadians,
         float angularSpeedRadiansPerSecond, const glm::vec3& color);

    void Destroy(PhysicsWorld& physics);

    // Advances the door's own open/close animation by one fixed step and
    // writes the resulting pose directly to its physics body — call once
    // per fixed step, BEFORE the player's own FixedUpdate this same step,
    // so the player's move-and-slide sees the door's up-to-date collision
    // pose (see Application::Run's own fixed-step ordering).
    void FixedUpdate(PhysicsWorld& physics, float fixedDeltaTime);

    // Presentation-only interpolated draw — same `alpha` contract as
    // DynamicBody::GetPresentedPosition/Orientation (see
    // docs/ARCHITECTURE.md, "Simulation/presentation boundary"): the door
    // interpolates its own swing ANGLE (a single scalar, `glm::mix`, no
    // slerp needed) between the previous and current fixed step, then
    // derives a full position/orientation from that interpolated angle —
    // never authoritative, called only from Application.cpp's shared
    // `drawScene` lambda (so the door renders, collides, and casts
    // shadows coherently — every shadow pass uses this same call).
    void Draw(Renderer& renderer, float presentationAlpha) const;

    // Interactable
    glm::vec3 GetInteractionPoint() const override;
    float GetInteractionRadius() const override;
    std::string GetPromptText() const override;
    bool CanInteract() const override { return true; }
    void Interact() override { m_open = !m_open; }

    // Test-only accessors (see tests/InteractableTests.cpp) — read the
    // authoritative animation state without needing a real PhysicsWorld.
    float GetCurrentAngleRadians() const { return m_currentAngle; }
    bool IsOpen() const { return m_open; }
    // Milestone 29: restores a persisted open/closed state instantly (the
    // panel is already at that angle; nothing swings on load).
    void SetOpen(bool open) {
        m_open = open;
        m_currentAngle = m_previousAngle = open ? m_openAngleRadians : 0.0f;
    }

private:
    BodyHandle m_bodyHandle;

    glm::vec3 m_baseCenter{0.0f};
    glm::quat m_baseOrientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_halfExtents{1.0f};
    glm::vec3 m_pivotWorld{0.0f};
    glm::vec3 m_hingeAxisWorld{0.0f, 1.0f, 0.0f};
    glm::vec3 m_color{1.0f};

    float m_openAngleRadians = 0.0f;
    float m_angularSpeedRadiansPerSecond = 0.0f;

    bool m_open = false;
    float m_currentAngle = 0.0f;   // authoritative, updated only by FixedUpdate
    float m_previousAngle = 0.0f;  // presentation-only interpolation baseline
};
