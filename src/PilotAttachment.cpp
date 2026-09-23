#include "PilotAttachment.h"

void BeginPilotAttachment(PilotAttachment& attachment, const BodyTransform& shipTransform,
                           const glm::vec3& playerPosition, const glm::quat& playerOrientation) {
    const glm::quat inverseShipRotation = glm::inverse(shipTransform.rotation);
    attachment.localOffset = inverseShipRotation * (playerPosition - shipTransform.position);
    attachment.localOrientation = glm::normalize(inverseShipRotation * playerOrientation);
    attachment.attached = true;
}

void ApplyPilotAttachment(const PilotAttachment& attachment, const BodyTransform& shipTransform,
                           glm::vec3& outPosition, glm::quat& outOrientation) {
    outPosition = shipTransform.position + shipTransform.rotation * attachment.localOffset;
    outOrientation = glm::normalize(shipTransform.rotation * attachment.localOrientation);
}

glm::vec3 ComputePilotReleaseVelocity(const BodyTransform& shipTransform,
                                       const glm::vec3& shipLinearVelocity,
                                       const glm::vec3& shipAngularVelocity,
                                       const glm::vec3& playerPosition) {
    const glm::vec3 r = playerPosition - shipTransform.position;
    return shipLinearVelocity + glm::cross(shipAngularVelocity, r);
}

glm::vec3 ComputePilotDismountPosition(const BodyTransform& shipTransform,
                                       const glm::vec3& playerPosition,
                                       const glm::vec3& gravityAcceleration,
                                       float shipSupportDistance,
                                       float playerMaxSupportDistance,
                                       float skinMargin) {
    const float gravityMagnitude = glm::length(gravityAcceleration);
    if (gravityMagnitude < 1.0e-5f) return playerPosition;

    const glm::vec3 gravityUp = -gravityAcceleration / gravityMagnitude;
    const float playerHeightAboveShipCenter =
        glm::dot(playerPosition - shipTransform.position, gravityUp);
    const float requiredHeight = shipSupportDistance + playerMaxSupportDistance + skinMargin;
    if (playerHeightAboveShipCenter >= requiredHeight) return playerPosition;

    return playerPosition + gravityUp * (requiredHeight - playerHeightAboveShipCenter);
}
