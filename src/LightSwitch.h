#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Interactable.h"

class Renderer;

// Milestone 16: the second, deliberately simple, non-door interactable —
// a wall-mounted lever that toggles a nearby lamp (an ordinary M14/M15
// point light) on and off. Exists specifically to prove the interaction
// abstraction (src/Interactable.h) isn't secretly `DoorManager`: it uses
// the exact same `Interactable` interface and the exact same
// `SelectInteractable`/HUD-prompt/interact-key path the door does, while
// having a completely different action (toggling a light, not
// opening/closing a collidable panel).
//
// Unlike Door, this has NO physics body at all — a small visible lever
// swinging between two rest angles (reusing src/HingeTransform.h, the
// same math the door uses) is purely decorative, the same "models are
// visual, not automatically physical" precedent Milestone 9's beacon
// already established. `Application.cpp`'s per-frame dynamic-light build
// reads `IsLampOn`/`GetLampPosition` to add (or omit) one more
// `DynamicLight` to that frame's list — this class never touches
// `Renderer`/`Light.h` GPU state itself, it only exposes plain data,
// exactly like every other Milestone 14 light source's own ownership
// split.
class LightSwitch : public Interactable {
public:
    // `hingeWorldPosition`/`baseOrientation`/`halfExtents`/`localHingeAxis`:
    // same authoring convention as Door (see src/Door.h) — the lever's
    // closed-pose hinge edge and orientation, never assuming world +Y is
    // the swing axis. `lampWorldPosition`/`lampColor`/`lampRange`: the
    // point light this switch controls (see src/Light.h) — plain data,
    // not owned by this class beyond remembering the values.
    LightSwitch(const glm::vec3& hingeWorldPosition, const glm::quat& baseOrientation,
                const glm::vec3& halfExtents, const glm::vec3& localHingeAxis,
                float toggleAngleRadians, float angularSpeedRadiansPerSecond, const glm::vec3& color,
                const glm::vec3& lampWorldPosition, const glm::vec3& lampColor, float lampRange);

    // No PhysicsWorld involved at all — see this class's own header
    // comment on why it has no collision body.
    void FixedUpdate(float fixedDeltaTime);
    void Draw(Renderer& renderer, float presentationAlpha) const;

    // Interactable
    glm::vec3 GetInteractionPoint() const override;
    float GetInteractionRadius() const override;
    std::string GetPromptText() const override;
    bool CanInteract() const override { return true; }
    void Interact() override { m_lampOn = !m_lampOn; }

    bool IsLampOn() const { return m_lampOn; }
    glm::vec3 GetLampPosition() const { return m_lampPosition; }
    glm::vec3 GetLampColor() const { return m_lampColor; }
    float GetLampRange() const { return m_lampRange; }

    // Test-only accessor (see tests/InteractableTests.cpp).
    float GetCurrentAngleRadians() const { return m_currentAngle; }

private:
    glm::vec3 m_baseCenter{0.0f};
    glm::quat m_baseOrientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_halfExtents{0.1f};
    glm::vec3 m_pivotWorld{0.0f};
    glm::vec3 m_hingeAxisWorld{0.0f, 1.0f, 0.0f};
    glm::vec3 m_color{1.0f};

    float m_toggleAngleRadians = 0.0f;
    float m_angularSpeedRadiansPerSecond = 0.0f;

    bool m_lampOn = false;
    float m_currentAngle = 0.0f;
    float m_previousAngle = 0.0f;

    glm::vec3 m_lampPosition{0.0f};
    glm::vec3 m_lampColor{1.0f};
    float m_lampRange = 8.0f;
};
