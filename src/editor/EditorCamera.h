#pragma once

#include <glm/glm.hpp>

class Window;

enum class EditorCameraGesture { None, Look, Pan };

// Viewport press ownership is retained until release. A press begun over a
// panel cannot become a camera drag by moving out of that panel while held.
struct EditorCameraGestureInput {
    bool focused = true;
    bool canStart = true;
    bool cancel = false;
    bool rightDown = false;
    bool middleDown = false;
    bool leftDown = false;
};

class EditorCameraGestureState {
public:
    void Update(const EditorCameraGestureInput& input);
    void Cancel();
    EditorCameraGesture Active() const { return m_active; }

private:
    EditorCameraGesture m_active = EditorCameraGesture::None;
    bool m_rightWasDown = false;
    bool m_middleWasDown = false;
    bool m_waitForRelease = false;
};

// Wheel motion is instantaneous rather than a captured gesture. Reuse the
// viewport/UI eligibility from the drag controller, and leave wheel input
// with a panel, text field, active drag or the running game.
bool EditorCameraCanDolly(const EditorCameraGestureInput& input, EditorCameraGesture active, bool editing);

// Milestone 28: the editor's free-flying viewport camera. Independent of
// any player: it has no gravity, no collision and no "up" beyond its own
// authored orientation, so it works the same over a flat floor or around
// a planet. Right mouse button held: mouse look + WASD/QE fly (Shift for
// speed). Middle mouse drag pans in the camera's view plane; wheel motion
// dollies toward/away from the focus. Left click is left for selection
// (see EditorApplication).
class EditorCamera {
public:
    void Update(const Window& window, float deltaSeconds, bool lookActive, int mouseDeltaX, int mouseDeltaY,
                bool fast);

    glm::mat4 ViewMatrix() const;
    glm::mat4 ProjectionMatrix(float aspectRatio) const;
    glm::vec3 Position() const { return m_position; }
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;
    glm::vec3 Focus() const { return m_position + Forward() * m_focusDistance; }
    float FocusDistance() const { return m_focusDistance; }

    // Drag the viewed scene with the pointer. The screen-space scale is
    // derived from this perspective camera at its most recent focus depth;
    // camera and focus translate equally, without changing orientation.
    void Pan(int mouseDeltaX, int mouseDeltaY, int viewportHeight);

    // Positive wheel steps approach the current focus; negative steps retreat.
    // Fractional steps scale the remaining distance exponentially (0.8 per
    // step), keeping the focus fixed and preserving orientation and lens.
    // Normal distance is bounded to 0.1..5000; a farther existing frame may
    // approach smoothly but cannot retreat farther. Zero/nonfinite input is
    // ignored, and finite extremes saturate without crossing the focus.
    void Dolly(float wheelSteps);

    // Moves the camera to look at `point` from `distance` away along its
    // current forward direction.
    void LookAt(const glm::vec3& point, float distance);
    void SetPose(const glm::vec3& position, float yawDegrees, float pitchDegrees);

    // A world-space ray through a viewport pixel (top-left origin).
    void PixelRay(int pixelX, int pixelY, int viewportWidth, int viewportHeight, glm::vec3& outOrigin,
                  glm::vec3& outDirection) const;

    float MoveSpeed() const { return m_moveSpeed; }
    void SetMoveSpeed(float metresPerSecond) { m_moveSpeed = metresPerSecond; }

private:
    glm::vec3 m_position{0.0f, 30.0f, -20.0f};
    float m_yawDegrees = 0.0f;
    float m_pitchDegrees = -30.0f;
    float m_moveSpeed = 12.0f;
    float m_focusDistance = 8.0f;
};
