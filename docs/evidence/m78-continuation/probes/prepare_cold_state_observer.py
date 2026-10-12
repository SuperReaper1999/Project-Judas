#!/usr/bin/env python3
"""Narrow copied M77 fixture: observe complete physical roundtrip and continue exact source snapshot.
No production/acceptance source, inputs, timestep or thresholds are edited.
"""
import hashlib,json,pathlib
r=pathlib.Path.cwd();p=r/'.cache/m78-continuation/cold-drive-observer/BalanceLabDriveObserver.cpp'
original=r/'tests/BalanceLabTests.cpp';source=p.read_text()
out=r/'.cache/m78-continuation/cold-state-observer';out.mkdir(exist_ok=True)
helpers=r'''
Json DiagnosticPhysicalState(RuntimeWorld& w,EntityId owner){
 auto vec=[](glm::vec3 v){return Json::array({v.x,v.y,v.z});};
 auto quat=[](glm::quat q){return Json::array({q.w,q.x,q.y,q.z});};
 auto matrix=[&](glm::mat3 m){return Json::array({vec(m[0]),vec(m[1]),vec(m[2])});};
 Json all=Json::array();auto& physics=w.Physics();
 for(auto body:physics.AliveBodies()){
  auto pose=physics.GetTransform(body);Shape shape;BodyTransform shapePose;
  if(!physics.GetBodyShape(body,shape,shapePose))throw std::runtime_error("observer shape unavailable");
  unsigned layer=0;CategoryMask mask=0;physics.GetCollisionFilter(body,layer,mask);
  std::string material;float friction=0,restitution=0;physics.GetPhysicalMaterial(body,material,friction,restitution);
  Json children=Json::array();for(const auto& c:shape.boxes)children.push_back({{"type",int(c.type)},
   {"localCenter",vec(c.localCenter)},{"halfExtents",vec(c.halfExtents)},
   {"rotation",quat(c.rotation)},{"radius",c.radius},{"assetId",c.assetId},{"key",c.key}});
  SaveArchive forces;physics.PersistBodyForces(forces,body);
  Json row={{"entity",w.EntityIdOfBody(body)},{"nativeHandle",body.id},{"sleeping",physics.IsSleeping(body)},{"motionType",int(physics.GetMotionType(body))},
   {"position",vec(pose.position)},{"rotation",quat(pose.rotation)},
   {"linearVelocity",vec(physics.GetLinearVelocity(body))},{"angularVelocity",vec(physics.GetAngularVelocity(body))},
   {"mass",physics.GetMass(body)},{"centerOfMass",vec(physics.GetCenterOfMass(body))},
   {"inertiaWorld",matrix(physics.GetInertiaWorld(body))},
   {"inertiaBody",matrix(glm::transpose(glm::mat3_cast(pose.rotation))*physics.GetInertiaWorld(body)*glm::mat3_cast(pose.rotation))},
   {"enabled",physics.IsBodyEnabled(body)},{"sensor",physics.IsBodySensor(body)},
   {"layer",layer},{"mask",mask},{"material",material},{"friction",friction},{"restitution",restitution},
   {"forcesSHA256",SceneFingerprintSha256(forces.bytes)},
   {"shape",{{"type",int(shape.type)},{"radius",shape.radius},{"halfHeight",shape.halfHeight},
     {"halfExtents",vec(shape.halfExtents)},{"pivotOffset",vec(shape.pivotOffset)},
     {"assetId",shape.assetId},{"boxes",children}}}};
  all.push_back(std::move(row));
 }
 auto* balance=w.PhysicalBalanceSnapshot(owner);if(!balance||!balance->configured)throw std::runtime_error("observer missing controller");
 Json mapped=Json::array(),joints=Json::array();for(std::size_t i=0;i<balance->controller.Bodies().size();++i){const auto& b=balance->controller.Bodies()[i];
  mapped.push_back({{"index",i},{"entity",w.EntityIdOfBody(b.body)},{"parent",b.parent},{"restRotation",quat(b.restRotation)}});
  if(i){JointState j;if(!physics.GetJoint(b.joint,j))throw std::runtime_error("observer missing mapped joint");auto& s=j.settings;
   joints.push_back({{"index",i},{"bodyA",w.EntityIdOfBody(s.bodyA)},{"bodyB",w.EntityIdOfBody(s.bodyB)},
    {"type",int(s.type)},{"anchorA",vec(s.anchorA)},{"anchorB",vec(s.anchorB)},
    {"frameA",quat(s.frameA)},{"frameB",quat(s.frameB)},{"enabled",s.enabled},
    {"limits",s.limits},{"motor",s.motor},{"spring",s.spring},{"lower",s.lower},{"upper",s.upper},
    {"speed",s.speed},{"maxForce",s.maxForce},{"rest",s.rest},{"stiffness",s.stiffness},
    {"damping",s.damping},{"rotationalResistance",s.rotationalResistance}});
  }
 }
 SaveArchive history;auto controllerCopy=balance->controller;controllerCopy.PersistHistory(history,[&](BodyHandle b){return w.EntityIdOfBody(b);},[&](uint64_t id){return w.RuntimeBody(id);});
 const auto& h=balance->controller.History();Json result={{"owner",owner},{"bodies",all},{"mapped",mapped},{"joints",joints},
  {"settings",SerializeBalanceDefinition(*w.RuntimeDefinition(owner)->ragdoll->balance)},
  {"historySHA256",SceneFingerprintSha256(history.bytes)},
  {"history",{{"elapsed",h.elapsed},{"referenceTime",h.referenceTime},{"swing",h.swing},
   {"swingTime",h.swingTime},{"transferTime",h.transferTime},{"rootReference",quat(h.rootReference)},
   {"supportReference",quat(h.supportReference)},{"frameSupport",w.EntityIdOfBody(h.frameSupport)},
   {"plannedSupport",w.EntityIdOfBody(h.plannedSupport)},{"stanceSupport",w.EntityIdOfBody(h.stanceSupport)}}},
  {"drives",DiagnosticDrives(w,owner)}};
 return result;
}
void DiagnosticRecordState(RuntimeWorld& w,EntityId owner,const fs::path& file){
 std::ofstream(file)<<DiagnosticPhysicalState(w,owner).dump(2)<<'\n';
}
'''
marker='void DiagnosticCaptureDrives('
assert source.count(marker)==1
source=source.replace(marker,helpers+'\n'+marker)
marker='Json sidecar={{"owner",owner}'
assert source.count(marker)==1
source=source.replace(marker,'DiagnosticRecordState(w,owner,out/"physical-before-save.json");\n '+marker)
marker='std::cout<<"DRIVE_OBSERVER restored-before-first-step "'
assert source.count(marker)==1
source=source.replace(marker,'DiagnosticRecordState(w,owner,out/"physical-immediately-restored.json");\n '+marker)
# Read-only complete phase snapshots for first ten resumed/uninterrupted fixed steps.
source=source.replace('if(!diagnosticColdRead||diagnosticRestoredSteps>=2)return;',
 'if(!diagnosticColdRead||diagnosticRestoredSteps>=10)return;')
