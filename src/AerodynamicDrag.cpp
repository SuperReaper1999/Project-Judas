#include "AerodynamicDrag.h"

#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "AtmosphereField.h"

namespace {
bool IsFinite(const glm::vec3& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}
}  // namespace

float BoxProjectedArea(const BodyBox& box, const glm::vec3& worldFlowDirection) {
    const float directionLength = glm::length(worldFlowDirection);
    if (!(directionLength > 1.0e-6f) || !std::isfinite(directionLength)) return 0.0f;

    const glm::vec3 direction =
        glm::inverse(glm::normalize(box.rotation)) * (worldFlowDirection / directionLength);
    const glm::vec3 size = 2.0f * box.halfExtents;
    return std::abs(direction.x) * size.y * size.z +
           std::abs(direction.y) * size.x * size.z +
           std::abs(direction.z) * size.x * size.y;
}

AerodynamicDragResult ApplyAerodynamicDrag(PhysicsWorld& physics, BodyHandle body,
                                           const AtmosphereField& atmosphere,
                                           const ReferenceFrame& planetFrame,
                                           float dragCoefficient) {
    AerodynamicDragResult result;
    if (!physics.IsDynamicBody(body) ||
        !(dragCoefficient > 0.0f) || !std::isfinite(dragCoefficient)) return result;

    const std::vector<BodyBox> boxes = physics.GetBodyBoxes(body);
    if (boxes.size() != 1) return result;

    const BodyTransform transform = physics.GetTransform(body);
    const BodyBox& box = boxes.front();
    // The single-box craft is centred on the body. An offset box would
    // require a centre-of-pressure torque and is outside this model.
    if (glm::length(box.center - transform.position) > 1.0e-4f) return result;

    const AtmosphereSample gas = atmosphere.Sample(box.center, planetFrame);
    result.density = gas.density;
    result.pressure = gas.pressure;
    // Vacuum contains no gas parcel with which an airspeed can be defined.
    if (!(result.density > 0.0f) || !std::isfinite(result.density)) return result;

    const glm::vec3 bodyPointVelocity = physics.GetLinearVelocity(body) +
        glm::cross(physics.GetAngularVelocity(body), box.center - transform.position);
    result.relativeAirVelocity = bodyPointVelocity - gas.velocity;
    result.relativeAirspeed = glm::length(result.relativeAirVelocity);
    if (!IsFinite(result.relativeAirVelocity) ||
        !(result.relativeAirspeed > 1.0e-6f) ||
        !std::isfinite(result.relativeAirspeed)) return result;

    result.projectedArea = BoxProjectedArea(box, result.relativeAirVelocity);
    if (!(result.projectedArea > 0.0f) || !std::isfinite(result.projectedArea)) {
        result.projectedArea = 0.0f;
        return result;
    }
    result.dynamicPressure = 0.5f * result.density *
                             result.relativeAirspeed * result.relativeAirspeed;
    if (!std::isfinite(result.dynamicPressure)) {
        result.dynamicPressure = 0.0f;
        return result;
    }
    result.force = -result.dynamicPressure * dragCoefficient * result.projectedArea *
                   (result.relativeAirVelocity / result.relativeAirspeed);
    if (!IsFinite(result.force)) {
        result.force = glm::vec3(0.0f);
        return result;
    }
    physics.ApplyForce(body, result.force);
    return result;
}
