#include "FluidWorld.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdio>

int main() {
    const struct Zero : GravityField { glm::vec3 Sample(const glm::vec3&) const override {return {};} } zeroGravity;
    FluidSettings settings;
    settings.smoothingRadius=.42f;
    settings.particleRadius=.072f;
    settings.maxDensityCorrection=.1f;
    const glm::quat rotation=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
    for(const bool rotate:{false,true}) {
        for(const float spacing:{.2f,.19f}) {
            FluidWorld world(settings);
            for(int z=-2;z<=2;++z)for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){
                auto position=spacing*glm::vec3(x,y,z);
                if(rotate)position=rotation*position;
                world.AddParticle(position,{},8.f);
            }
            world.Step(1.f/30,zeroGravity,{});
            const auto d=world.GetDiagnostics();
            std::printf("BLOCK rotate=%d spacing=%.9g mass=%.9g energy=%.9g momentum=%.9g %.9g %.9g\n",
                rotate,spacing,d.totalMass,d.kineticEnergy,d.totalMomentum.x,d.totalMomentum.y,d.totalMomentum.z);
        }
    }
}
