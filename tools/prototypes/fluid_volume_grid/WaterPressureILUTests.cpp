#include "WaterPressureILU.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
int checks=0,failures=0;
void Check(bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
}
int main(){
    using Real=CutCompensated;std::string error;WaterPressureILU factor;
    std::vector<std::map<std::size_t,Real>> rows(4);
    rows[0]={{0,4.},{1,-1.}};rows[1]={{0,-2.},{1,5.},{2,-1.}};
    rows[2]={{1,-1.},{2,4.},{3,-1.}};rows[3]={{2,-1.},{3,3.}};
    Check(factor.Build(rows,error),"positive nonsymmetric pressure factor prepares");
    std::vector<double> wanted{1.,-.2,.7,3.},rhs(4),answer;
    for(std::size_t i=0;i<4;++i)for(auto e:rows[i])rhs[i]+=e.second.Value()*wanted[e.first];
    Check(factor.Apply(rhs,answer,error),"finite-air search direction solves");
    double maximum=0.;for(std::size_t i=0;i<4;++i)maximum=std::max(maximum,std::abs(answer[i]-wanted[i]));
    Check(maximum<1e-14,"tridiagonal no-fill case reproduces independent known solution");
    rows[0].erase(0);Check(!factor.Build(rows,error),"missing diagonal rejects instead of inserting a stabilizer");
    std::vector<double> again;Check(factor.Apply(rhs,again,error)&&again==answer,"failed replacement preserves previous prepared factor");
    std::vector<std::map<std::size_t,Real>> bad(2);bad[0]={{0,1.},{1,2.}};bad[1]={{0,2.},{1,1.}};
    Check(!factor.Build(bad,error),"indefinite incomplete factor rejects rather than shifting physical rows");
    Check(!factor.Apply({1.},again,error),"invalid search-vector dimensions reject");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
