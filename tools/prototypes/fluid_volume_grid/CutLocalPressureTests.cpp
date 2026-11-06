#include "CutLocalPressure.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 for(bool water:{false,true})for(double density:{1.2,1.5}){
  CutSpatialField field;std::string error;
  check(PrepareCutRegionChart({{3,1,1},1.},{},{},{},{},field.chart,error)&&BuildCutRegionGeometry(field.chart,{},field.geometry,error),"compile local pressure channel");
  field.parent={0,1,2};field.flux.resize(field.geometry.faces.size());field.water.assign(3,water?1.:0.);field.gas.assign(3,water?0.:1.);field.gasMass.assign(3,water?0.:density);
  if(!water)field.boundaries={{2,0,1,0.,0.,true}};
  CutDynamicSettings settings;settings.dt=.01;settings.gasDensity=density;settings.gravity={-9.81,0.,0.};settings.iterationLimit=20;
  CutLocalPressureResult result;bool ok=AdvanceCutLocalPressure(field,settings,{},result,error);check(ok,water?"closed incompressible water reaches local volume closure":"gas reaches local EOS closure");
  if(ok){check(result.maximumNormalizedResidual<=1.,"original cell volume tolerances pass");double sum=0.;for(auto w:result.water)sum+=w.Value();check(std::abs(sum-(water?3.:0.))<1e-10,"water quantity is conserved");}
  else std::cerr<<error<<'\n';
 }
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
