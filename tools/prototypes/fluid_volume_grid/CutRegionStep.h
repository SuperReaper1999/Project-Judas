#pragma once

#include "CutRegionGeometry.h"
#include "CutRegionPhase.h"
#include "CutRegionPressure.h"
#include "GridPressureReaction.h"

namespace judas::volume_grid {

struct CutRegionState {
    std::vector<double> water;
    std::vector<GridVec> velocity;
    // Empty initial states reconstruct liquid below the gravity-aligned plane.
    // A transported interface can face the actual inlet instead.
    std::vector<CutRegionInterface> interfaces{};
};

struct CutRegionSettings {
    int upAxis = 1;
    double gravity = -9.81, referenceHeight = 0.;
};

struct CutRegionStep {
    CutRegionState state;
    // Pressure and phase flux arrays use these indices into geometry.faces.
    std::vector<std::size_t> activeFaces;
    CutRegionPressure pressure;
    CutRegionPhaseResult phase;
    std::vector<GridPressureLoad> loads;
    GridVec transportMomentumResidual{};
    double pressureWorkResidual = 0.;
    double transportKineticResidual = 0.;
};

// Capacity, aperture, pressure, phase transfer and momentum all use the
// same physical regions and the same oriented faces. State and output are
// separate so a rejected trial cannot advance either rigid or fluid state.
bool AdvanceCutRegionStep(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                          const CutRegionState& state, double dt, const CutRegionSettings& settings,
                          CutRegionStep& output, std::string& error);

} // namespace judas::volume_grid
