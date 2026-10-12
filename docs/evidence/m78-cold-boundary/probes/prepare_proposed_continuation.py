#!/usr/bin/env python3
"""Prepare reviewable proper C++ continuation helpers in owned copies only."""
import pathlib,hashlib,json,shutil,difflib
r=pathlib.Path.cwd();out=r/'.cache/m78-cold-boundary/proposed-continuation';out.mkdir(exist_ok=False);src=out/'src';src.mkdir()
# Copy headers so all isolated TUs see one consistent header identity. Production
# headers are never edited by this preparation script.
for p in (r/'src').glob('*.h'):shutil.copyfile(p,src/p.name)
ph=(r/'src/PhysicsWorld.h').read_text()
needle='    void PersistJointSolverState(SaveArchive&,JointHandle);'
ph=ph.replace(needle,needle+'''
    // Versioned, bounded continuation of the existing iterative joint solve.
    // This is native save state, not a scripting API or contact warm cache.
    void PersistJointContinuation(SaveArchive&,JointHandle);
    void PersistCompletedContactObservations(SaveArchive&,
        const std::function<uint64_t(BodyHandle)>& identity,
        const std::function<BodyHandle(uint64_t)>& resolve,
        const std::function<bool(BodyHandle)>& include = {});
    bool HasRestoredCompletedContactObservations(float fixedDeltaTime) const;
''')
(src/'PhysicsWorld.h').write_text(ph)
pw=(r/'src/PhysicsWorld.cpp').read_text()
needle='    struct JointRecord {JointHandle handle;JointState state;std::array<float,12> warm{};float previousCoordinate=0;};'
assert pw.count(needle)==1
pw=pw.replace(needle,'''    struct JointRecord {
        JointHandle handle;JointState state;std::array<float,12> warm{};
        float previousCoordinate=0,restoredContinuationDt=0;
    };
    void InvalidateRestoredContinuation(unsigned slot) {
        // Live-world cache policy remains unchanged. Newly restored seeds may
        // only continue the exact reconstructed physical context.
        for(auto& joint:joints)if(joint.restoredContinuationDt>0&&
            ((joint.state.settings.bodyA.id&kSlotMask)==slot||
             (joint.state.settings.bodyB.IsValid()&&(joint.state.settings.bodyB.id&kSlotMask)==slot))){
            joint.warm.fill(0);joint.state.angularDrive.impulses={0,0,0};
            joint.restoredContinuationDt=0;
        }
        restoredCompletedObservations=false;
    }
    std::string ContinuationBodyContext(const Body& body) const {
        SaveArchive a;auto field=[&](auto value){a(value);};
        field(body.rigidBody.position);field(body.rigidBody.orientation);
        field(body.rigidBody.linearVelocity);field(body.rigidBody.angularVelocity);
        field(body.rigidBody.inverseMass);field(body.rigidBody.inverseInertiaLocal);
        field(body.isDynamic);field(body.isKinematic);field(body.enabled);
        field(body.sensor);field(body.queryOnly);field(body.collisionLayer);
        field(body.collisionMask);field(body.friction);field(body.restitution);
        auto material=body.physicalMaterial;a(material);
        const auto& shape=body.shape;
        field(shape.type);field(shape.radius);field(shape.halfHeight);
        field(shape.halfExtents);field(shape.pivotOffset);
        auto assetId=shape.assetId;a(assetId);
        auto fingerprint=[&](const auto& asset){
            std::string source=asset?asset->sourceFingerprint:std::string{},
                        settings=asset?asset->settingsFingerprint:std::string{};
            a(source,settings);
        };
        fingerprint(shape.asset);
        float terrainBound=shape.terrain?shape.terrain->BoundRadius():0;a(terrainBound);
        uint32_t count=uint32_t(shape.boxes.size());a(count);
        a.Require(count<=65536,"continuation compound-shape limit");
        for(const auto& child:shape.boxes){
            field(child.type);field(child.localCenter);field(child.halfExtents);
            field(child.rotation);field(child.radius);field(child.key);
            auto id=child.assetId;a(id);fingerprint(child.asset);
        }
        return a.bytes;
    }
    std::string ContinuationJointContext(const JointRecord& joint) const {
        SaveArchive a;auto s=joint.state.settings;
        // Handles are validated/remapped by the world owner; never archive slots.
        a(s.type,s.anchorA,s.anchorB,s.frameA,s.frameB,s.enabled,s.limits,s.motor,
          s.spring,s.lower,s.upper,s.speed,s.maxForce,s.rest,s.stiffness,s.damping,
          s.rotationalResistance);
        const auto* bodyA=Get(s.bodyA);const auto* bodyB=Get(s.bodyB);
        a.Require(bodyA&&(!s.bodyB.IsValid()||bodyB),"stale joint continuation body");
        auto first=ContinuationBodyContext(*bodyA),second=bodyB?ContinuationBodyContext(*bodyB):std::string{};
        a(first,second);return a.bytes;
    }''')
