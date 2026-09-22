#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "GravityField.h"

class GravityVolume;

// Judas's gravity-CONTEXT mechanism, redesigned after M7-Final's second
// human-validation failure. See docs/ARCHITECTURE.md, "Gravity context
// ownership," for the full retrospective on BOTH prior designs
// (GravityResolver's weighted falloff blend, and this file's own first
// version, which fixed off-path contamination but still blended two
// fields together inside an explicit "transition" object). The retrospective
// insight: a consumer's own reorientation/velocity-integration machinery
// (PlayerController's rate-capped UpdateFrameOrientation, its continuous
// airborne velocity integration — architectural law #14) already turns a
// sudden change in the RAW sampled gravity value into a smooth, gradual
// transition. Blending was never necessary at the gravity-sampling layer;
// it was solving a problem the consumer side already solves, while
// failing to solve the real one — giving a flat surface like the plank
// its own coherent local gravity, rather than deriving it by averaging
// two unrelated planets' raw math.
//
// So: pure ownership routing, nothing else. A consumer's position belongs
// to exactly one region (or none). Sample returns that region's field's
// own raw value, completely unblended — never a mix of two fields, ever,
// anywhere. Crossing from one region to another is a literal,
// instantaneous change in the sampled value between one fixed step and
// the next; PlayerController already turns that into a smooth reorientation
// over about a second (see docs/ARCHITECTURE.md, "Orientation"), exactly
// as it always has.
//
// Like its predecessor, this is itself a GravityField — the entire point.
// PlayerController, DynamicBody, and everything else that depends only on
// `const GravityField&` need zero changes.
class GravityContextMap : public GravityField {
public:
    // Registers `field` as authoritative anywhere `volume` contains the
    // position. Checked in registration order; the FIRST region whose
    // volume contains a position wins. `field` and `volume` must outlive
    // this map — not owned, the same reference convention as every other
    // GravityField relationship in the engine.
    //
    // Composition-root responsibility: regions must not overlap each
    // other. There is no blending and no tie-break beyond registration
    // order, so an overlap would silently let one region's field leak
    // into territory meant for another — this class has no way to detect
    // that for you.
    void AddRegion(GravityField& field, const GravityVolume& volume);

    glm::vec3 Sample(const glm::vec3& worldPosition) const override;

private:
    struct Region {
        GravityField* field = nullptr;
        const GravityVolume* volume = nullptr;
    };

    std::vector<Region> m_regions;
};
