#pragma once
#include "CutHydroacousticGraph.h"
namespace judas::volume_grid {
struct CutCompensated {
 double hi=0.,lo=0.;
 CutCompensated()=default;
 CutCompensated(double value):hi(value){}
 double Value()const{return hi+lo;}
};
CutCompensated operator+(CutCompensated,CutCompensated);
CutCompensated operator-(CutCompensated,CutCompensated);
CutCompensated operator*(CutCompensated,CutCompensated);
CutCompensated operator/(CutCompensated,CutCompensated);
struct CutCondensedResult {
 std::vector<CutCompensated> pressure,flux;
 double maximumNormalizedResidual=0.;
 std::size_t eliminated=0,reduced=0;
 int refinements=0;
};
// Condensation is an algebraic preconditioner only. All original equations,
// including eliminated nodes, must pass their unchanged tolerances.
bool SolveCutCondensedPressure(const std::vector<double>& storage,
 const std::vector<CutHydroacousticGraphLink>& links,const std::vector<double>& source,
 const std::vector<double>& rowTolerance,const std::vector<bool>& condense,
 CutCondensedResult&,std::string&);
}
