#include "PhysicsWorld.h"
#include "PhysicalAnimation.h"
#include <cstdio>
#include <algorithm>
int main(int argc,char** argv){
 PhysicsWorld world;world.Init();world.SetSleepingEnabled(false);
 world.CreateShape(Shape::Box({5,.2f,5}),{{0,-.2f,0},{1,0,0,0}},false,0,.8f,0);
 std::vector<BodyHandle> bodies;std::vector<int> parents{-1,0,1,2,0,4,5};
 const glm::vec3 positions[]={{0,1.2f,0},{-.18f,.85f,0},{-.18f,.45f,0},{-.18f,.1f,.08f},{.18f,.85f,0},{.18f,.45f,0},{.18f,.1f,.08f}};
 const glm::vec3 sizes[]={{.25f,.25f,.15f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.12f,.1f,.24f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.12f,.1f,.24f}};
 const float masses[]={20,4,2,1,4,2,1};
 for(int i=0;i<7;i++) bodies.push_back(world.CreateShape(Shape::Box(sizes[i]),{positions[i],{1,0,0,0}},true,masses[i],.8f,0));
 for(int i=1;i<7;i++){const auto p=parents[i];glm::vec3 point=positions[i];point.y+=(i==3||i==6)?.15f:.2f;JointSettings j;j.type=JointType::Ball;j.bodyA=bodies[i];j.bodyB=bodies[p];j.anchorA=point-positions[i];j.anchorB=point-positions[p];world.CreateJoint(j);world.SetPairCollisionEnabled(j.bodyA,j.bodyB,false);}
 for(int a=0;a<7;a++)for(int b=a+1;b<7;b++)world.SetPairCollisionEnabled(bodies[a],bodies[b],false);
 PhysicalAnimationRegion drive;drive.stiffness=800;drive.damping=80;drive.maxTorque=150;
 bool control=argc<2;
 for(int k=0;k<600;k++){
  float dt=1.f/60;glm::vec3 com(0),v(0);float mass=0;
  for(int i=0;i<7;i++){com+=world.GetTransform(bodies[i]).position*masses[i];v+=world.GetLinearVelocity(bodies[i])*masses[i];mass+=masses[i];}com/=mass;v/=mass;
  auto support=(world.GetTransform(bodies[3]).position+world.GetTransform(bodies[6]).position)*.5f;
  glm::vec3 error=com-support;error.y=0;glm::vec3 vel=v;vel.y=0;
  // Joint-only virtual torque: reactions traverse the articulated chain.
  glm::vec3 torque=glm::cross(world.GetTransform(bodies[0]).rotation*glm::vec3(0,1,0),glm::vec3(0,1,0))*800.f-world.GetAngularVelocity(bodies[0])*180.f + glm::cross(glm::vec3(0,1,0),-(error*60.f+vel*20.f));
  if(glm::length(torque)>150)torque*=150/glm::length(torque);
  for(int i=0;i<7;i++)world.ApplyLinearAcceleration(bodies[i],{0,-9.81f,0},dt);
  for(int i=1;i<7;i++){auto p=parents[i];auto a=world.GetTransform(bodies[i]),b=world.GetTransform(bodies[p]);auto t=CalculatePhysicalTorque({1,0,0,0},a.rotation,b.rotation,world.GetAngularVelocity(bodies[i]),world.GetAngularVelocity(bodies[p]),world.GetInertiaWorld(bodies[i]),glm::inverse(world.GetInertiaWorld(bodies[p])),drive,dt).torque;
   if(control)t-=torque*.5f;
   if(glm::length(t)>150)t*=150/glm::length(t);
   world.ApplyTorque(bodies[i],t);world.ApplyTorque(bodies[p],-t);}
  if(k==120)world.ApplyImpulseAtPoint(bodies[0],{5,0,0},world.GetTransform(bodies[0]).position);
  world.Step(dt);
  if(k%60==0)std::printf("step=%d rootHeight=%.4f com=%.4f,%.4f,%.4f speed=%.4f up=%.4f contacts=%zu\n",k,world.GetTransform(bodies[0]).position.y,com.x,com.y,com.z,glm::length(v),(world.GetTransform(bodies[0]).rotation*glm::vec3(0,1,0)).y,world.LastStepContactCount());
 }
}
