#include "tools/prototypes/fluid_volume_grid/GridCellPhase.h"
#include <iomanip>
#include <iostream>
#include <random>
using namespace judas::volume_grid;
int main(){
    GridLayout g{{8,7,6},.23};
    std::mt19937 rng(284);
    const auto random=[&] { return (double(rng())+.5)/4294967296.; };
    int cases=0, changed=0, rejected=0;
    double maxError=0.;
    for(int i=0;i<1000;++i) {
        BoxMotion b{1};
        for(int a=0;a<3;++a){
            const double extent=g.cells[a]*g.h;
            b.lo[a]=(.11+.21*random())*extent;
            b.hi[a]=(.40+.22*random())*extent;
            b.translation[a]=(.24*random()-.1)*extent;
        }
        MovingGridGeometry m,s;
        GridCellPhase phase;
        std::string e;
        if(!BuildMovingGridGeometry(g,{b},m,e)){std::cerr<<e<<'\n';return 1;}
        for(int a=0;a<3;++a){
            b.lo[a]+=b.translation[a]; b.hi[a]+=b.translation[a]; b.translation[a]=0.;
        }
        if(!BuildMovingGridGeometry(g,{b},s,e)){std::cerr<<e<<'\n';return 1;}
        ++cases;
        changed+=m.endCapacity!=s.startCapacity;
        for(std::size_t c=0;c<g.CellCount();++c)
            maxError=std::max(maxError,std::abs(m.endCapacity[c]-s.startCapacity[c]));
        rejected+=!ReconstructGridCellPhase(g,{b},1,m.endCapacity,phase,e);
    }
    std::cout<<std::setprecision(17)<<cases<<" paths; "<<changed
        <<" canonical capacity mismatches; "<<rejected<<" exact-full reconstruction rejections; max error "
        <<maxError<<" m^3\n";
}
