#pragma once
#include "WaterInterfaceEvents.h"
namespace judas::volume_grid {
struct WaterFragmentFluxEdge {
    std::size_t from,to,shared=SIZE_MAX,interface=SIZE_MAX;
    CutCompensated amount;double area=0.;
};
struct WaterFragmentFlux {
    std::vector<WaterFragmentFluxEdge> edges;
    std::vector<WaterInterfaceSweep> sweeps;
    std::vector<std::vector<CutCompensated>> liquidFlux;
    double maximumMomentumResidual=0.;
    std::vector<GridVec> pressureVelocity;
};
// Conservative mixed flux recovery. Corrections are numerical only: the
// unchanged FULL local momentum/work rows must admit them before publication.
// Optional fixed integrated flows represent exact geometric crossing events.
bool BalanceWaterFragmentFlux(const WaterMaterialState&,const WaterFragmentGeometry&,
    const WaterGasProjectionResult&,const WaterGasSettings&,
    const std::vector<std::pair<std::size_t,CutCompensated>>& fixedInterfaces,
    WaterFragmentFlux&,std::string&);
}
