#!/usr/bin/env python3
"""Passive, bounded first-resumed joint solver journal in owned copied sources only."""
import pathlib, hashlib, json
r=pathlib.Path.cwd(); out=r/'.cache/m78-cold-boundary/row-journal';out.mkdir(exist_ok=False)
js=(r/'src/JointSolver.cpp').read_text()
fixture=(r/'.cache/m78-continuation/cold-state-observer/BalanceLabStateObserver.cpp').read_text()
helper=r'''
#include "JointSolver.h"
#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <cstdlib>
#include <nlohmann/json.hpp>
namespace {
using BJ=nlohmann::json;
std::ofstream bjFile;int bjTick=-1,bjBudget=8,bjPrepare=0,bjIteration=0;bool bjActive=false;
std::unordered_map<RigidBody*,std::pair<glm::vec3,glm::vec3>> bjInitialVelocity;
BJ bjVec(glm::vec3 v){return BJ::array({v.x,v.y,v.z});}
BJ bjQuat(glm::quat q){return BJ::array({q.w,q.x,q.y,q.z});}
BJ bjMat(glm::mat3 m){return BJ::array({bjVec(m[0]),bjVec(m[1]),bjVec(m[2])});}
BJ bjBody(const RigidBody& b){return {{"position",bjVec(b.position)},{"orientation",bjQuat(b.orientation)},{"linearVelocity",bjVec(b.linearVelocity)},{"angularVelocity",bjVec(b.angularVelocity)},{"inverseMass",b.inverseMass},{"inverseInertiaLocal",bjMat(b.inverseInertiaLocal)},{"inverseInertiaWorld",bjMat(b.InverseInertiaWorld())}};}
void bjWrite(BJ value){if(!bjActive)return;value["tick"]=bjTick;value["prepare"]=bjPrepare;value["iteration"]=bjIteration;bjFile<<value.dump()<<'\n';}
}
void M78BoundaryActivate(const std::string& path){bjFile.open(path);if(!bjFile)throw std::runtime_error("could not open passive boundary journal");bjTick=-1;const char* n=std::getenv("JUDAS_BOUNDARY_STEPS");bjBudget=n?std::max(1,std::min(32,std::atoi(n))):8;}
void M78BoundaryBeginTick(){if(!bjFile.is_open())return;++bjTick;bjActive=bjTick<bjBudget;bjPrepare=0;bjIteration=0;}
void M78BoundaryEndTick(){bjActive=false;}
'''
js=helper+js
old='m_rows.clear();m_bodies.clear();const float inf=std::numeric_limits<float>::infinity();'
new=old+r'''
 if(bjActive){++bjPrepare;bjIteration=0;bjInitialVelocity.clear();for(const auto& i:inputs)for(auto* b:{i.a,i.b})bjInitialVelocity.emplace(b,std::make_pair(b->linearVelocity,b->angularVelocity));bjWrite({{"kind","prepare_begin"},{"dt",dt},{"warm",warm},{"impact",impact},{"inputCount",inputs.size()}});for(const auto& i:inputs){bjWrite({{"kind","prepare_free_body"},{"body",i.state->settings.bodyA.id},{"state",bjBody(*i.a)}});bjWrite({{"kind","prepare_free_body"},{"body",i.state->settings.bodyB.id},{"state",bjBody(*i.b)}});}}
''';assert js.count(old)==1;js=js.replace(old,new)
old='auto ia=a.InverseInertiaWorld(),ib=b.InverseInertiaWorld();int index=0;'
new=old+r'''
  if(bjActive)bjWrite({{"kind","joint_begin"},{"jointOrdinal",std::size_t(&input-inputs.data())},{"bodyA",s.bodyA.id},{"bodyB",s.bodyB.id},{"type",int(s.type)},{"a",bjBody(a)},{"b",bjBody(b)},{"pivotA",bjVec(input.pivotOffsetA)},{"pivotB",bjVec(input.pivotOffsetB)},{"anchorA",bjVec(s.anchorA)},{"anchorB",bjVec(s.anchorB)},{"frameA",bjQuat(s.frameA)},{"frameB",bjQuat(s.frameB)},{"basis",bjMat(basis)},{"ra",bjVec(ra)},{"rb",bjVec(rb)},{"delta",bjVec(delta)},{"warm",*input.warm},{"limits",s.limits},{"lower",s.lower},{"upper",s.upper},{"spring",s.spring},{"motor",s.motor},{"drive",{{"enabled",input.state->angularDrive.enabled},{"target",bjQuat(input.state->angularDrive.target)},{"stiffness",input.state->angularDrive.stiffness},{"damping",input.state->angularDrive.damping},{"maxTorque",input.state->angularDrive.maxTorque},{"relativeAngularVelocity",bjVec(input.state->angularDrive.relativeAngularVelocity)},{"impulses",bjVec(input.state->angularDrive.impulses)}}}});
''';assert js.count(old)==1;js=js.replace(old,new)
old='m_rows.push_back({&a,&b,la,lb,aa,ab,ia,ib,1/(k+gamma),bias,gamma,low,high,lambda,cache,motor?&input.state->motorImpulse:nullptr,input.state});++index;'
new=old+r'''
   if(bjActive)bjWrite({{"kind","row_construct"},{"row",m_rows.size()-1},{"jointOrdinal",std::size_t(&input-inputs.data())},{"cacheIndex",index-1},{"bodyA",s.bodyA.id},{"bodyB",s.bodyB.id},{"linearA",bjVec(la)},{"linearB",bjVec(lb)},{"angularA",bjVec(aa)},{"angularB",bjVec(ab)},{"effective",k},{"mass",1/(k+gamma)},{"error",error},{"bias",bias},{"gamma",gamma},{"low",low},{"high",high},{"initialImpulse",lambda},{"soft",soft},{"motor",motor},{"resistance",resistance},{"aBeforeWarm",bjBody(a)},{"bBeforeWarm",bjBody(b)}});
''';assert js.count(old)==1;js=js.replace(old,new)
old='if(lambda!=0){Apply(m_rows.back(),lambda);input.state->reactionImpulse+=la*lambda;input.state->reactionAngularImpulse+=aa*lambda;}'
new=old+r'''
   if(bjActive)bjWrite({{"kind","row_warm_applied"},{"row",m_rows.size()-1},{"a",bjBody(a)},{"b",bjBody(b)}});
''';assert js.count(old)==1;js=js.replace(old,new)
old='index=7;if(!impact&&s.spring)axisRow(coordinate-s.rest,-inf,inf,true);'
new=r'''
   if(bjActive){auto av=bjInitialVelocity.at(&a),bv=bjInitialVelocity.at(&b);float unwarmed=s.type==JointType::Slider?glm::dot(bv.first+glm::cross(bv.second,rb)-av.first-glm::cross(av.second,ra),basis[0]):glm::dot(bv.second-av.second,basis[0]);bjWrite({{"kind","limit_prediction"},{"jointOrdinal",std::size_t(&input-inputs.data())},{"bodyA",s.bodyA.id},{"bodyB",s.bodyB.id},{"coordinate",coordinate},{"usedVelocity",velocity},{"prePrepareVelocity",unwarmed},{"usedPredicted",coordinate+velocity*dt},{"prePreparePredicted",coordinate+unwarmed*dt},{"lower",s.lower},{"upper",s.upper},{"limits",s.limits}});}
'''+old;assert js.count(old)==1;js=js.replace(old,new)
old='m_rows.push_back({&a,&b,{0,0,0},{0,0,0},-n,n,ia,ib,1/(effective+gamma),stiffnessBias,gamma,-cap,cap,lambda,cache,nullptr,input.state});'
new=old+r'''
    if(bjActive)bjWrite({{"kind","row_construct"},{"row",m_rows.size()-1},{"jointOrdinal",std::size_t(&input-inputs.data())},{"cacheIndex",100+k},{"bodyA",s.bodyA.id},{"bodyB",s.bodyB.id},{"linearA",bjVec(glm::vec3(0))},{"linearB",bjVec(glm::vec3(0))},{"angularA",bjVec(-n)},{"angularB",bjVec(n)},{"effective",effective},{"mass",1/(effective+gamma)},{"errorVector",bjVec(rotationError)},{"bias",stiffnessBias},{"gamma",gamma},{"low",-cap},{"high",cap},{"initialImpulse",lambda},{"aBeforeWarm",bjBody(a)},{"bBeforeWarm",bjBody(b)}});
''';assert js.count(old)==1;js=js.replace(old,new)
old='if(lambda!=0){Apply(m_rows.back(),lambda);input.state->reactionAngularImpulse-=n*lambda;}'
new=old+r'''
    if(bjActive)bjWrite({{"kind","row_warm_applied"},{"row",m_rows.size()-1},{"a",bjBody(a)},{"b",bjBody(b)}});
''';assert js.count(old)==1;js=js.replace(old,new)
old='void JointSolver::SolveIteration(){for(auto& r:m_rows){float velocity='
new='void JointSolver::SolveIteration(){if(bjActive){++bjIteration;bjWrite({{"kind","iteration_begin"},{"rowCount",m_rows.size()}});}for(auto& r:m_rows){if(bjActive)bjWrite({{"kind","row_solve_before"},{"row",std::size_t(&r-m_rows.data())},{"a",bjBody(*r.a)},{"b",bjBody(*r.b)},{"impulse",r.impulse},{"mass",r.mass},{"bias",r.bias},{"gamma",r.gamma},{"low",r.low},{"high",r.high}});float velocity='
assert js.count(old)==1;js=js.replace(old,new)
old='if(r.motor)*r.motor=total;}}'
new='if(r.motor)*r.motor=total;if(bjActive)bjWrite({{"kind","row_solve_after"},{"row",std::size_t(&r-m_rows.data())},{"rowVelocityBefore",velocity},{"total",total},{"change",change},{"a",bjBody(*r.a)},{"b",bjBody(*r.b)},{"cacheWritten",r.cache?BJ(*r.cache):BJ(nullptr)}});}}'
assert js.count(old)==1;js=js.replace(old,new)
fixture='#include <string>\nextern void M78BoundaryActivate(const std::string&);\nextern void M78BoundaryBeginTick();\nextern void M78BoundaryEndTick();\n'+fixture
old='const auto fixedBegin=PerformanceProfiler::Now();StepPlayedWorld(game,window,1.f/60);DiagnosticAfterStep(w);'
new='const auto fixedBegin=PerformanceProfiler::Now();M78BoundaryBeginTick();StepPlayedWorld(game,window,1.f/60);M78BoundaryEndTick();DiagnosticAfterStep(w);'
assert fixture.count(old)==1;fixture=fixture.replace(old,new)
old='Check(game.Begin(world,error),"restored game starts: "+error);Step(world,game,window,resources,recoverySave?1080:720);'
new='Check(game.Begin(world,error),"restored game starts: "+error);M78BoundaryActivate((out/"joint-boundary.jsonl").string());Step(world,game,window,resources,recoverySave?1080:720);'
assert fixture.count(old)==1;fixture=fixture.replace(old,new)
old='diagnosticColdTrace.open(out/"diagnostic-uninterrupted-trace.jsonl");stepTrace=&diagnosticColdTrace;'
new=old+'\n    M78BoundaryActivate((out/"joint-boundary.jsonl").string());'
assert fixture.count(old)==1;fixture=fixture.replace(old,new)
for name,s in [('JointSolver.cpp',js),('BalanceLabBoundaryObserver.cpp',fixture)]: (out/name).write_text(s)
identity={'scope':'Passive journal ONLY. First8 continued fixed steps; all actual row construction, current and pre-warm predicted limits, warm impulse apply,10 iterative solve/application, body inertia/commands. Original arithmetic remains exactly verbatim. Fixed dt60Hz, original gains/thresholds/fixtures unchanged. No contact cache or command restoration.','sourceSHA256':{p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in ['src/JointSolver.cpp','src/PhysicsWorld.cpp','src/PhysicalBalance.cpp','tests/BalanceLabTests.cpp']},'overlaySHA256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.cpp')}}
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n');print(out)
