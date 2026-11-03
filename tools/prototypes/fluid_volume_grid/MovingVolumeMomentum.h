#pragma once

#include "MovingVolumeGrid.h"

namespace judas::volume_grid {

struct MovingVolumeMomentumStep {
    std::array<std::vector<double>, 3> velocity;
    GridVec beforeMomentum{}, afterMomentum{}; // kg m/s.
    double beforeKineticEnergy = 0, afterKineticEnergy = 0;
    double maximumPrimalMassResidual = 0, maximumDualMassResidual = 0; // kg.
    double maximumDonorCfl = 0;
};

// Transport projected face momentum with the same primal material mass flux
// that advanced water and air. A staggered dual cell owns half the material
// mass in each adjacent primal cell, including boundary half cells. This
// algebraic dual mass is independent of the pressure operator's face metric.
// The transport does not enforce a zero wall-normal velocity on its output;
// imposing that constraint in a subsequent pressure step must account for the
// wall impulse. Interior closed faces and empty primal cells require a
// different velocity ownership map and are explicitly outside this operator.
bool AdvectMovingVolumeMomentum(const GridLayout& layout, const MovingGridGeometry& geometry,
                                const MovingVolumeState& before,
                                const MovingVolumeStep& projected,
                                MovingVolumeMomentumStep& output, std::string& error);

} // namespace judas::volume_grid
