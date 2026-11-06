#include "WaterGasPockets.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {using Real=CutCompensated;int checks=0,failures=0;void Check(bool a,const char* s){++checks;if(!a){++failures;std::cerr<<s<<'\n';}}}
int main(){
 std::string e;WaterGasSettings s;WaterGasResult result;
 Check(AdvanceWaterGasPockets({.6},{.25},{},{},s,result,e),"sealed compressed pocket solves");
 Check(std::abs(result.pressure[0].Value()-s.referencePressure)<1e-8&&result.mass[0].Value()==.6,"finite ideal-gas pressure retains trapped mass exactly");
 const double k=.000001,v0=.4,v1=.5,m0=.6,m1=.6;
 Check(AdvanceWaterGasPockets({m0,m1},{v0,v1},{{0,1,k}},{},s,result,e),"finite connected compartments solve");
 double a=s.referenceDensity*v0/s.referencePressure,b=s.referenceDensity*v1/s.referencePressure,g=s.dt*k;
 double r0=m0-s.referenceDensity*v0,r1=m1-s.referenceDensity*v1,det=a*b+g*(a+b);
 double p0=((b+g)*r0+g*r1)/det,p1=(g*r0+(a+g)*r1)/det;
 Check(std::abs(result.pressure[0].Value()-p0)<1e-7&&std::abs(result.pressure[1].Value()-p1)<1e-7,"independent two-by-two pressure inverse agrees");
 Check(result.pressure[0].Value()>result.pressure[1].Value()&&result.linkMass[0].Value()>0.,"finite throat moves mass down pressure without merging pockets");
 double wide=0.,tiny=0.;
 Check(RectangularGasMassConductance(.1,.01,.1,1.2,101325.,1.8e-5,wide,e)&&RectangularGasMassConductance(.1,1e-8,.1,1.2,101325.,1.8e-5,tiny,e),"actual positive micro throat retains finite positive conductance");
 Check(tiny>0.&&tiny<wide*1e-12,"thin opening resistance scales with its narrow dimension");
 Check(AdvanceWaterGasPockets({.6},{.25},{},{{0,tiny,0.}},s,result,e),"microscopic atmospheric vent remains finite");
 Check(result.pressure[0].Value()>.999999*s.referencePressure&&result.externalMass.Value()>0.,"tiny vent does not pin trapped air instantly to atmosphere");
 Check(AdvanceWaterGasPockets({.6},{.25},{},{{0,wide,0.}},s,result,e),"wide genuinely open atmospheric boundary admits finite outflow");
 Check(result.pressure[0].Value()<s.referencePressure&&result.externalMass.Value()>0.,"wide vent relaxes more than tiny throat");
 Real ledger=result.mass[0]+result.externalMass-Real(.6);
 Check(std::abs(ledger.Value())<1e-25,"external reservoir transfer participates in the physical mass ledger");
 auto saved=result;
 Check(!AdvanceWaterGasPockets({.6},{0.},{},{},s,result,e),"closing space with retained gas rejects rather than discarding material");
 Check(result.mass[0].hi==saved.mass[0].hi&&result.pressure[0].hi==saved.pressure[0].hi,"failed closure leaves old state intact");
 Check(AdvanceWaterGasPockets({.6,.6},{.25,.5},{},{},s,result,e),"disconnected gas compartments solve independently");
 Check(std::abs(result.pressure[1].Value())<1e-8&&std::abs(result.mass[1].Value()-.6)<1e-25,"compression cannot affect a disconnected pocket");
 Check(AdvanceWaterGasPockets({},{},{},{},s,result,e)&&result.mass.empty(),"no gas incurs a valid empty update");
 Check(!AdvanceWaterGasPockets({},{},{{0,1,1.}},{},s,result,e),"empty gas network cannot hide a connection");
 // Isothermal free energy is convex in volume. End-pressure work must
 // cover compression free energy; venting to the reference reservoir is passive.
 auto freeEnergy=[&](double mass,double volume){return s.referencePressure/s.referenceDensity*mass*(std::log(mass/(s.referenceDensity*volume))-1.)+s.referencePressure*volume;};
 Check(AdvanceWaterGasPockets({.6},{.25},{},{},s,result,e),"sealed work control prepares");
 double work=result.pressure[0].Value()*(.5-.25),change=freeEnergy(.6,.25)-freeEnergy(.6,.5);
 Check(change>0.&&work>=change,"finite gas compression has positive stored energy and nonnegative implicit work loss");
 Check(AdvanceWaterGasPockets({.6},{.25},{},{{0,wide,0.}},s,result,e),"passive vent work control prepares");
 Check(freeEnergy(result.mass[0].Value(),.25)<=freeEnergy(.6,.25),"finite reference-reservoir exchange cannot create gas free energy");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
