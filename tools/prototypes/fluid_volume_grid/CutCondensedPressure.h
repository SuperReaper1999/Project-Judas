#pragma once
#include "CutHydroacousticGraph.h"
#include <cmath>
namespace judas::volume_grid {
struct CutCompensated {
 double hi=0.,lo=0.;
 CutCompensated()=default;
 CutCompensated(double value):hi(value){}
 double Value()const{return hi+lo;}
};
// Keep these small value operations visible to hot-loop callers. The
// arithmetic sequence is unchanged from the pinned slow reference.
inline CutCompensated NormalizeCutCompensated(double a,double b){CutCompensated r;r.hi=a+b;r.lo=b-(r.hi-a);return r;}
inline CutCompensated operator+(CutCompensated a,CutCompensated b){double s=a.hi+b.hi,v=s-a.hi;double e=(a.hi-(s-v))+(b.hi-v);return NormalizeCutCompensated(s,e+a.lo+b.lo);}
inline CutCompensated operator-(CutCompensated a,CutCompensated b){b.hi=-b.hi;b.lo=-b.lo;return a+b;}
inline CutCompensated operator*(CutCompensated a,CutCompensated b){double p=a.hi*b.hi;double e=std::fma(a.hi,b.hi,-p)+a.hi*b.lo+a.lo*b.hi;return NormalizeCutCompensated(p,e+a.lo*b.lo);}
inline CutCompensated operator/(CutCompensated a,CutCompensated b){double q=a.hi/b.hi;auto r=a-b*CutCompensated(q);double q2=r.hi/b.hi;auto result=CutCompensated(q)+CutCompensated(q2);r=a-b*result;return result+CutCompensated(r.hi/b.hi);}
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
