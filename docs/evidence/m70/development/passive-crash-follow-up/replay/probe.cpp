// Focused numerical diagnostic. Private access exists only in this probe;
// the shipping runtime and public JudasJS surface are unchanged. The captured
// anchors are already after the ordinary velocity solve. Re-solving that phase
// masks the exact failure, so this fixture invokes the authoritative CCD phase.
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <string>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define private public
#include "PhysicsWorld.h"
#undef private
#include "PhysicsWorld.cpp"
#include "third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <chrono>
using Json=nlohmann::json;
glm::vec3 V(const Json& j){return {j.at(0).get<float>(),j.at(1).get<float>(),j.at(2).get<float>()};}
glm::quat Q(const Json& j){return {j.at(0).get<float>(),j.at(1).get<float>(),j.at(2).get<float>(),j.at(3).get<float>()};}
int main(int argc,char** argv){
 if(argc!=2)return 2;Json data;std::ifstream(argv[1])>>data;PhysicsWorld world;world.Init();std::map<unsigned,BodyHandle> bodies;
 for(const auto& b:data.at("bodies")){
  const auto& shapeJson=b.at("shape");Shape shape;shape.type=static_cast<ShapeType>(shapeJson.at("type").get<int>());shape.radius=shapeJson.at("radius").get<float>();shape.halfHeight=shapeJson.at("halfHeight").get<float>();shape.halfExtents=V(shapeJson.at("halfExtents"));shape.pivotOffset=V(shapeJson.at("pivotOffset"));
  if(shape.type!=ShapeType::Box&&shape.type!=ShapeType::Sphere&&shape.type!=ShapeType::Capsule){std::cerr<<"unsupported fixture shape\n";return 2;}
  const auto& pose=b.contains("startAnchor")?b.at("startAnchor"):b.at("current");BodyTransform t{V(pose.at("position")),Q(pose.at("rotation"))};const bool dynamic=b.at("isDynamic").get<bool>();const float invMass=b.at("current").at("inverseMass").get<float>();
  auto handle=b.at("queryOnly").get<bool>()?world.CreateQueryCapsule(shape.radius,shape.halfHeight,t):world.CreateShape(shape,t,dynamic,invMass>0?1/invMass:0,b.at("friction").get<float>(),b.at("restitution").get<float>());
  if(!handle.IsValid())return 2;bodies[b.at("handle").get<unsigned>()]=handle;
  if(dynamic){glm::mat3 inverse(0);const auto& inertia=b.at("current").at("inverseInertiaLocal");for(int c=0;c<3;++c)inverse[c]=V(inertia.at(c));if(!world.SetMassDistribution(handle,1/invMass,glm::inverse(inverse)))return 2;world.SetLinearVelocity(handle,V(pose.at("linearVelocity")));world.SetAngularVelocity(handle,V(pose.at("angularVelocity")));}
  world.SetCollisionFilter(handle,b.at("collisionLayer").get<unsigned>(),b.at("collisionMask").get<CategoryMask>());world.SetBodyTags(handle,b.at("tags").get<CategoryMask>());world.SetBodySensor(handle,b.at("sensor").get<bool>());world.SetBodyQueriesEnabled(handle,b.at("queriesEnabled").get<bool>());world.SetBodyEnabled(handle,b.at("enabled").get<bool>());
 }
 for(const auto& p:data.at("suppressedPairs"))world.SetPairCollisionEnabled(bodies.at(p.at("a").get<unsigned>()),bodies.at(p.at("b").get<unsigned>()),false);
 for(const auto& j:data.at("joints")){
  const auto& p=j.at("settings");JointSettings s;s.type=static_cast<JointType>(p.at("type").get<int>());s.bodyA=bodies.at(p.at("bodyA").get<unsigned>());const auto other=p.at("bodyB").get<unsigned>();if(other)s.bodyB=bodies.at(other);s.anchorA=V(p.at("anchorA"));s.anchorB=V(p.at("anchorB"));s.frameA=Q(p.at("frameA"));s.frameB=Q(p.at("frameB"));
  s.enabled=p.at("enabled").get<bool>();s.limits=p.at("limits").get<bool>();s.motor=p.at("motor").get<bool>();s.spring=p.at("spring").get<bool>();s.lower=p.at("lower").get<float>();s.upper=p.at("upper").get<float>();s.speed=p.at("speed").get<float>();s.maxForce=p.at("maxForce").get<float>();s.rest=p.at("rest").get<float>();s.stiffness=p.at("stiffness").get<float>();s.damping=p.at("damping").get<float>();s.rotationalResistance=p.at("rotationalResistance").get<float>();if(!world.CreateJoint(s).IsValid())return 2;
 }
 std::cout<<"Restored "<<world.AliveBodyCount()<<" bodies / "<<data.at("joints").size()<<" joints; CCD-only fixture setup, no extra gravity or drive forces; joint warm cache not restored.\n"<<std::flush;
 auto& w=*world.m_impl;const float dt=data.at("stepDuration").get<float>();
 for(const auto& b:data.at("bodies")){
  const auto handle=bodies.at(b.at("handle").get<unsigned>());auto& native=w.bodies[handle.id&PhysicsWorld::Impl::kSlotMask];
  const auto& pose=b.contains("startAnchor")?b.at("startAnchor"):b.at("current");
  native.rigidBody.position=V(pose.at("position"));native.rigidBody.orientation=Q(pose.at("rotation"));native.rigidBody.linearVelocity=V(pose.at("linearVelocity"));native.rigidBody.angularVelocity=V(pose.at("angularVelocity"));native.rigidBody.inverseMass=b.at("current").at("inverseMass").get<float>();
  for(int c=0;c<3;++c)native.rigidBody.inverseInertiaLocal[c]=V(b.at("current").at("inverseInertiaLocal").at(c));
  native.previousPosition=native.rigidBody.position;native.previousOrientation=native.rigidBody.orientation;native.sleeping=b.at("sleeping").get<bool>();native.deferredAcceleration=b.at("deferredAcceleration").get<bool>();native.lastAcceleration=V(b.at("lastAcceleration"));native.motion.Clear();
  w.CoverStepReach(native,dt);
 }
 w.stats={};w.GenerateCandidatePairs();w.separatedPairs.clear();
 for(auto pair:w.candidatePairs)for(int pa=0;pa<PrimitiveCount(w.bodies[pair.first].shape);++pa)for(int pb=0;pb<PrimitiveCount(w.bodies[pair.second].shape);++pb)w.separatedPairs.push_back({pair.first,pair.second,unsigned(pa),unsigned(pb)});
 w.supportPairs.clear();for(const auto& contact:data.at("startContacts"))if(!contact.at("newImpact").get<bool>())w.supportPairs.push_back({contact.at("a").get<unsigned>(),contact.at("b").get<unsigned>(),0,0});std::sort(w.supportPairs.begin(),w.supportPairs.end());w.supportPairs.erase(std::unique(w.supportPairs.begin(),w.supportPairs.end()),w.supportPairs.end());
 std::cout<<"CCD-only whitebox: "<<w.candidatePairs.size()<<" candidates, "<<w.supportPairs.size()<<" original supports; exact first anchors/inertia; shadowed RigidMotion header (see run metadata).\n"<<std::flush;
 auto start=std::chrono::steady_clock::now();w.AdvanceImpacts(dt);auto end=std::chrono::steady_clock::now();auto stats=world.LastStepStats();Json result={{"elapsedMs",std::chrono::duration<double,std::milli>(end-start).count()},{"events",stats.impactEvents},{"queries",stats.impactQueries},{"searchIterations",stats.impactSearchIterations},{"samplingFallbacks",stats.impactSamplingFallbacks},{"caps",stats.impactEventCapFallback},{"motionSegments",stats.motionSegments}};std::cout<<result.dump()<<'\n';world.Shutdown();
}
