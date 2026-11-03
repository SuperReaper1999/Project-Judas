#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace judas::volume_grid {

// One physical opening, with positive mixture flux directed left to right.
// The bounds and preference are absolute water volumes in the actual
// departure direction. The caller computes them from the swept geometry.
struct CutRegionPhaseFace {
    std::size_t left, right;
    double signedVolume, minWater, maxWater, preferredWater;
};

struct CutRegionPhaseInput {
    std::vector<double> startCapacity, endCapacity, startWater;
    std::vector<CutRegionPhaseFace> faces;
    // Empty disables relaying. A marked region may forward phase it receives
    // during this interval. Its geometry must admit the incoming and outgoing
    // phase through the face bounds.
    std::vector<bool> relayAllowed;
    double balanceTolerance = 0.;
};

struct CutRegionPhaseResult {
    // Signed volume on each physical opening, with the input face orientation.
    std::vector<double> waterFlux, airFlux, endWater;
    double maximumVolumeResidual = 0., maximumWaterResidual = 0.;
    double endpointWaterCorrection = 0., conservationResidual = 0.;
};

// Partition the fixed pressure flux and publish one water inventory per cut
// region. Same-interval relays require a directed path from an initial
// inventory of that phase. A rejected interval preserves the previous result.
bool TransportCutRegionPhases(const CutRegionPhaseInput& input, CutRegionPhaseResult& output,
                              std::string& error);

} // namespace judas::volume_grid
