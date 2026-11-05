#pragma once

#include "CutRegionGasTopology.h"

namespace judas::volume_grid {

// Experimental common-pressure formulation. Water remains incompressible;
// gas has finite mass and rho = rho0 + p/c^2. This is not a runtime backend.
struct CutDynamicState {
    std::vector<double> water, gasMass, pressure, faceSpeed, ventSpeed;
    std::vector<CutRegionInterface> interfaces;
};

struct CutDynamicSettings {
    double dt = 1. / 600.;
    GridVec gravity{0., -9.81, 0.};
    double waterDensity = 1000., gasDensity = 1.2, soundSpeed = 343.;
    double volumeTolerance = 1e-14;
    double relativeVolumeTolerance = 1e-10;
    int iterationLimit = 40;
};

struct CutDynamicStep {
    CutDynamicState state;
    std::vector<double> totalFlux, waterFlux, gasMassFlux;
    double maximumVolumeResidual = 0.;
    int nonlinearIterations = 0, linearIterations = 0;
    std::size_t pureThroughFlowOwners = 0;
    double externalGasMass = 0.;
};

// Sealed chart except explicit finite gas vents; no ambient-pressure
// shortcut for trapped gas. Each phase
// flux comes from a finite departure prism on the actual shared aperture.
// Rejection is transactional. Geometry, donor budgets and nonlinear EOS
// closure are independently audited before publication.
bool AdvanceCutDynamic(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                       const CutDynamicState& initial, const CutDynamicSettings& settings,
                       CutDynamicStep& output, std::string& error,
                       const std::vector<CutRegionGasVent>& vents = {});

} // namespace judas::volume_grid
