#pragma once
#include "CutSpatialTransport.h"
#include "WaterGasPockets.h"
#include <cstdint>

namespace judas::volume_grid {
// Frozen-grid pressure/quantity prototype. Each containing cell owns independent
// extensive water, air volume and air mass, including sub-coordinate-ULP phases.
// No interface fragmentation, gas-owner union or deletion of minority material.
struct WaterBulkState {
    std::vector<CutCompensated> water, gas, gasMass;
    std::vector<double> faceRate, faceInertance;
    CutCompensated externalGasMass;
    std::uint64_t geometryStamp = 0;
};
struct WaterBulkGeometry {
    CutSpatialField field;
    std::vector<double> capacity, potentialBase, potentialSlope;
    std::uint64_t stamp = 0;
    double density = 0.;
    int upAxis = 1;
};
struct WaterBulkAudit {
    double water = 0., air = 0., potentialEnergy = 0., kineticEnergy = 0.;
    double maximumPartitionResidual = 0.;
};
bool PrepareWaterBulkStorage(const CutSpatialField &, double density, const GridVec &gravity,
                             WaterBulkGeometry &, WaterBulkState &, std::string &);
bool AuditWaterBulkStorage(const WaterBulkGeometry &, const WaterBulkState &, const WaterGasSettings &,
                           WaterBulkAudit &, std::string &);
} // namespace judas::volume_grid
