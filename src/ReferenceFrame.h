#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// A classical moving coordinate frame. Pose and both velocities describe the
// frame in world coordinates; angularVelocity is a world-space vector. A
// frame owns no bodies and does not affect physics or forces.
struct ReferenceFrame {
    glm::vec3 originPosition{0.0f};                 // world-space metres
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // frame coordinates -> world
    glm::vec3 linearVelocity{0.0f};                 // origin velocity, m/s
    glm::vec3 angularVelocity{0.0f};                // world-space, radians/s
};

// Position and direction coordinates change with the frame's pose. Directions
// are rotated only; positions are also translated by the frame origin.
glm::vec3 PositionToWorld(const ReferenceFrame& frame, const glm::vec3& positionInFrame);
glm::vec3 PositionFromWorld(const ReferenceFrame& frame, const glm::vec3& positionInWorld);
glm::vec3 DirectionToWorld(const ReferenceFrame& frame, const glm::vec3& directionInFrame);
glm::vec3 DirectionFromWorld(const ReferenceFrame& frame, const glm::vec3& directionInWorld);

// Frame-relative velocity is the derivative of position coordinates in the
// translating, rotating frame. Converting it requires the frame point's full
// velocity at the object's position, including omega x r.
glm::vec3 FramePointVelocity(const ReferenceFrame& frame, const glm::vec3& positionInWorld);
glm::vec3 VelocityToWorld(const ReferenceFrame& frame, const glm::vec3& positionInFrame,
                          const glm::vec3& velocityInFrame);
glm::vec3 VelocityFromWorld(const ReferenceFrame& frame, const glm::vec3& positionInWorld,
                            const glm::vec3& velocityInWorld);
// Same relative-velocity calculation with an explicit semantic name.
glm::vec3 RelativeVelocityToFrame(const ReferenceFrame& frame, const glm::vec3& positionInWorld,
                                  const glm::vec3& velocityInWorld);
