#include "WaterVolumeAdvance.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
int checks=0,failures=0;
void Check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
CutSpatialField Columns(double left,double right){
    CutSpatialField f;f.chart.layout={{2,1,1},1.};f.parent={0,1};f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1}};
    f.geometry.startCapacity=f.geometry.endCapacity={1.,1.};f.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};
    f.water={left,right};f.gas={1.-left,1.-right};f.gasMass={1.2*(1.-left),1.2*(1.-right)};f.flux={0.};return f;
}
}
int main(){
    WaterGasSettings settings;WaterVolumeStep step;std::string e;auto f=Columns(.5,.5);
    Check(AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{},step,e),"quiet sealed columns advance: "+e);
    if(step.water.empty()){std::cerr<<e<<"\n";return 1;}
    Check(step.maximumSpeed<1e-12&&std::abs(step.water[0].Value()-.5)<1e-14,"quiet pool remains quiet without artificial damping");
    Check(std::abs(step.waterLedger)<1e-25&&std::abs(step.gasLedger)<1e-25,"quiet step independently preserves water and gas mass");
    bool quiet=true;for(int tick=0;tick<120&&quiet;++tick)quiet=AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{},step,e,&step);
    Check(quiet&&step.maximumSpeed<1e-12,"quiet water remains stationary over 120 coupled intervals: "+e);
    auto shifted=f;shifted.chart.regions[0].lo[0]=-.1;auto untouched=step;
    Check(!AdvanceWaterVolume(shifted,settings,1000.,{0.,-9.81,0.},{},step,e,&step)&&step.chartStamp==untouched.chartStamp,"a stale prepared state rejects after chart changes");
    f=Columns(.6,.4);Check(AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{},step,e),"unequal sealed columns advance: "+e);
    Check(step.water[0].Value()<.6&&step.water[1].Value()>.4,"pressure moves real water from higher to lower column");
    Check(step.maximumPartitionResidual<=1.&&std::abs(step.waterLedger)<1e-20&&std::abs(step.gasLedger)<1e-20,"flow publishes only after original partition and quantity controls");
    f=Columns(.6,.4);
    Check(AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{{-4.,0.,0.},{-4.,0.,0.}},step,e)&&std::abs(step.gasLedger)<1e-14,"receding air remaps into the already connected local dry slab without losing mass: "+e);
    auto before=step;f.gasMass[0]=-1.;
    Check(!AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{},step,e)&&step.water[0].hi==before.water[0].hi,"invalid source leaves the entire previous step unchanged");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