needle='    std::vector<CachedContact> completedContactObservations;'
pw=pw.replace(needle,needle+'''
    uint64_t completedObservationEpoch=0;
    float completedObservationDt=0;
    bool completedObservationsReady=false,restoredCompletedObservations=false;
''')
needle='    Impl& w = *m_impl;\n    const Clock::time_point stepStart = Clock::now();'
assert pw.count(needle)==1
pw=pw.replace(needle,needle+'''
    w.completedObservationsReady=false;w.restoredCompletedObservations=false;
    for(auto& joint:w.joints)if(joint.restoredContinuationDt>0){
        if(joint.restoredContinuationDt!=fixedDeltaTime){
            joint.warm.fill(0);joint.state.angularDrive.impulses={0,0,0};
        }
        joint.restoredContinuationDt=0;
    }
''')
needle='    w.stats.geometryNumericGapFallbacks = geometryAfter.numericGapFallbacks - geometryBefore.numericGapFallbacks;'
pw=pw.replace(needle,needle+'''
    ++w.completedObservationEpoch;w.completedObservationDt=fixedDeltaTime;
    w.completedObservationsReady=true;
''')
at=pw.index('void PhysicsWorld::PersistJointSolverState(')
helpers='''
void PhysicsWorld::PersistJointContinuation(SaveArchive& a,JointHandle handle) {
    auto found=std::find_if(m_impl->joints.begin(),m_impl->joints.end(),
        [&](const auto& joint){return joint.handle.id==handle.id;});
    a.Require(found!=m_impl->joints.end(),"stale joint continuation handle");
    auto& joint=*found;auto context=m_impl->ContinuationJointContext(joint);
    const auto expectedContext=context;uint32_t version=1;
    float dt=m_impl->completedObservationsReady?m_impl->completedObservationDt:0;
    auto warm=joint.warm;auto drive=joint.state.angularDrive;
    a(version,context,dt);a.Require(version==1,"newer joint continuation format");
    a.Require(context==expectedContext,"joint continuation physical context differs");
    a.Require(dt>=0&&dt<=.1f,"invalid joint continuation interval");
    for(auto& value:warm)a(value);
    a(drive.enabled,drive.target,drive.relativeAngularVelocity,drive.stiffness,
      drive.damping,drive.maxTorque,drive.impulses);
    a.Require(std::abs(glm::dot(drive.target,drive.target)-1.f)<1e-4f,"nonunit completed angular-drive target");
    if(dt==0)a.Require(std::all_of(warm.begin(),warm.end(),[](float x){return x==0;})&&
        drive.impulses==glm::vec3(0),"joint history has no completed interval");
    if(a.reading){
        a.Require(SetJointAngularDrive(handle,drive),"invalid restored angular drive");
        // Preserve the actual completed producer command, including its target
        // deadband, rather than recomputing a slightly different quaternion.
        joint.state.angularDrive=drive;joint.warm=warm;
        joint.restoredContinuationDt=dt;
    }
}

bool PhysicsWorld::HasRestoredCompletedContactObservations(float dt) const {
    return m_impl&&std::isfinite(dt)&&dt>0&&m_impl->restoredCompletedObservations&&
        m_impl->completedObservationsReady&&m_impl->completedObservationEpoch!=0&&
        m_impl->completedObservationDt==dt;
}

void PhysicsWorld::PersistCompletedContactObservations(SaveArchive& a,
    const std::function<uint64_t(BodyHandle)>& identity,
    const std::function<BodyHandle(uint64_t)>& resolve,
    const std::function<bool(BodyHandle)>& include) {
    auto& w=*m_impl;uint32_t version=1;auto epoch=w.completedObservationEpoch;
    auto dt=w.completedObservationDt;bool ready=w.completedObservationsReady;
    a(version,ready,epoch,dt);a.Require(version==1,"newer completed-observation format");
    a.Require((!ready&&epoch==0&&dt==0)||(ready&&epoch!=0&&dt>0&&dt<=.1f),
        "invalid completed-observation publication");
    auto eligible=[&](const CachedContact& c){
        auto* first=w.Get(w.MakeHandle(c.key.slotA));auto* second=w.Get(w.MakeHandle(c.key.slotB));
        return first&&second&&first->generation==c.key.generationA&&second->generation==c.key.generationB&&
            first->enabled&&second->enabled&&!first->sensor&&!second->sensor&&!first->queryOnly&&!second->queryOnly&&
            w.CanCollide(c.key.slotA,c.key.slotB)&&c.normalImpulse>0&&
            c.observationRevisionA==first->contactObservationRevision&&c.observationRevisionB==second->contactObservationRevision&&
            (!include||(include(w.MakeHandle(c.key.slotA))&&include(w.MakeHandle(c.key.slotB))));
    };
    std::vector<const CachedContact*> live;
    if(!a.reading&&ready)for(const auto& c:w.completedContactObservations)if(eligible(c))live.push_back(&c);
    uint32_t count=uint32_t(live.size());a(count);a.Require(count<=65536,"saved completed-observation limit");
    a.Require(ready||count==0,"unpublished completed-observation rows");
    std::vector<CachedContact> restored;restored.reserve(count);
    using Signature=std::tuple<unsigned,unsigned,int,int,double,double,double,float,float,float>;
    std::set<Signature> seen;
    for(uint32_t i=0;i<count;++i){
        uint64_t first=0,second=0;int partA=0,partB=0;
        glm::dvec3 anchorA{0},anchorB{0};glm::vec3 normal{0};float impulse=0,separation=0;
        std::string contextA,contextB;
        if(!a.reading){const auto& c=*live[i];const auto& bodyA=w.bodies[c.key.slotA];const auto& bodyB=w.bodies[c.key.slotB];
            first=identity(w.MakeHandle(c.key.slotA));second=identity(w.MakeHandle(c.key.slotB));
            partA=c.key.partA;partB=c.key.partB;anchorA=c.localAnchorA;normal=c.normal;
            impulse=c.normalImpulse;separation=c.separation;
            const auto point=glm::dvec3(bodyA.rigidBody.position)+glm::mat3_cast(glm::dquat(bodyA.rigidBody.orientation))*anchorA;
            // The existing observation has only an A anchor. Its corresponding
            // B anchor is derived from that SAME point for slot-order remapping.
            anchorB=glm::transpose(glm::mat3_cast(glm::dquat(bodyB.rigidBody.orientation)))*(point-glm::dvec3(bodyB.rigidBody.position));
            contextA=w.ContinuationBodyContext(bodyA);contextB=w.ContinuationBodyContext(bodyB);
        }
        a(first,second,partA,partB,anchorA,anchorB,normal,impulse,separation,contextA,contextB);
        a.Require(first&&second&&first!=second&&impulse>0&&glm::dot(normal,normal)>.99f&&glm::dot(normal,normal)<1.01f,
            "invalid completed-observation row");
        if(a.reading){auto ha=resolve(first),hb=resolve(second);auto* bodyA=w.Get(ha);auto* bodyB=w.Get(hb);
            a.Require(bodyA&&bodyB&&bodyA->enabled&&bodyB->enabled&&!bodyA->sensor&&!bodyB->sensor&&!bodyA->queryOnly&&!bodyB->queryOnly,
                "completed-observation body unavailable");
            auto sa=ha.id&Impl::kSlotMask,sb=hb.id&Impl::kSlotMask;
            a.Require(contextA==w.ContinuationBodyContext(*bodyA)&&contextB==w.ContinuationBodyContext(*bodyB),
                "completed-observation physical context differs");
            a.Require(w.CanCollide(sa,sb)&&partA>=0&&partA<PrimitiveCount(bodyA->shape)&&partB>=0&&partB<PrimitiveCount(bodyB->shape),
                "completed-observation filter/primitive differs");
            if(sa>sb){std::swap(sa,sb);std::swap(bodyA,bodyB);std::swap(partA,partB);std::swap(anchorA,anchorB);normal=-normal;}
            a.Require(seen.insert({sa,sb,partA,partB,anchorA.x,anchorA.y,anchorA.z,normal.x,normal.y,normal.z}).second,
                "duplicate completed-observation row");
            restored.push_back({ContactKey{sa,sb,bodyA->generation,bodyB->generation,partA,partB},anchorA,normal,impulse,glm::vec3(0),
                separation,bodyA->contactObservationRevision,bodyB->contactObservationRevision});
        }
    }
    if(a.reading){std::stable_sort(restored.begin(),restored.end(),[](const auto& x,const auto& y){return x.key<y.key;});
        w.completedContactObservations=std::move(restored);w.completedObservationEpoch=epoch;
        w.completedObservationDt=dt;w.completedObservationsReady=ready;w.restoredCompletedObservations=ready;
    }
}

'''
pw=pw[:at]+helpers+pw[at:]
# Invalidation is limited to newly restored continuation; existing live physics
# keeps its existing cache and update behavior.
needle='    ++body->contactObservationRevision;\n    body->rigidBody.position = position+rotation*body->shape.pivotOffset;'
pw=pw.replace(needle,'    m_impl->InvalidateRestoredContinuation(handle.id&Impl::kSlotMask);\n'+needle)
needle='Wake(h);b->rigidBody.inverseMass=1/mass;'
pw=pw.replace(needle,'m_impl->InvalidateRestoredContinuation(h.id&Impl::kSlotMask);'+needle)
needle='joint.state.settings=settings;joint.warm.fill(0);'
pw=pw.replace(needle,'joint.state.settings=settings;joint.warm.fill(0);if(joint.restoredContinuationDt>0){joint.state.angularDrive.impulses={0,0,0};joint.restoredContinuationDt=0;}')
needle='++b->contactObservationRevision;b->enabled=value;'
pw=pw.replace(needle,'m_impl->InvalidateRestoredContinuation(h.id&Impl::kSlotMask);'+needle)
needle='if(b->sensor!=value)++b->contactObservationRevision;'
pw=pw.replace(needle,'if(b->sensor!=value)m_impl->InvalidateRestoredContinuation(h.id&Impl::kSlotMask);'+needle)
needle='++m_impl->Get(a)->contactObservationRevision;++m_impl->Get(b)->contactObservationRevision;'
pw=pw.replace(needle,'m_impl->InvalidateRestoredContinuation(a.id&Impl::kSlotMask);m_impl->InvalidateRestoredContinuation(b.id&Impl::kSlotMask);'+needle)
needle='Wake(h);b->physicalMaterial=id;'
pw=pw.replace(needle,'m_impl->InvalidateRestoredContinuation(h.id&Impl::kSlotMask);'+needle)
# Collision filter has a dedicated generation-aware cache invalidation path.
needle='bool PhysicsWorld::SetCollisionFilter('
at=pw.index(needle);end=pw.index('\n}',at)+2
fragment=pw[at:end]
if '++body->contactObservationRevision;'in fragment:
 fragment=fragment.replace('++body->contactObservationRevision;','m_impl->InvalidateRestoredContinuation(handle.id&Impl::kSlotMask);++body->contactObservationRevision;')
