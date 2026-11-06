#pragma once
#include "CutPhaseFronts.h"
namespace judas::volume_grid {
struct CutSpatialField {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutConnectedOwners fragments;
    std::vector<std::size_t> parent, originalFace;
    std::vector<double> water, gas, gasMass, flux, mobility;
    std::vector<CutPhaseBoundary> boundaries;
};
struct CutSpatialResult {
    std::vector<double> water, gasMass, fraction, ownerWater, ownerGasMass;
    double externalWater=0., externalGasMass=0., residual=0., maximumFraction=0.;
    std::size_t iterations=0;
};
// Frozen geometry diagnostic. Subdivide at aperture edges and midplanes;
// preserve actual cut geometry and initial material placement. No owner mixing.
bool BuildCutSpatialField(const CutRegionChart&,const CutRegionGeometry&,
                         const CutConnectedOwners&,const CutDynamicState&,
                         const std::vector<double>& faceFlux,
                         const std::vector<CutPhaseBoundary>&,
                         CutSpatialField&,std::string&);
// Conservative backward-Euler donor-cell transport on the spatial field.
// Water quantity and gas mass are advected independently. Pressure acts
// through the supplied flux, not a density knob. This is NOT a pressure/EOS
// timestep; candidates outside capacity or ledger bounds reject.
bool AdvanceCutSpatial(const CutSpatialField&,const CutConnectedOwners&,
                       double fraction,
                       CutSpatialResult&,std::string&, CutSpatialResult* diagnostic=nullptr);
}
