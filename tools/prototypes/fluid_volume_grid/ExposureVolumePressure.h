#pragma once

#include "MovingVolumeGrid.h"

namespace judas::volume_grid {

// Single-phase prescribed-motion probe. Temporarily group a newly exposed
// cell with an adjacent starting material owner through a real opening.
// Pressure is shared within each group; publish its conservative internal
// transfer by recovering the original fine cell's continuity equation.
// The body and momentum remain outside this pressure-only probe.
bool AdvanceExposureVolumePressure(const GridLayout& layout, const MovingGridGeometry& geometry,
                                    const MovingVolumeState& state, double dt, bool water,
                                    MovingVolumeStep& output, std::string& error);

} // namespace judas::volume_grid
