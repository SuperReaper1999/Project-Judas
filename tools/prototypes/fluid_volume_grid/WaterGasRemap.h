#pragma once
#include "WaterGasProjection.h"
namespace judas::volume_grid {
struct WaterGasDeparture {std::size_t node,cell;CutCompensated volume;};
struct WaterGasRemapResult {std::vector<CutCompensated> volume,mass;double maximumPartitionResidual=0.,freeEnergyChange=0.,maximumGeometricCorrection=0.,reconstructionWorkBound=0.;};
// Remap Lagrangian air pressure compartments to Eulerian phase cells. A
// receding side-front touches the cell's existing upper dry slab. It may mix
// there, never through a closed wall or with another disconnected new front.
// Native and swept parcels retain all mass. Tiny native parcels stay positive;
// the unchanged containing-cell partition budget gates geometric reconstruction.
bool RemapWaterGas(const CutSpatialField&,const WaterGasGeometry&,
    const WaterGasProjectionResult&,const std::vector<CutCompensated>& water,
    const std::vector<WaterGasDeparture>&,const WaterGasSettings&,
    WaterGasRemapResult&,std::string&);
}
