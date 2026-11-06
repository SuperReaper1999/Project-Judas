#pragma once
#include "WaterMaterialFragments.h"
namespace judas::volume_grid {
struct WaterFragmentInterface {
    std::size_t water,gas,patch;
    int axis,sign;
    WaterCoordinate lo,hi; // water containing-cell coordinates
};
struct WaterFragmentGeometry {
    WaterMimeticGeometry liquid;
    WaterGasGeometry air;
    std::vector<WaterMaterialFace> faces;
    std::vector<std::pair<std::size_t,std::size_t>> sharedWater;
    std::vector<WaterFragmentInterface> interfaces;
    std::vector<CutCompensated> waterVolume;
    std::vector<std::size_t> carrier;
};
bool PrepareWaterFragmentGeometry(const CutSpatialField&,const WaterMaterialState&,
    double density,const GridVec& gravity,const WaterGasSettings&,
    WaterFragmentGeometry&,std::string&);
}
