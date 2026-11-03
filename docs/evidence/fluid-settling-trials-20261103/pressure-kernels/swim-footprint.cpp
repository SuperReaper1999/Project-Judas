#include "FluidWorld.h"
#include "GravityField.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

int main() {
    const struct Zero : GravityField {
        glm::vec3 Sample(const glm::vec3&) const override { return {}; }
    } gravity;
    for (const float spacing : {.2f, .25f}) {
        FluidSettings settings;
        settings.smoothingRadius*=spacing/.05f;
        settings.particleRadius*=spacing/.05f;
        settings.maxDensityCorrection*=spacing/.05f;
        FluidWorld fluid(settings);
        const int nX=int(std::lround(3.f/spacing));
        const int nY=int(std::lround(3.f/spacing));
        const int nZ=int(std::lround(2.f/spacing));
        const float x0=-.5f*float(nX-1),z0=-.5f*float(nZ-1);
        for(int y=0;y<nY;++y)for(int z=0;z<nZ;++z)for(int x=0;x<nX;++x)
            fluid.AddParticle({(x0+float(x))*spacing,float(y)*spacing+spacing*.5f,(z0+float(z))*spacing},
                {},1000.f*spacing*spacing*spacing);
        for(int i=0;i<=61;++i) {
            if(i>0) fluid.Step(i==1 ? 1.f/60 : 1.f/30,gravity,{});
            if(i!=0 && i!=1 && i!=61) continue;
            float minZ=std::numeric_limits<float>::infinity();
            double meanSpeed=0;
            unsigned beyondNominal=0,beyondPlayer=0;
            for(const auto& particle:fluid.Particles()){
                minZ=std::min(minZ,particle.position.z);
                meanSpeed+=glm::length(particle.velocity);
                beyondNominal+=std::abs(particle.position.z)>1.f;
                beyondPlayer+=particle.position.z < -2.495f;
            }
            std::printf("spacing=%.6f steps=%d centres_min_z=%.9g count_outside_authored_footprint=%u count_beyond_candidate_player=%u mean_speed=%.9g\n",
                spacing,i,minZ,beyondNominal,beyondPlayer,meanSpeed/fluid.Particles().size());
        }
    }
}
