#pragma once
#include "PhysicsWorld.h"

namespace JudasDeformables {
inline double EffectiveMass(PhysicsWorld& world,BodyHandle body,glm::dvec3 point,glm::dvec3 normal){
    if(!world.IsDynamicBody(body))return 0;
    const double mass=world.GetMass(body);if(mass<=0)return 0;
    const auto arm=point-glm::dvec3(world.GetCenterOfMass(body));
    const auto angular=glm::cross(arm,normal);
    return 1/mass+glm::dot(angular,glm::dmat3(glm::inverse(world.GetInertiaWorld(body)))*angular);
}
inline glm::dvec3 PointVelocity(PhysicsWorld& world,BodyHandle body,glm::dvec3 point){
    return glm::dvec3(world.GetLinearVelocity(body))+glm::cross(glm::dvec3(world.GetAngularVelocity(body)),point-glm::dvec3(world.GetCenterOfMass(body)));
}
inline void ApplyReaction(PhysicsWorld& world,BodyHandle body,glm::dvec3 impulse,glm::dvec3 point){
    // Preserve the local lever arm before crossing the binary32 MK2 impulse API.
    const auto moment=glm::cross(point-glm::dvec3(world.GetCenterOfMass(body)),impulse);
    const auto linear=glm::vec3(impulse),angular=glm::vec3(moment);
    if(glm::length(linear)>1e-8f||glm::length(angular)>1e-8f)world.Wake(body);
    world.ApplyLinearImpulse(body,linear,false);world.ApplyAngularImpulse(body,angular,false);
}
}
