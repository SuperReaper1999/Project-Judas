#pragma once
#include "MovingGridGeometry.h"
#include "CutCondensedPressure.h"
namespace judas::volume_grid {
struct WaterCellPatch {
    int axis=0,sign=1;
    double area=0.;
    GridVec offset{}; // face centroid MINUS water centroid, already in local metres
    bool fixed=false;
};
struct WaterCellProjection {
    std::vector<std::size_t> free;
    std::size_t reference=0; // eliminated integrated flux, q_ref = -sum(q_other)
    std::vector<CutCompensated> mobility; // row-major free patch integrated-flux response H
    std::vector<double> gravityHead; // local rho*g dot face-centroid offset
    std::vector<double> predictor; // original free volume flux, m^3/s
    std::vector<double> mass; // full patch normal-velocity mass metric
    GridVec sourceVelocity{};
};
// Constant-density liquid only. A compatible symmetric positive mass metric
// acts on pressure HEAD (p - rho*g dot x), preserving hydrostatic balance
// before matrix products rather than subtracting large rounded forces. Fixed solid
// normal velocities are excluded before projection; no gas inertial mass.
bool PrepareWaterMimeticCell(double waterVolume,double density,double dt,
    const GridVec& gravity,const std::vector<WaterCellPatch>&,
    WaterCellProjection&,std::string&,const GridVec& sourceVelocity={});
}
