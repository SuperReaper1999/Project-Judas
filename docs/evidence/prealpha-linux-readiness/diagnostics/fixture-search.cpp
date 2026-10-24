#include "PhysicsWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
int main(){for(int variant=0;variant<5;++variant)for(float speed:{10.f,100.f,1000.f,5000.f})for(float omega:{0.f,100.f,1000.f}){
 PhysicsWorld w;w.Init();w.CreateStaticBox({.6f,0,0},{.05f,1,1},0,1);if(variant!=4)w.CreateStaticBox({-.6f,0,0},{.05f,1,1},0,1);
 auto h=variant==0?w.CreateDynamicSphere({0,0,0},.05f,1,0,1):w.CreateDynamicBox({0,0,0},{.08f,.2f,.05f},1,0,1);
 if(variant==2)w.ResetBody(h,{0,0,0},glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3))));
 if(variant==3){w.SetMotionType(h,BodyMotionType::Kinematic);w.SetKinematicVelocity(h,{speed,0,0},{0,0,omega});h=w.CreateDynamicSphere({.3f,0,0},.05f,1,0,1);}else{w.SetLinearVelocity(h,{speed,0,0});w.SetAngularVelocity(h,{0,0,omega});}
 w.Step(1.f/60);auto s=w.LastStepStats();if(s.impactSafetyFallback||s.impactMissingManifolds||s.impactEnergyRejections)std::printf("v%d speed%g spin%g holds%zu body%zu step%zu response%zu attempts%zu missing%zu energy%zu pos%g vel%g\n",variant,speed,omega,s.impactSafetyFallback,s.impactBodyCapHolds,s.impactStepCapHolds,s.impactResponseHolds,s.impactResponseAttempts,s.impactMissingManifolds,s.impactEnergyRejections,w.GetTransform(h).position.x,w.GetLinearVelocity(h).x);
}}
