#pragma once
#include "WaterBulkStorage.h"
namespace judas::volume_grid {
struct WaterBulkProjectionStats {
    std::size_t newtonIterations = 0, linearApplications = 0;
    double maximumVolumeResidual = 0., elapsedMs = 0.;
    double sourceEnergy = 0., endEnergy = 0., inertialLoss = 0.;
    double maximumWaterReconciliation = 0., maximumMassReconciliation = 0.;
};
// Bounded frozen-inertia pressure/quantity stage, not yet a complete momentum
// transport or moving-body integrator. Water and finite air flow are coupled;
// gas has compressible storage and resistance but no gas inertial wave system.
bool ProjectWaterBulk(const WaterBulkGeometry &, const WaterBulkState &, const WaterGasSettings &,
                      WaterBulkState &, WaterBulkProjectionStats &, std::string &);
} // namespace judas::volume_grid
