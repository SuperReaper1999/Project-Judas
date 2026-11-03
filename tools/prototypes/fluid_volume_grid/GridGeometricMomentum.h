#pragma once

#include "GridHalfCellGeometry.h"
#include "HalfDualVolumeMomentum.h"

namespace judas::volume_grid {

struct GridGeometricMomentumStart {
    std::array<GridHalfCellStorage, 3> storage;
    std::array<std::vector<double>, 3> faceMass;
    std::array<std::vector<double>, 3> faceParticipation;
    MovingVolumeState predictor;
};

// Derive the pressure face metric from the exact material owned on either
// side. A geometrically open face can have zero starting mass. The pressure
// solve must eliminate that direction or otherwise provide its new inertia.
bool BuildGridGeometricMomentumStart(const GridLayout& layout, const MovingGridGeometry& geometry,
                                     const std::vector<BoxMotion>& stationary, int phaseAxis,
                                     const std::vector<double>& waterPlane,
                                     const MovingVolumeState& material,
                                     const HalfDualVolumeState& owned,
                                     GridGeometricMomentumStart& output, std::string& error);

// Apply the actual pressure and gravity increments to the material that
// existed at the start. Eliminated directions get their state from the
// subsequent material transport, with no fictitious initial impulse.
bool ApplyGridGeometricPressure(const GridLayout& layout, const MovingGridGeometry& geometry,
                                const GridGeometricMomentumStart& start,
                                const MovingVolumeStep& projected,
                                const HalfDualVolumeState& owned, double dt,
                                HalfDualPressureUpdate& output, std::string& error);

struct GridGeometricTransport {
    HalfDualFaceMerge merged;
    std::vector<double> water;
    double waterVolumeRoundoff = 0.;
    GridVec momentumResidual{};
    GridVec phaseMomentumRoundoff{};
    double absolutePhaseMomentumRoundoff = 0.;
    double phaseRoundoffEnergy = 0.;
    double phaseMixingLoss = 0.;
    double kineticChange = 0.;
    // m³ reconciled between geometric and transported half inventories.
    double phaseFluxRoundoff = 0.;
};

// Recover real half-cell fluxes from the accepted primal phase transfers.
// A shared transverse face is split by geometric phase support. Midpoint
// transfers follow continuity on each actual half and each phase separately.
bool AdvectGridGeometricMomentum(const GridLayout& layout, const MovingGridGeometry& geometry,
                                   const std::vector<BoxMotion>& moving, int phaseAxis,
                                   const std::vector<double>& startPlane,
                                   const GridGeometricMomentumStart& start,
                                   const MovingVolumeStep& projected, const HalfDualVolumeState& pressed,
                                   GridGeometricTransport& output, std::string& error);

} // namespace judas::volume_grid
