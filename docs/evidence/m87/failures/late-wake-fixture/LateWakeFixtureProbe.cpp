#include "PhysicsWorld.h"
#include "SaveArchive.h"
#include <cstdio>
constexpr float dt=1.f/60;
void run(float half){
 PhysicsWorld world;world.Init();auto a=world.CreateDynamicBox({0,3,0},{half,half,half},2,0,0),b=world.CreateDynamicBox({1,3,0},{half,half,half},1,0,0);
 JointSettings s;s.type=JointType::Fixed;s.bodyA=a;s.anchorB={0,3,0};auto anchor=world.CreateJoint(s);s={};s.type=JointType::Fixed;s.bodyA=a;s.bodyB=b;s.anchorA={.5f,0,0};s.anchorB={-.5f,0,0};world.CreateJoint(s);
 for(unsigned i=0;i<180;++i){world.ApplyLinearAcceleration(a,{0,-9.81f,0},dt);world.ApplyLinearAcceleration(b,{0,-9.81f,0},dt);world.Step(dt);}
 for(auto h:{a,b}){SaveArchive save;world.PersistSleep(save,h);SaveArchive read(save.bytes);bool asleep=false,deferred=false;float quiet=0;glm::vec3 acceleration;read(asleep,quiet,acceleration,deferred);auto pose=world.GetTransform(h);auto v=world.GetLinearVelocity(h),w=world.GetAngularVelocity(h);std::printf("FIXTURE half=%.9g body=%u sleeping=%d quiet=%.9g p=(%.9g,%.9g,%.9g) v=(%.9g,%.9g,%.9g) w=(%.9g,%.9g,%.9g)\n",half,h.id,int(asleep),quiet,pose.position.x,pose.position.y,pose.position.z,v.x,v.y,v.z,w.x,w.y,w.z);}
 JointState state;world.GetJoint(anchor,state);std::printf("ANCHOR reaction=(%.9g,%.9g,%.9g)\n",state.reactionImpulse.x,state.reactionImpulse.y,state.reactionImpulse.z);
}
int main(){run(.1f);run(.2f);return 0;}
