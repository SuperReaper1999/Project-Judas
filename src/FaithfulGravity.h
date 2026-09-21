#pragma once

#include "GravityField.h"

// Judas's canonical uniform, constant-direction gravity implementation —
// conventional gravity suitable for an ordinary/local game environment
// with a single, fixed "down."
//
// For Milestone 3 this returns (0, -9.81, 0) everywhere. That vector is
// this implementation's own configuration/test data, NOT an engine-wide
// definition of "down" — see docs/ARCHITECTURE.md, "Current gravity." A
// future radial/planetary gravity, or a composite/multi-source field, is a
// separate implementation of GravityField, not a variant of this one —
// FaithfulGravity is intentionally the simple case, not the general one.
class FaithfulGravity : public GravityField {
public:
    glm::vec3 Sample(const glm::vec3& worldPosition) const override;
};
