#!/usr/bin/env python3
"""Copy the unchanged application fixture and add narrow actuator observers.

Original fixture/source and engine are not edited. The optional diagnostic
overlay applies only ACTUAL captured angular-drive commands from a sidecar;
it does not synthesize contact observations, gains, effort or a new trajectory.
Acceptance statements and the original fixed timestep remain unchanged.
"""
import hashlib, json, pathlib

r=pathlib.Path.cwd();original=r/'tests/BalanceLabTests.cpp'
out=r/'.cache/m78-continuation/cold-drive-observer';out.mkdir(exist_ok=False)
helpers=r'''
bool diagnosticColdRead=false;int diagnosticRestoredSteps=0;EntityId diagnosticOwner=0;
std::ofstream diagnosticColdTrace;
Json DiagnosticDrives(RuntimeWorld& w,EntityId owner){
 Json rows=Json::array();auto* b=w.PhysicalBalanceSnapshot(owner);
 if(!b||!b->configured)throw std::runtime_error("diagnostic owner unavailable");
 auto vec=[](glm::vec3 v){return Json::array({v.x,v.y,v.z});};
 const auto& mapped=b->controller.Bodies();
 for(std::size_t i=1;i<mapped.size();++i){JointState state;
  if(!w.Physics().GetJoint(mapped[i].joint,state))throw std::runtime_error("diagnostic stale joint");
  const auto& d=state.angularDrive;
  rows.push_back({{"index",i},{"bodyA",w.EntityIdOfBody(state.settings.bodyA)},
   {"bodyB",w.EntityIdOfBody(state.settings.bodyB)},
   {"enabled",d.enabled},{"target",{d.target.w,d.target.x,d.target.y,d.target.z}},
   {"stiffness",d.stiffness},{"damping",d.damping},{"maxTorque",d.maxTorque},
   {"relativeAngularVelocity",vec(d.relativeAngularVelocity)},
   {"disposableImpulses",vec(d.impulses)}});
 }
 return rows;
}
void DiagnosticCaptureDrives(RuntimeWorld& w,EntityId owner,const fs::path& out,const GameSnapshot& snapshot){
 Json payloadHashes=Json::object();for(const auto& [name,chunk]:snapshot.participants)
  payloadHashes[name]={{"version",chunk.version},{"SHA256",SceneFingerprintSha256(chunk.data)}};
 Json sidecar={{"owner",owner},{"drives",DiagnosticDrives(w,owner)},
  {"participantIdentities",payloadHashes},{"sceneFingerprint",snapshot.content}};
 const auto* b=w.PhysicalBalanceSnapshot(owner);const auto& h=b->controller.History();
 sidecar["history"]={{"elapsed",h.elapsed},{"referenceTime",h.referenceTime},{"swing",h.swing},
  {"swingTime",h.swingTime},{"transferTime",h.transferTime}};
 std::ofstream(out/"actual-drive-sidecar.json")<<sidecar.dump(2)<<'\n';
 std::cout<<"DRIVE_OBSERVER capture "<<sidecar.dump()<<'\n';
}
void DiagnosticRestoreDrives(RuntimeWorld& w,EntityId owner,const fs::path& out,const GameSnapshot& snapshot){
 std::cout<<"DRIVE_OBSERVER restored-before-first-step "<<DiagnosticDrives(w,owner).dump()<<'\n';
 diagnosticColdRead=true;diagnosticOwner=owner;
 diagnosticColdTrace.open(out/"diagnostic-cold-read-trace.jsonl");stepTrace=&diagnosticColdTrace;
 const char* overlay=std::getenv("JUDAS_DIAGNOSTIC_RESTORE_DRIVES");
 if(!overlay||std::string(overlay)!="1")return;
 std::ifstream file(out/"actual-drive-sidecar.json");Json sidecar;file>>sidecar;
 if(sidecar.at("owner").get<EntityId>()!=owner||sidecar.at("sceneFingerprint")!=snapshot.content)
  throw std::runtime_error("diagnostic sidecar owner/fingerprint mismatch");
 for(const auto& [name,chunk]:snapshot.participants){const auto& expected=sidecar.at("participantIdentities").at(name);
  if(expected.at("version")!=chunk.version||expected.at("SHA256")!=SceneFingerprintSha256(chunk.data))
   throw std::runtime_error("diagnostic sidecar participant differs from exact saved candidate");
 }
 const auto* b=w.PhysicalBalanceSnapshot(owner);const auto& mapped=b->controller.Bodies();
 if(sidecar.at("drives").size()!=mapped.size()-1)throw std::runtime_error("diagnostic joint count mismatch");
 for(const auto& row:sidecar.at("drives")){auto index=row.at("index").get<std::size_t>();
  if(index==0||index>=mapped.size())throw std::runtime_error("diagnostic joint index mismatch");
  JointState actual;if(!w.Physics().GetJoint(mapped[index].joint,actual)||
   row.at("bodyA")!=w.EntityIdOfBody(actual.settings.bodyA)||row.at("bodyB")!=w.EntityIdOfBody(actual.settings.bodyB))
   throw std::runtime_error("diagnostic safe body mapping mismatch");
  JointAngularDrive drive;drive.enabled=row.at("enabled");auto q=row.at("target");
  drive.target={q.at(0).get<float>(),q.at(1).get<float>(),q.at(2).get<float>(),q.at(3).get<float>()};
  drive.stiffness=row.at("stiffness");drive.damping=row.at("damping");drive.maxTorque=row.at("maxTorque");
  auto v=row.at("relativeAngularVelocity");drive.relativeAngularVelocity={v.at(0).get<float>(),v.at(1).get<float>(),v.at(2).get<float>()};
  // Deliberately DO NOT restore disposable warm impulses.
  if(!w.Physics().SetJointAngularDrive(mapped[index].joint,drive))throw std::runtime_error("diagnostic actual drive rejected");
 }
 std::cout<<"DRIVE_OBSERVER actual-command-overlay "<<DiagnosticDrives(w,owner).dump()<<'\n';
}
void DiagnosticAfterStep(RuntimeWorld& w){
 if(!diagnosticColdRead||diagnosticRestoredSteps>=2)return;
 ++diagnosticRestoredSteps;
 std::cout<<"DRIVE_OBSERVER after-restored-step-"<<diagnosticRestoredSteps<<" "<<DiagnosticDrives(w,diagnosticOwner).dump()<<'\n';
}
'''
source=original.read_text().replace('#include "../third_party/nlohmann/json.hpp"','#include "nlohmann/json.hpp"')
source='#include <cstdlib>\n'+source
marker='void Step(RuntimeWorld& w,GameSession& game,Window& window,ResourceManager& resources,int count=1'
assert source.count(marker)==1
source=source.replace(marker,helpers+'\n'+marker)
marker='StepPlayedWorld(game,window,1.f/60);const auto fixedEnd'
assert source.count(marker)==1
source=source.replace(marker,'StepPlayedWorld(game,window,1.f/60);DiagnosticAfterStep(w);const auto fixedEnd')
marker='WorldPersistence::Published(world);auto* b=world.PhysicalBalanceSnapshot(savedOwner);'
assert source.count(marker)==1
source=source.replace(marker,'WorldPersistence::Published(world);DiagnosticRestoreDrives(world,savedOwner,out,snapshot);auto* b=world.PhysicalBalanceSnapshot(savedOwner);')
marker='"atomic save retains physical and native task state: "+error);'
assert source.count(marker)==1
source=source.replace(marker,marker+'\n   DiagnosticCaptureDrives(world,savedOwner,out,snapshot);')
(out/'BalanceLabDriveObserver.cpp').write_text(source)
(out/'IDENTITY.json').write_text(json.dumps({
 'fixtureSourceSHA256':hashlib.sha256(original.read_bytes()).hexdigest(),
 'observerSourceSHA256':hashlib.sha256(source.encode()).hexdigest(),
 'scope':'Original acceptance and timestep statements untouched; external actual-drive sidecar and opt-in command-only overlay; no contact or warm history synthesis'
},indent=2)+'\n')
print(out/'BalanceLabDriveObserver.cpp')
