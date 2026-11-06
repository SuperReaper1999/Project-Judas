#pragma once
#include "CutDirectedTransport.h"
namespace judas::volume_grid {
struct CutResidualInterval {double lo=0.,hi=0.;};
// Error-free sums/products of the represented binary64 coefficients and
// compensated state. Subnormal product roundoff is enclosed explicitly;
// overflow and nonfinite input reject rather than claiming an exact audit.
bool BoundCutDirectedResidual(const std::vector<double>& capacity,
 const std::vector<CutDirectedEdge>&,const std::vector<CutCompensated>& exterior,
 const std::vector<CutCompensated>& rhs,const std::vector<CutCompensated>& state,
 std::vector<CutResidualInterval>&,std::string&);
}
