#pragma once
#include "CutOrthogonalField.h"
namespace judas::volume_grid {
struct CutTransparentGridReport {
    std::size_t inputCells=0,outputCells=0,removedPlanes=0;
    double waterError=0.,gasMassError=0.,minimumCapacityBefore=0.,minimumCapacityAfter=0.;
    GridVec waterFirstMomentChange{},gasFirstMomentChange{};
};
// Initial-state finite-volume projection only. Remove close algorithmic planes,
// never a plane carrying ANY fluid/solid boundary. Geometry and apertures remain
// exact; material is conservatively averaged within the resulting bounded cells.
// Existing velocity states and partial/heterogeneous exterior ports reject.
bool CoarsenCutTransparentGrid(const CutSpatialField&,CutSpatialField&,
    CutTransparentGridReport&,std::string&,double minimumSpacingFraction=.25);
}
