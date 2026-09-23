#include "ShadowTransforms.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float kDirectionalNearPlane = 0.1f;
constexpr float kSpotNearPlane = 0.1f;
// A fixed margin (degrees) added to the spotlight's own outer cone before
// doubling it into a vertical field of view — see ShadowTransforms.h's own
// comment on ComputeSpotShadowMatrix for why the shadow frustum must not be
// tighter than the light's illuminated cone.
constexpr float kSpotFovMarginDegrees = 4.0f;
}  // namespace

glm::vec3 ChooseShadowUpHint(const glm::vec3& forward) {
    const glm::vec3 f = glm::length(forward) > 1.0e-6f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 worldY(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(f, worldY)) > 0.999f) {
        return glm::vec3(1.0f, 0.0f, 0.0f);
    }
    return worldY;
}

glm::mat4 ComputeDirectionalShadowMatrix(const glm::vec3& focusPosition,
                                          const glm::vec3& lightDirectionToLight, float halfExtent,
                                          float shadowDistance) {
    const glm::vec3 dirToLight = glm::length(lightDirectionToLight) > 1.0e-6f
                                      ? glm::normalize(lightDirectionToLight)
                                      : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 eye = focusPosition + dirToLight * shadowDistance;
    const glm::vec3 up = ChooseShadowUpHint(dirToLight);
    const glm::mat4 view = glm::lookAt(eye, focusPosition, up);
    const glm::mat4 projection =
        glm::ortho(-halfExtent, halfExtent, -halfExtent, halfExtent, kDirectionalNearPlane,
                   shadowDistance * 2.0f);
    return projection * view;
}

glm::mat4 ComputeSpotShadowMatrix(const glm::vec3& lightPosition, const glm::vec3& lightDirection,
                                   float outerConeDegrees, float range) {
    const glm::vec3 dir = glm::length(lightDirection) > 1.0e-6f ? glm::normalize(lightDirection)
                                                                 : glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 up = ChooseShadowUpHint(dir);
    const glm::mat4 view = glm::lookAt(lightPosition, lightPosition + dir, up);
    const float fovyDegrees =
        glm::clamp(outerConeDegrees * 2.0f + kSpotFovMarginDegrees, 10.0f, 170.0f);
    const glm::mat4 projection =
        glm::perspective(glm::radians(fovyDegrees), 1.0f, kSpotNearPlane, std::max(range, 1.0f));
    return projection * view;
}
