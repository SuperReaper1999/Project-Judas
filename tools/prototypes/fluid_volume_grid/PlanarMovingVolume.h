#pragma once

#include "GridPressureReaction.h"
#include "MovingVolumeGrid.h"

namespace judas::volume_grid {

struct PlanarMovingStep {
    MovingVolumeStep fluid; // Pressure is relative to the verified hydrostatic reference.
    double referenceHeight = 0;
    std::vector<GridPressureLoad> loads; // Mean total pressure load about the chart origin.
};

struct EvolvingVolumeInterval {
    PlanarMovingStep final;
    std::vector<GridPressureLoad> meanLoads; // Time weighted across accepted substeps.
    int substeps = 0, rejectedAttempts = 0;
    double maximumResidual = 0, maximumCfl = 0;
};

// Uses exact cut geometry to verify a flat initial water interface. Gravity is
// signed along axis, and water lies below height. Stationary boxes may intersect
// or cross the interface. The returned phase volumes can depart from the
// reference. A general evolving free surface is outside this reference step.
bool AdvancePlanarMovingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                               const MovingVolumeState& state, double dt, int axis, double height,
                               double signedGravity, PlanarMovingStep& output, std::string& error);

// Reconstructs a low-order planar interface in each cell from its actual
// water volume. A common flat pressure reference preserves resting pools;
// local phase geometry supplies the residual gravity and donor transport.
bool AdvanceEvolvingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                           const MovingVolumeState& state, double dt, int axis,
                           double signedGravity, PlanarMovingStep& output, std::string& error);

// Subdivides only a rejected transport interval. Output is transactional
// over the entire prescribed motion, including the time-weighted reaction.
bool AdvanceEvolvingVolumeInterval(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                   const MovingVolumeState& state, double dt, int axis,
                                   double signedGravity, int subdivisionLimit,
                                   EvolvingVolumeInterval& output, std::string& error);

} // namespace judas::volume_grid
