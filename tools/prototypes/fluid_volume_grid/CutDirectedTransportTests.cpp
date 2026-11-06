#include "CutDirectedTransport.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 CutDirectedTransport system;std::string error;std::vector<CutCompensated> value;
 check(system.Build({1e-12,1.},{{0,1,1e10},{1,0,1e10}},{0.,0.},error),"condense stiff circulating cells without discarding storage");
 check(system.Solve({1e-12,0.},1e-30,value,error),"solve original quantity equations to strict absolute budget");
 check(value.size()==2&&value[0].Value()>=0.&&value[1].Value()>=0.,"positive physical input has nonnegative transported state");
 if(value.size()==2){auto sum=CutCompensated(1e-12)*value[0]+value[1];check(std::abs((sum-CutCompensated(1e-12)).Value())<1e-30,"closed circulation preserves independent material quantity");}
 check(system.Solve({0.,0.},1e-30,value,error)&&value[0].Value()==0.&&value[1].Value()==0.,"dry material stays absent without numerical seeding");
 check(system.Build({1.,1.},{{0,1,2.}},{0.,1.},error),"assemble directed throughflow");
 check(system.Solve({1.,0.},1e-25,value,error)&&std::abs(value[0].Value()-1./3.)<1e-15&&std::abs(value[1].Value()-1./3.)<1e-15,"match independent triangular transport solution");
 check(system.SolveFraction({1e-100,0.},1e-12,value,error)&&value[0].Value()>0.&&value[1].Value()>0.,"independent minority mass is not dropped by a fraction tolerance");
 auto previous=value;check(!system.Solve({1.},1e-25,value,error)&&value.size()==previous.size(),"invalid right hand side preserves prior output");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
