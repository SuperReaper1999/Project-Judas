#include "PhysicsWorld.h"
#include "PhysicalAnimation.h"
#include <cstdio>
#include <algorithm>
int main(int argc,char** argv){
 PhysicsWorld world;world.Init();world.SetSleepingEnabled(false);
 world.CreateShape(Shape::Box({50,.2f,50}),{{0,-.2f,0},{1,0,0,0}},false,0,.8f,0);
 std::vector<JointHandle> joints;std::vector<BodyHandle> bodies;std::vector<int> parents{-1,0,1,2,0,4,5,0,7,8,0,10,11};
 const glm::vec3 positions[]={{0,1.2f,0},{-.18f,.85f,0},{-.18f,.45f,0},{-.18f,.1f,.08f},{.18f,.85f,0},{.18f,.45f,0},{.18f,.1f,.08f},{-.4f,1.1f,.15f},{-.4f,.8f,.3f},{-.4f,.55f,.4f},{.4f,1.1f,.15f},{.4f,.8f,.3f},{.4f,.55f,.4f}};
 const glm::vec3 sizes[]={{.25f,.25f,.15f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.12f,.1f,.24f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.12f,.1f,.24f},{.08f,.18f,.08f},{.07f,.17f,.07f},{.13f,.08f,.13f},{.08f,.18f,.08f},{.07f,.17f,.07f},{.13f,.08f,.13f}};
 const float masses[]={20,4,2,1,4,2,1,2,1,1,2,1,1};
 const float initialAngle=argc>1?std::atof(argv[1]):0;
 auto initial=glm::angleAxis(initialAngle,glm::vec3(1,0,0));
 float minimum=100;for(int i=0;i<13;i++){auto c=initial*(positions[i]-positions[0]);auto m=glm::mat3_cast(initial);for(int k=0;k<3;k++)m[k]=glm::abs(m[k]);auto ext=m*sizes[i];minimum=std::min(minimum,c.y-ext.y);}
 for(int i=0;i<13;i++) bodies.push_back(world.CreateShape(Shape::Box(sizes[i]),{initial*(positions[i]-positions[0])+glm::vec3(0,-minimum+.02f,0),initial},true,masses[i],.8f,0));
 for(int i=1;i<13;i++){const auto p=parents[i];glm::vec3 point=positions[i];point.y+=(i==3||i==6)?.15f:.2f;JointSettings j;j.type=(i==2||i==5||i==8||i==11)?JointType::Hinge:JointType::Ball;j.limits=true;j.lower=-2.7f;j.upper=2.7f;j.bodyA=bodies[i];j.bodyB=bodies[p];j.anchorA=point-positions[i];j.anchorB=point-positions[p];joints.push_back(world.CreateJoint(j));world.SetPairCollisionEnabled(j.bodyA,j.bodyB,false);}
 for(int a=0;a<13;a++)for(int b=a+1;b<13;b++)world.SetPairCollisionEnabled(bodies[a],bodies[b],false);
 PhysicalAnimationRegion drive;drive.stiffness=800;drive.damping=80;drive.maxTorque=150;
 bool control=true;std::vector<glm::quat> initialTargets(13,glm::quat(1,0,0,0));
 for(int k=0;k<1200;k++){
  float dt=1.f/60;glm::vec3 com(0),v(0);float mass=0;
  for(int i=0;i<13;i++){com+=world.GetTransform(bodies[i]).position*masses[i];v+=world.GetLinearVelocity(bodies[i])*masses[i];mass+=masses[i];}com/=mass;v/=mass;
  auto support=(world.GetTransform(bodies[3]).position+world.GetTransform(bodies[6]).position)*.5f;
  glm::vec3 error=com-support;error.y=0;glm::vec3 vel=v;vel.y=0;
  // Joint-only virtual torque: reactions traverse the articulated chain.
  glm::vec3 torque=glm::cross(world.GetTransform(bodies[0]).rotation*glm::vec3(0,1,0),glm::vec3(0,1,0))*800.f-world.GetAngularVelocity(bodies[0])*180.f + glm::cross(glm::vec3(0,1,0),-(error*60.f+vel*20.f));
  if(glm::length(torque)>150)torque*=150/glm::length(torque);
  for(int i=0;i<13;i++)world.ApplyLinearAcceleration(bodies[i],{0,-9.81f,0},dt);
  // Experimental reference angles; only dynamic-to-dynamic angular drives.
  float hip=0,knee=0,rootAngle=0;
  if(initialAngle!=0){float t=std::clamp(float(k-600)/360,0.f,1.f);rootAngle=initialAngle*(1-t);hip=-initialAngle*(1-t);knee=initialAngle*(1-t);}
  auto referenceRoot=glm::angleAxis(rootAngle,glm::vec3(1,0,0));
  auto thigh=glm::angleAxis(hip,glm::vec3(1,0,0));auto shin=glm::angleAxis(hip+knee,glm::vec3(1,0,0));
  for(int i=1;i<13;i++){JointAngularDrive d;d.enabled=k>=120;d.stiffness=800;d.damping=100;d.maxTorque=150;
    if(i==1||i==4||i==7||i==10)d.target=glm::inverse(thigh);
    if(i==2||i==5)d.target=glm::inverse(shin)*thigh;
    if(i==3||i==6||i==9||i==12){
      auto e=glm::cross(glm::vec3(0,1,0),-(error*.8f+vel*.3f));auto n=glm::length(e);
      auto correction=n>.00001f?glm::angleAxis(std::min(n,.25f),e/n):glm::quat(1,0,0,0);
      // Physical foot orientation supplies world reference for leg effort.
      auto actualFoot=world.GetTransform(bodies[i]).rotation;
      d.target=glm::inverse(actualFoot)*correction*referenceRoot*(i<7?shin:thigh);
    }
    if(k==120)initialTargets[i]=glm::inverse(world.GetTransform(bodies[i]).rotation)*world.GetTransform(bodies[parents[i]]).rotation;
    d.target=glm::slerp(initialTargets[i],d.target,std::clamp(float(k-120)/180,0.f,1.f));
    world.SetJointAngularDrive(joints[i-1],d);
  }
  world.Step(dt);
  if(k%120==0){for(int i: {0,1,2,6,7,8}){JointState j;world.GetJoint(joints[i],j);std::printf("JOINT %d torque=%.3f,%.3f,%.3f active=%d target=%.2f,%.2f,%.2f,%.2f\n",i,j.angularDrive.impulses.x/dt,j.angularDrive.impulses.y/dt,j.angularDrive.impulses.z/dt,j.active,j.angularDrive.target.w,j.angularDrive.target.x,j.angularDrive.target.y,j.angularDrive.target.z);}}

  if(k%120==0){for(int i: {0,3,6,9,12}){std::vector<PhysicsWorld::BodyContactSample> sample;bool valid=world.ReadBodyContactSample(bodies[i],sample);auto t=world.GetTransform(bodies[i]);float load=0;for(auto c:sample)load+=c.normalImpulse;std::printf("BODY %d step=%d pos=%.3f,%.3f,%.3f up=%.3f contacts=%zu valid=%d impulse=%.3f\n",i,k,t.position.x,t.position.y,t.position.z,(t.rotation*glm::vec3(0,1,0)).y,sample.size(),valid,load);}}

  if(k%60==0)std::printf("step=%d rootHeight=%.4f com=%.4f,%.4f,%.4f speed=%.4f up=%.4f contacts=%zu\n",k,world.GetTransform(bodies[0]).position.y,com.x,com.y,com.z,glm::length(v),(world.GetTransform(bodies[0]).rotation*glm::vec3(0,1,0)).y,world.LastStepContactCount());
 }
}
