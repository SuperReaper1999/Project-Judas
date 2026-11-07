#pragma once
#include "WaterBulkStorage.h"
namespace judas::volume_grid {
struct WaterBulkMassReceipt {
    std::size_t cell;
    CutCompensated outward;
};
struct WaterBulkReconcileStats {
    double maximumWaterCorrection = 0., maximumMassCorrection = 0.;
    bool needsRefinement = false;
};
// Reconcile a converged local EOS reconstruction with independent component
// material ledgers. Only solver-sized defects may move, in the same connected
// component. This never clips a negative quantity or fills a sealed air pocket.
bool ReconcileWaterBulk(const WaterBulkGeometry &, const WaterBulkState &source,
                        const std::vector<WaterGasLink> &waterConnectivity,
                        const std::vector<WaterGasLink> &gasConnectivity,
                        const std::vector<WaterBulkMassReceipt> &, const WaterGasSettings &,
                        WaterBulkState &, WaterBulkReconcileStats &, std::string &);
} // namespace judas::volume_grid
