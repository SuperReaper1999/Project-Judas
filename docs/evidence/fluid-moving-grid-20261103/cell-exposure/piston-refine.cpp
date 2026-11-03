#include "ExposureVolumePressure.h"
#include "GridPressureReaction.h"
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
int main(){
    const GridLayout layout{{4,4,4},.25};
    for (int level=0; level<12; ++level) {
        const double dt=std::ldexp(1./60, -level);
        const BoxMotion body{1,{.25,.5,.25},{.75,.875,.75},{0,2.4*dt,0}};
        MovingGridGeometry geometry; std::string error;
        if(!BuildMovingGridGeometry(layout,{body},geometry,error)) {std::cout<<error<<'\n';return 1;}
        MovingVolumeState state; state.water=geometry.startCapacity;
        for(int a=0;a<3;++a)state.velocity[a].resize(layout.FaceCount(a));
        MovingVolumeStep old, pressure;
        const bool previous=AdvanceMovingVolume(layout,geometry,state,dt,{},old,error);
        const auto prior=error;
        const double previousImpulse = previous
            ? dt*AccumulateGridPressureReaction(geometry.averagePatches,old.pressure).front().force[1] : 0.;
        if(!AdvanceExposureVolumePressure(layout,geometry,state,dt,true,pressure,error)) {
            std::cout << "fail "<<level<<' '<<error<<'\n'; return 2;
        }
        const auto load=AccumulateGridPressureReaction(geometry.averagePatches,pressure.pressure).front();
        std::cout<<std::setprecision(16)<<dt<<" impulse "<<dt*load.force[1]<<" work "<<pressure.pressureFluidWork
                 <<" raw divergence "<<pressure.residual<<" previous "<<previous<<" impulse "<<previousImpulse<<(prior.empty() ? prior : " " + prior)<<'\n';
    }
}
