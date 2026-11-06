#pragma once
#include "CutOwnedFlux.h"
#include "CutDynamicTransport.h"
namespace judas::volume_grid {
struct CutPhaseBoundary {
    std::size_t fragment;
    int axis, sign;
    double volume, incomingWater = 0.;
    bool gasOnly = true;
};
struct CutPhaseFrontSettings {
    double relativeError = 1e-4;
    double referenceGasDensity = 1.2, soundSpeed = 343.;
    std::size_t maximumSteps = 20000;
};
struct CutPhaseFrontResult {
    std::vector<double> water, gas, waterFlux;
    std::vector<CutRegionInterface> interfaces;
    std::vector<double> ownerWater, ownerGasMass;
    double externalWater = 0., externalGasMass = 0.;
    double maximumPartitionError = 0.;
    std::size_t steps = 0, rejectedSteps = 0;
};
// Time-resolved, geometry-aware transport on an independently conservative
// internal flux reconstruction. Inputs are integrated over the whole interval;
// the integrator uses normalized time [0,1]. No physical state is published
// until bounds, shared-face ledgers and the spatial partition all pass.
bool AdvanceCutPhaseFronts(const CutRegionChart&, const CutRegionGeometry&,
                           const CutConnectedOwners&, const CutDynamicState& source,
                           const std::vector<double>& density,
                           const std::vector<double>& external,
                           const std::vector<CutPhaseBoundary>& boundary,
                           const std::vector<double>& predictor,
                           const std::vector<double>& mobility,
                           const CutPhaseFrontSettings&, CutPhaseFrontResult&,
                           std::string& error);
}
