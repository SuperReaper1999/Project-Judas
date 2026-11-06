#pragma once
#include "CutCondensedPressure.h"
#include "CutSpatialTransport.h"
#include <cstdint>
namespace judas::volume_grid {
enum class CutLocalTransport { Reference, Fast };
struct CutLocalPressureResult {
 std::vector<CutCompensated> pressure,water,gasMass,flux,portFlux;
 double dt=0.,waterDensity=0.,gasDensity=0.,soundSpeed=0.;
 std::uint64_t frozenGeometryStamp=0;
 CutCompensated externalWater,externalGasMass;
 double maximumVolumeResidual=0.,maximumNormalizedResidual=0.;
 int iterations=0;
 double transportMilliseconds=0.,tangentMilliseconds=0.,linearMilliseconds=0.;
 int transportEvaluations=0,linearApplications=0,rejectedTransportTrials=0;
 std::size_t fastSolves=0,fastCorrections=0,fastSweeps=0,fastFallbacks=0;
};
// Frozen-geometry, local-volume constrained prototype. Independent water
// quantity and gas mass remain spatial fields. No output on failed admission.
// Optional continuation retains normal face velocity on the SAME frozen chart.
// It is a staggered acoustic/transport experiment, not qualified momentum advection.
bool AdvanceCutLocalPressure(const CutSpatialField&,const CutDynamicSettings&,
 const std::vector<double>& pressureSeed,CutLocalPressureResult&,std::string&,
 CutLocalTransport transport = CutLocalTransport::Reference,
 const CutLocalPressureResult* previous = nullptr);
}
