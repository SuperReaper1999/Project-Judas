#pragma once
#include "CutConnectedOwners.h"

namespace judas::volume_grid {
struct CutBoundaryTransfer { std::size_t fragment; double volume; };
struct CutOwnedFlux {
    std::vector<double> faceVolume, divergence;
    double maximumResidual = 0., maximumBoundaryCorrection = 0.;
};
// Reconstruct internal fluxes with minimum change in the retained kinetic
// metric. A constrained projection first corrects proposed external fluxes
// at pure-water owners. Compressibility is assigned only to
// represented gas; full-water fragments have zero physical divergence.
bool ReconstructCutOwnedFlux(const CutRegionChart&, const CutRegionGeometry&,
                             const CutConnectedOwners&, const std::vector<double>& external,
                             const std::vector<CutBoundaryTransfer>& vents,
                             const std::vector<double>& gasVolume,
                             const std::vector<double>& predictor,
                             const std::vector<double>& mobility,
                             CutOwnedFlux&, std::string& error,
                             double relativePressureResidual = 1e-10, bool projectExternal = true,
                             const std::vector<double>* ownerSources = nullptr,
                             const std::vector<double>* fragmentSources = nullptr);
}
