#pragma once

#include "CutSelectiveTransport.h"
#include <array>

namespace judas::volume_grid {

using CutMomentumVector = std::array<CutCompensated, 3>;
enum class CutMomentumBackend { Fast, Reference };
struct CutMomentumBoundary {
    std::size_t cell;
    CutCompensated mass; // Outward positive. Incoming velocity is prescribed.
    CutMomentumVector incomingVelocity{};
};
struct CutMassMomentumResult {
    std::vector<CutMomentumVector> velocity, momentum;
    CutMomentumVector momentumResidual{}, externalMomentum{};
    CutMomentumVector momentumRowL1{};
    double beforeEnergy = 0., afterEnergy = 0., impulseWork = 0.;
    double externalEnergy = 0., unsteadyLoss = 0., mixingLoss = 0.;
    double energyResidual = 0., massDefectWork = 0., maximumMassResidual = 0.;
    double maximumLocalMomentumResidual = 0.;
};

// Implicit upwind momentum carried by EXACTLY the admitted material mass
// flux, with an independently checked work/kinetic-energy identity. There is
// no damping coefficient, speed limit or energy correction. This is a cell
// carrier operator; it is not a bridge to the pressure solver's staggered
// half-column masses. Such a bridge must provide its own compatible fluxes.
// All mass owners must remain positive. Failure publishes nothing.
bool TransportCutMassMomentum(const std::vector<double>& capacity,
    const std::vector<CutCompensated>& startMass,
    const std::vector<CutCompensated>& endMass,
    const std::vector<CutTransportFace>& signedMassFlux,
    const std::vector<CutMomentumBoundary>&,
    const std::vector<CutMomentumVector>& startVelocity,
    const std::vector<CutMomentumVector>& impulse,
    double localMassTolerance, double momentumTolerance,
    CutMassMomentumResult&, std::string&,
    CutMomentumBackend backend = CutMomentumBackend::Fast);

} // namespace judas::volume_grid
