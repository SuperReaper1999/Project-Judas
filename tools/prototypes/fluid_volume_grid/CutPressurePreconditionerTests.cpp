#include "CutPressurePreconditioner.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 int checks=0,failures=0;std::string error;auto check=[&](bool ok,const char* name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 for(bool condense:{false,true}){
  CutPressurePreconditioner prepared;std::vector<double> p;
  check(prepared.Build({1.,1.},{{0,1,2.}},{condense,condense},error),"prepare positive pressure accelerator");
  check(prepared.Apply({1.,-1.},p,error)&&std::abs(p[0]-.2)<1e-12&&std::abs(p[1]+.2)<1e-12,"match independently inverted two-row operator");
  check(prepared.Apply({0.,1.},p,error)&&std::abs(p[0]-.4)<1e-12&&std::abs(p[1]-.6)<1e-12,"reuse factorization with a different source");
  check(prepared.Apply({0.,0.},p,error)&&p[0]==0.&&p[1]==0.,"zero source returns zero correction even with empty reduced graph");
  check(!prepared.Build({-1.},{},{false},error)&&prepared.Apply({1.,-1.},p,error)&&std::abs(p[0]-.2)<1e-12,"failed preparation preserves previous operator");
  auto prior=p;check(!prepared.Apply({1.},p,error)&&p==prior,"invalid source preserves previous correction");
 }
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
