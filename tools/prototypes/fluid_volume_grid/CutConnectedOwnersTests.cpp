#include "CutOwnedProjection.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

using namespace judas::volume_grid;
namespace {
int checks=0, failures=0;
void Check(bool value,const char* name){++checks;failures+=!value;std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';}
CutRegionFace Face(std::size_t l,std::size_t r,int axis,GridVec lo,GridVec hi){
 const double a=(hi[(axis+1)%3]-lo[(axis+1)%3])*(hi[(axis+2)%3]-lo[(axis+2)%3]);
 return {l,r,axis,lo,hi,a,a,a,0.,.5};
}
CutConnectedOwners Map(const CutRegionChart& chart,const CutRegionGeometry& geom,std::vector<std::size_t> mapping){
 CutConnectedOwners o;o.owner=std::move(mapping);
 const auto n=*std::max_element(o.owner.begin(),o.owner.end())+1;
 o.members.resize(n);o.capacity.resize(n);o.lo.resize(n,GridVec{1e20,1e20,1e20});o.hi.resize(n,GridVec{-1e20,-1e20,-1e20});
 for(std::size_t f=0;f<o.owner.size();++f){auto i=o.owner[f];o.members[i].push_back(f);o.capacity[i]+=geom.startCapacity[f];for(int a=0;a<3;++a){o.lo[i][a]=std::min(o.lo[i][a],chart.regions[f].lo[a]);o.hi[i][a]=std::max(o.hi[i][a],chart.regions[f].hi[a]);}}
 return o;
}
}
int main(){
 std::string error;
 {
  CutRegionChart chart;chart.layout={{2,1,1},1.};
  chart.regions={{{0.,0.,0.},{.01,1.,1.},0},{{.01,0.,0.},{1.,1.,1.},1}};
  for(int y=0;y<100;++y)chart.faces.push_back(Face(0,1,0,{.01,double(y)/100.,0.},{.01,double(y+1)/100.,1.}));
  CutRegionGeometry g;Check(BuildCutRegionGeometry(chart,{},g,error),"compile subdivided open seam");
  CutConnectedOwners o;Check(BuildCutConnectedOwners(chart,g,.125,o,error),"compile connected material owners");
  Check(o.members.size()==1 && o.members[0].size()==2,"a wide seam is recognized independently of quadrature tessellation");
  Check(o.capacity[0]==g.startCapacity[0]+g.startCapacity[1],"capacity sums actual fragments rather than a bounding box");
  auto state=CutDynamicState{};state.water={.01,.99};state.gasMass={0.,0.};
  CutDynamicSettings cfg;cfg.gravity={};CutOwnedProjection projected;
  Check(ProjectCutOwnedMaterial(chart,g,o,state,cfg,{},projected,error),"static owner projection admits a represented quiet material");
  Check(projected.water==std::vector<double>{1.} && projected.gasMass==std::vector<double>{0.},"projected material retains its original quantity");
  state.faceSpeed.assign(g.faces.size(),0.);state.faceSpeed[0]=.1;
  projected.water={-7.};
  Check(!ProjectCutOwnedMaterial(chart,g,o,state,cfg,{},projected,error) && projected.water==std::vector<double>{-7.},"unreconstructed internal momentum cannot be silently discarded");
  chart.faces={Face(0,1,0,{.01,0.,0.},{.01,1e-9,1e-9})};
  Check(BuildCutRegionGeometry(chart,{},g,error) && BuildCutConnectedOwners(chart,g,.125,o,error),"compile finite microscopic throat");
  Check(o.members.size()==2,"a microscopic throat does not become instantaneous shared ownership");
  Check(g.faces[0].meanArea>0. && g.faces[0].meanArea<1e-17,"finite throat is retained rather than deleted");
  chart.faces.clear();Check(BuildCutRegionGeometry(chart,{},g,error) && BuildCutConnectedOwners(chart,g,.125,o,error) && o.members.size()==2,"no pressure or material ownership crosses a wall");
 }
 {
  CutRegionChart chart;chart.layout={{4,1,1},2.};
  chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1},{{2.,0.,0.},{3.,1.,1.},2},{{3.,0.,0.},{4.,1.,1.},3}};
  chart.faces={Face(0,1,0,{1.,0.,0.},{1.,1.,1.}),Face(1,2,0,{2.,0.,0.},{2.,1.,1.}),Face(2,3,0,{3.,0.,0.},{3.,1.,1.})};
  CutRegionGeometry g;Check(BuildCutRegionGeometry(chart,{},g,error),"compile a three-fragment material column");
  auto o=Map(chart,g,{0,0,0,1});std::vector<CutRegionWaterPlane> planes;
  Check(ReconstructCutRegionWaterPlanes(chart,g,0,{1.,0.,0.,0.},{},planes,error),"retain the starting interface geometry within the connected owner");
  for(const auto& sample:std::vector<std::pair<double,double>>{{.5,0.},{2.,0.},{2.5,.5}}){
   CutOwnedDeparture d;Check(MeasureCutOwnedDeparture(chart,g,o,planes,2,sample.first,d,error),"continue a normal material departure across internal fragments");
   Check(std::abs(d.water-sample.second)<1e-14 && std::abs(d.water+d.gas-sample.first)<1e-14 && d.unsupportedVolume==0.,"analytic plug front reaches the outlet at the correct material volume");
  }
  Check(o.capacity[0]==3.,"crossed quadrature faces do not add or remove physical water capacity");
  CutOwnedDeparture sentinel;sentinel.water=-7.;auto bad=o;bad.owner.clear();
  Check(!MeasureCutOwnedDeparture(chart,g,bad,planes,2,.1,sentinel,error) && sentinel.water==-7.,"invalid ownership rejects without publishing a partial measure");
 }
 {
  // One connected U-shaped owner. The normal ray cannot jump the dry/solid
  // gap, even though another route connects its two arms elsewhere.
  CutRegionChart chart;chart.layout={{2,2,1},1.};
  chart.regions={{{.35,0.,0.},{.8,.4,1.},0},{{0.,0.,0.},{.3,.4,1.},0},{{0.,.4,0.},{.8,.8,1.},0},{{.8,0.,0.},{1.8,.4,1.},1}};
  chart.faces={Face(0,3,0,{.8,0.,0.},{.8,.4,1.})};
  CutRegionGeometry g;Check(BuildCutRegionGeometry(chart,{},g,error),"compile a connected nonrectangular material footprint");
  auto o=Map(chart,g,{0,0,0,1});std::vector<CutRegionWaterPlane> planes;
  Check(ReconstructCutRegionWaterPlanes(chart,g,1,{0.,.12,0.,0.},{},planes,error),"retain water behind the gap independently of its connected owner");
  CutOwnedDeparture d;Check(MeasureCutOwnedDeparture(chart,g,o,planes,0,.24,d,error),"measure an occluded footprint on the actual union");
  Check(d.water==0. && std::abs(d.gas-.18)<1e-14 && std::abs(d.unsupportedVolume-.06)<1e-14,"a bounding box cannot invent water across the gap");
 }
 {
  CutRegionChart chart;chart.layout={{1,1,1},1.};chart.regions={{{0.,0.,0.},{1.,1.,1.},0}};
  CutRegionGeometry g;Check(BuildCutRegionGeometry(chart,{},g,error),"compile minority gas owner");CutConnectedOwners o;
  Check(BuildCutConnectedOwners(chart,g,.125,o,error),"build minority gas geometry owner");
  CutDynamicState state;state.water={1.};state.gasMass={1.2e-20};CutDynamicSettings cfg;cfg.gravity={};CutOwnedProjection p;
  Check(ProjectCutOwnedMaterial(chart,g,o,state,cfg,{},p,error) && p.gasMass[0]==1.2e-20,"gas smaller than one water ULP remains physically owned");
  auto bad=o;bad.members[0].push_back(0);p.water={-7.};
  Check(!ProjectCutOwnedMaterial(chart,g,bad,state,cfg,{},p,error) && p.water==std::vector<double>{-7.},"duplicate geometric fragments cannot duplicate physical material");
 }
 {
  CutHydroacousticGraphResult scalar,local;
  Check(SolveCutHydroacousticGraph({2.,0.},{{0,1,3.}},{-1.,3.},1e-13,scalar,error),"retain the original scalar pressure contract");
  Check(SolveCutHydroacousticGraphLocal({2.,0.},{{0,1,3.}},{-1.,3.},{1e-13,1e-13},local,error) && scalar.pressure==local.pressure && scalar.iterations==local.iterations,"uniform local tolerances preserve the scalar pressure result exactly");
  Check(SolveCutHydroacousticGraphLocal({1e-20,1.},{},{1e-20,1e-3},{1e-30,1e-13},local,error) && std::abs(local.pressure[0]-1.)<1e-14,"a tiny owner closes its own equation rather than borrowing another owner's tolerance");
  auto before=local.pressure;Check(!SolveCutHydroacousticGraphLocal({1.},{},{1.},{std::numeric_limits<double>::quiet_NaN()},local,error) && local.pressure==before,"invalid local tolerance rejects transactionally");
 }
 std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
