#pragma once
#include "WaterMimeticCell.h"
#include "CutCondensedPressure.h"
namespace judas::volume_grid {
struct WaterCellAudit {
    GridVec meanVelocity{},wallReaction{};
    double centrePressure=0.,beforeEnergy=0.,afterEnergy=0.,implicitLoss=0.;
    double work=0.,workResidual=0.,maximumMomentumResidual=0.;
    std::vector<double> wallPressure;
};
// Independent full-mass momentum/work audit, including fixed-wall reaction.
// Physical face pressures are supplied in free-patch order; fluxes are outward.
bool AuditWaterCell(double volume,double density,double dt,const GridVec& gravity,
    const std::vector<WaterCellPatch>&,const WaterCellProjection&,
    const std::vector<CutCompensated>& flux,const std::vector<CutCompensated>& pressure,
    WaterCellAudit&,std::string&,bool relativeHead=false,
    CutCompensated physicalHeadOffset={});
}