source=source.replace('++diagnosticRestoredSteps;\n std::cout',
 '++diagnosticRestoredSteps;\n std::ofstream(diagnosticStateOutput/("physical-step-"+std::to_string(diagnosticRestoredSteps)+".json"))<<DiagnosticPhysicalState(w,diagnosticOwner).dump(2)<<\'\\n\';\n std::cout')
source=source.replace('std::ofstream diagnosticColdTrace;','std::ofstream diagnosticColdTrace;fs::path diagnosticStateOutput;')
source=source.replace('diagnosticColdRead=true;diagnosticOwner=owner;',
 'diagnosticColdRead=true;diagnosticOwner=owner;diagnosticStateOutput=out;')
marker='Check(glm::length(unchanged.position-livePose.position)<1e-6f,"failed detached restores leave live physical state untouched");'
assert source.count(marker)==1
source=source.replace(marker,marker+r'''
   if(const char* proceed=std::getenv("JUDAS_DIAGNOSTIC_CONTINUE_WRITE");proceed&&std::string(proceed)=="1"){
    diagnosticColdRead=true;diagnosticOwner=savedOwner;diagnosticStateOutput=out;
    diagnosticColdTrace.open(out/"diagnostic-uninterrupted-trace.jsonl");stepTrace=&diagnosticColdTrace;
    Step(world,game,window,resources,recoverySave?1080:720);Metrics(world,savedOwner);
    std::cout<<"STATE_OBSERVER uninterrupted exact-snapshot standing="<<Standing(world,savedOwner)<<'\n';
    stepTrace=nullptr;
   }
''')
(out/'BalanceLabStateObserver.cpp').write_text(source)
(out/'IDENTITY.json').write_text(json.dumps({'originalFixtureSHA256':hashlib.sha256(original.read_bytes()).hexdigest(),
 'sourceSHA256':hashlib.sha256(source.encode()).hexdigest(),
 'scope':'Read-only physical roundtrip/first ten steps; optional exact cold-write uninterrupted continuation. No original acceptance/inputs/timestep changes.'},indent=2)+'\n')
print(out/'BalanceLabStateObserver.cpp')
