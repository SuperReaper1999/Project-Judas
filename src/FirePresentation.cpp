#include "FirePresentation.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

bool IsFinite(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool IsFinite(const glm::quat& q) {
    return std::isfinite(q.w) && std::isfinite(q.x) &&
           std::isfinite(q.y) && std::isfinite(q.z);
}

glm::vec3 BoundedDrift(const glm::vec3& velocity) {
    constexpr float kMaximumVisualDriftMetresPerSecond = 2.5f;
    const float length = glm::length(velocity);
    return length > kMaximumVisualDriftMetresPerSecond
        ? velocity * (kMaximumVisualDriftMetresPerSecond / length)
        : velocity;
}

}  // namespace

std::vector<FireVisualPrimitive> BuildFirePresentation(const FirePresentationInput& input) {
    if (!IsFinite(input.position) || !IsFinite(input.orientation) ||
        !IsFinite(input.bodyVelocity) || !IsFinite(input.gasVelocity) ||
        !IsFinite(input.gravityAcceleration) ||
        !std::isfinite(input.burnRateKgPerSecond) ||
        !std::isfinite(input.temperatureKelvin) || !std::isfinite(input.gasDensity) ||
        !std::isfinite(input.sourceRadius) ||
        input.burnRateKgPerSecond <= 0.0f) {
        return {};
    }

    const float orientationLength = glm::length(input.orientation);
    if (orientationLength <= 1.0e-6f) return {};
    const glm::quat orientation = input.orientation / orientationLength;

    // Render scale is a bounded, monotone response to measured fuel burn and
    // temperature. Neither quantity is changed by this function.
    const float warmth = std::clamp((input.temperatureKelvin - 293.15f) / 800.0f,
                                   0.0f, 1.0f);
    const float rate = input.burnRateKgPerSecond;
    const float burnStrength = std::sqrt(rate / (rate + 0.015f));
    const float sourceRadius = std::clamp(input.sourceRadius, 0.05f, 2.0f);
    const float radius = (0.20f + 0.50f * sourceRadius) * burnStrength *
                         (0.40f + 0.60f * warmth);

    // Hot-gas buoyancy is only a visual approximation of product motion.
    // Its direction follows the *supplied local acceleration*, and its
    // strength vanishes continuously with gas density and thermal contrast.
    // Gas flow is measured RELATIVE to the burning body, so boosting an
    // entire moving frame does not tilt the visual plume.
    const float density = std::max(input.gasDensity, 0.0f);
    const float gasFraction = density / (density + 0.01f);
    const glm::vec3 buoyantVelocity =
        -input.gravityAcceleration * (0.18f * warmth * gasFraction);
    const glm::vec3 drift = BoundedDrift(input.gasVelocity - input.bodyVelocity +
                                         buoyantVelocity);

    std::vector<FireVisualPrimitive> visuals;
    visuals.reserve(13);
    const glm::vec3 flameColor(1.0f, 0.30f + 0.42f * warmth,
                              0.06f + 0.18f * warmth);
    visuals.push_back({input.position, radius * 0.85f, flameColor,
                       0.68f * burnStrength, FireVisualKind::Flame});

    // Equal opposite pairs make the no-gravity/no-flow case isotropic. The
    // axes live in the body's local frame and rotate with its presented
    // orientation. A common physical drift deforms the symmetric halo into
    // a plume without selecting world +Y (or any other world axis).
    constexpr std::array<glm::vec3, 6> kLocalDirections{{
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
    }};
    for (std::size_t i = 0; i < kLocalDirections.size(); ++i) {
        const glm::vec3 radial = orientation * kLocalDirections[i];
        const float travelSeconds = 0.18f + 0.06f * static_cast<float>(i / 2);
        visuals.push_back({input.position + radial * (sourceRadius + 0.25f * radius) +
                               drift * travelSeconds,
                           radius * 0.54f, flameColor,
                           0.56f * burnStrength, FireVisualKind::Flame});
    }

    if (gasFraction > 0.0f) {
        const glm::vec3 smokeColor(0.26f + 0.10f * warmth,
                                   0.24f + 0.08f * warmth,
                                   0.22f + 0.07f * warmth);
        for (std::size_t i = 0; i < kLocalDirections.size(); ++i) {
            const glm::vec3 radial = orientation * kLocalDirections[i];
            const float travelSeconds = 0.60f + 0.10f * static_cast<float>(i / 2);
            visuals.push_back({input.position + radial * (sourceRadius + 0.90f * radius) +
                                   drift * travelSeconds,
                               radius * 0.64f, smokeColor,
                               0.25f * burnStrength * gasFraction,
                               FireVisualKind::Smoke});
        }
    }
    return visuals;
}
