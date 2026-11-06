#include "CutExactResidual.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 double q=std::ldexp(1.,50),small=std::ldexp(1.,-100);CutCompensated state(1.);state.lo=small;
 std::vector<CutResidualInterval> out;std::string error;
 check(BoundCutDirectedResidual({0.,0.},{{0,1,q}},{0.,0.},{q,-q},{state,0.},out,error),"bound cancellation in represented compensated state");
 double residual=std::ldexp(1.,-50);
 check(out.size()==2&&out[0].lo<=-residual&&out[0].hi>=-residual&&out[1].lo<=residual&&out[1].hi>=residual,"expansion enclosure contains independent exact binary answer");
 check(out[0].hi<0.&&out[1].lo>0.,"audit does not falsely report zero after large cancellation");
 check(BoundCutDirectedResidual({0.,0.},{{0,1,std::ldexp(1.,-800)}},{0.,0.},{0.,0.},{std::ldexp(1.,-400),0.},out,error),"subnormal product uncertainty is enclosed");
 check(out[1].lo<=0.&&out[1].hi>0.,"underflow does not claim an exact zero residual");
 auto sentinel=out;check(!BoundCutDirectedResidual({1.},{},{0.},{0.},{std::numeric_limits<double>::infinity()},out,error)&&out.size()==sentinel.size(),"nonfinite data rejects transactionally");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
