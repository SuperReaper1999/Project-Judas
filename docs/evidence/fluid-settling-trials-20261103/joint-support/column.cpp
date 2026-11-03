#include "FluidWorld.h"
#include "GravityField.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

struct ConstantGravity final : GravityField {
    glm::vec3 Sample(const glm::vec3&) const override { return {0,-9.81f,0}; }
};

int main() {
    FluidSettings s;
    s.substeps=1;
    const float spacing=.05f;
    const double h=s.smoothingRadius;
    const auto kernel=[&](double distance) {
        return 315/(64*std::acos(-1.)*std::pow(h,9))*std::pow(h*h-distance*distance,3);
    };
    const float mass=static_cast<float>(s.restDensity/(kernel(0)+kernel(spacing)));
    FluidWorld world(s);
    const float bottom=s.particleRadius;
    world.AddParticle({0,bottom,0},{0,0,0},mass);
    world.AddParticle({0,bottom+spacing,0},{0,0,0},mass);
    FluidBoxCollider floor{};
    floor.owner={1,1};
    floor.previousPose.position=floor.currentPose.position={0,-.125f,0};
    floor.previousPose.rotation=floor.currentPose.rotation={1,0,0,0};
    floor.halfExtents={1,.125f,1};
    const ConstantGravity gravity;
    const float dt=1.0f/60.0f;
    std::vector<FluidContactImpulse> reactions;
    std::cout<<std::setprecision(9)<<"# paired equilibrium mass="<<mass<<" target mean support per step="<<2*mass*9.81f*dt
             <<" target top and bottom heights "<<bottom+spacing<<' '<<bottom
             <<" density at release="<<(kernel(0)+kernel(spacing))*mass<<'\n';
    for(int i=0;i<600;++i) {
        world.Step(dt,gravity,{floor},{},{},&reactions);
        const auto& p=world.Particles();
        const double support=std::accumulate(reactions.begin(),reactions.end(),0.0,
            [](double sum,const FluidContactImpulse& reaction){return sum-reaction.impulse.y;});
        std::cout<<i+1<<' '<<p[0].position.y<<' '<<p[1].position.y<<' '<<p[0].velocity.y<<' '<<p[1].velocity.y<<' '<<support<<'\n';
    }
}
