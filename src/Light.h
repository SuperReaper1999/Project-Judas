#pragma once

#include <glm/glm.hpp>

// Presentation-only point/spot data, in the active local world chart. Owners
// supply interpolated transforms; no light reads input, gameplay or gravity.
// M92 bins a relevance-bounded set per camera rather than truncating the first
// five registered sources. Directional sunlight remains SetLighting state.
enum class LightKind {
    Point,
    Spot,
};

struct DynamicLight {
    LightKind kind = LightKind::Point;

    glm::vec3 position{0.0f};  // world space

    // World space, normalized — the direction the light itself FACES
    // (e.g. a flashlight's forward vector). Meaningful only for
    // LightKind::Spot; ignored for LightKind::Point (a point light is
    // omnidirectional by definition).
    glm::vec3 direction{0.0f, 0.0f, -1.0f};

    // Already intensity-scaled — see Renderer::SetDynamicLights's own doc
    // comment for the exact convention (this is NOT a separate
    // "intensity" multiplier kept apart from color; M14 does not claim
    // photometric units, see docs/ARCHITECTURE.md, "Milestone 14,
    // Point-light attenuation").
    glm::vec3 color{1.0f};

    // World units — the distance at which this light's contribution has
    // fallen to (approximately) zero. See Renderer.cpp's attenuation
    // formula for the exact falloff curve this shapes.
    float range = 10.0f;

    // Degrees from the light's own `direction` — LightKind::Spot only.
    // Inside `innerConeDegrees` the light contributes at full strength;
    // beyond `outerConeDegrees` it contributes nothing; between the two it
    // fades smoothly (see Renderer.cpp's spotlight cone formula).
    // `innerConeDegrees` must be <= `outerConeDegrees`.
    float innerConeDegrees = 15.0f;
    float outerConeDegrees = 25.0f;

    // Assigned per view by SelectShadowLights; -1 is unshadowed.
    int shadowMapIndex = -1;
    bool castsShadow = false;
    float shadowPriority = 0;
    unsigned long long identity = 0;
};

// One directional layer and up to eight selected spot layers. Historical
// torch/ship indices remain valid for low-level callers; runtime selection is
// generic. Point-light cubemap shadows are not part of this representation.
constexpr int kDirectionalShadowSlot=0,kTorchShadowSlot=1,kShipHeadlightShadowSlot=2;
constexpr int kShadowMapCount=9;
// Historical M14 constant retained for source compatibility, NOT a runtime cap.
constexpr int kMaxDynamicLights=5;
