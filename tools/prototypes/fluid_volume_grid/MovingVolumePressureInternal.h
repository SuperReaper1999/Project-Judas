#pragma once

#include "MovingVolumeGrid.h"

namespace judas::volume_grid::detail {

// Uses the same mass as the dual momentum transport. The caller owns
// hydrostatic gravity splitting because the geometry changes this metric.
bool AdvanceVolumeWithDualMass(const GridLayout& layout, const MovingGridGeometry& geometry,
                               const MovingVolumeState& state, double dt,
                               MovingVolumeStep& output, std::string& error);

// Same mass metric, with the opening-face predictor supplied by persistent
// side-owned momentum. Do not zero a start-closed face's supplied velocity.
bool AdvanceVolumeWithOwnedDualMass(const GridLayout& layout, const MovingGridGeometry& geometry,
                                    const MovingVolumeState& state, double dt,
                                    MovingVolumeStep& output, std::string& error);

} // namespace judas::volume_grid::detail
