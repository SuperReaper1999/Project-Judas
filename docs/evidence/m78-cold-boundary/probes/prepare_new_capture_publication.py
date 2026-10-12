#!/usr/bin/env python3
"""Copied continuation proof: actual completed observation publication, no production edit."""
import pathlib, hashlib, json
r=pathlib.Path.cwd(); out=r/'.cache/m78-cold-boundary/new-capture-publication';out.mkdir(exist_ok=False)
prior=r/'.cache/m78-cold-boundary/new-capture-command-history'
pw=(prior/'PhysicsWorld.cpp').read_text()
# Private copied Impl only: no production/public class layout or binding change.
needle='    std::vector<CachedContact> completedContactObservations;'
assert pw.count(needle)==1
pw=pw.replace(needle,needle+'\n    uint64_t diagnosticCompletedEpoch=0;float diagnosticCompletedDt=0;bool diagnosticCompletedReady=false,diagnosticRestoredPublication=false;')
at=pw.index('namespace {')
declarations='''
namespace {
thread_local SaveArchive* m78DiagnosticObservationArchive=nullptr;
thread_local std::function<uint64_t(BodyHandle)> m78DiagnosticObservationId;
thread_local std::function<BodyHandle(uint64_t)> m78DiagnosticObservationHandle;
thread_local bool* m78DiagnosticReadyResult=nullptr;thread_local float m78DiagnosticReadyDt=0;
}
bool M78DiagnosticCompletedObservationsReady(const PhysicsWorld& physics,float dt){
 struct Guard{~Guard(){m78DiagnosticReadyResult=nullptr;}} guard;
 bool result=false;m78DiagnosticReadyResult=&result;m78DiagnosticReadyDt=dt;
 std::vector<PhysicsWorld::BodyContactSample> ignored;physics.ReadBodyContactSample({},ignored);return result;
}
void M78DiagnosticPersistCompletedObservations(PhysicsWorld& physics,SaveArchive& a,
 std::function<uint64_t(BodyHandle)> id,std::function<BodyHandle(uint64_t)> handle){
 struct Guard{~Guard(){m78DiagnosticObservationArchive=nullptr;m78DiagnosticObservationId={};m78DiagnosticObservationHandle={};}} guard;
 if(m78DiagnosticObservationArchive)throw std::runtime_error("nested diagnostic observations");
 m78DiagnosticObservationArchive=&a;m78DiagnosticObservationId=std::move(id);m78DiagnosticObservationHandle=std::move(handle);
 SaveArchive ignored;physics.PersistJointSolverState(ignored,{});
}
'''
pw=pw[:at]+declarations+pw[at:]
needle='    Impl& w = *m_impl;\n    const Clock::time_point stepStart = Clock::now();'
assert pw.count(needle)==1
pw=pw.replace(needle,needle+'\n    w.diagnosticCompletedReady=false;w.diagnosticRestoredPublication=false;')
needle='    w.stats.geometryNumericGapFallbacks = geometryAfter.numericGapFallbacks - geometryBefore.numericGapFallbacks;'
assert pw.count(needle)==1
pw=pw.replace(needle,needle+'\n    ++w.diagnosticCompletedEpoch;w.diagnosticCompletedDt=fixedDeltaTime;w.diagnosticCompletedReady=true;')
needle='    output.clear();auto* body=m_impl?m_impl->Get(handle):nullptr;'
assert pw.count(needle)==1
pw=pw.replace(needle,'''    if(m78DiagnosticReadyResult){*m78DiagnosticReadyResult=m_impl&&m_impl->diagnosticRestoredPublication&&m_impl->diagnosticCompletedReady&&m_impl->diagnosticCompletedEpoch!=0&&m_impl->diagnosticCompletedDt==m78DiagnosticReadyDt;return true;}
'''+needle)
needle='void PhysicsWorld::PersistJointSolverState(SaveArchive& a,JointHandle h){'
assert pw.count(needle)==1
archive='''
 if(m78DiagnosticObservationArchive){
  auto& a=*m78DiagnosticObservationArchive;auto& w=*m_impl;
  bool ready=w.diagnosticCompletedReady;auto epoch=w.diagnosticCompletedEpoch;float dt=w.diagnosticCompletedDt;
  a(ready,epoch,dt);a.Require(!ready||(epoch!=0&&dt>0&&dt<=.1f),"invalid diagnostic completed publication");
  uint32_t n=0;if(!a.reading)for(const auto& c:w.completedContactObservations){auto ha=w.MakeHandle(c.key.slotA),hb=w.MakeHandle(c.key.slotB);auto* ba=w.Get(ha);auto* bb=w.Get(hb);
   a.Require(ba&&bb&&ba->generation==c.key.generationA&&bb->generation==c.key.generationB,"stale diagnostic observation capture");
   if(m78DiagnosticObservationId(ha)&&m78DiagnosticObservationId(hb)&&c.observationRevisionA==ba->contactObservationRevision&&c.observationRevisionB==bb->contactObservationRevision)++n;
  }
  a(n);a.Require(n<=65536,"diagnostic observation record bound");std::vector<CachedContact> records;records.reserve(n);size_t captured=0;
  for(uint32_t i=0;i<n;++i){uint64_t idA=0,idB=0,revA=0,revB=0;int partA=0,partB=0;glm::dvec3 anchorA{0},anchorB{0};glm::vec3 normal{0};float impulse=0,separation=0;
   if(!a.reading){const CachedContact* item=nullptr;while(captured<w.completedContactObservations.size()){auto& c=w.completedContactObservations[captured++];auto* ba=w.Get(w.MakeHandle(c.key.slotA));auto* bb=w.Get(w.MakeHandle(c.key.slotB));
     if(ba&&bb&&m78DiagnosticObservationId(w.MakeHandle(c.key.slotA))&&m78DiagnosticObservationId(w.MakeHandle(c.key.slotB))&&c.observationRevisionA==ba->contactObservationRevision&&c.observationRevisionB==bb->contactObservationRevision){item=&c;break;}}
    a.Require(item!=nullptr,"diagnostic observation capture mismatch");auto& c=*item;auto& ba=w.bodies[c.key.slotA];auto& bb=w.bodies[c.key.slotB];
    idA=m78DiagnosticObservationId(w.MakeHandle(c.key.slotA));idB=m78DiagnosticObservationId(w.MakeHandle(c.key.slotB));partA=c.key.partA;partB=c.key.partB;anchorA=c.localAnchorA;normal=c.normal;impulse=c.normalImpulse;separation=c.separation;revA=c.observationRevisionA;revB=c.observationRevisionB;
    auto point=glm::dvec3(ba.rigidBody.position)+glm::mat3_cast(glm::dquat(ba.rigidBody.orientation))*anchorA;
    anchorB=glm::transpose(glm::mat3_cast(glm::dquat(bb.rigidBody.orientation)))*(point-glm::dvec3(bb.rigidBody.position));
   }
   a(idA,idB,partA,partB,anchorA,anchorB,normal,impulse,separation,revA,revB);
   a.Require(idA&&idB&&idA!=idB&&impulse>=0&&glm::dot(normal,normal)>.99f&&glm::dot(normal,normal)<1.01f,"invalid diagnostic observation");
   if(a.reading){auto ha=m78DiagnosticObservationHandle(idA),hb=m78DiagnosticObservationHandle(idB);auto* ba=w.Get(ha);auto* bb=w.Get(hb);
    a.Require(ba&&bb&&ba->enabled&&bb->enabled&&!ba->sensor&&!bb->sensor&&!ba->queryOnly&&!bb->queryOnly,"diagnostic observation body unavailable");
    auto sa=ha.id&Impl::kSlotMask,sb=hb.id&Impl::kSlotMask;a.Require(w.CanCollide(sa,sb)&&partA>=0&&partA<PrimitiveCount(ba->shape)&&partB>=0&&partB<PrimitiveCount(bb->shape),"diagnostic observation filter/part mismatch");
    if(sa>sb){std::swap(sa,sb);std::swap(ba,bb);std::swap(partA,partB);std::swap(anchorA,anchorB);normal=-normal;}
    records.push_back({ContactKey{sa,sb,ba->generation,bb->generation,partA,partB},anchorA,normal,impulse,glm::vec3(0),separation,ba->contactObservationRevision,bb->contactObservationRevision});
   }
  }
  if(a.reading){std::stable_sort(records.begin(),records.end(),[](const auto& x,const auto& y){return x.key<y.key;});w.completedContactObservations=std::move(records);w.diagnosticCompletedReady=ready;w.diagnosticRestoredPublication=ready;w.diagnosticCompletedEpoch=epoch;w.diagnosticCompletedDt=dt;}
  return;
 }
'''
pw=pw.replace(needle,needle+archive)
(out/'PhysicsWorld.cpp').write_text(pw)

