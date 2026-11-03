#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace judas::volume_grid {

inline constexpr std::size_t CutHydroacousticAmbient = std::numeric_limits<std::size_t>::max();

enum class CutFluidPhase { Water, Gas };

struct CutHydroacousticRegion {
    double waterVolume = 0., gasVolume = 0., gasMass = 0.;
    // End capacities measured with the initial phase interface held fixed.
    double fixedWaterVolume = 0., fixedGasVolume = 0.;
    double interfaceArea = 0.;
    double surfaceHead = 0.; // Initial water elevation relative to the pressure datum.
};

struct CutHydroacousticPort {
    std::size_t left, right; // Ambient can own only the right endpoint of a gas port.
    CutFluidPhase phase;
    double area, length, speed; // Signed speed from left to right at the start.
};

struct CutHydroacousticSettings {
    double dt = 0.;
    double gravity = 9.81, waterDensity = 1000., gasDensity = 1.2, soundSpeed = 343.;
    double volumeTolerance = 1e-15;
};

struct CutHydroacousticResult {
    std::vector<double> waterPressure, gasPressure; // Midpoint gauge pressure in Pa.
    std::vector<double> waterVolume, gasVolume, gasMass, surfaceHead;
    // Gas transfers use mass / reference gas density; water transfers use m³.
    std::vector<double> portVolume, portSpeed;
    double maximumVolumeError = 0., waterError = 0., gasMassError = 0.;
    double exteriorGasMass = 0., wallWork = 0., energyChange = 0., workError = 0.;
    int pressureIterations = 0;
};

// Linearized midpoint pressure, finite gas momentum and occupied-volume
// storage use one pressure graph. The caller supplies physical phase ports
// and phase capacities. Fixed topology and small acoustic excursions are
// assumed over the interval. Failure preserves the previous result.
bool SolveCutHydroacousticPressure(const std::vector<CutHydroacousticRegion>& regions,
                                  const std::vector<CutHydroacousticPort>& ports,
                                  const CutHydroacousticSettings& settings,
                                  CutHydroacousticResult& output, std::string& error);

} // namespace judas::volume_grid
