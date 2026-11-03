#pragma once

#include "GridOrientedSweep.h"
#include "GridPressureReaction.h"
#include "MovingVolumeGrid.h"

namespace judas::volume_grid {

struct OrientedMovingVolumeStep {
    MovingVolumeStep fluid;
    GridPressureLoad load; // Torque about the moving body centre.
    double pressureWorkResidual = 0; // J, excluding the exact hydrostatic component.
    double maximumSweepResidual = 0; // m³, from independent geometric checks.
    std::size_t geometryEvaluations = 0;
};

// Prescribed translation and angular motion in a sealed, single-phase chart.
// The water-filled and air-only controls share the same moving geometry. This
// operator does not yet couple a free body or transport a waterline.
bool AdvanceOrientedMovingVolume(const GridLayout& layout, const GridOrientedMotion& motion,
                                 const MovingVolumeState& state, double dt, const GridVec& gravity,
                                 OrientedMovingVolumeStep& output, std::string& error);

} // namespace judas::volume_grid
