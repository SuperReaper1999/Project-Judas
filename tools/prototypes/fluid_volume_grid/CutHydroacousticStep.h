#pragma once

#include "CutHydroacousticPressure.h"
#include "CutRegionGasTopology.h"
#include "GridPressureReaction.h"

namespace judas::volume_grid {

struct CutHydroacousticState {
    std::vector<double> waterVolume, gasMass;
    std::vector<CutRegionInterface> interfaces;
    // Water and gas on the same physical face indices as the cut geometry.
    std::vector<std::array<double, 2>> faceSpeed;
    // One retained exterior port velocity for each region and vent pair.
    std::vector<double> ventSpeed;
};

struct CutHydroacousticStep {
    CutHydroacousticState state;
    CutHydroacousticResult pressure;
    // Dynamic gauge traction; the equilibrium hydrostatic load is separate.
    std::array<std::vector<GridPressureLoad>, 2> loads;
    double wallSweepError = 0., reactionWorkError = 0.;
};

// Fixed-phase-topology motion. Actual cut volumes and split wall patches
// supply the pressure sources, port openings and work-adjoint reactions.
// The prototype rejects crossed/shared free-surface events.
bool AdvanceCutHydroacoustic(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                             const CutHydroacousticState& state, const std::vector<CutRegionGasVent>& vents,
                             int upAxis, double referenceHeight, const CutHydroacousticSettings& settings,
                             CutHydroacousticStep& output, std::string& error);

} // namespace judas::volume_grid
