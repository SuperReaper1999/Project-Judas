#include "CutDualLinear.h"
#include <iostream>
#include <cmath>
#include <limits>
using namespace judas::volume_grid;
int main(){
    int checks=0,failed=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::cerr<<name<<'\n';}};
    using Real=CutCompensated;std::string error;CutDualLinear solver;std::vector<Real> result;
    check(solver.Build({2.,2.,2.},{{0,1,1.},{1,2,1.},{2,0,1.}},error),"prepare signed momentum cycle");
    for(double scale:{1e-200,1.,1e200}){
        check(solver.Solve({scale,0.,0.},result,error),"solve across RHS exponents without changing rows");
        for(int i=0;i<3;++i){Real exact=Real(scale)*Real(i==0?4.:(i==1?2.:1.))/Real(7.);
            check(std::abs(((result[i]-exact)/Real(scale)).Value())<1e-24,"independent cyclic inverse to compensated precision");}
    }
    CutDirectedTransport reference;std::vector<Real> oracle;
    check(reference.Build({1.,1.,1.},{{0,1,1.},{1,2,1.},{2,0,1.}},std::vector<Real>(3),error)&&reference.SolveFraction({1.,-2.,3.},1e-26,oracle,error)&&solver.Solve({1.,-2.,3.},result,error),"same operator solved by retained compensated reference");
    for(int i=0;i<3;++i)check(std::abs((oracle[i]-result[i]).Value())<1e-24,"reference momentum comparison");
    auto saved=result;check(!solver.Build({-1.},{},error)&&solver.Solve({1.,-2.,3.},result,error),"failed factor replacement preserves prior operator");
    check(!solver.Solve({std::numeric_limits<double>::quiet_NaN(),0.,0.},result,error)&&result[0].hi==saved[0].hi,"NaN rejection preserves publication");
    check(solver.Build({1.,1.},{{0,1,.25},{0,1,.75}},error)&&solver.Solve({2.,-1.},result,error),"duplicate incoming routes assembled without loss");
    check(std::abs((result[0]-Real(2.)).Value())<1e-24&&std::abs((result[1]-Real(1.)).Value())<1e-24,"independent triangular momentum rows");
    std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}