elif '++b->contactObservationRevision;'in fragment:
 fragment=fragment.replace('++b->contactObservationRevision;','m_impl->InvalidateRestoredContinuation(handle.id&Impl::kSlotMask);++b->contactObservationRevision;')
else:fragment=fragment.replace('m_impl->Wake(handle.id&Impl::kSlotMask);','m_impl->InvalidateRestoredContinuation(handle.id&Impl::kSlotMask);m_impl->Wake(handle.id&Impl::kSlotMask);')
pw=pw[:at]+fragment+pw[end:]
(src/'PhysicsWorld.cpp').write_text(pw)
pb=(r/'src/PhysicalBalance.cpp').read_text()
needle='if(m_waitingForSample){m_waitingForSample=false;m_observation={};return;} // Reacquire completed contacts after a restored world steps.'
pb=pb.replace(needle,'''if(m_waitingForSample){
        m_waitingForSample=false;m_observation={};
        // A full world snapshot can restore the actual completed publication.
        // A controller-History-only restore still waits for a real world step.
        if(!world.HasRestoredCompletedContactObservations(dt))return;
    }''')
(src/'PhysicalBalance.cpp').write_text(pb)
wh=(r/'src/WorldPersistence.h').read_text().replace('static void ArticulationExtended(RuntimeWorld&,SaveArchive&,unsigned);','static void ArticulationExtended(RuntimeWorld&,SaveArchive&,unsigned,std::string* deferred=nullptr);\n static void JointContinuation(RuntimeWorld&,SaveArchive&);')
(src/'WorldPersistence.h').write_text(wh)
wp=(r/'src/WorldPersistence.cpp').read_text()
wp=wp.replace('void WorldPersistence::ArticulationExtended(RuntimeWorld& w,SaveArchive& a,unsigned version){','void WorldPersistence::ArticulationExtended(RuntimeWorld& w,SaveArchive& a,unsigned version,std::string* deferred){')
needle='\n}\nvoid WorldPersistence::PhysicalAnimation(RuntimeWorld& w,SaveArchive& a){'
assert wp.count(needle)==1
wp=wp.replace(needle,'''
 if(version>=4){
  std::string continuation;
  if(!a.reading){SaveArchive state;JointContinuation(w,state);continuation=std::move(state.bytes);}
  a(continuation);
  if(a.reading){a.Require(deferred!=nullptr,"joint continuation requires deferred fixup");*deferred=std::move(continuation);}
 }
}
void WorldPersistence::JointContinuation(RuntimeWorld& w,SaveArchive& a){
 struct Entry{bool articulated;EntityId owner;uint32_t index;JointHandle joint;};
 std::vector<Entry> entries;
 for(auto id:w.m_jointOwners){auto handle=w.RuntimeJoint(id);if(handle.IsValid())entries.push_back({false,id,0,handle});}
 for(const auto& [id,rig]:w.m_ragdolls)for(uint32_t index=0;index<rig.joints.size();++index)entries.push_back({true,id,index,rig.joints[index]});
 uint32_t n=uint32_t(entries.size());a(n);a.Require(n<=8192&&n==entries.size(),"joint continuation mapping count differs");
 std::set<std::tuple<bool,EntityId,uint32_t>> seen;
 for(uint32_t i=0;i<n;++i){bool articulated=false;EntityId owner=0,bodyA=0,bodyB=0;uint32_t index=0;JointHandle joint;
  if(!a.reading){auto e=entries.at(i);articulated=e.articulated;owner=e.owner;index=e.index;joint=e.joint;JointState state;a.Require(w.Physics().GetJoint(joint,state),"stale joint continuation");bodyA=w.EntityIdOfBody(state.settings.bodyA);bodyB=w.EntityIdOfBody(state.settings.bodyB);}
  a(articulated,owner,index,bodyA,bodyB);a.Require(owner&&bodyA&&seen.insert({articulated,owner,index}).second,"invalid/duplicate joint continuation mapping");
  if(a.reading){auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.articulated==articulated&&e.owner==owner&&e.index==index;});a.Require(found!=entries.end(),"missing joint continuation mapping");joint=found->joint;JointState state;a.Require(w.Physics().GetJoint(joint,state)&&w.EntityIdOfBody(state.settings.bodyA)==bodyA&&w.EntityIdOfBody(state.settings.bodyB)==bodyB,"joint continuation body identity differs");}
  w.Physics().PersistJointContinuation(a,joint);
 }
}
void WorldPersistence::PhysicalAnimation(RuntimeWorld& w,SaveArchive& a){''')
needle='if(name=="articulation")ArticulationExtended(w,a,version);else fn(w,a);'
assert wp.count(needle)==1
wp=wp.replace(needle,'if(name=="articulation"&&(!w.m_jointOwners.empty()||std::any_of(w.m_ragdolls.begin(),w.m_ragdolls.end(),[](const auto& r){return !r.second.joints.empty();})))version=4;\n '+needle)
needle='chunks["contacts"]={1,std::move(contacts.bytes)};'
assert wp.count(needle)==1
wp=wp.replace(needle,'''if(chunks.at("articulation").version>=4)w.Physics().PersistCompletedContactObservations(contacts,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);},[&](BodyHandle h){return !w.IsTransientEntity(w.EntityIdOfBody(h))||std::any_of(w.m_ragdolls.begin(),w.m_ragdolls.end(),[&](const auto& r){return std::any_of(r.second.bodies.begin(),r.second.bodies.end(),[&](const auto& b){return b.body.id==h.id;});});});
 chunks["contacts"]={chunks.at("articulation").version>=4?2u:1u,std::move(contacts.bytes)};''')
