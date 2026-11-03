#include "CoupledVolumeBox.h"
#include "GridPhaseGeometry.h"
#include <iomanip>
#include <iostream>
#include <numeric>
using namespace judas::volume_grid;
int main(){
    const GridLayout layout{{8,8,8},.25};
    const double height=.9;
    for(double impulse:{0.,-.1,.1}){
        CoupledBoxState body{1,{.62,.74,.62},{.95,1.06,.95},{0,impulse,0},0};
        body.mass=(500.5)*((body.hi[0]-body.lo[0])*(body.hi[1]-body.lo[1])*(body.hi[2]-body.lo[2]));
        GridPhaseGeometry phase;
        std::string error;
        if(!BuildGridPhaseGeometry(layout,{{body.owner,body.lo,body.hi,{}}},1,height,phase,error))return 1;
        MovingVolumeState s;
        s.water=phase.water;
        for(int a=0;a<3;++a)s.velocity[a].resize(layout.FaceCount(a));
        double total=std::accumulate(s.water.begin(),s.water.end(),0.);
        for(int i=0;i<24;++i){
            CoupledBoxStep r;
            if(!AdvanceEvolvingCoupledVolumeBox(layout,body,s,1./60,1,-9.81,r,error)) {
                std::cout << "FAIL " << impulse << " step " << i << ' ' << error << '\n';
                break;
            }
            std::cout<<std::setprecision(10)<<impulse<<','<<i<<','<<r.body.lo[1]<<','<<r.body.velocity[1]<<','<<r.pressureForce[1]
                     <<','<<r.pressureEnergyResidual<<','<<std::accumulate(r.fluid.state.water.begin(),r.fluid.state.water.end(),0.)-total
                     <<','<<r.geometryEvaluations<<'\n';
            body=r.body;
            s=r.fluid.state;
        }
    }
}
