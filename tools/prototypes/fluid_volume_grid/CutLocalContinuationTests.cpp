#include "CutLocalPressure.h"
#include <cmath>
#include <iostream>

using namespace judas::volume_grid;
int main(){
 int checks=0,failures=0;
 auto check=[&](bool ok,const char* name){++checks;failures+=!ok;if(!ok)std::cerr<<"FAIL "<<name<<'\n';};
 for(bool water:{false,true}){
  CutSpatialField field;std::string error;
  check(PrepareCutRegionChart({{3,1,1},1.},{},{},{},{},field.chart,error)&&BuildCutRegionGeometry(field.chart,{},field.geometry,error),"compile unchanged frozen channel");
  field.parent={0,1,2};field.flux.resize(field.geometry.faces.size());field.water.assign(3,water?1.:0.);field.gas.assign(3,water?0.:1.);field.gasMass.assign(3,water?0.:1.2);
  CutDynamicSettings settings;settings.dt=.01;settings.gravity={-9.81,0.,0.};settings.iterationLimit=30;
  CutLocalPressureResult current;
  bool ok=AdvanceCutLocalPressure(field,settings,{},current,error,CutLocalTransport::Fast);
  check(ok,"initial pressure interval admits");
  for(int step=1;ok&&step<500;++step){
   CutLocalPressureResult next;
   ok=AdvanceCutLocalPressure(field,settings,{},next,error,CutLocalTransport::Fast,&current);
   check(ok,"retain material and velocity across repeated intervals");
   if(!ok){std::cerr<<error<<'\n';break;}
   CutCompensated totalW,totalM;
   for(std::size_t i=0;i<3;++i){totalW=totalW+next.water[i];totalM=totalM+next.gasMass[i];}
   check(std::abs((totalW-CutCompensated(water?3.:0.)).Value())<1e-13&&std::abs((totalM-CutCompensated(water?0.:3.6)).Value())<1e-13,"cold source quantities are not silently restored each interval");
   if(water){double maximum=0.;for(auto q:next.flux)maximum=std::max(maximum,std::abs(q.Value()));check(maximum<1e-12,"closed hydrostatic water remains at rest");}
   current=std::move(next);
  }
  if(ok){
   auto previous=current;
   check(!AdvanceCutLocalPressure(field,settings,{},current,error,CutLocalTransport::Fast,&current)&&current.water[0].hi==previous.water[0].hi,"aliased continuation rejects transactionally");
   CutLocalPressureResult rejected;rejected.water={-7.};
   auto changed=field;changed.chart.regions[0].hi[0]+=.001;
   check(!AdvanceCutLocalPressure(changed,settings,{},rejected,error,CutLocalTransport::Fast,&current)&&rejected.water[0].Value()==-7.,"changed frozen chart rejects without replacing state");
   auto changedLaw=settings;changedLaw.gasDensity=1.5;
   check(!AdvanceCutLocalPressure(field,changedLaw,{},rejected,error,CutLocalTransport::Fast,&current),"changed material law is not mistaken for unchanged continuation");
   auto moving=field;moving.geometry.motion=BoxMotion{};moving.geometry.motion->translation={.001,0.,0.};
   check(!AdvanceCutLocalPressure(moving,settings,{},rejected,error,CutLocalTransport::Fast,&current),"unsupported solid motion remains explicit rejection");
  }
 }
 {
  CutSpatialField field;std::string error;
  check(PrepareCutRegionChart({{3,1,1},1.},{},{},{},{},field.chart,error)&&BuildCutRegionGeometry(field.chart,{},field.geometry,error),"compile uniform throughflow control");
  field.parent={0,1,2};field.flux.resize(field.geometry.faces.size());field.water.assign(3,0.);field.gas.assign(3,1.);field.gasMass.assign(3,1.2);field.boundaries={{0,0,-1,0.,0.,true},{2,0,1,0.,0.,true}};
  CutDynamicSettings settings;settings.dt=.01;settings.gravity={0.,0.,0.};
  CutLocalPressureResult previous,next;
  bool prepared=AdvanceCutLocalPressure(field,settings,{},previous,error,CutLocalTransport::Fast);
  check(prepared,"prepare uniform pressure state");
  if(prepared){
   for(auto& q:previous.flux)q=.001;
   previous.portFlux={-.001,.001};
   bool ok=AdvanceCutLocalPressure(field,settings,{},next,error,CutLocalTransport::Fast,&previous);
   check(ok,"uniform flow continues without resetting its velocity");
   bool retained=ok;for(auto q:next.flux)retained=retained&&std::abs(q.Value()-.001)<1e-15;
   check(retained,"known uniform face speed is retained");
  }
 }
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
