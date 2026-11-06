#pragma once
#include "WaterMimeticCell.h"
#include "WaterGasGeometry.h"
namespace judas::volume_grid {
struct WaterPatchOwner {std::size_t shared=SIZE_MAX,gas=SIZE_MAX;};
struct WaterMimeticRegion {
    std::size_t sourceCell=0;
    GridVec origin{},centroid{};
    std::vector<WaterCellPatch> patches;
    std::vector<WaterPatchOwner> owners;
    std::vector<CutCompensated> potential;
    WaterCellProjection response;
};
struct WaterMimeticGeometry {
    double density=0.;GridVec gravity{};
    std::vector<WaterMimeticRegion> regions;
    std::vector<std::size_t> cellWater;
    std::vector<double> cellCapacity;
    std::size_t sharedFaces=0,freeSurfaces=0,solidFaces=0;
};
// Frozen rectilinear liquid prisms with explicit shared, gas-facing and solid
// patches. Positive gas layers stay separate even below one coordinate ULP.
bool PrepareWaterMimeticGeometry(const CutSpatialField&,const WaterGasGeometry&,
    double density,double dt,const GridVec& gravity,WaterMimeticGeometry&,std::string&,const std::vector<GridVec>& sourceVelocity={});
}
