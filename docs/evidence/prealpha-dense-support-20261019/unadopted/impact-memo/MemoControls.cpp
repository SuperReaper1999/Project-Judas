#include "RigidBody.h"
#include "ContactSolver.h"
#include "JointSolver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <iostream>
#include <string>
#include <vector>
#define private public
#include "ImpactSolver.h"
#undef private
#include "ContactCapture.h"

namespace {
using contact_capture::Json;
int checks=0, failures=0;
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
RigidBody Sphere(float mass,glm::vec3 p,glm::vec3 v){RigidBody b;b.position=p;b.linearVelocity=v;b.inverseMass=1/mass;b.inverseInertiaLocal=SolidSphereInverseInertia(mass,.5f);return b;}
ImpactContact Pair(RigidBody& a,RigidBody& b){return {&a,&b,SphereVsSphere(a.position,.5f,b.position,.5f),.4f,.7f,false};}
Json Snapshot(ImpactSolver& solver,const ImpactResult& result,std::vector<RigidBody*>& bodies){
 contact_capture::Record identity;
 for(std::size_t i=0;i<bodies.size();++i){identity.bodies.push_back(bodies[i]);identity.bodyIndex.emplace(bodies[i],i);}
 Json out={{"result",contact_capture::Result(result)},{"bodies",Json::array()},{"rows",Json::array()},{"saved_order",Json::array()}};
 for(auto* b:bodies)out["bodies"].push_back(contact_capture::Body(*b));
 for(const auto& row:solver.Constraints())out["rows"].push_back(contact_capture::Constraint(row,identity));
 for(const auto& saved:solver.m_saved)out["saved_order"].push_back(identity.bodyIndex.at(saved.body));
 return out;
}
}
int main(){
 Json out=Json::array();ImpactSolver solver;
 {
  auto a=Sphere(2,{0,0,0},{2,.3f,0}),b=Sphere(1,{1,0,0},{-1,0,0}),c=Sphere(3,{2,0,0},{-.5f,0,0});
  std::vector<RigidBody*> bodies{&a,&b,&c};std::vector<ImpactContact> contacts{Pair(b,c),Pair(a,b),Pair(b,c)};
  const auto r=solver.Solve(contacts,1.f/60);auto snapshot=Snapshot(solver,r,bodies);
  Check(snapshot["saved_order"]==Json({1,2,0}),"deduplicate without changing first encounter order");Check(r.contacts==3,"all duplicate physical rows retained");Check(r.energyBudgetSatisfied,"repeated-row energy guard");out.push_back(snapshot);
 }
 {
  // Reuse one solver across changed body poses; no rotation lookup survives Solve.
  auto a=Sphere(1,{0,0,0},{1,1,0}),b=Sphere(2,{1,0,0},{-.5f,0,0});std::vector<RigidBody*> bodies{&a,&b};
  for(float angle:{0.f,.7f,-1.2f}){a.orientation=glm::angleAxis(angle,glm::normalize(glm::vec3(1,2,3)));b.orientation=glm::angleAxis(-angle*.5f,glm::vec3(0,1,0));a.linearVelocity={1,1,0};b.linearVelocity={-.5f,0,0};a.angularVelocity={.2f,-.1f,.3f};b.angularVelocity={-.4f,.2f,0};auto p=Pair(a,b);p.geometry.hasLocalAnchors=true;p.geometry.preciseNormal={-1,0,0};p.geometry.localAnchorA={.3,.1,0};p.geometry.localAnchorB={-.3,-.1,0};p.geometry.signedSeparation=0;
   auto r=solver.Solve({p},1.f/60);Check(solver.m_saved.size()==2,"per-call saved state reset");Check(r.contacts==1,"changed-pose one contact retained");out.push_back(Snapshot(solver,r,bodies));}
 }
 {
  auto a=Sphere(1,{0,0,0},{1,0,0}),b=Sphere(1,{1,0,0},{0,0,0}),c=Sphere(2,{0,1,0},{.3f,-.4f,0});
  JointState state;state.settings.type=JointType::Ball;std::array<float,12> warm{};JointSolver joints;joints.Prepare({{&a,&c,&state,&warm}},1.f/60,false,true);
  Check(!joints.Empty(),"hard joint control contains actual constraint rows");auto r=solver.Solve({Pair(a,b)},1.f/60,false,&joints);std::vector<RigidBody*> bodies{&a,&b,&c};auto snapshot=Snapshot(solver,r,bodies);Check(snapshot["saved_order"]==Json({0,1,2}),"joint-only participant remembered after contacts");Check(r.ambiguous,"hard joint coupling policy retained");Check(r.energyBudgetSatisfied,"hard joint energy guard");out.push_back(snapshot);
 }
 {
  RigidBody a,b;a.orientation=b.orientation={0,0,0,0};Contact c;c.hit=true;c.normal={1,0,0};
  try{auto r=solver.Solve({{&a,&b,c,0,0,false}},1.f/60);Check(r.contacts==0 && solver.m_saved.empty(),"unused static contact orientations stay unvalidated");out.push_back({{"static_skip",true}});}catch(const std::exception& e){Check(false,"unused static contact must not gain orientation exception");out.push_back({{"static_skip_error",e.what()}});}
  // Material connections carry no instantaneous impact rows. Their infinite-
  // mass participants must not suddenly require unused ContactRotation state.
  JointState state;state.constitutive.material.enabled=true;std::array<float,12> warm{};JointSolver joints;joints.Prepare({{&a,&b,&state,&warm}},1.f/60,false,true);Check(joints.Empty()&&joints.Bodies().size()==2,"joint-only lazy control has bodies but no rows");
  try{auto r=solver.Solve({},1.f/60,false,&joints);Check(solver.m_saved.size()==2&&r.energyBudgetSatisfied,"unused joint body orientations stay unvalidated");out.push_back({{"joint_only_lazy",true}});}catch(const std::exception& e){Check(false,"unused joint bodies must not gain orientation exception");out.push_back({{"joint_only_lazy_error",e.what()}});}
 }
 {
  auto a=Sphere(1,{0,0,0},{1,0,0}),b=Sphere(1,{1,0,0},{0,0,0});const auto valid=Pair(a,b);
  auto Invalid=[&](const char* label,ImpactContact p,float dt,const char* expected){try{(void)solver.Solve({p},dt);Check(false,label);out.push_back({{"missing_exception",label}});}catch(const std::invalid_argument& e){Check(std::string(e.what())==expected,label);out.push_back({{"exception",e.what()}});}};
  Invalid("invalid interval diagnostic",valid,-1,"invalid impact interval");auto p=valid;p.a=nullptr;Invalid("null participant diagnostic",p,0,"invalid impact contact");p=valid;p.a=p.b;Invalid("self contact diagnostic",p,0,"invalid impact contact");p=valid;p.geometry.hasLocalAnchors=true;p.geometry.signedSeparation=.001;Invalid("positive gap diagnostic",p,0,"impact requires actual contact, not positive separation");p=valid;p.friction=-1;Invalid("invalid material diagnostic",p,0,"invalid impact material");a.linearVelocity.x=std::numeric_limits<float>::infinity();Invalid("nonfinite velocity diagnostic",valid,0,"nonfinite impact velocity");
 }
 Json report={{"checks",checks},{"failures",failures},{"physical_and_diagnostic_bits",out}};std::cout<<report.dump(2)<<'\n';return failures?1:0;
}
