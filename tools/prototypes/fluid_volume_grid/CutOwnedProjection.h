#pragma once

#include "CutConnectedOwners.h"
#include "CutDynamicTransport.h"
#include "CutHydroacousticGraph.h"
#include "CutOwnedFlux.h"

namespace judas::volume_grid {

struct CutOwnedProjection {
    std::vector<double> pressure, water, gasMass;
    std::vector<double> faceVolume, waterFlux;
    std::vector<CutBoundaryTransfer> ventVolume;
    double externalGasMass = 0., residual = 0.;
    int iterations = 0;
};

// Restrict the existing pressure graph to connected material owners (P^T A P),
// retaining original boundary quadrature, phase footprints and finite vents.
// This is a projection/transfer-admission gate, NOT a complete time step:
// publishing an evolved spatial partition and native momentum is separate.
// Failure preserves the previous proposal. diagnosticTrial optionally captures
// an unpublished closed-pressure candidate even if transport admission fails;
// it must not alias output and is never a physical-state publication.
bool ProjectCutOwnedMaterial(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const CutConnectedOwners& owners, const CutDynamicState& source,
                              const CutDynamicSettings& settings, const std::vector<CutRegionGasVent>& vents,
                              CutOwnedProjection& output, std::string& error,
                              CutHydroacousticGraphFailure* pressureFailure = nullptr,
                              CutOwnedProjection* diagnosticTrial = nullptr);

} // namespace judas::volume_grid
