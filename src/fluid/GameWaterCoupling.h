#pragma once
#include "GameWater.h"
#include "PhysicsWorld.h"

namespace JudasWater {
struct BodyLoad {
    float immersedVolume = 0;
    glm::vec3 center{0}, force{0}, torque{0};
};
// Applies bounded-resolution buoyancy and implicit quadratic drag through MK2's
// ordinary force interface. No body pose/velocity scripting or animation.
BodyLoad ApplyBoxLoad(Grid &, PhysicsWorld &, BodyHandle, const glm::vec3 &half, float dt);
SolidBox ReadBox(const PhysicsWorld &, BodyHandle, const glm::vec3 &half);
std::vector<SolidBox> ReadBodySolids(const PhysicsWorld &, BodyHandle);
BodyLoad ApplyBodyLoad(Grid &, PhysicsWorld &, BodyHandle, float seconds);
} // namespace JudasWater