needle='bool WorldPersistence::Restore(RuntimeWorld& w,const std::map<std::string,SaveChunk>& chunks,std::string& error){JUDAS_PROFILE_SCOPE("Save restoration");try{'
assert wp.count(needle)==1
wp=wp.replace(needle,needle+'std::string deferredJointContinuation;bool hasJointContinuation=false;')
wp=wp.replace('(it->second.version==2||it->second.version==3)','(it->second.version==2||it->second.version==3||it->second.version==4)')
needle='if(name=="articulation")ArticulationExtended(w,a,it->second.version);else fn(w,a);'
assert wp.count(needle)==1
wp=wp.replace(needle,'if(name=="articulation"){ArticulationExtended(w,a,it->second.version,&deferredJointContinuation);hasJointContinuation=it->second.version>=4;}else fn(w,a);')
needle='if(auto it=chunks.find("sleep");it!=chunks.end())'
assert wp.count(needle)==1
wp=wp.replace(needle,'''if(hasJointContinuation){SaveArchive state(deferredJointContinuation);JointContinuation(w,state);state.Finish();}
 if(auto it=chunks.find("sleep");it!=chunks.end())''')
needle='if(it==chunks.end()||it->second.version!=1)throw std::runtime_error("required participant missing/newer");'
assert wp.count(needle)==1
wp=wp.replace(needle,'if(it==chunks.end()||(it->second.version!=1&&!(std::string(name)=="contacts"&&it->second.version==2)))throw std::runtime_error("required participant missing/newer");')
needle='else if(std::string(name)=="contacts")w.Physics().PersistTouches(a,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);});'
assert wp.count(needle)==1
wp=wp.replace(needle,'''else if(std::string(name)=="contacts"){
 a.Require((hasJointContinuation&&it->second.version==2)||(!hasJointContinuation&&it->second.version==1),"required joint/contact continuation pair missing");
 w.Physics().PersistTouches(a,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);});
 if(it->second.version>=2)w.Physics().PersistCompletedContactObservations(a,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);});
}''')
(src/'WorldPersistence.cpp').write_text(wp)
paths=['PhysicsWorld.cpp','PhysicsWorld.h','PhysicalBalance.cpp','WorldPersistence.cpp','WorldPersistence.h']
identity={'scope':'Proposed proper source helpers, owned copies only. No production edits. Versioned joint continuation with physical-context equality, dt invalidation, actual commands and selected numerical history; actual completed contact publication in existing contacts2, deferred continuation in existing articulation4. No new participant or public JS API.',
 'sourceSHA256':{p:hashlib.sha256((r/'src'/p).read_bytes()).hexdigest()for p in paths},
 'proposedSHA256':{p:hashlib.sha256((src/p).read_bytes()).hexdigest()for p in paths}}
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
patch=''.join(''.join(difflib.unified_diff((r/'src'/p).read_text().splitlines(True),(src/p).read_text().splitlines(True),fromfile='a/src/'+p,tofile='b/src/'+p))for p in paths)
(out/'proposed.patch').write_text(patch)
print(json.dumps(identity,indent=2))
