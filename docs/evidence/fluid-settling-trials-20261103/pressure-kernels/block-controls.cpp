#include "FluidWorld.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    struct Zero : GravityField {
        glm::vec3 Sample(const glm::vec3&) const override { return {}; }
    } gravity;
    const glm::quat rotation=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
    for(const float spacing:{.05f,.20f,.25f,.65f}) {
        for(const float supportRatio:{1.6f,2.1f,2.5f}) {
            for(const bool rotated:{false,true}) {
                for(const float size:{1.f,.95f}) {
                    FluidSettings settings;
                    settings.smoothingRadius=supportRatio*spacing;
                    settings.particleRadius=.36f*spacing;
                    settings.maxDensityCorrection=.5f*spacing;
                    FluidWorld world(settings);
                    const float mass=1000*spacing*spacing*spacing;
                    for(int z=-2;z<=2;++z)for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){
                        auto position=(spacing*size)*glm::vec3(x,y,z);
                        if(rotated)position=rotation*position;
                        world.AddParticle(position,{},mass);
                    }
                    world.Step(1.f/30,gravity,{});
                    const auto state=world.GetDiagnostics();
                    double meanSpeed=0.;
                    for(const auto& particle:world.Particles())meanSpeed+=glm::length(particle.velocity);
                    meanSpeed/=world.Particles().size();
                    std::printf("spacing=%.6f ratio=%.6f rotated=%d size=%.6f mass=%.9g pressure_error=%.9g mean_speed=%.9g energy=%.9g bulk_speed=%.9g\n",
                        spacing,supportRatio,rotated,size,state.totalMass,state.maxPositiveDensityError,meanSpeed,state.kineticEnergy,glm::length(state.totalMomentum)/state.totalMass);
                }
            }
        }
    }

    using Clock=std::chrono::steady_clock;
    for(const float size:{1.f,.95f}) {
        std::vector<double> timings;
        float maximumPressure=0;
        for(int sample=0;sample<80;++sample){
            FluidWorld world;
            const float spacing=.05f*size;
            for(int z=0;z<10;++z)for(int y=0;y<12;++y)for(int x=0;x<10;++x)
                world.AddParticle(spacing*glm::vec3(x,y,z),{},.125f);
            const auto begin=Clock::now();
            world.Step(1.f/30,gravity,{});
            timings.push_back(std::chrono::duration<double,std::milli>(Clock::now()-begin).count());
            maximumPressure=std::max(maximumPressure,world.GetDiagnostics().maxPositiveDensityError);
        }
        std::sort(timings.begin(),timings.end());
        std::printf("PERF particles=1200 samples=%zu size=%.6f median_ms=%.6f p95_ms=%.6f worst_ms=%.6f pressure_error=%.6g\n",
            timings.size(),size,timings[timings.size()/2],timings[75],timings.back(),maximumPressure);
    }
}
