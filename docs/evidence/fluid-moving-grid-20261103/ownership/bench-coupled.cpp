#include "CoupledVolumeBox.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
using namespace judas::volume_grid;
int main(){
    for(const GridLayout grid : {GridLayout{{8,7,7},.25},GridLayout{{11,30,11},.2}}) {
        CoupledBoxState body{1,{.62,.74,.62},{.95,1.06,.95},{.08,0,0},25.};
        std::string error;
        MovingGridGeometry geometry;
        if(!BuildMovingGridGeometry(grid,{{body.owner,body.lo,body.hi,{}}},geometry,error))return 2;
        MovingVolumeState state;
        state.water=geometry.startCapacity;
        for(int a=0;a<3;++a)state.velocity[a].resize(grid.FaceCount(a));
        int evaluations=0;
        std::vector<double> times;
        for(int i=0;i<65;++i){
            CoupledBoxStep result;
            const auto start=std::chrono::steady_clock::now();
            if(!AdvanceCoupledVolumeBox(grid,body,state,1./60,{0,0,0},result,error)) {
                std::cout<<"REJECT "<<grid.CellCount()<<' '<<i<<' '<<error<<'\n';
                return 3;
            }
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(i>=5){times.push_back(ms); evaluations+=result.geometryEvaluations;}
            body=result.body;
            state=std::move(result.fluid.state);
        }
        std::sort(times.begin(),times.end());
        std::cout<<std::setprecision(10)<<"cells="<<grid.CellCount()<<" mean_ms="<<std::accumulate(times.begin(),times.end(),0.)/times.size()
                 <<" p95="<<times[56]<<" worst="<<times.back()<<" mean_pressure_calls="<<double(evaluations)/times.size()
                 <<" end_x="<<body.lo[0]<<" vx="<<body.velocity[0]<<'\n';
    }
}
