#pragma once

#include "CutConnectedOwners.h"
#include "CutDynamicTransport.h"
#include "CutHydroacousticGraph.h"

namespace judas::volume_grid {

struct CutOwnedProjection {
    std::vector<double> pressure, water, gasMass;
    std::vector<double> faceVolume, waterFlux;
    double externalGasMass = 0., residual = 0.;
    int iterations = 0;
};

// Restrict the existing pressure graph to connected material owners (P^T A P),
// retaining original boundary quadrature, phase footprints and finite vents.
// This is a projection/transfer-admission gate, NOT a complete time step:
// publishing an evolved spatial partition and native momentum is separate.
// Failure preserves the previous proposal.
bool ProjectCutOwnedMaterial(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const CutConnectedOwners& owners, const CutDynamicState& source,
                              const CutDynamicSettings& settings, const std::vector<CutRegionGasVent>& vents,
                              CutOwnedProjection& output, std::string& error,
                              CutHydroacousticGraphFailure* pressureFailure = nullptr);

} // namespace judas::volume_grid
