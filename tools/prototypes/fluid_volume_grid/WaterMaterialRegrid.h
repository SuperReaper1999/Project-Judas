#pragma once
#include "WaterMaterialFragments.h"

namespace judas::volume_grid {
// Geometric union only; payload is not interpreted. Used for volume pieces
// and unit-extruded planar apertures so both share one exact tiling algorithm.
bool CanonicalWaterMaterialBoxes(const std::vector<WaterMaterialFragment>&,
    std::vector<WaterMaterialFragment>&,std::string&,std::size_t workBudget=10000000);
struct WaterMaterialRegridStats {
    std::size_t piecesBefore=0, piecesAfter=0, components=0, work=0;
    std::size_t maximumCellBefore=0, maximumCellAfter=0;
    CutCompensated volumeError, momentError, airMassError, momentumError;
    double mixingKineticPerDensityLoss=0., gasMixingPotentialLoss=0., milliseconds=0.;
};
// Work-bounded exact cell-local retiling. It does not approximate unresolved
// interfaces or guarantee a fixed bound on intrinsic topology. It removes subdivision
// history, not physical interfaces. Different gas carriers are never unioned.
// Water mean momentum is the same connected-cell average used by the pressure
// compiler; its passive mixing loss is measured. Geometry and first moments
// are unchanged. A work-budget/geometry failure leaves output untouched.
bool RegridWaterMaterial(const CutSpatialField&, const WaterMaterialState&,
    WaterMaterialState&, WaterMaterialRegridStats&, std::string&,
    std::size_t workBudget=10000000);
}
