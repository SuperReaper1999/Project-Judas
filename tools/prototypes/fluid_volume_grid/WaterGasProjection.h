#pragma once
#include "WaterMimeticGeometry.h"
namespace judas::volume_grid {
struct WaterGasProjectionResult {
    std::vector<CutCompensated> head,gasAbsolutePressure,gasPressure,gasMass,gasVolume,gasLinkMass,gasVentMass,displacedGasVolume;
    std::vector<std::vector<CutCompensated>> liquidFlux,liquidHeadDifference;
    std::vector<CutCompensated> liquidReferencePressure;
    CutCompensated externalGasMass,headDatum;
    double maximumNormalizedResidual=0.,strictMinorityResidual=0.,elapsedMilliseconds=0.;
    int iterations=0,linearApplications=0,linearFallbacks=0;
};
// Coupled incompressible liquid projection + finite isothermal gas storage.
// This is a frozen-geometry pressure/displacement solve, NOT water advection
// or a native MK2 step. Original mass and volume rows gate publication.
bool ProjectWaterGasPressure(const WaterMimeticGeometry&,const WaterGasGeometry&,
    const WaterGasSettings&,WaterGasProjectionResult&,std::string&,int iterationLimit=30);
// A numerical initial guess only; quantities, geometry and momentum remain
// owned by the current input. Used while locating a conservative crossing.
bool ProjectWaterGasPressure(const WaterMimeticGeometry&,const WaterGasGeometry&,
    const WaterGasSettings&,WaterGasProjectionResult&,std::string&,
    const WaterGasProjectionResult* numericalSeed,int iterationLimit=30);
}
