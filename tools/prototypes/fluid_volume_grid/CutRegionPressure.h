#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace judas::volume_grid {

struct CutRegionPressureFace {
    std::size_t left, right;
    double conductance, predictedVolume;
    bool thin = false;
};

struct CutRegionPressure {
    std::vector<double> pressure;
    std::vector<double> volumeFlux, pressureJump;
    double maximumBalanceError = 0.;
    double pressureWork = 0.;
    double pressureCorrectionEnergy = 0.;
    std::size_t reducedConnections = 0;
    int iterations = 0;
};

// Project signed shared volumes onto outgoing-minus-incoming = old capacity
// minus new capacity. Elimination only changes coordinates in the same
// pressure operator. All physical fluxes are recovered and checked before
// publication. A failed projection leaves output untouched.
bool ProjectCutRegionPressure(const std::vector<double>& volumeSource,
                              const std::vector<CutRegionPressureFace>& faces,
                              double cellVolume, double tolerance,
                              CutRegionPressure& output, std::string& error);

} // namespace judas::volume_grid
