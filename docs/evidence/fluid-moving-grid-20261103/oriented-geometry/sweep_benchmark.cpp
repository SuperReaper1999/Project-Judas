#include "tools/prototypes/fluid_volume_grid/GridOrientedSweep.h"
#include <chrono>
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){
    const GridLayout grid{{5,5,5},.25};
    const double x=.37,y=.61;
    const std::array<GridVec,3> axes{{
        {std::cos(y),0.,-std::sin(y)},
        {std::sin(y)*std::sin(x),std::cos(x),std::cos(y)*std::sin(x)},
        {std::sin(y)*std::cos(x),-std::sin(x),std::cos(y)*std::cos(x)}
    }};
    const GridOrientedBox body{11,{.62,.59,.64},{.16,.09,.11},axes};
    const GridOrientedMotion translation{body,{.12,-.06,.04}};
    const GridOrientedMotion rotation{body,{}, {.6,0.,.8},.29};
    std::string e;
    for (const auto& motion : {translation, rotation}) {
        std::vector<double> ms;
        for(int i=0;i<11;++i){
            const auto start=std::chrono::steady_clock::now();
            GridOrientedSweep sweep;
            if(!BuildGridOrientedSweep(grid,motion,sweep,e)){std::cerr<<e<<'\n';return 1;}
            ms.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(ms.begin(),ms.end());
        std::cout<<(motion.angle==0.?"translation":"angular")<<" n="<<ms.size()<<" median="<<ms[5]<<" ms max="<<ms.back()<<" ms\n";
    }
}
