#include "CutSpatialTransport.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 CutRegionChart chart;chart.layout={{2,1,2},1.};double thin=1e-6;
 chart.regions={{{0.,0.,0.},{1.,1.,thin},0},{{0.,0.,thin},{.5,1.,1.+thin},1},{{.5,0.,thin},{1.,1.,1.+thin},2}};
 chart.faces={{0,1,2,{0.,0.,thin},{.5,1.,thin},.5,.5,.5,0.,.5},{0,2,2,{.5,0.,thin},{1.,1.,thin},.5,.5,.5,0.,.5}};
 CutRegionGeometry geometry;std::string error;check(BuildCutRegionGeometry(chart,{},geometry,error),"compile disjoint coface apertures");
 CutConnectedOwners owners;owners.owner={0,0,0};owners.members={{0,1,2}};owners.capacity={1.+thin};owners.lo={{0.,0.,0.}};owners.hi={{1.,1.,1.+thin}};
 CutDynamicState source;source.water={0.,geometry.startCapacity[1],0.};source.gasMass={1.2*thin,0.,1.2*geometry.startCapacity[2]};
 CutSpatialField field;
 check(BuildCutSpatialField(chart,geometry,owners,source,{-1.,1.},{{1,2,1,-1.,1.,false},{2,2,1,1.,0.,false}},field,error),"retain spatial field through connected owner");
 if(field.parent.empty()){std::cerr<<error;return 1;}
 double previousError=1.;
 for(double h:{1e-8,1e-9,1e-10}){
  CutSpatialResult out;check(AdvanceCutSpatial(field,owners,h,out,error),"spatial interval advances without collapsing time");
  if(out.water.empty()){std::cerr<<error;continue;}
  double left=0.,right=0.,sum=0.,mass=0.;
  for(std::size_t i=0;i<field.parent.size();++i){sum+=out.water[i];mass+=out.gasMass[i];if(field.parent[i]==0){if(field.chart.regions[i].hi[0]<=.5)left+=out.water[i];else right+=out.water[i];}}
  check(left>right&&right>0.,"water progresses from inlet-side cells to outlet-side cells");
  double ratio=h/(.5*thin+h);
  check(std::abs(left-.5*thin*ratio)<1e-15&&std::abs(right-.5*thin*ratio*ratio)<1e-15,"spatial result matches independent two-cell implicit chain");
  check(std::all_of(out.gasMass.begin(),out.gasMass.end(),[](double m){return m>=0.;}),"independent gas mass remains nonnegative");
  check(std::abs(sum-out.ownerWater[0])<1e-12&&std::abs(mass-out.ownerGasMass[0])<1e-12,"spatial quantities match conservative owner ledgers");
  auto half=field;CutSpatialResult a,b;bool stepped=AdvanceCutSpatial(half,owners,h/2.,a,error);
  if(stepped){half.water=a.water;half.gasMass=a.gasMass;stepped=AdvanceCutSpatial(half,owners,h/2.,b,error);}
  double defect=0.;if(stepped)for(std::size_t i=0;i<out.water.size();++i)defect+=std::abs(out.water[i]-b.water[i]);
  check(stepped&&defect/h<previousError,"normalized temporal error decreases with time subdivision");previousError=defect/h;
  std::cout<<"TEMPORAL h="<<h<<" defectOverH="<<previousError<<" iterations="<<out.iterations<<'\n';
 }
 CutSpatialResult sentinel;sentinel.water={-7.};
 check(!AdvanceCutSpatial(field,owners,-1.,sentinel,error)&&sentinel.water==std::vector<double>{-7.},"invalid interval preserves output");
 check(!AdvanceCutSpatial(field,owners,1.,sentinel,error,&sentinel)&&sentinel.water==std::vector<double>{-7.},"diagnostic cannot alias published output");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
