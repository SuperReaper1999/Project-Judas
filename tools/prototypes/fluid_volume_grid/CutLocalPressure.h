#pragma once
#include "CutCondensedPressure.h"
#include "CutSpatialTransport.h"
namespace judas::volume_grid {
struct CutLocalPressureResult {
 std::vector<CutCompensated> pressure,water,gasMass,flux;
 CutCompensated externalWater,externalGasMass;
 double maximumVolumeResidual=0.,maximumNormalizedResidual=0.;
 int iterations=0;
 double transportMilliseconds=0.,tangentMilliseconds=0.,linearMilliseconds=0.;
 int transportEvaluations=0,linearApplications=0;
};
// Frozen-geometry, local-volume constrained prototype. Independent water
// quantity and gas mass remain spatial fields. No output on failed admission.
bool AdvanceCutLocalPressure(const CutSpatialField&,const CutDynamicSettings&,
 const std::vector<double>& pressureSeed,CutLocalPressureResult&,std::string&);
}
