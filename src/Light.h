#pragma once

#include <glm/glm.hpp>

// Milestone 14: Judas's own minimal dynamic-light representation — plain
// rendering-relevant data, nothing else. `Renderer` consumes a list of
// these (see `Renderer::SetDynamicLights`); it never reaches into
// `PlayerController`, `FlyingPrimitiveControl`, `PilotAttachment`, or any
// physics body to get there (see docs/ARCHITECTURE.md, "Milestone 14,
// Judas-owned light representation"). The composition root
// (`Application.cpp`) is the only place that knows a light is "the
// player's torch" or "the spacecraft's headlight" — by the time a
// `DynamicLight` reaches `Renderer`, it is already expressed in WORLD
// space for the CURRENT render frame, built from whichever owner's
// PRESENTED (interpolated) transform is current that frame — see
// "Milestone 14, Dynamic means dynamic" for why presented, not
// authoritative, transforms feed this.
//
// The existing Milestone 9 directional light (the "sun") is NOT
// represented here — it stays exactly what it was
// (`Renderer::SetLighting`'s `direction`/`lightColor`/`ambientColor`,
// unchanged), a single always-on world-space direction with no position
// and no falloff. `DynamicLight` covers the two NEW light kinds this
// milestone adds: point and spot. A future third dynamic kind would add a
// new `LightKind` enumerator and a matching branch in the fragment
// shader, not a parallel representation.
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
};

// A small, explicit, documented cap — not a scale limit this milestone
// needed to solve. One player torch plus a small fixed spacecraft light
// rig (one headlight spot, two wingtip point lights) is 4 total; this
// value comfortably covers that with one slot of headroom, sized for a
// fixed-length GLSL uniform array (see Renderer.cpp) — never a dynamically
// resizable light list. See docs/ARCHITECTURE.md, "Milestone 14, Light
// limits," for why clustered/deferred/tiled lighting was deliberately not
// built to support more than this.
constexpr int kMaxDynamicLights = 5;
