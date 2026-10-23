#pragma once
// Small frame helpers shared by the balance controllers.
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "PhysicsWorld.h"
namespace PhysicalBalanceMath {
inline glm::vec3 Tangent(glm::vec3 v,glm::vec3 n){return v-n*glm::dot(v,n);}
inline glm::quat Align(glm::vec3 from,glm::vec3 to,glm::vec3 fallback){
    from=glm::normalize(from);to=glm::normalize(to);
    if(glm::dot(from,to)<-.9999f){auto axis=Tangent(fallback,from);if(glm::dot(axis,axis)<1e-8f)axis=Tangent(glm::vec3(from.z,from.x,from.y),from);return glm::angleAxis(3.1415926536f,glm::normalize(axis));}
    return glm::normalize(glm::quat(1+glm::dot(from,to),glm::cross(from,to)));
}
inline glm::vec3 Local(BodyTransform t,glm::vec3 p){return glm::inverse(t.rotation)*(p-t.position);}
inline glm::vec3 World(BodyTransform t,glm::vec3 p){return t.position+t.rotation*p;}
inline glm::quat LinkFrame(glm::vec3 localDirection,glm::vec3 localAxis,glm::vec3 direction,glm::vec3 axis){
    auto y=glm::normalize(localDirection),x=glm::normalize(Tangent(localAxis,y));
    glm::mat3 local(x,y,glm::cross(x,y));
    y=glm::normalize(direction);x=glm::normalize(Tangent(axis,y));
    glm::mat3 target(x,y,glm::cross(x,y));
    return glm::normalize(glm::quat_cast(target*glm::transpose(local)));
}
}
