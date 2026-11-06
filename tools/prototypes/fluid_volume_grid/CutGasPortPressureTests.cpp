#include "CutLocalPressure.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 int checks=0,failures=0;std::string error;auto check=[&](bool ok,const char* name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 CutSpatialField field;check(PrepareCutRegionChart({{1,1,1},1.},{},{},{},{},field.chart,error)&&BuildCutRegionGeometry(field.chart,{},field.geometry,error),"compile one-cell gas outlet");
 field.parent={0};field.water={.5};field.gas={.5};field.gasMass={.6};field.boundaries={{0,1,1,0.,0.,true}};
 CutDynamicSettings settings;settings.dt=.01;settings.gravity={0.,9.81,0.};settings.iterationLimit=30;
 CutLocalPressureResult result;bool ok=AdvanceCutLocalPressure(field,settings,{},result,error,CutLocalTransport::Fast);check(ok,"part-filled top gas outlet reaches original volume admission");
 if(ok){
  double c2=settings.soundSpeed*settings.soundSpeed,u=settings.dt*settings.dt*9.81,g=settings.dt*settings.dt/(1.2*.5);
  double a=g/c2,b=.5/c2+1.2*g+u/c2,c=1.2*u;
  double expected=-2*c/(b+std::sqrt(b*b-4*a*c));
  check(std::abs(result.pressure[0].Value()-expected)<1e-8,"pressure agrees with independent quadratic gas-mass balance");
  check(result.water[0].Value()==.5&&result.externalWater.Value()==0.,"physical gas port leaves water unchanged");
  check(std::abs((result.gasMass[0]+result.externalGasMass-CutCompensated(.6)).Value())<1e-14,"physical gas port closes the original mass ledger");
 }
 settings.gasDensity=1.5;field.gasMass={.75};CutLocalPressureResult alternate;
 bool alternative=AdvanceCutLocalPressure(field,settings,{},alternate,error,CutLocalTransport::Fast);
 check(alternative&&alternate.water[0].Value()==.5&&std::abs((alternate.gasMass[0]+alternate.externalGasMass-CutCompensated(.75)).Value())<1e-14,"non-default ambient density is used consistently at gas ports");
 settings.gasDensity=1.2;field.gasMass={.6};
 field.boundaries[0].sign=-1;CutLocalPressureResult sentinel;sentinel.water={-7.};
 check(!AdvanceCutLocalPressure(field,settings,{},sentinel,error,CutLocalTransport::Fast)&&sentinel.water[0].Value()==-7.,"water-covered gas opening remains explicit unsupported topology");
 CutSpatialField blocked;std::vector<BoxMotion> roof{{1,{0.,.75,0.},{1.,1.,1.},{}}};
 bool built=PrepareCutRegionChart({{1,1,1},1.},roof,{},{},{},blocked.chart,error)&&BuildCutRegionGeometry(blocked.chart,{},blocked.geometry,error);
 check(built&&blocked.chart.regions.size()==1,"compile solid-blocked gas opening");
 if(built&&blocked.chart.regions.size()==1){
  blocked.parent={0};blocked.water={.25};blocked.gas={.5};blocked.gasMass={.6};blocked.boundaries={{0,1,1,0.,0.,true}};
  CutLocalPressureResult sealed;sealed.water={-7.};bool sealedOk=AdvanceCutLocalPressure(blocked,settings,{},sealed,error,CutLocalTransport::Fast);
  check(!sealedOk&&sealed.water[0].Value()==-7.,"an interior solid face cannot be misdeclared as an exterior gas opening");
 }
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
