#include "PhysicsWorld.h"
#include "SaveArchive.h"
#include "WorldPersistence.h"
#include "RuntimeWorld.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <functional>
#include <limits>
#include <string>

namespace {
int checks=0, failures=0;
constexpr float dt=1.f/60;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL %s\n",name);}}
bool Throws(const std::function<void()>& fn){try{fn();return false;}catch(const std::exception&){return true;}}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=3e-6f){return glm::length(a-b)<tolerance;}
void Word(std::string& bytes,size_t offset,uint32_t value){for(int i=0;i<4;++i)bytes.at(offset+i)=char(value>>(8*i));}
void CopyMotion(PhysicsWorld& from,BodyHandle a,PhysicsWorld& to,BodyHandle b){
 auto pose=from.GetTransform(a);to.ResetBody(b,pose.position,pose.rotation);
 to.SetLinearVelocity(b,from.GetLinearVelocity(a));to.SetAngularVelocity(b,from.GetAngularVelocity(a));
}
struct Articulation {
 PhysicsWorld world;BodyHandle a,b;JointHandle joint;
 Articulation(){world.Init();a=world.CreateShape(Shape::Box({.2f,.3f,.2f}),{{-.5f,2,0},glm::quat(1,0,0,0)},true,2,.8f,0);
  b=world.CreateShape(Shape::Box({.2f,.3f,.2f}),{{.5f,2,0},glm::quat(1,0,0,0)},true,3,.8f,0);
  JointSettings s;s.type=JointType::Ball;s.bodyA=a;s.bodyB=b;s.anchorA={.5f,0,0};s.anchorB={-.5f,0,0};joint=world.CreateJoint(s);
  JointAngularDrive drive;drive.enabled=true;drive.target=glm::angleAxis(.35f,glm::vec3(0,0,1));drive.stiffness=80;drive.damping=8;drive.maxTorque=20;
  Check(world.SetJointAngularDrive(joint,drive),"generic angular drive accepted");world.SetAngularVelocity(a,{.3f,0,.5f});
 }
 void MatchMotion(const Articulation& source){auto& from=const_cast<PhysicsWorld&>(source.world);CopyMotion(from,source.a,world,a);CopyMotion(from,source.b,world,b);}
 std::string Save(){SaveArchive archive;world.PersistJointContinuation(archive,joint);return archive.bytes;}
 void Restore(const std::string& bytes){SaveArchive archive(bytes);world.PersistJointContinuation(archive,joint);archive.Finish();}
};
void JointContinuation(){
 Articulation source;for(int i=0;i<5;++i)source.world.Step(dt);auto saved=source.Save();
 Articulation restored;restored.MatchMotion(source);auto pose=restored.world.GetTransform(restored.a);auto velocity=restored.world.GetLinearVelocity(restored.a);
 restored.Restore(saved);JointState a,b;source.world.GetJoint(source.joint,a);restored.world.GetJoint(restored.joint,b);
 Check(a.angularDrive.enabled==b.angularDrive.enabled&&a.angularDrive.target==b.angularDrive.target&&a.angularDrive.relativeAngularVelocity==b.angularDrive.relativeAngularVelocity&&a.angularDrive.stiffness==b.angularDrive.stiffness&&a.angularDrive.damping==b.angularDrive.damping&&a.angularDrive.maxTorque==b.angularDrive.maxTorque&&a.angularDrive.impulses==b.angularDrive.impulses,"actual latched command and drive history restore exactly");
 Check(restored.world.GetTransform(restored.a).position==pose.position&&restored.world.GetTransform(restored.a).rotation==pose.rotation&&restored.world.GetLinearVelocity(restored.a)==velocity,"continuation restore does not change physical pose or velocity");
 source.world.Step(dt);restored.world.Step(dt);
 Check(Near(source.world.GetTransform(source.a).position,restored.world.GetTransform(restored.a).position)&&Near(source.world.GetLinearVelocity(source.a),restored.world.GetLinearVelocity(restored.a))&&Near(source.world.GetAngularVelocity(source.a),restored.world.GetAngularVelocity(restored.a))&&Near(source.world.GetAngularVelocity(source.b),restored.world.GetAngularVelocity(restored.b)),"same fixed-step joint continuation closes physical state");
 Articulation wrong;wrong.MatchMotion(source);
 Check(Throws([&]{wrong.Restore(saved);}),"different captured physical boundary rejects joint continuation");
 Articulation truncated;truncated.MatchMotion(restored);auto current=restored.Save();
 Check(Throws([&]{truncated.Restore(current.substr(0,current.size()-1));}),"truncated joint record rejects before use");
 auto nonfinite=current;Word(nonfinite,nonfinite.size()-4,0x7fc00000);Check(Throws([&]{truncated.Restore(nonfinite);}),"nonfinite numerical history rejects");
 auto newer=current;Word(newer,0,99);Check(Throws([&]{truncated.Restore(newer);}),"unsupported newer solver identity rejects");
 // Version 1 (MK1) has no warm interval: its impulses were per whole step.
 {SaveArchive layout(current);uint32_t version=0;std::string context;float interval=0;layout(version,context,interval);const auto at=layout.position;
  auto legacy=current.substr(0,at)+current.substr(at+4);Word(legacy,0,1);auto perStep=current;uint32_t bits=0;std::memcpy(&bits,&interval,4);Word(perStep,at,bits);
  Articulation old,stated;old.MatchMotion(restored);stated.MatchMotion(restored);old.Restore(legacy);stated.Restore(perStep);old.world.Step(dt);stated.world.Step(dt);
  Check(version==2&&old.world.GetAngularVelocity(old.a)==stated.world.GetAngularVelocity(stated.a)&&old.world.GetLinearVelocity(old.b)==stated.world.GetLinearVelocity(stated.b),"version 1 joint history restores as per-step impulses");}
 auto nonunit=current;SaveArchive scan(current);uint32_t version=0;std::string context;float interval=0,warmInterval=0;scan(version,context,interval);if(version>=2)scan(warmInterval);for(int i=0;i<12;++i){float impulse=0;scan(impulse);}bool enabled=false;scan(enabled);Word(nonunit,scan.position,0x40000000);
 Check(Throws([&]{truncated.Restore(nonunit);}),"unnormalized saved latched quaternion cannot bypass command validation");
 Articulation stale;stale.world.DestroyBody(stale.a);Check(Throws([&]{stale.Restore(current);}),"destroyed joint/body handles cannot accept saved continuation");
 // A changed timestep must discard numerical guesses, without discarding the
 // authored actuator command. It must match an otherwise identical cold solve.
 Articulation changed,cold;changed.MatchMotion(restored);cold.MatchMotion(restored);changed.Restore(current);
 JointState command;restored.world.GetJoint(restored.joint,command);cold.world.SetJointAngularDrive(cold.joint,command.angularDrive);
 changed.world.Step(dt*.5f);cold.world.Step(dt*.5f);
 Check(Near(changed.world.GetLinearVelocity(changed.a),cold.world.GetLinearVelocity(cold.a))&&Near(changed.world.GetAngularVelocity(changed.a),cold.world.GetAngularVelocity(cold.a)),"changed cadence discards restored guesses without rescaling them");
 changed.world.GetJoint(changed.joint,command);Check(command.angularDrive.enabled,"cadence change retains valid actuator intent");
 for(int mutation=0;mutation<4;++mutation){Articulation guard;guard.MatchMotion(restored);guard.Restore(current);
  if(mutation==0){auto p=guard.world.GetTransform(guard.a);guard.world.ResetBody(guard.a,p.position,p.rotation);}
  if(mutation==1)guard.world.SetBodyEnabled(guard.a,false);
  if(mutation==2)guard.world.SetMassDistribution(guard.a,2.5f,glm::mat3(.2f));
  if(mutation==3){JointState state;guard.world.GetJoint(guard.joint,state);state.settings.lower-=.1f;guard.world.SetJoint(guard.joint,state.settings);}
  JointState state;guard.world.GetJoint(guard.joint,state);Check(state.angularDrive.impulses==glm::vec3(0)&&state.angularDrive.enabled,"changed reconstructed context invalidates guesses while retaining actuator intent");
 }
}
struct Contacts {
 PhysicsWorld world;BodyHandle floor,body;
 explicit Contacts(bool reverse=false){world.Init();auto createFloor=[&]{floor=world.CreateShape(Shape::Box({3,.5f,3}),{{0,-.5f,0},glm::quat(1,0,0,0)},false,0,.8f,0);};
  auto createBody=[&]{body=world.CreateShape(Shape::Box({.5f,.5f,.5f}),{{0,.5f,0},glm::quat(1,0,0,0)},true,4,.8f,0);};
  if(reverse){createBody();createFloor();}else{createFloor();createBody();}
 }
 uint64_t Id(BodyHandle h)const{return h.id==floor.id?1:h.id==body.id?2:0;}
 BodyHandle Resolve(uint64_t id)const{return id==1?floor:id==2?body:BodyHandle{};}
 std::string Save(){SaveArchive a;world.PersistCompletedContactObservations(a,[&](BodyHandle h){return Id(h);},[&](uint64_t id){return Resolve(id);});return a.bytes;}
 void Restore(const std::string& bytes){SaveArchive a(bytes);world.PersistCompletedContactObservations(a,[&](BodyHandle h){return Id(h);},[&](uint64_t id){return Resolve(id);});a.Finish();}
 void Match(Contacts& source){CopyMotion(source.world,source.floor,world,floor);CopyMotion(source.world,source.body,world,body);}
};
void CompletedPublication(){
 Contacts boot,unpublished;auto beforeStep=boot.Save();unpublished.Restore(beforeStep);
 Check(!unpublished.world.HasRestoredCompletedContactObservations(dt),"unpublished boot state is not a completed-empty observation");
 // The existing contact regression uses this same ordinary four-step resting
 // box setup. Merely touching a sphere with no continuing load need not publish
 // a positive support impulse; an archived observation must be real.
 for(int i=0;i<4;++i){boot.world.ApplyLinearAcceleration(boot.body,{0,-9.81f,0},dt);boot.world.Step(dt);}
 std::vector<PhysicsWorld::BodyContactSample> original;Check(boot.world.ReadBodyContactSample(boot.body,original)&&!original.empty()&&original.front().normalImpulse>0,"source exposes actual solved contact load");
 auto saved=boot.Save();Contacts restored(true);restored.Match(boot);auto velocity=restored.world.GetLinearVelocity(restored.body);restored.Restore(saved);
 Check(restored.world.HasRestoredCompletedContactObservations(dt)&&!restored.world.HasRestoredCompletedContactObservations(dt*.5f),"restored publication is usable only at its genuine completed cadence");
 std::vector<PhysicsWorld::BodyContactSample> actual;Check(restored.world.ReadBodyContactSample(restored.body,actual)&&actual.size()==original.size(),"restored samples survive canonical body-order reversal");
 bool matches=actual.size()==original.size();for(size_t i=0;i<actual.size()&&i<original.size();++i)matches&=actual[i].other.id==restored.floor.id&&actual[i].other.id!=boot.floor.id&&Near(actual[i].point,original[i].point)&&Near(actual[i].normal,original[i].normal)&&Near(actual[i].supportVelocity,original[i].supportVelocity)&&actual[i].normalImpulse==original[i].normalImpulse&&actual[i].separation==original[i].separation;
 Check(matches,"readonly observation preserves real point normal load separation and support velocity with new safe handles");
 Check(restored.world.GetLinearVelocity(restored.body)==velocity&&restored.world.LastStepTouchEvents().empty()&&restored.world.LastStepContacts().empty(),"observation restore neither replays impulses nor synthesizes callbacks/solver contacts");
 SaveArchive incomplete;boot.world.PersistCompletedContactObservations(incomplete,[&](BodyHandle h){return boot.Id(h);},[&](uint64_t id){return boot.Resolve(id);},[&](BodyHandle h){return h.id!=boot.floor.id;});Contacts omitted;omitted.Match(boot);omitted.Restore(incomplete.bytes);
 Check(!omitted.world.HasRestoredCompletedContactObservations(dt),"excluded readable contact cannot masquerade as a complete empty publication");
 Contacts bad;bad.Match(boot);bad.world.ResetBody(bad.body,{0,3,0},glm::quat(1,0,0,0));Check(Throws([&]{bad.Restore(saved);}),"observation from a different physical boundary rejects");
 Contacts truncated;truncated.Match(boot);Check(Throws([&]{truncated.Restore(saved.substr(0,saved.size()-1));}),"truncated observation record rejects");
 auto newer=saved;Word(newer,0,99);Check(Throws([&]{truncated.Restore(newer);}),"newer observation format rejects");
 SaveArchive scan(saved);uint32_t version=0,count=0;bool ready=false;uint64_t epoch=0;float interval=0;scan(version,ready,epoch,interval);auto countOffset=scan.position;scan(count);auto excessive=saved;Word(excessive,countOffset,65537);Check(Throws([&]{truncated.Restore(excessive);}),"observation row count is bounded before allocation");
 uint64_t first=0,second=0;int partA=0,partB=0;glm::dvec3 anchorA{0},anchorB{0};scan(first,second,partA,partB,anchorA,anchorB);auto nonfinite=saved;Word(nonfinite,scan.position,0x7fc00000);Check(Throws([&]{truncated.Restore(nonfinite);}),"nonfinite contact normal rejects");
 restored.world.Step(dt);Check(!restored.world.HasRestoredCompletedContactObservations(dt),"restored-only publication token expires at ordinary next Step");
 // Completed-empty and unpublished are different even though both expose zero
 // contacts. No geometry-derived substitute or synthetic support is acceptable.
 PhysicsWorld emptySource,emptyRestored;emptySource.Init();emptyRestored.Init();emptySource.Step(dt);SaveArchive a;emptySource.PersistCompletedContactObservations(a,[](BodyHandle){return uint64_t(0);},[](uint64_t){return BodyHandle{};});SaveArchive read(a.bytes);emptyRestored.PersistCompletedContactObservations(read,[](BodyHandle){return uint64_t(0);},[](uint64_t){return BodyHandle{};});read.Finish();
 Check(emptyRestored.HasRestoredCompletedContactObservations(dt),"genuine completed-empty epoch survives restoration");
 // Pair suppression after a solve invalidates the old contact for consumers;
 // capturing must use that same filtering, rather than make a valid save fail.
 boot.world.SetPairCollisionEnabled(boot.floor,boot.body,false);auto suppressed=boot.Save();Contacts filtered;filtered.Match(boot);filtered.world.SetPairCollisionEnabled(filtered.floor,filtered.body,false);filtered.Restore(suppressed);actual.clear();
 Check(filtered.world.ReadBodyContactSample(filtered.body,actual)&&actual.empty(),"captured observations respect changed bilateral pair filtering");
}
void IndependentQueryPublication(){
 Contacts source;
 for(int i=0;i<4;++i){source.world.ApplyLinearAcceleration(source.body,{0,-9.81f,0},dt);source.world.Step(dt);}
 const auto saved=source.Save();
 std::vector<PhysicsWorld::BodyContactSample> original;
 Check(source.world.ReadBodyContactSample(source.body,original)&&!original.empty(),"query isolation starts from actual positive physical contact");
 for(int mutation=0;mutation<5;++mutation){
  Contacts restored;restored.Match(source);
  auto proxy=restored.world.CreateQueryCapsule(.3f,.5f,{{100,3,0},glm::quat(1,0,0,0)});
  restored.Restore(saved);
  if(mutation==0){auto pose=restored.world.GetTransform(proxy);restored.world.ResetBody(proxy,pose.position,pose.rotation);}
  if(mutation==1)restored.world.SetBodyEnabled(proxy,false);
  if(mutation==2)restored.world.SetCollisionFilter(proxy,1,CategoryBit(0));
  if(mutation==3)restored.world.DestroyBody(proxy);
  if(mutation==4)restored.world.SetBodyTags(proxy,CategoryBit(0));
  std::vector<PhysicsWorld::BodyContactSample> actual;
  Check(restored.world.HasRestoredCompletedContactObservations(dt),"independent query-only refresh does not demote physical publication");
  Check(restored.world.ReadBodyContactSample(restored.body,actual)&&!actual.empty()&&actual.size()==original.size()&&actual.front().normalImpulse==original.front().normalImpulse,"query-only mutation preserves real completed load without replay");
 }
 Contacts physical;physical.Match(source);physical.Restore(saved);
 auto pose=physical.world.GetTransform(physical.body);physical.world.ResetBody(physical.body,pose.position+glm::vec3(0,1,0),pose.rotation);
 Check(!physical.world.HasRestoredCompletedContactObservations(dt),"ordinary physical mutation still demotes restored publication");
 Contacts connected;connected.Match(source);
 auto proxy=connected.world.CreateQueryCapsule(.3f,.5f,{{100,3,0},glm::quat(1,0,0,0)});
 JointSettings settings;settings.type=JointType::Ball;settings.bodyA=connected.body;settings.bodyB=proxy;
 auto joint=connected.world.CreateJoint(settings);Check(joint.IsValid(),"query-only endpoint can be explicitly mechanically connected");
 connected.Restore(saved);pose=connected.world.GetTransform(proxy);connected.world.ResetBody(proxy,pose.position,pose.rotation);
 Check(!connected.world.HasRestoredCompletedContactObservations(dt),"mechanically connected query proxy remains a physical invalidation");
}
void WorldParticipant(){
 Scene scene;auto a=scene.CreateObject("first joint body").id;auto b=scene.CreateObject("second joint body").id;
 for(auto id:{a,b}){auto* o=scene.Find(id);o->body=SceneBodyComponent{};o->body->managed=true;o->body->motion=SceneBodyMotion::Dynamic;}
 scene.Find(a)->transform.position={-1,2,0};scene.Find(b)->transform.position={1,2,0};
 auto owner=scene.CreateObject("ordinary authored joint").id;SceneJointComponent definition;definition.bodyA=a;definition.bodyB=b;definition.settings.type=JointType::Ball;definition.settings.anchorA={1,0,0};definition.settings.anchorB={-1,0,0};scene.Find(owner)->joint=definition;
 RuntimeWorld source,restored;std::string error;
 Check(source.Build(scene,nullptr,error),"ordinary private runtime fixture builds");source.Physics().Step(dt);
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(source,chunks,error),"ordinary WorldPersistence captures continuation");
 Check(chunks.at("contacts").version==2&&chunks.size()<=17,"completed publication uses versioned existing participant without growing slot limit");
 Scene loaded;Check(WorldPersistence::SceneFrom(chunks,loaded,error)&&restored.Build(loaded,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,chunks,error),"normal reconstruction restores completed publication at safe outer boundary");
 Check(restored.Physics().HasRestoredCompletedContactObservations(dt),"WorldPersistence publishes actual completed-empty state to next producer phase");
 auto broken=chunks;broken["contacts"].data.pop_back();RuntimeWorld reject;reject.Build(loaded,nullptr,error,nullptr,nullptr,true);WorldPersistence::Prepare(reject,error);Check(!WorldPersistence::Restore(reject,broken,error),"truncated versioned context rejects private candidate");
 auto newer=chunks;newer["contacts"].version=99;RuntimeWorld rejectVersion;rejectVersion.Build(loaded,nullptr,error,nullptr,nullptr,true);WorldPersistence::Prepare(rejectVersion,error);Check(!WorldPersistence::Restore(rejectVersion,newer,error),"unsupported newer publication version rejects");
 auto missing=chunks;missing.erase("contacts");RuntimeWorld rejectMissing;rejectMissing.Build(loaded,nullptr,error,nullptr,nullptr,true);WorldPersistence::Prepare(rejectMissing,error);Check(!WorldPersistence::Restore(rejectMissing,missing,error),"new articulated context cannot silently lose its completed publication");
}
void PhysicalEntityPersistence(){
 Scene scene;auto id=scene.CreateObject("ordinary static prescribed body").id;auto* o=scene.Find(id);o->body=SceneBodyComponent{};o->body->motion=SceneBodyMotion::Static;o->body->managed=false;o->body->shape=SceneShape::Box;
 RuntimeWorld source,restored;std::string error;Check(source.Build(scene,nullptr,error),"static body builds");Check(WorldPersistence::Prepare(source,error),"normal persistence preparation materializes static entity record");auto body=source.RuntimeBody(id);auto* record=source.FindEntity(id);
 Check(body.IsValid()&&record&&record->slot==std::numeric_limits<std::size_t>::max(),"ordinary static body uses slotless entity state");
 const glm::vec3 position(2,3,4),velocity(.1f,-.2f,.3f),angular(.4f,.5f,-.6f);const auto rotation=glm::angleAxis(.43f,glm::normalize(glm::vec3(1,2,3)));
 source.Physics().ResetBody(body,position,rotation);source.Physics().SetLinearVelocity(body,velocity);source.Physics().SetAngularVelocity(body,angular);
 EntityPhysicalState cached;source.GetEntityState(id,cached);Check(cached.position!=position,"fixture exposes independently prescribed body vs cached entity state");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(source,chunks,error),"existing world participant captures live static motion");Scene loaded;const bool ready=WorldPersistence::SceneFrom(chunks,loaded,error)&&restored.Build(loaded,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,chunks,error);Check(ready,"normal private restoration succeeds");if(!ready)std::printf("ERROR %s\n",error.c_str());
 auto result=restored.RuntimeBody(id);const auto a=source.Physics().GetTransform(body),b=restored.Physics().GetTransform(result);
 Check(a.position==b.position&&a.rotation==b.rotation,"actual prescribed static pose round trips exactly");Check(source.Physics().GetLinearVelocity(body)==restored.Physics().GetLinearVelocity(result)&&source.Physics().GetAngularVelocity(body)==restored.Physics().GetAngularVelocity(result),"actual prescribed static velocities round trip exactly");Check(restored.Physics().GetMotionType(result)==BodyMotionType::Static,"static authority is unchanged");

 // Reduced entities have retained authoritative state and no native body.
 for(auto fidelity:{SimulationFidelity::Coarse,SimulationFidelity::Dormant}){
  Scene reduced;auto did=reduced.CreateObject("ordinary managed body").id;auto* def=reduced.Find(did);def->body=SceneBodyComponent{};def->body->managed=true;def->body->motion=SceneBodyMotion::Dynamic;
  RuntimeWorld live,again;Check(live.Build(reduced,nullptr,error),"managed fixture builds");auto obsolete=live.RuntimeBody(did);Check(live.SetEntityFidelity(did,fidelity,&error),"normal reduced authority transition succeeds");
  EntityPhysicalState desired;desired.position={7,8,9};desired.rotation=glm::angleAxis(.23f,glm::normalize(glm::vec3(1,1,2)));desired.linearVelocity={.7f,.8f,.9f};desired.angularVelocity={.1f,.2f,.3f};Check(live.SetEntityState(did,desired)&&!live.RuntimeBody(did).IsValid()&&!live.Physics().IsDynamicBody(obsolete),"reduced cached state is independent from obsolete native handle");
  std::map<std::string,SaveChunk> saved;Scene next;bool ok=WorldPersistence::Capture(live,saved,error)&&WorldPersistence::SceneFrom(saved,next,error)&&again.Build(next,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(again,error)&&WorldPersistence::Restore(again,saved,error);Check(ok,"Coarse or Dormant cached-state snapshot restores");EntityPhysicalState actual;Check(again.GetEntityState(did,actual)&&actual.position==desired.position&&actual.rotation==desired.rotation&&actual.linearVelocity==desired.linearVelocity&&actual.angularVelocity==desired.angularVelocity&&again.FindEntity(did)->fidelity==fidelity,"reduced pose/velocity/fidelity stay authoritative");
 }
 // A motor observation capsule is derived query geometry, not root authority.
 for(bool enabled:{true,false}){
  ProjectClassification categories;categories.collision.names[7]="MotorFixture";categories.collision.nextId=8;categories.tags.names[0]="Fixture";categories.tags.nextId=1;
  Scene motorScene;auto mid=motorScene.CreateObject("offset character motor").id;
  CharacterMotorSettings motorSettings;motorSettings.offset={.9f,1.3f,-.4f};motorSettings.collisionLayer=7;motorSettings.collisionMask=CategoryBit(0);motorSettings.enabled=enabled;
  motorScene.Find(mid)->characterMotor=motorSettings;motorScene.Find(mid)->tags=CategoryBit(0);
  RuntimeWorld live,again;Check(live.Build(motorScene,nullptr,error,&categories)&&WorldPersistence::Prepare(live,error),"offset motor fixture prepares");
  EntityPhysicalState desired;desired.position={3,4,5};desired.rotation=glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3)));desired.linearVelocity={.2f,.3f,.4f};
  Check(live.SetEntityState(mid,desired),"normal root motor state is authored");live.UpdateCharacters(1.f/60);
  auto* motor=live.RuntimeCharacter(mid);EntityPhysicalState rootState;live.GetEntityState(mid,rootState);auto proxy=live.RuntimeBody(mid);
  Check(motor&&proxy.IsValid()&&live.Physics().GetTransform(proxy).position!=motor->position,"nonzero-offset proxy differs from root");
  std::map<std::string,SaveChunk> saved;Scene next;
  bool ok=WorldPersistence::Capture(live,saved,error)&&WorldPersistence::SceneFrom(saved,next,error)&&again.Build(next,nullptr,error,&categories,nullptr,true)&&WorldPersistence::Prepare(again,error)&&WorldPersistence::Restore(again,saved,error);
  Check(ok,"offset motor ordinary snapshot restores");if(!ok)std::printf("ERROR %s\n",error.c_str());
  auto* actual=again.RuntimeCharacter(mid);auto restoredProxy=again.RuntimeBody(mid);
  Check(actual&&actual->position==motor->position&&actual->orientation==motor->orientation&&actual->velocity==motor->velocity,"root motor pose and velocity are not replaced by observation proxy");
  // The normal entity transform setter normalizes this cached quaternion again;
  // motor state and query geometry above/below retain exact captured bits.
  EntityPhysicalState restoredRoot;
  Check(again.GetEntityState(mid,restoredRoot)&&restoredRoot.position==rootState.position&&glm::length(glm::vec4(restoredRoot.rotation.x,restoredRoot.rotation.y,restoredRoot.rotation.z,restoredRoot.rotation.w)-glm::vec4(rootState.rotation.x,rootState.rotation.y,rootState.rotation.z,rootState.rotation.w))<2e-7f&&restoredRoot.linearVelocity==rootState.linearVelocity,"entity root state remains motor root rather than offset proxy");
  Check(again.Physics().GetLinearVelocity(restoredProxy)==live.Physics().GetLinearVelocity(proxy),"observation proxy velocity policy remains unchanged");
  const auto originalPose=live.Physics().GetTransform(proxy),restoredPose=again.Physics().GetTransform(restoredProxy);
  Check(originalPose.position==restoredPose.position&&originalPose.rotation==restoredPose.rotation,"motor query geometry is exact before first resumed callback");
  unsigned layer=0;CategoryMask mask=0;
  Check(again.Physics().GetCollisionFilter(restoredProxy,layer,mask)&&layer==7&&mask==CategoryBit(0),"motor query collision layer and mask restore before first step");
  Check(again.Physics().IsBodyEnabled(restoredProxy)==enabled,"disabled motor handoff does not expose an enabled query capsule");
  PhysicsQueryFilter filter;filter.requiredTags=CategoryBit(0);
  auto originalTagged=live.Physics().QueryBodiesInAabb({-20,-20,-20},{20,20,20},filter),restoredTagged=again.Physics().QueryBodiesInAabb({-20,-20,-20},{20,20,20},filter);
  Check(originalTagged.size()==(enabled?1u:0u)&&restoredTagged.size()==originalTagged.size(),"motor authored tags are queryable before first resumed callback");
  filter.excludedTags=CategoryBit(0);
  Check(again.Physics().QueryBodiesInAabb({-20,-20,-20},{20,20,20},filter).empty(),"motor excluded-tag filtering works before first step");
  filter={};filter.includeLayers=CategoryBit(7);
  const auto origin=originalPose.position+glm::vec3(3,0,0);
  const auto sourceHit=live.Physics().Raycast(origin,{-1,0,0},6,filter),restoredHit=again.Physics().Raycast(origin,{-1,0,0},6,filter);
  Check(sourceHit.hit==enabled&&restoredHit.hit==sourceHit.hit,"motor enabled and disabled layer-filtered rays match before first step");
  Check(!sourceHit.hit||(glm::length(sourceHit.point-restoredHit.point)<3e-6f&&std::abs(sourceHit.distance-restoredHit.distance)<3e-6f),"motor restored nonzero-offset ray geometry matches actual source");
  again.UpdateCharacters(1.f/60);actual=again.RuntimeCharacter(mid);
  Check(again.Physics().GetTransform(restoredProxy).position==actual->position+actual->orientation*actual->settings.offset,"normal first fixed motor update retains offset observation geometry");
 }
}
}
int main(){try{JointContinuation();CompletedPublication();IndependentQueryPublication();WorldParticipant();PhysicalEntityPersistence();}catch(const std::exception& e){Check(false,e.what());}std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;}
