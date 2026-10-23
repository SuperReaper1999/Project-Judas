// Fast iteration harness for physical balance stepping. Builds the controller
// development rigs in a bare PhysicsWorld (no renderer, assets or scripts) and
// reports measured outcomes for a corrective-step scenario family. It is a
// development instrument, not an acceptance suite: the acceptance checks stay
// in the controller and balance-lab programs.
//
// Usage: judas_balance_step_harness [legacy|capture] [rig 0|1|2] [impulse N s]
// Solver selection is the ordinary environment (JUDAS_PHYSICS_MK2,
// JUDAS_VALIDATION_VELOCITY_ITERATIONS). BALANCE_TRACE=1 prints per-frame state.
#include "PhysicalBalance.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
namespace {
struct Fixture {
 PhysicsWorld world;PhysicalBalanceController controller;BalanceSettings settings;
 std::vector<BalanceBody> mapping;std::vector<glm::vec3> gravity;
 glm::quat frame;BodyHandle ground;std::string error;
 Fixture(float fallen=0,glm::quat orientation=glm::quat(1,0,0,0),bool floor=true,bool ceiling=false,int variant=0):frame(orientation){
  const bool slender=variant==1;world.Init();
  if(floor)ground=world.CreateShape(Shape::Box({5,.2f,5}),{frame*glm::vec3(0,-.2f,0),frame},false,0,.8f,0);
  if(ceiling)world.CreateShape(Shape::Box({5,.2f,5}),{frame*glm::vec3(0,.8f,0),frame},false,0,.8f,0);
  glm::vec3 positions[]={{0,1.4f,0},{-.4f,1.05f,0},{-.4f,.65f,0},{-.4f,.3f,.08f},{.4f,1.05f,0},{.4f,.65f,0},{.4f,.3f,.08f}};
  glm::vec3 sizes[]={{.25f,.25f,.15f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.3f,.3f,.3f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.3f,.3f,.3f}};
  const int parents[]={-1,0,1,2,0,4,5};float masses[]={4,1,1,10,1,1,10};
  if(slender){positions[0]={0,1.42f,0};positions[1]={-.4f,.87f,0};positions[2]={-.4f,.47f,0};positions[3]={-.4f,.12f,.08f};positions[4]={.4f,.87f,0};positions[5]={.4f,.47f,0};positions[6]={.4f,.12f,.08f};sizes[0]={.25f,.4f,.15f};sizes[3]=sizes[6]={.35f,.12f,.38f};const float lightMasses[]={10,3,2,2,3,2,2};std::copy(lightMasses,lightMasses+7,masses);}
  if(variant==2){positions[0]={0,1.35f,0};positions[1]={-.34f,1.0f,0};positions[2]={-.34f,.62f,0};positions[3]={-.34f,.28f,.06f};positions[4]={.34f,1.0f,0};positions[5]={.34f,.62f,0};positions[6]={.34f,.28f,.06f};sizes[0]={.28f,.24f,.18f};sizes[1]=sizes[4]={.1f,.18f,.1f};sizes[2]=sizes[5]={.08f,.2f,.08f};sizes[3]=sizes[6]={.28f,.28f,.33f};const float differentMasses[]={3.5f,1.5f,.75f,12,1.5f,.75f,9};std::copy(differentMasses,differentMasses+7,masses);}

  auto rotation=glm::angleAxis(fallen,glm::vec3(1,0,0));float minimum=100;
  for(int i=0;i<7;i++){auto centre=rotation*(positions[i]-positions[0]);auto basis=glm::mat3_cast(rotation);for(int k=0;k<3;k++)basis[k]=glm::abs(basis[k]);minimum=std::min(minimum,centre.y-(basis*sizes[i]).y);}
  for(int i=0;i<7;i++){auto body=world.CreateShape(Shape::Box(sizes[i]),{frame*(rotation*(positions[i]-positions[0])+glm::vec3(0,-minimum+.02f,0)),frame*rotation},true,masses[i],.8f,0);mapping.push_back({body,{},parents[i],glm::quat(1,0,0,0)});}
  for(int i=1;i<7;i++){glm::vec3 point=positions[i];point.y+=(i==3||i==6)?(variant==2?.14f:.15f):(variant==2&&(i==1||i==4)?.18f:.2f);JointSettings j;j.type=(i==2||i==5)?JointType::Hinge:JointType::Ball;j.limits=true;j.lower=-2.7f;j.upper=2.7f;j.bodyA=mapping[i].body;j.bodyB=mapping[parents[i]].body;j.anchorA=point-positions[i];j.anchorB=point-positions[parents[i]];mapping[i].joint=world.CreateJoint(j);world.SetPairCollisionEnabled(j.bodyA,j.bodyB,false);}
  settings.standingHeight=variant==2?1.35f:(slender?1.42f:1.4f);if(slender)settings.touchdownTolerance=.2f;
  
  for(int f=0;f<2;f++){auto& e=settings.effectors[f];e.upper=f?4:1;e.lower=f?5:2;e.foot=f?6:3;e.contactOffset={0,-sizes[e.foot].y,0};e.halfExtents=sizes[e.foot];for(int a=0;a<3;a++)for(float sign:{-1.f,1.f}){glm::vec3 normal(0);normal[a]=sign;e.patches.push_back({normal*e.halfExtents,normal,(slender||variant==2)?.52f:.44f});}}
  gravity.assign(7,frame*glm::vec3(0,-9.81f,0));
  controller.Configure(world,mapping,settings,error);
 }
 void Advance(int count,bool control=true,bool applyGravity=true){for(int k=0;k<count;k++){if(control)controller.Prepare(world,gravity,1.f/60);if(applyGravity)for(size_t i=0;i<mapping.size();++i)world.ApplyLinearAcceleration(mapping[i].body,gravity[i],1.f/60);world.Step(1.f/60);}}
 bool Standing(){auto pose=world.GetTransform(mapping[0].body);float upright=glm::dot(pose.rotation*settings.up,frame*glm::vec3(0,1,0));float height=glm::dot(pose.position,frame*glm::vec3(0,1,0));float speed=0,mass=0;for(const auto& b:mapping){speed+=glm::length(world.GetLinearVelocity(b.body))*world.GetMass(b.body);mass+=world.GetMass(b.body);}return upright>.97f&&height>1.2f&&speed/mass<.1f;}
};
bool g_capture=false;
struct Outcome { bool standing=false;unsigned landings=0;float clearance=0,moved=0,minUp=1;int standFrame=-1;std::string reason; };
Outcome Push(int rig,glm::vec3 impulse,bool allowSteps,bool trace){
 Fixture f(0,glm::quat(1,0,0,0),true,false,rig);f.settings.captureStepping=g_capture;f.controller.Configure(f.world,f.mapping,f.settings,f.error);
 f.Advance(120,false);f.controller.Request(f.world,BalanceTask::Stand,allowSteps,true,f.error);
 for(int k=0;k<360;k++){f.Advance(1);if(std::getenv("BALANCE_TRACE_STAND")&&k%6==0){const auto& ob=f.controller.Observation();std::printf("S %3d stood %d up %.3f h %.3f v %.3f,%.3f,%.3f L %.2f/%.2f c %u/%u",k,int(f.controller.History().hasStood),ob.upright,ob.height,ob.velocity.x,ob.velocity.y,ob.velocity.z,ob.supports[0].load,ob.supports[1].load,ob.supports[0].contacts,ob.supports[1].contacts);for(size_t b=0;b<f.mapping.size();++b)std::printf(" %.2f",glm::length(f.world.GetLinearVelocity(f.mapping[b].body)));std::printf("\n");}}
 Outcome o;if(!f.Standing()){auto r=f.world.GetTransform(f.mapping[0].body);char b[160];std::snprintf(b,sizeof b," (root %.3f,%.3f,%.3f up %.3f speed %.3f)",r.position.x,r.position.y,r.position.z,(r.rotation*glm::vec3(0,1,0)).y,glm::length(f.world.GetLinearVelocity(f.mapping[0].body)));o.reason="did not stand before push: "+f.controller.History().reason+b;return o;}
 f.world.ApplyLinearImpulse(f.mapping[0].body,impulse);
 const auto& e=f.settings.effectors;glm::vec3 start[2]={f.world.GetTransform(f.mapping[e[0].foot].body).position,f.world.GetTransform(f.mapping[e[1].foot].body).position};
 for(int k=0;k<720;k++){f.Advance(1);
  for(int s=0;s<2;s++){auto p=f.world.GetTransform(f.mapping[e[s].foot].body).position;o.clearance=std::max(o.clearance,p.y-start[s].y);o.moved=std::max(o.moved,glm::length(p-start[s]));}
  auto root=f.world.GetTransform(f.mapping[0].body);o.minUp=std::min(o.minUp,(root.rotation*glm::vec3(0,1,0)).y);
  if(o.standFrame<0&&f.Standing()&&k>30)o.standFrame=k;
  if(trace){const auto& h=f.controller.History();const auto& ob=f.controller.Observation();auto c=ob.com;std::printf("T %3d st %.2f sw %2d land %u up %.3f h %.3f com %.3f,%.3f,%.3f v %.3f,%.3f c %u/%u L %.2f/%.2f root %.3f,%.3f,%.3f",k,h.swingTime,h.swing,h.landings,ob.upright,ob.height,c.x,c.y,c.z,ob.velocity.x,ob.velocity.z,ob.supports[0].contacts,ob.supports[1].contacts,ob.supports[0].load,ob.supports[1].load,root.position.x,root.position.y,root.position.z);auto f0=f.world.GetTransform(f.mapping[e[0].foot].body).position,f1=f.world.GetTransform(f.mapping[e[1].foot].body).position;std::printf("   feet %.3f,%.3f,%.3f  %.3f,%.3f,%.3f\n",f0.x,f0.y,f0.z,f1.x,f1.y,f1.z);}}
 o.standing=f.Standing();o.landings=f.controller.History().landings;o.reason=f.controller.History().reason;return o;
}
void Report(const char* name,const Outcome& o){std::printf("%-22s standing %d landings %u lift %.3f moved %.3f minUp %.3f stand@%d reason %s\n",name,int(o.standing),o.landings,o.clearance,o.moved,o.minUp,o.standFrame,o.reason.c_str());}
}
int main(int argc,char** argv){
 g_capture=argc>1&&!std::strcmp(argv[1],"capture");const int rig=argc>2?std::atoi(argv[2]):1;const float impulse=argc>3?float(std::atof(argv[3])):29.f;
 const bool trace=std::getenv("BALANCE_TRACE")!=nullptr;
 std::printf("controller %s rig %d impulse %.1f mk2 %d sweeps %s\n",g_capture?"capture":"legacy",rig,impulse,std::getenv("JUDAS_PHYSICS_MK2")!=nullptr,std::getenv("JUDAS_VALIDATION_VELOCITY_ITERATIONS")?std::getenv("JUDAS_VALIDATION_VELOCITY_ITERATIONS"):"default");
 int required=0,passed=0;
 // The slender rig (1) is the declared corrective-step comparison: the push
 // needs a real step and defeats in-place balance. Heavier-footed rigs only
 // have to remain or return to standing.
 auto allowed=Push(rig,{0,0,impulse},true,trace);Report("step forward",allowed);++required;passed+=rig==1?allowed.standing&&allowed.landings>=1&&allowed.moved>.15f&&allowed.clearance>.025f:allowed.standing;
 auto denied=Push(rig,{0,0,impulse},false,false);Report("denied forward",denied);if(rig==1){++required;passed+=!denied.standing&&denied.landings==0;}
 auto weak=Push(rig,{0,0,2},true,false);Report("weak push",weak);++required;passed+=weak.standing&&weak.landings==0;
 int directions=0;for(int a=1;a<8;a++){float t=a*.7853982f;auto o=Push(rig,{impulse*std::sin(t),0,impulse*std::cos(t)},true,false);char n[32];std::snprintf(n,sizeof n,"step %3d deg",a*45);Report(n,o);directions+=o.standing;}
 std::printf("HARNESS required %d/%d, other directions standing %d/7\n",passed,required,directions);
 return passed==required?0:1;
}
