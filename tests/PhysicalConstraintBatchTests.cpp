#include "PhysicsWorld.h"
#include "PhysicsConstraintBatch.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks=0;
void Check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
unsigned Owner(const PhysicsConstraintBatch& batch,BodyHandle handle){
 for(unsigned i=0;i<batch.bodies.size();++i)
  if(batch.bodies[i].handle.id==handle.id&&batch.bodies[i].handle.world==handle.world)return i;
 throw std::runtime_error("registered body absent from copied batch");
}
void Zero(const PhysicsConstraintBatch& batch,PhysicsConstraintProposal& out){
 out.linearImpulses.assign(batch.bodies.size(),glm::dvec3(0));
 out.angularImpulses.assign(batch.bodies.size(),glm::dvec3(0));
 out.contactImpulses.assign(batch.contacts.size(),0);
 out.jointImpulses.assign(batch.joints.size(),0);
}
void Pressure(bool supported,bool fixed,float angle){
 PhysicsWorld world;world.Init();
 const glm::quat q=glm::angleAxis(angle,glm::normalize(glm::vec3(1,2,-3)));
 const glm::vec3 up=q*glm::vec3(0,1,0);
 if(supported)world.CreateStaticBox(-.5f*up,q,{10,.5f,10},0,0);
 const auto body=world.CreateDynamicSphere(.5f*up,.5f,80,0,0);
 JointHandle joint;
 if(fixed){JointSettings settings;settings.bodyA=body;settings.anchorB=.5f*up;joint=world.CreateJoint(settings);Check(joint.IsValid(),"fixed guide exists");}
 // An unrelated body may share the floor but must not join this participant.
 const auto unrelated=world.CreateDynamicSphere(q*glm::vec3(3,.5f,0),.5f,5,0,0);
 double fluid=-1,pending=fluid;unsigned proposed=0,published=0;
 PhysicsConstraintParticipant participant;participant.bodies={body};
 participant.propose=[&](const auto& batch,auto& out){
  ++proposed;Check(batch.context.count==4,"native cadence is unchanged");
  const auto owner=Owner(batch,body);for(const auto& b:batch.bodies)Check(b.handle.id!=unrelated.id,"static floor did not bridge unrelated bodies");
  Zero(batch,out);
  const double speed=glm::dot(glm::dvec3(batch.bodies[owner].linearVelocity),glm::dvec3(up));
  const double mobility=supported||fixed?0:batch.bodies[owner].inverseMass;
  const double pressure=std::max(0.,speed-fluid)/(1./8000+mobility);
  out.linearImpulses[owner]=-glm::dvec3(up)*pressure;pending=fluid+pressure/8000;
  if(supported){Check(batch.contacts.size()==1,"native sphere floor constraint is present");out.contactImpulses[0]=float(pressure);}
  if(fixed)for(unsigned i=0;i<batch.joints.size();++i){const auto& row=batch.joints[i];
   const auto direction=row.a==owner?row.linearA:row.linearB;
   out.jointImpulses[i]=float(-glm::dot(out.linearImpulses[owner],glm::dvec3(direction)));}
  return true;
 };
 participant.published=[&](const auto&){++published;fluid=pending;};
 PhysicsWorld::VelocityCoupling coupling;
 coupling.physicalBatch=&participant;
 for(unsigned step=0;step<30;++step)world.Step(1.f/60,&coupling);
 Check(proposed==120&&published==120,"all native/material publications acknowledged once");
 const double actual=glm::dot(glm::dvec3(world.GetLinearVelocity(body)),glm::dvec3(up));
 Check(std::abs(fluid-actual)<2e-5,"actual endpoint matches the material boundary");
 if(supported||fixed)Check(glm::length(world.GetLinearVelocity(body))<2e-5,"native support/guide remains physically calm");
 else Check(std::abs(actual+8000./8080)<2e-5,"free coupled endpoint preserves momentum");
}
void Rejections(){
 for(unsigned mode=0;mode<5;++mode){
  PhysicsWorld w;w.Init();w.CreateStaticBox({0,-.5f,0},{10,.5f,10},mode==4?.5f:0,0);
  auto b=w.CreateDynamicSphere({0,.5f,0},.5f,80,mode==4?.5f:0,0);
  PhysicsConstraintParticipant p;p.bodies={b};unsigned published=0;
  p.propose=[&](const auto& batch,auto& out){Zero(batch,out);auto owner=Owner(batch,b);
   if(mode==0)out.linearImpulses.clear();
   if(mode==1)out.linearImpulses[owner].x=std::numeric_limits<double>::quiet_NaN();
   if(mode==2)out.contactImpulses[0]=-1;
   if(mode==3)out.linearImpulses[owner].y=-80; // Missing matching native reaction.
   return true;};
  p.published=[&](const auto&){++published;};PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;
  bool rejected=false;try{w.Step(1.f/60,&c);}catch(const std::runtime_error&){rejected=true;}
  Check(rejected&&published==0,"invalid/unsupported batch failed before material publication");
  Check(w.GetLinearVelocity(b)==glm::vec3(0),"rejected proposal did not write physical velocity");
  w.Step(1.f/60);Check(w.GetLinearVelocity(b)==glm::vec3(0),"rejection releases the native step guard");
 }
 PhysicsWorld w;w.Init();auto stale=w.CreateDynamicSphere({0,0,0},.5f,1,0,0);w.DestroyBody(stale);
 PhysicsConstraintParticipant p;p.bodies={stale};p.propose=[](const auto&,auto&){return true;};
 PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;bool rejected=false;
 try{w.Step(1.f/60,&c);}catch(const std::invalid_argument&){rejected=true;}
 Check(rejected,"stale generation rejected before stepping");
 PhysicsWorld other;other.Init();p.bodies={other.CreateDynamicSphere({0,0,0},.5f,1,0,0)};rejected=false;
 try{w.Step(1.f/60,&c);}catch(const std::invalid_argument&){rejected=true;}
 Check(rejected,"foreign world owner rejected");
}
void SpeculativeGap(){
 PhysicsWorld w;w.Init();w.CreateStaticBox({0,-.5f,0},{10,.5f,10},0,0);
 const auto body=w.CreateDynamicSphere({0,.5001f,0},.5f,80,0,0);w.SetLinearVelocity(body,{0,-.1f,0});
 PhysicsConstraintParticipant p;p.bodies={body};bool sawGap=false;unsigned publications=0;
 p.propose=[&](const auto& batch,auto& out){Zero(batch,out);const auto owner=Owner(batch,body);
  Check(batch.contacts.size()==1,"actual native speculative row is exposed");const auto& c=batch.contacts[0];
  if(c.separation>0){sawGap=true;Check(c.minimumNormalSpeed<0&&std::abs(c.minimumNormalSpeed+c.separation/batch.context.deltaTime)<1e-6,"native speculative speed allowance retained");}
  const auto direction=c.a==owner?c.normal:-c.normal;
  const double speed=glm::dot(glm::dvec3(c.normal),glm::dvec3(batch.bodies[c.a].linearVelocity-batch.bodies[c.b].linearVelocity));
  out.linearImpulses[owner]=-80.*glm::dvec3(direction);
  out.contactImpulses[0]=float(80+(c.minimumNormalSpeed-speed)/batch.bodies[owner].inverseMass);return true;};
 p.published=[&](const auto&){++publications;};PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;
 w.Step(1.f/60,&c);Check(sawGap&&publications==4,"speculative coupling executes at native boundaries");
 Check(w.GetTransform(body).position.y>=.5f-1e-6,"speculative coupling does not cross native floor");
}
void JointIdentityAndLifetime(){
 PhysicsWorld w;w.Init();const auto body=w.CreateDynamicSphere({0,0,0},.5f,80,0,0);
 JointSettings settings;settings.bodyA=body;const auto first=w.CreateJoint(settings),second=w.CreateJoint(settings);
 PhysicsConstraintParticipant p;p.bodies={body};unsigned seen=0;
 p.propose=[&](const auto& batch,auto&){unsigned a=0,b=0;for(const auto& row:batch.joints){a+=row.joint.id==first.id;b+=row.joint.id==second.id;}
  Check(a==6&&b==6,"distinct joints sharing endpoints retain their actual identities");++seen;return false;};
 PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;w.Step(1.f/60,&c);Check(seen==4,"identity snapshot is stable over native intervals");
 unsigned published=0;p.published=[&](const auto&){++published;};
 p.propose=[&](const auto& batch,auto& out){Zero(batch,out);w.DestroyJoint(first);return true;};
 bool rejected=false;try{w.Step(1.f/60,&c);}catch(const std::runtime_error&){rejected=true;}
 Check(rejected&&published==0,"joint destruction cannot redirect a staged reaction to another joint");
}
void TopologyMutation(){
 for(bool handled:{false,true}){
  PhysicsWorld w;w.Init();const auto body=w.CreateDynamicSphere({0,0,0},.5f,80,0,0);
  PhysicsConstraintParticipant p;p.bodies={body};unsigned published=0;
  p.propose=[&](const auto& batch,auto& out){Zero(batch,out);w.CreateDynamicSphere({3,0,0},.5f,1,0,0);return handled;};
  p.published=[&](const auto&){++published;};PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;
  bool rejected=false;try{w.Step(1.f/60,&c);}catch(const std::runtime_error&){rejected=true;}
  Check(rejected&&published==0,"topology mutation rejected even when proposer declines");
  Check(w.GetLinearVelocity(body)==glm::vec3(0),"topology rejection does not publish candidate velocity");
 }
}
void CoupledClock(){
 PhysicsWorld w;w.Init();const auto body=w.CreateDynamicSphere({0,0,0},.5f,80,0,0);w.SetLinearVelocity(body,{1,0,0});
 PhysicsConstraintParticipant p;p.bodies={body};unsigned published=0;double time=0;
 p.propose=[&](const auto& b,auto& out){Zero(b,out);return true;};
 p.published=[&](const auto& c){++published;time+=c.deltaTime;};PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;
 w.StepCoupled(1.f/60,3,{},{},&c);
 Check(published==12&&std::abs(time-1.f/60)<1e-8,"native material clock spans every coupled interval");
 Check(std::abs(w.GetTransform(body).position.x-1.f/60)<1e-6,"coupled projection retains physical displacement");
 c.solve=[](float,unsigned){};bool incompatible=false;
 try{w.Step(1.f/60,&c);}catch(const std::invalid_argument&){incompatible=true;}
 Check(incompatible,"unqualified legacy velocity mutation composition rejected");c.solve={};
 for(float dt:{0.f,-.01f,std::numeric_limits<float>::quiet_NaN()}){bool rejected=false;
  try{w.Step(dt,&c);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"invalid material interval rejected before integration");}
}
void OffsetAnchor(){
 PhysicsWorld w;w.Init();CompoundBox child;child.localCenter={.3f,-.1f,.2f};child.halfExtents={.5f,.5f,.5f};child.key=1;
 auto shape=Shape::Compound({child});const auto offset=child.localCenter;
 const auto body=w.CreateShape(shape,{{2,3,4},glm::quat(1,0,0,0)},true,80,0,0);
 JointSettings joint;joint.bodyA=body;joint.anchorA=offset;joint.anchorB=w.GetCenterOfMass(body);
 Check(glm::length(w.GetCenterOfMass(body)-w.GetTransform(body).position)>.3f,"offset fixture has real nonzero authored-pivot/COM displacement");w.CreateJoint(joint);
 PhysicsConstraintParticipant p;p.bodies={body};unsigned snapshots=0;
 p.propose=[&](const auto& batch,auto&){const auto owner=Owner(batch,body);Check(batch.bodies[owner].center==w.GetCenterOfMass(body),"snapshot exposes actual COM rather than authored pivot");
  for(const auto& row:batch.joints)if(glm::length(row.linearA)>0)Check(glm::length(row.angularA)<1e-6,"prepared joint retains nonzero shape pivot-offset correction");
  ++snapshots;return false;};PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;w.Step(1.f/60,&c);
 Check(snapshots==4,"offset fixture visits every native snapshot");
}
void DeclineAndWake(){
 PhysicsWorld w;w.Init();w.CreateStaticBox({0,-.5f,0},{10,.5f,10},0,0);
 const auto body=w.CreateDynamicSphere({0,.5f,0},.5f,80,0,0);
 for(unsigned i=0;i<180;++i)w.Step(1.f/60);
 Check(w.IsSleeping(body),"wake regression begins from a genuinely sleeping body");
 PhysicsConstraintParticipant p;p.bodies={body};unsigned calls=0,published=0;
 p.propose=[&](const auto& b,auto&){++calls;Check(b.bodies[Owner(b,body)].inverseMass>0,"material owner wakes before predictor construction");return false;};
 p.published=[&](const auto&){++published;};PhysicsWorld::VelocityCoupling c;c.physicalBatch=&p;
 w.Step(1.f/60,&c);Check(calls==4&&published==0,"declined proposals never commit foreign state");
 Check(w.GetLinearVelocity(body)==glm::vec3(0),"decline retains ordinary native response");
 p.maximumBodies=1;bool rejected=false;try{w.Step(1.f/60,&c);}catch(const std::runtime_error&){rejected=true;}
 Check(rejected,"connected static boundary counts toward declared resource bound");
}
}
int main(){try{
 for(float angle:{0.f,.7f}){Pressure(false,false,angle);Pressure(false,true,angle);}
 Pressure(true,false,0);Rejections();DeclineAndWake();SpeculativeGap();JointIdentityAndLifetime();TopologyMutation();CoupledClock();OffsetAnchor();
 std::printf("SUMMARY checks=%u failures=0\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,e.what());return 1;}}