wp=(prior/'WorldPersistence.cpp').read_text()
at=wp.index('namespace {')
wp=wp[:at]+'''extern void M78DiagnosticPersistCompletedObservations(PhysicsWorld&,SaveArchive&,std::function<uint64_t(BodyHandle)>,std::function<BodyHandle(uint64_t)>);
'''+wp[at:]
needle='chunks["articulation-command-diagnostic"]={preserveImpulses?2u:1u,std::move(commandState.bytes)};'
assert wp.count(needle)==1
wp=wp.replace(needle,'''M78DiagnosticPersistCompletedObservations(w.Physics(),commandState,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);});
 chunks["articulation-command-diagnostic"]={preserveImpulses?4u:3u,std::move(commandState.bytes)};''')
wp=wp.replace('if(it->second.version!=1&&it->second.version!=2)throw std::runtime_error("diagnostic newer command section");','if(it->second.version<1||it->second.version>4)throw std::runtime_error("diagnostic newer command section");')
wp=wp.replace('if(it->second.version==2)', 'if(it->second.version==2||it->second.version==4)')
needle='  }a.Finish();\n }else if(auto it=chunks.find("articulation");'
assert wp.count(needle)==1
wp=wp.replace(needle,'''  }
  if(it->second.version>=3)M78DiagnosticPersistCompletedObservations(w.Physics(),a,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);});
  a.Finish();
 }else if(auto it=chunks.find("articulation");''')
