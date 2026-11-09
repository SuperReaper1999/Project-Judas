#pragma once

#include "PhysicsHandles.h"
#include <glm/glm.hpp>

namespace JudasVehicles {
struct WheelRow {
    glm::vec3 direction{0}, chassisAngular{0}, groundAngular{0};
    float spin=0, inverseMass=0;
};
// Scratch belongs to one wheel and one physical interval. No native pointers,
// shared ownership, saved contact handles or second copy of body velocities.
struct WheelConstraintState {
    BodyHandle chassis;
    glm::vec3 axle{0};
    WheelRow normal, longitudinal, lateral;
    float dt=0, normalImpulse=0, stopImpulse=0, longImpulse=0, lateralImpulse=0;
    float brakeImpulse=0, brakeLimit=0, shaftInverse=0;
    float gamma=0, bias=0, stopBias=0, tangentCrossMass=0, friction=0;
    bool active=false, sleeping=false, tangent=false;
};
}
