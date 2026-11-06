#pragma once
#include "WaterFragmentGeometry.h"
namespace judas::volume_grid {
struct WaterInterfaceSweep {WaterFragmentInterface face;CutCompensated volume;};
struct WaterInterfaceEventReport {
    double maximumFraction=1.;std::size_t limitingFragment=SIZE_MAX;
    std::size_t piecesBefore=0,piecesAfter=0;
    CutCompensated waterChange,maximumSweepError,availableVolume;
    std::size_t failedInterface=SIZE_MAX;
};
// Fast bound from the initial receiver pieces only. Translating fronts can
// continue into newly occupied space, so this is not an authoritative event
// time or a substitute for ApplyWaterInterfaceEvents and pressure re-projection.
bool BoundWaterInterfaceEvents(const WaterMaterialState&,
    const std::vector<WaterInterfaceSweep>&,WaterInterfaceEventReport&,std::string&);
// Conservative geometric prefix. The caller supplies the accepted physical
// interval's integrated fluxes and updated air carrier masses. Ordinary growth
// and recession create local pieces; separated air fronts retain distinct mass.
bool ApplyWaterInterfaceEvents(const CutSpatialField&,const WaterMaterialState&,
    const std::vector<WaterInterfaceSweep>&,const std::vector<CutCompensated>& airCarrierMass,
    WaterMaterialState&,WaterInterfaceEventReport&,std::string&,
    const std::vector<std::size_t>* connectedCarriers=nullptr);
}
