#pragma once

#include "CutRegionPhase.h"
#include "MovingGridGeometry.h"

namespace judas::volume_grid {

struct CutRegionMomentumResult {
    std::vector<GridVec> velocity;
    double massResidual = 0.;
    GridVec momentumResidual{};
    double beforeKineticEnergy = 0.;
    double afterImpulseKineticEnergy = 0.;
    double afterTransportKineticEnergy = 0.;
    double afterEndImpulseKineticEnergy = 0.;
    double impulseWork = 0.;
    double impulseWorkResidual = 0.;
    double endImpulseWork = 0.;
    double endImpulseWorkResidual = 0.;
    double mixingLoss = 0.;
    double kineticResidual = 0.;
};

// Apply the integrated pressure and gravity impulse to initial region mass,
// then carry momentum with the accepted water and air flux on each face.
// Water and air share one region velocity and fixed densities of 1000 and
// 1 kg/m³. Failure leaves the previous result intact.
bool TransportCutRegionMomentum(const CutRegionPhaseInput& input,
                                 const CutRegionPhaseResult& phases,
                                 const std::vector<GridVec>& startVelocity,
                                 const std::vector<GridVec>& impulse,
                                 CutRegionMomentumResult& output, std::string& error);

// An additional endpoint impulse acts on the actual arrived or retained
// material after conservative transport. An empty vector means no terminal
// impulse. An initially unowned impulse is never applied to a ghost mass.
bool TransportCutRegionMomentum(const CutRegionPhaseInput& input,
                                 const CutRegionPhaseResult& phases,
                                 const std::vector<GridVec>& startVelocity,
                                 const std::vector<GridVec>& impulse,
                                 const std::vector<GridVec>& endImpulse,
                                 CutRegionMomentumResult& output, std::string& error);

} // namespace judas::volume_grid
