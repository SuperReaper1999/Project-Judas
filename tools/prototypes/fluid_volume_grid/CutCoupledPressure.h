#pragma once
#include "CutLocalPressure.h"
#include "CutDualMomentum.h"

namespace judas::volume_grid {
struct CutCoupledState {
    CutLocalPressureResult material;
    std::vector<CutCompensated> velocity;
    CutDualMomentumAudit momentum;
    int nonlinearIterations=0;
    double elapsedMilliseconds=0.;
};
enum class CutCoupledLinearization { Newton, Oseen };
struct CutCoupledSettings {
    int iterationLimit=60;
    double momentumTolerance=1e-10;
    CutCoupledLinearization linearization=CutCoupledLinearization::Newton;
    CutMomentumBackend momentumBackend=CutMomentumBackend::Fast;
};
bool AdvanceCutCoupledPressure(const CutSpatialField&,const CutVelocityDual&,
    const CutDynamicSettings&,const CutCoupledSettings&,CutCoupledState&,std::string&,
    const CutCoupledState* previous=nullptr,const std::vector<double>& pressureSeed={});
} // namespace judas::volume_grid
