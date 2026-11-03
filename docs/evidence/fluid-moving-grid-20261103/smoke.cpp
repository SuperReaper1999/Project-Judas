#include "GridPressureReaction.h"
#include "MovingVolumeGrid.h"
#include <iomanip>
#include <iostream>
#include <numeric>
using namespace judas::volume_grid;
int main() {
    GridLayout g{{8,7,7},.25};
    MovingVolumeState s;
    for(int axis=0;axis<3;++axis)s.velocity[axis].resize(g.FaceCount(axis));
    std::string error;
    const GridVec delta{.03,-.007,.012};
    const BoxMotion box{1,{.49,.57,.68},{1.02,.76,1.11},delta};
    MovingGridGeometry geometry;
    if(!BuildMovingGridGeometry(g,{box},geometry,error)) {std::cerr<<error<<'\n';return 2;}
    s.water=geometry.startCapacity;
    MovingVolumeStep r;
    if(!AdvanceMovingVolume(g,geometry,s,1./30,{0,0,0},r,error)) {std::cerr<<error<<'\n';return 3;}
    const auto load=AccumulateGridPressureReaction(geometry.averagePatches,r.pressure);
    std::cout<<std::setprecision(15)<<"iterations "<<r.iterations<<" flux residual "<<r.residual
        <<" volume before "<<std::accumulate(s.water.begin(),s.water.end(),0.)
        <<" after "<<std::accumulate(r.state.water.begin(),r.state.water.end(),0.)<<" work "<<r.pressureFluidWork
        <<" solid work "<<std::inner_product(delta.begin(),delta.end(),load[0].force.begin(),0.)
        <<" energy identity "<<r.pressureKineticWork-r.pressureFluidWork+r.pressureCorrectionEnergy
        <<" CFL "<<r.maxCfl<<'\n';
}
