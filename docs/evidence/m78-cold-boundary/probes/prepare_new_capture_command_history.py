#!/usr/bin/env python3
"""Copied new-capture actuator-command proof; no production edits."""
import pathlib, hashlib, json, shutil
r = pathlib.Path.cwd()
out = r / '.cache/m78-cold-boundary/new-capture-command-history'
out.mkdir(exist_ok=False)
source = r / '.cache/m78-cold-boundary/new-capture-passive-history/WorldPersistence.cpp'
s = source.read_text()
# Declare after the real header includes; public class layouts remain untouched.
at = s.index('namespace {')
s = s[:at] + 'extern void M78DiagnosticPersistDriveImpulse(PhysicsWorld&,SaveArchive&,JointHandle);\n' + s[at:]
# Only member function code accesses ordinary existing RuntimeWorld ownership.
needle = 'chunks["sleep"]={1,std::move(sleeping.bytes)};'
assert s.count(needle) == 1
capture = '''
 SaveArchive commandState;uint32_t commandCount=0;for(const auto& [_,rig]:w.m_ragdolls)commandCount+=uint32_t(rig.joints.size());
 const char* impulseFlag=std::getenv("M78_DIAGNOSTIC_PERSIST_DRIVE_IMPULSES");bool preserveImpulses=impulseFlag&&std::string(impulseFlag)=="1";
 if(commandCount){commandState(commandCount);for(const auto& [owner,rig]:w.m_ragdolls)for(uint32_t index=0;index<rig.joints.size();++index){
  JointState joint;commandState.Require(w.Physics().GetJoint(rig.joints[index],joint),"diagnostic stale command joint");
  auto id=owner,bodyA=w.EntityIdOfBody(joint.settings.bodyA),bodyB=w.EntityIdOfBody(joint.settings.bodyB);auto d=joint.angularDrive;
  commandState(id,index,bodyA,bodyB,d.enabled,d.target,d.relativeAngularVelocity,d.stiffness,d.damping,d.maxTorque);if(preserveImpulses)commandState(d.impulses);
 }chunks["articulation-command-diagnostic"]={preserveImpulses?2u:1u,std::move(commandState.bytes)};}
'''
s = s.replace(needle, needle + capture)
needle = 'if(!w.m_deformableOwners.empty()){auto it=chunks.find("deformables");'
assert s.count(needle) == 1
restore = '''
 if(auto it=chunks.find("articulation-command-diagnostic");it!=chunks.end()){
  if(it->second.version!=1&&it->second.version!=2)throw std::runtime_error("diagnostic newer command section");
  SaveArchive a(it->second.data);uint32_t records=0;a(records);a.Require(records<=8192,"diagnostic command bound");uint32_t expected=0;for(const auto& [_,rig]:w.m_ragdolls)expected+=uint32_t(rig.joints.size());a.Require(records==expected,"diagnostic incomplete commands");std::set<std::pair<EntityId,uint32_t>> seen;
  for(uint32_t i=0;i<records;++i){EntityId owner=0,bodyA=0,bodyB=0;uint32_t index=0;JointAngularDrive d;a(owner,index,bodyA,bodyB,d.enabled,d.target,d.relativeAngularVelocity,d.stiffness,d.damping,d.maxTorque);if(it->second.version==2)a(d.impulses);
   auto rig=w.m_ragdolls.find(owner);a.Require(rig!=w.m_ragdolls.end()&&index<rig->second.joints.size()&&seen.insert({owner,index}).second,"diagnostic invalid/duplicate command mapping");auto handle=rig->second.joints[index];JointState current;a.Require(w.Physics().GetJoint(handle,current)&&w.EntityIdOfBody(current.settings.bodyA)==bodyA&&w.EntityIdOfBody(current.settings.bodyB)==bodyB,"diagnostic command body identity mismatch");
   a.Require(w.Physics().SetJointAngularDrive(handle,d),"diagnostic restored command rejected");if(it->second.version==2){SaveArchive value;value(d.impulses);SaveArchive apply(value.bytes);M78DiagnosticPersistDriveImpulse(w.Physics(),apply,handle);apply.Finish();}
  }a.Finish();
 }else if(auto it=chunks.find("articulation");it!=chunks.end()&&it->second.version>=4)throw std::runtime_error("diagnostic required command section missing");
'''
s = s.replace(needle, restore + needle)
s = '#include <cstdlib>\n' + s
(out/'WorldPersistence.cpp').write_text(s)

p = (r/'src/PhysicsWorld.cpp').read_text()
# Diagnostic-only hook delegates through the existing helper and avoids a
# production header/API/layout change. A proper retained helper would be direct.
at = p.index('void PhysicsWorld::PersistJointSolverState(')
p = p[:at] + '''
namespace {thread_local SaveArchive* m78DiagnosticDriveArchive=nullptr;}
void M78DiagnosticPersistDriveImpulse(PhysicsWorld& physics,SaveArchive& a,JointHandle handle){
 struct Guard{~Guard(){m78DiagnosticDriveArchive=nullptr;}} guard;
 if(m78DiagnosticDriveArchive)throw std::runtime_error("nested diagnostic drive restoration");
 m78DiagnosticDriveArchive=&a;SaveArchive unused;physics.PersistJointSolverState(unused,handle);
}
''' + p[at:]
needle = 'void PhysicsWorld::PersistJointSolverState(SaveArchive& a,JointHandle h){for(auto& j:m_impl->joints)if(j.handle.id==h.id){'
assert p.count(needle) == 1
p = p.replace(needle, needle + 'if(m78DiagnosticDriveArchive){(*m78DiagnosticDriveArchive)(j.state.angularDrive.impulses);return;}')
(out/'PhysicsWorld.cpp').write_text(p)
identity = dict(scope='Diagnostic-only new articulationv4 passive12 capture plus explicit authoritative angular commands companion, restored after PhysicalBalance Configure and before sleep. Optional companionv2 includes actual drive impulse history, versusv1 excludes it. No reactions or completed observations are saved. Existing versions1–3 remain unchanged; original a106 lacks fields and is not reconstructed. No production/timestep-contract edit.',
 sourceSHA256={str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [r/'src/WorldPersistence.cpp',r/'src/PhysicsWorld.cpp',r/'tests/BalanceLabTests.cpp']},
 overlaySHA256={x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in out.glob('*.cpp')})
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
print(identity)
