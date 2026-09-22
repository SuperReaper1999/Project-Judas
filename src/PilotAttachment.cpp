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
