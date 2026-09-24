#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Presentation only. Combustion, heat, and fuel remain authoritative elsewhere.
// Judas does not simulate individual combustion-product parcels; the visual
// plume approximates their motion from local gravity, gas-relative flow, and
// temperature. The caller supplies a body's *presented* pose and physical
// values sampled for this render frame.
struct FirePresentationInput {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    float burnRateKgPerSecond = 0.0f;
    float temperatureKelvin = 293.15f;
    glm::vec3 bodyVelocity{0.0f};
    glm::vec3 gasVelocity{0.0f};
    float gasDensity = 0.0f;
    glm::vec3 gravityAcceleration{0.0f};
    // Approximate distance from body centre to its visible surface. Flame
    // lobes start outside this radius so an opaque body does not hide them.
    float sourceRadius = 0.45f;
};

enum class FireVisualKind { Flame, Smoke };

struct FireVisualPrimitive {
    glm::vec3 position{0.0f};
    float radius = 0.0f;
    glm::vec3 color{0.0f};
    float alpha = 0.0f;
    FireVisualKind kind = FireVisualKind::Flame;
};

// Produces at most thirteen small spheres for Renderer::DrawSphere. The six
// paired directions are a body-local drawing convention, not a physical up.
// With zero gravity and no gas-relative airflow the visual expands
// symmetrically around its source; every direction rotates with the body.
std::vector<FireVisualPrimitive> BuildFirePresentation(const FirePresentationInput& input);
