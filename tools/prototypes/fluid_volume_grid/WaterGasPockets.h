#pragma once
#include "CutCondensedPressure.h"
namespace judas::volume_grid {
// Water V2's isothermal pressure compartments. These links transmit finite
// mass; a positive opening NEVER merges owners or pins a whole pocket to air.
struct WaterGasLink { std::size_t left,right; double massConductance; };
struct WaterGasVent { std::size_t node; double massConductance, gaugePressure=0.; };
struct WaterGasSettings {
    double dt=1./60.,referenceDensity=1.2,referencePressure=101325.;
    double massTolerance=1.2e-14,relativeTolerance=1e-10;
    bool trace=false;
};
struct WaterGasResult {
    std::vector<CutCompensated> pressure,mass,linkMass,ventMass;
    CutCompensated externalMass;
    double maximumMassResidual=0.,maximumNormalizedResidual=0.;
};
// Capacities are supplied by actual water/solid displacement. Source mass is
// independently owned. A closed zero-volume pocket with remaining mass rejects.
// No gas velocity, gas wave, thermal evolution or water state is synthesized.
bool AdvanceWaterGasPockets(const std::vector<CutCompensated>& sourceMass,
    const std::vector<CutCompensated>& endGasVolume,
    const std::vector<WaterGasLink>&,const std::vector<WaterGasVent>&,
    const WaterGasSettings&,WaterGasResult&,std::string&);
// Series combination of viscous rectangular-duct resistance and an acoustic
// relaxation resistance. A reduced, passive gas model, not a dynamic-air solver.
bool RectangularGasMassConductance(double width,double height,double length,
    double density,double pressure,double viscosity,double& result,std::string&);
}
