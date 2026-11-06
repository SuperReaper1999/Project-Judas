#pragma once
#include "CutCondensedPressure.h"
#include <functional>
namespace judas::volume_grid {
using CutKrylovOperator=std::function<bool(const std::vector<double>&,std::vector<double>&)>;
struct CutKrylovStats {int applications=0,restarts=0;double relativeResidual=1.;};
// Flexible right preconditioning. Every cycle verifies the ORIGINAL J*x residual;
// no Newton correction is returned as solved merely from a Hessenberg estimate.
bool SolveCutCoupledKrylov(const std::vector<double>& rhs,const CutKrylovOperator&,
 const CutKrylovOperator& precondition,std::vector<CutCompensated>&,CutKrylovStats&,
 std::string&,int width=40,int cycles=8,double relativeTolerance=.005);
}
