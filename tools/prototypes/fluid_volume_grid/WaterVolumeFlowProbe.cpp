#include "WaterVolumeAdvance.h"
#include <cmath>
#include <iostream>
#include <iomanip>
using namespace judas::volume_grid;
CutSpatialField Columns(double left,double right){
    CutSpatialField f;f.chart.layout={{2,1,1},1.};f.parent={0,1};f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1}};
    f.geometry.startCapacity=f.geometry.endCapacity={1.,1.};f.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};
    f.water={left,right};f.gas={1.-left,1.-right};f.gasMass={1.2*(1.-left),1.2*(1.-right)};f.flux={0.};return f;
}
double Energy(const WaterVolumeStep& s){double result=0.;for(std::size_t i=0;i<s.water.size();++i){double w=s.water[i].Value(),v2=0.;for(double v:s.velocity[i])v2+=v*v;result+=.5*1000.*w*v2+.5*1000.*9.81*w*w;double g=s.gas[i].Value(),m=s.gasMass[i].Value();if(g>0.)result+=101325./1.2*m*(std::log(m/(1.2*g))-1.)+101325.*g;}return result;}
int main(){std::cout<<std::setprecision(17);auto f=Columns(.6,.4);WaterGasSettings settings;WaterVolumeStep state;std::string e;double initial=.5*1000.*9.81*(.36+.16),maximum=initial,previousEnergy=initial;
for(int step=0;step<240;++step){WaterVolumeStep next;bool ok=AdvanceWaterVolume(f,settings,1000.,{0.,-9.81,0.},{},next,e,step?&state:nullptr);if(!ok){std::cout<<"FLOW step="<<step<<" rejected="<<e<<"\n";return 1;}double energy=Energy(next);maximum=std::max(maximum,energy);
if(energy>previousEnergy+1e-8*std::max(previousEnergy,1.)){std::cout<<"ENERGY_FAILURE step="<<step<<" before="<<previousEnergy<<" after="<<energy<<"\n";return 2;}previousEnergy=energy;
CutCompensated w,m;for(std::size_t i=0;i<next.water.size();++i){w=w+next.water[i];m=m+next.gasMass[i];}if(std::abs((w-CutCompensated(1.)).Value())>1e-12||std::abs((m-CutCompensated(1.2)).Value())>1e-12){std::cout<<"QUANTITY_FAILURE step="<<step<<"\n";return 3;}std::cout<<"FLOW step="<<step<<" W0="<<next.water[0].Value()<<" W1="<<next.water[1].Value()<<" speed="<<next.maximumSpeed<<" energy="<<energy<<" growth="<<(energy-initial)<<" waterLedger="<<next.waterLedger<<" gasLedger="<<next.gasLedger<<" ms="<<next.totalMilliseconds<<"\n";state=std::move(next);}
std::cout<<"FLOW final maximumEnergyGrowth="<<maximum-initial<<"\n";return maximum>initial*(1.+1e-8)?2:0;}
