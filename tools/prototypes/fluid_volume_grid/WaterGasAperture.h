#pragma once
#include "WaterMaterialRegrid.h"

namespace judas::volume_grid {
// Input rectangles are extruded over [0,1] on their normal axis. They are
// integration pieces of one real opening, not individual wall-bounded pipes.
// A nonrectangular opening is represented by deterministic exact rectangular
// channels in parallel; this is a declared finite-resistance approximation.
bool MeasureWaterGasAperture(const std::vector<WaterMaterialFragment>& rectangles,
    int axis,double length,const WaterGasSettings&,double& area,double& conductance,
    std::string&);
}