(out/'WorldPersistence.cpp').write_text(wp)
pb=(r/'src/PhysicalBalance.cpp').read_text()
at=pb.index('namespace {')
pb=pb[:at]+'extern bool M78DiagnosticCompletedObservationsReady(const PhysicsWorld&,float);\n'+pb[at:]
needle='if(m_waitingForSample){m_waitingForSample=false;m_observation={};return;} // Reacquire completed contacts after a restored world steps.'
assert pb.count(needle)==1
pb=pb.replace(needle,'if(m_waitingForSample){m_waitingForSample=false;m_observation={};if(!M78DiagnosticCompletedObservationsReady(world,dt))return;} // Copied proof: known completed publication permits normal producer phase.')
(out/'PhysicalBalance.cpp').write_text(pb)
identity=dict(scope='Copied-only new-save full articulated continuation plus actual completed PhysicsWorld observation ledger, explicit completed epoch/dt readiness, normal first producer phase ONLY with genuine restored completed publication. Standalone controller History has no published world and must retain its original wait. No production edits, no fixture/gain/threshold/cadence change. No contact warmstart history restored.',
 sourceSHA256={str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest()for x in [r/'src/WorldPersistence.cpp',r/'src/PhysicsWorld.cpp',r/'src/PhysicalBalance.cpp',r/'tests/BalanceLabTests.cpp']},
 overlaySHA256={x.name:hashlib.sha256(x.read_bytes()).hexdigest()for x in out.glob('*.cpp')})
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n');print(json.dumps(identity,indent=2))
