#pragma once

#include "HalfDualVolumeMomentum.h"
#include "PlanarMovingVolume.h"

namespace judas::volume_grid {

struct OwnedEvolvingVolumeStep {
    PlanarMovingStep phase;
    HalfDualVolumeState halfVelocity;
    GridVec residualGravityImpulse{}, pressureImpulse{}, stationaryWallImpulse{};
    GridVec transportMomentumError{};
    double advectiveKineticChange = 0;
    double mergeKineticLoss = 0;
    double stationaryWallKineticLoss = 0;
};

struct OwnedEvolvingVolumeInterval {
    OwnedEvolvingVolumeStep final;
    std::vector<GridPressureLoad> meanLoads;
    int substeps = 0, rejectedAttempts = 0;
    double maximumResidual = 0, maximumCfl = 0;
};

// Evolving water and air with momentum ownership on both sides of each
// aperture. The shared face velocity is rebuilt after material advection
// and is the predictor of the next pressure step.
bool AdvanceOwnedEvolvingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                const MovingVolumeState& state, const HalfDualVolumeState& owned,
                                double dt, int axis, double signedGravity,
                                OwnedEvolvingVolumeStep& output, std::string& error);

bool AdvanceOwnedEvolvingVolumeInterval(const GridLayout& layout,
                                        const std::vector<BoxMotion>& boxes,
                                        const MovingVolumeState& state,
                                        const HalfDualVolumeState& owned,
                                        double dt, int axis, double signedGravity,
                                        int subdivisionLimit, OwnedEvolvingVolumeInterval& output,
                                        std::string& error);

} // namespace judas::volume_grid
