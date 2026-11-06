#pragma once
#include "WaterFragmentFlux.h"
namespace judas::volume_grid {
struct WaterFragmentAdvanceStats {
    std::size_t intervals=0,rejected=0,geometricConstraints=0;
    double geometryMs=0.,pressureMs=0.,transportMs=0.,totalMs=0.,minimumInterval=0.;
    CutCompensated advancedTime;
};
// Atomic whole-interval advance. Geometry retries cannot publish partial time,
// remove material, relax the physical audits or spend a trapped-air mass floor.
bool AdvanceWaterFragments(const CutSpatialField&,const WaterMaterialState&,
    double density,const GridVec& gravity,const WaterGasSettings&,
    WaterMaterialState&,WaterFragmentAdvanceStats&,std::string&,
    std::size_t intervalBudget=256);
}
