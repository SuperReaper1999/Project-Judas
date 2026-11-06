#pragma once
#include "CutCoupledPressure.h"
#include "CutConservativeBalance.h"

namespace judas::volume_grid {
// One nonlinear evaluation. Its material factors, dual masses and momentum
// rows all describe this same (pressure, velocity) state. Never published.
class CutCoupledResidual {
public:
    bool Build(const CutSpatialField&,const CutVelocityDual&,const CutDynamicSettings&,
        const std::vector<CutCompensated>& sourceWater,const std::vector<CutCompensated>& sourceGas,
        const std::vector<CutCompensated>& sourceVelocity,const std::vector<CutCompensated>& pressure,
        const std::vector<CutCompensated>& velocity,CutMomentumBackend,std::string&);
    bool Tangent(const std::vector<double>& pressure,const std::vector<double>& velocity,
        std::vector<CutCompensated>& volume,std::vector<CutCompensated>& momentum,std::string&);
    CutLocalPressureResult material;
    CutDualMomentumAudit audit;
    CutDualMomentum momentum;
    std::vector<CutCompensated> volumeRows,momentumRows,rawVolume,velocity;
private:
    bool Prepare(const CutSpatialField&,const CutVelocityDual&,const CutDynamicSettings&,
        const std::vector<CutCompensated>&,const std::vector<CutCompensated>&,
        const std::vector<CutCompensated>&,const std::vector<CutCompensated>&,
        const std::vector<CutCompensated>&,CutMomentumBackend,std::string&);
    const CutSpatialField* field_=nullptr;
    const CutVelocityDual* dual_=nullptr;
    CutDynamicSettings settings_;
    CutSelectiveTransport transport_;
    std::vector<CutTransportFace> faces_;
    std::vector<CutTransportGasPort> ports_;
    std::vector<CutCompensated> massFlux_,portMass_,rho_;
};
} // namespace judas::volume_grid
