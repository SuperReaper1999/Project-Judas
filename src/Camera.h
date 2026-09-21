#pragma once

#include <glm/glm.hpp>

class Window;

// A free-flying camera for exploring the Milestone 2 test scene.
//
// On "up": this camera keeps a fixed reference axis (world +Y) purely to
// build a stable local right/up/forward frame for mouse look and vertical
// movement in a scene that currently has no gravity, planets, or other
// reference frames. This is a convention scoped to this one class for this
// one demo scene, NOT an engine-wide definition of "up". When arbitrary
// gravity and planetary/spacecraft reference frames exist, camera
// orientation will need to derive from whatever frame the camera is
// currently in rather than a hardcoded world axis.
class Camera {
public:
    Camera(glm::vec3 position, float yawDegrees, float pitchDegrees);

    void Update(const Window& window, float deltaTime);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 Front() const;

    glm::vec3 m_position;
    float m_yaw;    // degrees, rotation around the reference up-axis
    float m_pitch;  // degrees, clamped to avoid gimbal flip at the poles
};
