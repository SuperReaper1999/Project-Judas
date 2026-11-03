#include "tools/prototypes/fluid_volume_grid/MovingGridGeometry.h"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>

using namespace judas::volume_grid;

int main() {
    const GridLayout grid{{8,7,6},.23};
    std::mt19937 rng(0x45ba63c9);
    const auto fraction = [&] { return (static_cast<double>(rng())+.5)/4294967296.; };
    double maxFace=0,maxWall=0,maxVolume=0,maxTranslationArea=0;
    for (int sample=0;sample<1024;++sample) {
        BoxMotion box{17};
        double trueVolume=1.;
        for (int axis=0;axis<3;++axis) {
            const double limit=grid.cells[axis]*grid.h;
            const double extent=(.02+.96*fraction())*limit;
            box.lo[axis]=(.001+.998*fraction())*(limit-extent);
            box.hi[axis]=box.lo[axis]+extent;
            const double endLo=(.001+.998*fraction())*(limit-extent);
            box.translation[axis]=sample%8==axis?0.:endLo-box.lo[axis];
            trueVolume*=extent;
        }
        MovingGridGeometry geometry;
        std::string error;
        if(!BuildMovingGridGeometry(grid,{box},geometry,error)) {
            std::cerr<<sample<<": "<<error<<'\n';return 1;
        }
        std::vector<double> wall(grid.CellCount());
        double tArea[3][2]{};
        for (auto p:geometry.averagePatches) {
            wall[p.cell] -= p.area*p.sign*box.translation[p.axis];
            tArea[p.axis][(p.sign+1)/2] += p.area;
            assert(p.owner==17);
        }
        for (int axis=0;axis<3;++axis) {
            const double expectedArea=(box.hi[(axis+1)%3]-box.lo[(axis+1)%3])*
                (box.hi[(axis+2)%3]-box.lo[(axis+2)%3]);
            for(double a:tArea[axis])maxTranslationArea=std::max(maxTranslationArea,std::abs(a-expectedArea));
        }
        double vol0=0,vol1=0;
        for (int k=0;k<grid.cells[2];++k)for(int j=0;j<grid.cells[1];++j)for(int i=0;i<grid.cells[0];++i) {
            const auto cell=grid.Cell(i,j,k);
            const double delta=geometry.endCapacity[cell]-geometry.startCapacity[cell];
            const double swept=geometry.solidSweep[0][grid.Face(0,i+1,j,k)]-geometry.solidSweep[0][grid.Face(0,i,j,k)]+
                geometry.solidSweep[1][grid.Face(1,i,j+1,k)]-geometry.solidSweep[1][grid.Face(1,i,j,k)]+
                geometry.solidSweep[2][grid.Face(2,i,j,k+1)]-geometry.solidSweep[2][grid.Face(2,i,j,k)];
            maxFace=std::max(maxFace,std::abs(delta-swept));
            maxWall=std::max(maxWall,std::abs(delta-wall[cell]));
            const double cellVolume=grid.h*grid.h*grid.h;
            vol0+=cellVolume-geometry.startCapacity[cell];
            vol1+=cellVolume-geometry.endCapacity[cell];
        }
        maxVolume=std::max({maxVolume,std::abs(vol0-trueVolume),std::abs(vol1-trueVolume)});
    }
    std::cout<<std::setprecision(17)<<"1024 randomized trajectories. Max face GCL error "<<maxFace
             <<" m³; patch work GCL "<<maxWall<<" m³; integrated volume "<<maxVolume
             <<" m³; patch area "<<maxTranslationArea<<" m²\n";
    assert(maxFace<1e-13 && maxWall<1e-13 && maxVolume<1e-12 && maxTranslationArea<1e-12);
}
