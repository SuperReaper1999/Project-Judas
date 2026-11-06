#include "CutCoupledPressure.h"
#include <iostream>
using namespace judas::volume_grid;
int main(){
 for(double h:{.1,.01,.0001,.000001,1e-9})for(auto mode:{CutCoupledLinearization::Newton,CutCoupledLinearization::Oseen}){
  CutSpatialField f;f.chart.layout={{1,1,1},1.};f.parent={0,0};f.chart.regions={{{0.,0.,0.},{1.,h,1.},0},{{0.,h,0.},{1.,1.,1.},0}};
  f.geometry.startCapacity=f.geometry.endCapacity={h,1.-h};f.geometry.faces={{0,1,1,{0.,h,0.},{1.,h,1.},1.,1.,1.,0.,.5}};
  f.flux={0.};f.water={0.,1.-h};f.gas={h,0.};f.gasMass={1.2*h,0.};CutVelocityDual dual;std::string error;
  if(!dual.Build(f,error)){std::cerr<<error;return 2;}CutDynamicSettings s;s.dt=double(1.f/60.f);CutCoupledSettings o;o.linearization=mode;CutCoupledState result;
  bool ok=AdvanceCutCoupledPressure(f,dual,s,o,result,error);
  std::cout<<"THIN h="<<h<<" newton="<<(mode==CutCoupledLinearization::Newton)<<" accepted="<<ok<<" iterations="<<result.nonlinearIterations<<" speed="<<result.momentum.maximumSpeed<<" reason="<<error<<std::endl;
 }
}
