#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace judas::volume_grid {

// Orient each face in the direction of its fixed mixture transfer. All
// volumes are nonnegative. The bounds come from geometric phase ownership.
struct GridPhaseFluxEdge {
    std::size_t from, to;
    double volume, minWater, maxWater, preferredWater;
};

struct GridPhaseFluxRepairInput {
    std::vector<double> startCapacity, endCapacity, startWater;
    std::vector<GridPhaseFluxEdge> edges;
    // Empty means no relays. A relay may forward incoming material during
    // the interval only if the caller established geometric eligibility.
    std::vector<bool> relayAllowed;
    double balanceTolerance = 0;
    // Positive enables locally audited ordinary phase inventories.
    // The ratio is water density divided by air density.
    double localOrdinaryDensityRatio = 0;
};

struct GridPhaseFluxRepairResult {
    std::vector<double> waterFlux, endWater;
    double maximumVolumeResidual = 0, maximumWaterResidual = 0;
    double endpointWaterCorrection = 0, conservationResidual = 0;
};

struct GridPhaseInventoryRoundoff {
    double water, air;
};

// Air can be the difference of nearly equal volumes. Only a locally measured
// mixture residual, bounded by the mass scale, can add to its arithmetic bound.
GridPhaseInventoryRoundoff MeasureGridPhaseInventoryRoundoff(
    double capacity, double volumeResidual, double massInAirVolumeUnits);

// Preserve an admissible preferred partition. Otherwise find a feasible
// bounded circulation, with ordinary donors spending only their start
// inventory. Face order and mixture transfer are preserved. The caller
// still owns swept-face causality and pressure inertia. Failed repair leaves
// the previous result intact.
bool RepairGridPhaseFlux(const GridPhaseFluxRepairInput& input,
                         GridPhaseFluxRepairResult& output, std::string& error);

} // namespace judas::volume_grid
