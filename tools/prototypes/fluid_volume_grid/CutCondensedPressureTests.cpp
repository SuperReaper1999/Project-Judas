#include "CutCondensedPressure.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 auto tiny=CutCompensated(1e15)+CutCompensated(1e-15)-CutCompensated(1e15);
 check(std::abs(tiny.Value()-1e-15)<1e-30,"compensated addition retains small independent quantity");
 std::string error;
 for(bool eliminate:{false,true}){CutCondensedResult out;
  check(SolveCutCondensedPressure({1.,0.,0.},{{0,1,1e10},{1,2,1e-10}},{1.,1e-12,-1e-12},{1e-25,1e-29,1e-29},{false,eliminate,eliminate},out,error),"solve original constrained rows across twenty conductance decades");
  check(out.maximumNormalizedResidual<=1.,"every original row including eliminated rows passes");
  check(out.pressure.size()==3&&std::abs(out.pressure[0].Value()-1.)<1e-14&&std::abs(out.pressure[2].Value()-.99)<1e-14,"independent analytic pressure profile is retained");
 }
 CutCondensedResult out;
 check(SolveCutCondensedPressure({1.,0.},{{0,1,1e-5}},{1.,1e-25},{1e-25,1e-35},{false,true},out,error),"recover sub-ULP pressure differences as physical flux");
 check(out.flux.size()==1&&std::abs(out.flux[0].Value()+1e-25)<1e-35,"small physical flux survives large common pressure");
 double rounded=out.pressure.size()==2?1e-5*(out.pressure[0].Value()-out.pressure[1].Value()):0.;
 check(std::abs(rounded+1e-25)>1e-35,"negative control detects premature pressure rounding");
 out.pressure={CutCompensated(-7.)};
 check(!SolveCutCondensedPressure({1.,0.},{{0,1,1e10}},{1.-1e-15,1e-15},{1e-25,1e-29},{false,true},out,error)&&out.pressure.size()==1&&out.pressure[0].Value()==-7.,"precision-exhausted high-conductance case rejects without publishing");
 check(!SolveCutCondensedPressure({-1.},{},{0.},{1e-10},{false},out,error)&&out.pressure[0].Value()==-7.,"invalid storage rejects without publishing pressure");
 check(!SolveCutCondensedPressure({0.,0.},{{0,1,1.}},{1.,0.},{1e-10,1e-10},{true,false},out,error),"incompatible sealed volume source rejects");
 check(!SolveCutCondensedPressure({1.},{},{std::numeric_limits<double>::quiet_NaN()},{1e-10},{false},out,error),"nonfinite source rejects");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
