#include "planar2d_3d_control.h"
#include <cstdio>
int main(){
    try {
        std::printf("3D_PHYSICAL_TRAJECTORY_DIGEST=%016llx steps=300 bodies=6\n",
                    static_cast<unsigned long long>(PlanarStage0Trajectory3D()));
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
