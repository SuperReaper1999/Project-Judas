#include "WaterGasGeometry.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
int checks=0,failures=0;void Check(bool ok,const char* s){++checks;if(!ok){++failures;std::cerr<<s<<'\n';}}
CutSpatialField Pair(double gas){
 CutSpatialField f;f.chart.layout={{2,1,1},1.};f.parent={0,1};f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1}};
 f.geometry.startCapacity=f.geometry.endCapacity={1.,1.};f.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};f.water={1.-gas,1.-gas};f.gas={gas,gas};f.gasMass={1.2*gas,1.2*gas};f.flux={0.};return f;
}
bool Prepare(const CutSpatialField& f,WaterGasGeometry& g,std::string& e){std::vector<CutRegionWaterPlane> planes;return ReconstructCutRegionPhasePlanes(f.chart,f.geometry,1,f.water,f.gas,{},planes,e)&&PrepareWaterGasGeometry(f,planes,{},1.8e-5,g,e);}
}
int main(){
 std::string e;WaterGasGeometry g;auto f=Pair(.5);
 Check(Prepare(f,g,e),"compile actual jointly dry phase opening");
 Check(g.nodeCell.size()==2&&g.links.size()==1&&std::abs(g.sharedDryArea[0]-.5)<1e-15,"partial water face retains only shared dry area");
 f.boundaries={{0,0,-1,0.,0.,true}};Check(Prepare(f,g,e)&&g.vents.size()==1,"explicit physical exterior produces a finite vent");
 double wide=g.links[0].massConductance;
 f=Pair(1e-8);f.boundaries={{0,0,-1,0.,0.,true}};
 Check(Prepare(f,g,e),"compile microscopic dry slit without rounding it closed or wide open");
 Check(std::abs(g.sharedDryArea[0]-1e-8)<1e-20&&g.links[0].massConductance>0.&&g.links[0].massConductance<wide*1e-15,"physical slit width determines finite resistance");
 auto compressed=g.volume;compressed[0]=compressed[0]*CutCompensated(.5);WaterGasResult state;
 Check(AdvanceWaterGasPockets(g.mass,compressed,g.links,g.vents,{},state,e),"compressed slit-connected pocket advances");
 Check(state.pressure[0].Value()>.999*101325.&&std::abs(state.pressure[1].Value())<1.,"a positive microscopic path does not merge pressure owners");
 Check(state.externalMass.Value()>0.,"tiny positive atmospheric exchange is retained");
 f=Pair(.5);f.water[1]=1.;f.gas[1]=f.gasMass[1]=0.;
 Check(Prepare(f,g,e)&&g.nodeCell.size()==1&&g.links.empty(),"water-covered face blocks gas exchange");
 f.boundaries={{1,0,1,0.,0.,true}};
 Check(Prepare(f,g,e)&&g.vents.empty(),"covered exterior does not impose atmosphere on a trapped pocket");
 auto saved=g;f.gasMass[1]=1.;
 Check(!Prepare(f,g,e)&&g.nodeCell==saved.nodeCell,"unowned gas rejects transactionally");
 f=Pair(.5);f.boundaries={{0,0,1,0.,0.,true}};
 Check(!Prepare(f,g,e),"an internal wall cannot be declared an atmospheric exterior");
 f=Pair(.5);f.geometry.faces[0].meanArea=0.;
 Check(Prepare(f,g,e)&&g.links.empty(),"explicitly closed face does not transmit gas");
 f.geometry.faces[0].meanArea=.5;
 Check(!Prepare(f,g,e),"unknown partially obstructed aperture rejects rather than opening the whole face");
 f=Pair(0.);Check(Prepare(f,g,e)&&g.nodeCell.empty(),"fully wet geometry has a valid empty gas network");
 f=Pair(.5);f.geometry.endCapacity[0]=.9;
 Check(!Prepare(f,g,e),"frozen gas compiler rejects moving capacities");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
