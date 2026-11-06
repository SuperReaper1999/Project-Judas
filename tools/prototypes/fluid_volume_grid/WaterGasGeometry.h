#pragma once
#include "WaterGasPockets.h"
#include "CutSpatialTransport.h"
namespace judas::volume_grid {
struct WaterGasGeometry {
    std::vector<std::size_t> cellNode,nodeCell;
    std::vector<CutCompensated> volume,mass,containingCapacity;
    std::vector<WaterGasLink> links;
    std::vector<WaterGasVent> vents;
    std::vector<double> sharedDryArea;
};
// Frozen conforming rectilinear geometry; one independently compressible gas
// compartment per represented dry cell. Shared dry footprints and explicit
// exterior ports produce resistive links, never pressure-owner unioning.
bool PrepareWaterGasGeometry(const CutSpatialField&,
    const std::vector<CutRegionWaterPlane>&,const WaterGasSettings&,
    double dynamicViscosity,WaterGasGeometry&,std::string&);
}
