#include "CutSelectiveTransport.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 int checks=0,failures=0;using R=CutCompensated;std::string error;
 auto check=[&](bool ok,const char* name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 CutSelectiveTransport system;
 // Ask for the strict budget used by this known-answer ledger check.
 bool ok=system.Build({1.},{},{{0,.1}},{.5},{.6},1.2,1e-28,error);check(ok,"part-filled cell prepares selective gas outflow");
 if(ok){
  check(system.Water()[0].Value()==.5,"gas-only outlet removes no water");
  check(std::abs(system.Mass()[0].Value()-.5)<1e-15,"gas transport uses gas density rather than mixture dilution");
  check(std::abs((system.Mass()[0]+system.GasPortMass()[0]-R(.6)).Value())<1e-28,"gas mass including exterior flux is conserved");
  std::vector<R> dw,dm;check(system.Tangent({}, {1.},dw,dm,error)&&dw[0].Value()==0.&&std::abs(dm[0].Value()+5./6.)<1e-12,"known one-cell outlet tangent");
 }
 check(system.Build({1.},{},{{0,-.1}},{.5},{.6},1.2,1e-14,error)&&std::abs(system.Mass()[0].Value()-.72)<1e-15,"gas inflow retains ambient density and independent water");
 std::vector<double> c{1.,1.};std::vector<R> w{.8,.2},m{.24,.96};std::vector<CutTransportFace> f{{0,1,.1}};std::vector<CutTransportGasPort> p{{1,.07}};
 check(system.Build(c,f,p,w,m,1.2,1e-14,error),"coupled two-cell selective operator prepares");
 std::vector<R> dw,dm;ok=system.Tangent({1.},{.25},dw,dm,error);check(ok,"water-dependent gas-density tangent solves");
 CutSelectiveTransport plus,minus;double epsilon=1e-6;auto pf=f,mf=f;auto pp=p,mp=p;pf[0].volume=.1+epsilon;mf[0].volume=.1-epsilon;pp[0].volume=.07+.25*epsilon;mp[0].volume=.07-.25*epsilon;
 ok=ok&&plus.Build(c,pf,pp,w,m,1.2,1e-14,error)&&minus.Build(c,mf,mp,w,m,1.2,1e-14,error);
 for(std::size_t i=0;ok&&i<2;++i)ok=std::abs((plus.Water()[i].Value()-minus.Water()[i].Value())/(2*epsilon)-dw[i].Value())<1e-8&&std::abs((plus.Mass()[i].Value()-minus.Mass()[i].Value())/(2*epsilon)-dm[i].Value())<1e-8;
 check(ok,"full selective tangent agrees with finite differences");
 R totalW,totalM;for(auto x:system.Water())totalW=totalW+x;for(auto x:system.Mass())totalM=totalM+x;for(auto x:system.GasPortMass())totalM=totalM+x;
 check(std::abs((totalW-R(1.)).Value())<1e-14&&std::abs((totalM-R(1.2)).Value())<1e-14,"two-cell water and exterior gas ledgers close");
 auto previous=system.Water();check(!system.Build({1.},{},{{0,.1}},{1.},{0.},1.2,1e-14,error)&&system.Water()[0].hi==previous[0].hi,"missing outlet gas capacity rejects without replacing prepared evaluation");
 check(!system.Tangent({1.},{.25},dw,dw,error),"aliased tangent outputs reject instead of overwriting a phase");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
