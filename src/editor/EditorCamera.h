#pragma once

#include <glm/glm.hpp>

class Window;

// Milestone 28: the editor's free-flying viewport camera. Independent of
// any player: it has no gravity, no collision and no "up" beyond its own
// authored orientation, so it works the same over a flat floor or around
// a planet. Right mouse button held: mouse look + WASD/QE fly (Shift for
// speed). Left click is left for selection (see EditorApplication).
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
};
