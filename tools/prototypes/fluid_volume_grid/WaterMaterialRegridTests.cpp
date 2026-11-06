#include "WaterMaterialRegrid.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using R=CutCompensated;
int checks=0,failures=0;
void Check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
CutSpatialField Cell(){CutSpatialField f;f.chart.layout={{1,1,1},1.};f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0}};f.geometry.startCapacity=f.geometry.endCapacity={1.};return f;}
WaterMaterialFragment Box(R x0,R x1,R y0,R y1,R z0,R z1,bool water,std::size_t carrier){WaterMaterialFragment p;p.lo={x0,y0,z0};p.hi={x1,y1,z1};p.water=water;p.carrier=carrier;if(!water)p.gasMass=R(1.2)*MaterialVolume(p);return p;}
R Water(const WaterMaterialState& s){R v;for(auto p:s.fragments)if(p.water)v=v+MaterialVolume(p);return v;}
R Mass(const WaterMaterialState& s){R v;for(auto p:s.fragments)if(!p.water)v=v+p.gasMass;return v;}
bool Contains(const WaterMaterialFragment& p,R x,R y,R z){WaterCoordinate point{x,y,z};for(int a=0;a<3;++a)if((point[a]-p.lo[a]).Value()<0.||(p.hi[a]-point[a]).Value()<=0.)return false;return true;}
void Windmill(){
    auto field=Cell();WaterMaterialState source,after;std::string error;WaterMaterialRegridStats stats;
    const double bounds[5][4]={{0.,.6,0.,.4},{.6,1.,0.,.6},{.4,1.,.6,1.},{0.,.4,.4,1.},{.4,.6,.4,.6}};
    for(int i=0;i<5;++i){auto b=bounds[i];auto p=Box(b[0],b[1],b[2],b[3],0.,.5,true,i);p.velocity={double(i),1.,-2.};source.fragments.push_back(p);source.fragments.push_back(Box(b[0],b[1],b[2],b[3],.5,1.,false,100));}
    source.importedWater=Water(source);source.importedGasMass=Mass(source);
    bool ok=RegridWaterMaterial(field,source,after,stats,error);
    Check(ok,"non-slicing T-junction partition reconstructs: "+error);if(!ok)return;
    Check(after.fragments.size()==2&&stats.components==2,"ten history pieces reduce to the two real phase regions");
    Check(std::abs((Water(after)-Water(source)).Value())<1e-28&&std::abs((Mass(after)-Mass(source)).Value())<1e-28,"independent water and air ledgers survive retiling");
    Check(stats.momentError.Value()<1e-28&&stats.momentumError.Value()<1e-14,"volume first moments and water momentum are retained");
    Check(stats.mixingKineticPerDensityLoss>0.,"existing connected-cell velocity averaging records its passive kinetic loss");
    auto saved=after;Check(!RegridWaterMaterial(field,source,after,stats,error,1)&&after.fragments.size()==saved.fragments.size(),"work-budget rejection preserves the published state");
}
void Pocket(){
    auto field=Cell();WaterMaterialState source,after;WaterMaterialRegridStats stats;std::string error;
    R a(.5),b=a+R(1e-20);
    source.fragments={Box(0.,a,0.,1.,0.,1.,true,0),Box(b,1.,0.,1.,0.,1.,true,0),Box(a,b,0.,a,0.,1.,true,0),Box(a,b,b,1.,0.,1.,true,0),Box(a,b,a,b,0.,a,true,0),Box(a,b,a,b,b,1.,true,0),Box(a,b,a,b,a,b,false,7)};
    source.importedWater=Water(source);source.importedGasMass=Mass(source);
    bool ok=RegridWaterMaterial(field,source,after,stats,error);
    Check(ok,"sub-coordinate-ULP trapped pocket survives exact union: "+error);if(!ok)return;
    R mass;bool dry=false,wet=false;for(auto p:after.fragments){if(!p.water){mass=mass+p.gasMass;dry|=Contains(p,a+R(5e-21),a+R(5e-21),a+R(5e-21));}else wet|=Contains(p,a+R(5e-21),a+R(5e-21),a+R(5e-21));}
    Check(dry&&!wet&&mass.Value()>0.&&std::abs((mass-Mass(source)).Value())<1e-80,"retiling retains real dry support and its positive mass rather than filling it");
}
void Disconnected(){
    auto field=Cell();WaterMaterialState source,after;WaterMaterialRegridStats stats;std::string error;
    source.fragments={Box(0.,.3,0.,1.,0.,1.,false,0),Box(.3,.7,0.,1.,0.,1.,true,1),Box(.7,1.,0.,1.,0.,1.,false,0)};
    Check(RegridWaterMaterial(field,source,after,stats,error)&&stats.components==3,"a reused gas label cannot erase a water separator");
    std::size_t gas=0;std::size_t first=SIZE_MAX;bool distinct=true;for(auto p:after.fragments)if(!p.water){++gas;if(first==SIZE_MAX)first=p.carrier;else distinct &= p.carrier!=first;}
    Check(gas==2&&distinct,"disconnected positive-air compartments retain separate identities");
}
void RealGap(){
    auto field=Cell();WaterMaterialState source,after;WaterMaterialRegridStats stats;std::string error;R lo(.5),hi=lo+R(1e-20);
    source.fragments={Box(0.,lo,0.,1.,0.,1.,true,0),Box(lo,hi,0.,1.,0.,1.,false,1),Box(hi,1.,0.,1.,0.,1.,true,0)};
    Check(RegridWaterMaterial(field,source,after,stats,error)&&stats.components==3&&after.fragments.size()==3,
          "a real sub-coordinate-ULP gap remains a separator");
    R mass;for(auto p:after.fragments)if(!p.water)mass=mass+p.gasMass;
    Check(mass.Value()>0.&&std::abs((mass-Mass(source)).Value())<1e-45,"a real thin air film retains its positive mass");
}
void ManyCuts(){
    auto field=Cell();WaterMaterialState source,after;WaterMaterialRegridStats stats;std::string error;
    for(int x=0;x<8;++x)for(int y=0;y<8;++y){source.fragments.push_back(Box(double(x)/8,double(x+1)/8,double(y)/8,double(y+1)/8,0.,.5,true,0));source.fragments.push_back(Box(double(x)/8,double(x+1)/8,double(y)/8,double(y+1)/8,.5,1.,false,1));}
    Check(RegridWaterMaterial(field,source,after,stats,error)&&after.fragments.size()==2,"128 integration pieces reconstruct to two unchanged regions: "+error);
    Check(stats.work<=10000000&&stats.momentError.Value()<1e-28&&std::abs((Mass(after)-Mass(source)).Value())<1e-28,
          "bounded work retains the quantity and first-moment contracts");
}

}
int main(){Windmill();Pocket();Disconnected();RealGap();ManyCuts();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}
