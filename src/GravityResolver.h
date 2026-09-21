#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "GravityField.h"

// Judas's minimal spatial gravity-resolution mechanism (Milestone 7-B):
// combines several independent GravityField implementations, each paired
// with a position-based region of influence, into one coherent effective
// GravityField a consumer can sample without ever knowing which concrete
// implementation(s) contributed, or how many there are. See
// docs/ARCHITECTURE.md, "Gravity resolution."
//
// This is itself a GravityField — the entire point. PlayerController,
// DynamicBody, and everything else that already depends only on
// `const GravityField&` needs zero changes to participate in a world with
// more than one gravity environment; Application (the composition root)
// simply constructs a GravityResolver instead of binding a single concrete
// implementation directly, exactly as swapping FaithfulGravity for
// RadicalGravity in Milestone 5 needed no consumer-side changes.
//
// Not a generalized gravity-volume/scripting system (see
// docs/ARCHITECTURE.md, "Deliberately Not Implemented") — each zone is
// just a center point and a smooth radial falloff, the minimum needed to
// resolve "which environment(s) apply here, and how strongly" for this
// milestone's two environments.
class GravityResolver : public GravityField {
public:
    // Registers `field` as active near `falloffCenter`: full strength
    // (weight 1.0) at or inside `innerRadius`, smoothly fading to zero
    // weight at or beyond `outerRadius`. `field` must outlive this
    // resolver — not owned, the same convention as every other
    // GravityField reference in the engine.
    void AddZone(GravityField& field, const glm::vec3& falloffCenter, float innerRadius,
                 float outerRadius);

    // Combines every zone with nonzero weight at `worldPosition` into one
    // effective acceleration: directions are blended via spherical
    // (great-circle) interpolation weighted by each zone's influence —
    // never a plain vector sum/average, which could partially or fully
    // cancel two very different directions down to a near-zero,
    // directionless result (see docs/ARCHITECTURE.md, "Transition
    // semantics"). Magnitude is a weighted average AMONG contributing
    // zones, then scaled by their combined total weight (clamped to 1) —
    // not just the weighted average alone, which would let a single
    // zone's own fade-out produce its full, undiminished magnitude right
    // up until the exact instant its weight reaches zero, a hard cliff
    // rather than the smooth fade the falloff shape is supposed to
    // produce. Returns exactly the zero vector if `worldPosition` has no meaningful
    // weight in any zone (outside every zone's outer radius, or every
    // contributing zone's own sample happened to be degenerate there) —
    // the same "no defined gravity direction here" signal
    // PlayerController::ComputeLocalUp and RadicalGravity's own
    // exactly-at-center case already handle safely.
    glm::vec3 Sample(const glm::vec3& worldPosition) const override;

private:
    struct Zone {
        GravityField* field = nullptr;
        glm::vec3 falloffCenter{0.0f};
        float innerRadius = 0.0f;
        float outerRadius = 0.0f;
    };

    std::vector<Zone> m_zones;
};
