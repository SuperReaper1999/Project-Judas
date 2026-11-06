#pragma once
#include <cstdint>
#include "WaterGasProjection.h"
#include "WaterCellAudit.h"
namespace judas::volume_grid {
struct WaterVolumeStep {
    std::vector<CutCompensated> water,gas,gasMass;
    std::uint64_t chartStamp=0;
    std::vector<std::array<CutCompensated,3>> momentum;
    std::vector<GridVec> velocity;
    WaterGasProjectionResult pressure;
    double maximumPartitionResidual=0.,maximumMomentumResidual=0.;
    double waterLedger=0.,gasLedger=0.,maximumSpeed=0.;
    double pressureMilliseconds=0.,totalMilliseconds=0.;
};
// Frozen solid chart, horizontal single-interface cells. Pressure, independent
// conservative water transfers and finite air mass all gate one publication.
// Interface crossings beyond represented cells reject, never clip or delete.
bool AdvanceWaterVolume(const CutSpatialField&,const WaterGasSettings&,double waterDensity,
    const GridVec& gravity,const std::vector<GridVec>& sourceVelocity,
    WaterVolumeStep&,std::string&,const WaterVolumeStep* previous=nullptr);
}
