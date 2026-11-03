#pragma once

#include "MovingGridGeometry.h"

namespace judas::volume_grid {

struct MovingVolumeState {
    std::vector<double> water; // Absolute water volume per cell, in m³.
    std::array<std::vector<double>, 3> velocity; // Positive chart-axis face speed.
};

struct MovingVolumeStep {
    MovingVolumeState state;
    std::vector<double> pressure; // Pa, with zero mean per sealed component.
    std::array<std::vector<double>, 3> flux; // Shared, time-integrated mixture volume, in m³.
    std::array<std::vector<double>, 3> materialMassFlux; // Same transport as flux, in kg.
    double residual = 0; // Maximum error in the cell volume balance, in m³.
    double pressureFluidWork = 0;
    double pressureKineticWork = 0; // Face kinetic-energy change during pressure projection.
    double pressureCorrectionEnergy = 0;
    double maxCfl = 0;
    double waterRoundoffCorrection = 0;
    int iterations = 0;
};

// Prescribed moving boxes with sealed exterior boundaries. Project mixture
// volume and transport water and air through the same shared face flux.
// Momentum advection and rigid feedback are outside this first moving-grid
// operator. Failure preserves output, including all previously published state.
bool AdvanceMovingVolume(const GridLayout& layout, const MovingGridGeometry& geometry,
                         const MovingVolumeState& state, double dt, const GridVec& gravity,
                         MovingVolumeStep& output, std::string& error);

} // namespace judas::volume_grid
