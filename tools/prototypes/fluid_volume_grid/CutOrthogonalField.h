#pragma once
#include "CutSpatialTransport.h"

namespace judas::volume_grid {
struct CutOrthogonalRemap {
    std::size_t originalCells=0,orthogonalCells=0;
    double waterError=0.,gasMassError=0.;
    std::array<std::size_t,3> intervals{};
};
// Frozen, rectilinear prototype compiler. It retains all input cut planes
// and source minority phases; it does not silently coarsen or delete slivers.
// This is initial-state cooking, not a moving-solid or runtime-state remap.
bool BuildCutOrthogonalField(const CutSpatialField&,CutSpatialField&,
    CutOrthogonalRemap&,std::string&,std::size_t cellBudget=262144);
} // namespace judas::volume_grid
