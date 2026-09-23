#include "ReferenceFrame.h"

#include <glm/gtc/quaternion.hpp>

namespace {
glm::quat UnitOrientation(const ReferenceFrame& frame) {
    return glm::normalize(frame.orientation);
}
}  // namespace

glm::vec3 PositionToWorld(const ReferenceFrame& frame, const glm::vec3& positionInFrame) {
    return frame.originPosition + UnitOrientation(frame) * positionInFrame;
}

glm::vec3 PositionFromWorld(const ReferenceFrame& frame, const glm::vec3& positionInWorld) {
    return glm::inverse(UnitOrientation(frame)) * (positionInWorld - frame.originPosition);
}

glm::vec3 DirectionToWorld(const ReferenceFrame& frame, const glm::vec3& directionInFrame) {
    return UnitOrientation(frame) * directionInFrame;
}

glm::vec3 DirectionFromWorld(const ReferenceFrame& frame, const glm::vec3& directionInWorld) {
    return glm::inverse(UnitOrientation(frame)) * directionInWorld;
}

glm::vec3 FramePointVelocity(const ReferenceFrame& frame, const glm::vec3& positionInWorld) {
    return frame.linearVelocity +
           glm::cross(frame.angularVelocity, positionInWorld - frame.originPosition);
}

glm::vec3 VelocityToWorld(const ReferenceFrame& frame, const glm::vec3& positionInFrame,
                          const glm::vec3& velocityInFrame) {
    const glm::vec3 positionInWorld = PositionToWorld(frame, positionInFrame);
    return FramePointVelocity(frame, positionInWorld) + DirectionToWorld(frame, velocityInFrame);
}

glm::vec3 VelocityFromWorld(const ReferenceFrame& frame, const glm::vec3& positionInWorld,
                            const glm::vec3& velocityInWorld) {
    const glm::vec3 pointVelocity = FramePointVelocity(frame, positionInWorld);
    return DirectionFromWorld(frame, velocityInWorld - pointVelocity);
}

glm::vec3 RelativeVelocityToFrame(const ReferenceFrame& frame, const glm::vec3& positionInWorld,
                                  const glm::vec3& velocityInWorld) {
    return VelocityFromWorld(frame, positionInWorld, velocityInWorld);
}
