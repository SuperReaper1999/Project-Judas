#include "PhysicsWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
int main(){for(float speed:{1.f,5.f,20.f,100.f})for(float spin:{0.f,5.f,20.f,100.f}){
PhysicsWorld w;w.Init();auto b=w.CreateDynamicBox({0,0,0},{.08f,.2f,.05f},1,0,1);w.CreateStaticBox({.6f,0,0},{.05f,1,1},0,1);w.CreateStaticBox({-.6f,0,0},{.05f,1,1},0,1);w.SetLinearVelocity(b,{speed,0,0});w.SetAngularVelocity(b,{0,0,spin});w.Step(1.f/60);
auto v=w.GetLinearVelocity(b);auto om=glm::inverse(w.GetTransform(b).rotation)*w.GetAngularVelocity(b);std::printf("VEL %.9g %.9g %.9g OMEGA %.9g %.9g %.9g ",v.x,v.y,v.z,om.x,om.y,om.z);glm::vec3 inertia((.2f*.2f+.05f*.05f)/3,(.08f*.08f+.05f*.05f)/3,(.08f*.08f+.2f*.2f)/3);double before=.5*speed*speed+.5*inertia.z*spin*spin,after=.5*glm::dot(v,v)+.5*glm::dot(inertia*om,om);std::printf("speed%g spin%g before%.9g after%.9g ratio%.9g hold%zu\n",speed,spin,before,after,after/before,w.LastStepStats().impactSafetyFallback);
}}
