#include "WorldPersistence.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <limits>
#include <sstream>

namespace {
void transform(SaveArchive& a,SceneTransform& t){a(t.position,t.rotation,t.scale);}
void physical(SaveArchive& a,EntityPhysicalState& s){a(s.position,s.rotation,s.linearVelocity,s.angularVelocity);}
void pose(SaveArchive& a,SkeletalPose& p){uint32_t n=uint32_t(p.local.size());a(n);a.Require(n<=kModelNodeLimit,"saved pose joint limit");if(a.reading)p.local.resize(n);for(auto& t:p.local)a(t.translation,t.rotation,t.scale);}
void playback(SaveArchive& a,AnimationPlayback& p){a(p.clip,p.playing,p.loop,p.stopped,p.speed,p.time);a.Require(p.time>=0,"negative saved animation time");}
void contribution(SaveArchive& a,PoseContribution& c,const Skeleton& s){pose(a,c.pose);pose(a,c.reference);a(c.weight,c.enabled,c.additive,c.order);std::vector<std::string> keys;if(!a.reading)for(int n:c.mask)keys.push_back(SkeletonJointKey(s,n));a(keys);if(a.reading){std::string e;a.Require(ResolveJointMask(s,keys,c.mask,e),"saved joint mask invalid");a.Require(ValidPose(s,c.pose,e),"saved contribution pose invalid");if(!c.reference.local.empty())a.Require(ValidPose(s,c.reference,e),"saved reference pose invalid");}a.Require(c.weight>=0&&c.weight<=1,"saved pose weight invalid");}
void layerSettings(SaveArchive& a,AnimationLayerSettings& s){a(s.id,s.clip,s.referenceClip,s.enabled,s.additive,s.weight,s.speed,s.time,s.referenceTime,s.mask);std::string error;a.Require(ValidAnimationLayers({s},error),"invalid saved animation layer");}
void constraint(SaveArchive& a,JointSettings& s,unsigned version){a(s.type,s.anchorA,s.anchorB,s.frameA,s.frameB,s.enabled,s.limits,s.motor,s.spring,s.lower,s.upper,s.speed,s.maxForce,s.rest,s.stiffness,s.damping);if(version>=2)a(s.rotationalResistance);a.Require(ValidJointSettings(s),"invalid saved constraint");}
uint32_t count(SaveArchive& a,size_t size,uint32_t limit=8192){uint32_t n=uint32_t(size);a(n);a.Require(n<=limit,"participant record limit");return n;}
}
void WorldPersistence::Entities(RuntimeWorld& w,SaveArchive& a){
 std::vector<SceneObject> objects;std::string text;uint64_t next=w.m_nextRuntimeId;double time=w.m_simulationTime;
 if(!a.reading){
  for(const auto& o:w.ScriptObjects()){
   const char* component=o.fluidVolume?"legacy PBF":o.atmosphere?"atmosphere":o.combustible?"combustion":o.vehicle?"legacy vehicle":o.door?"legacy door":o.lightSwitch?"legacy switch":nullptr;
   if(component)throw std::runtime_error("entity "+std::to_string(o.id)+" component "+component+": no modern slot participant; separate legacy contract only");
  }
  Scene scene;scene.Settings()=w.m_settings;
  for(auto o:w.ScriptObjects()){
   const auto* e=w.FindEntity(o.id);if(e&&e->transient){bool articulated=false;for(auto& [_,r]:w.m_ragdolls)for(auto& b:r.bodies)articulated|=b.entity==o.id;if(!articulated)continue;}
   if(o.deformable){std::string error;auto* deform=w.RuntimeDeformable(o.id,error);if(!deform)throw std::runtime_error(error);*o.deformable=deform->settings;}
   auto t=w.PresentedTransform(o.id,o.transform,1);o.transform=t;o.tags=w.TagsOf(o.id);o.renderLayer=w.RenderLayerOf(o.id);
   auto body=w.RuntimeBody(o.id);if(o.body&&body.IsValid()){w.Physics().GetCollisionFilter(body,o.body->collisionLayer,o.body->collisionMask);o.body->enabled=w.Physics().IsBodyEnabled(body);o.body->sensor=w.Physics().IsBodySensor(body);}
   if(o.particleEmitter)for(auto& source:w.m_particleEmitters)if(source.id==o.id)*o.particleEmitter=source.pool.settings;
   if(o.renderCamera)for(auto& camera:w.m_renderCameras)if(camera.id==o.id)*o.renderCamera=camera.settings;
   if(o.audioEmitter)for(auto& source:w.m_audioEmitters)if(source.id==o.id)*o.audioEmitter=source.settings;
   if(o.joint){JointState state;if(w.Physics().GetJoint(w.RuntimeJoint(o.id),state)){o.joint->settings=state.settings;if(!o.joint->bodyB){auto inverse=glm::inverse(o.transform.rotation);o.joint->settings.anchorB=inverse*(state.settings.anchorB-o.transform.position);o.joint->settings.frameB=inverse*state.settings.frameB;}o.joint->settings.bodyA={};o.joint->settings.bodyB={};}}
   if(!scene.InsertObject(o))throw std::runtime_error("duplicate snapshot entity");
  }
  objects=scene.Objects();SaveSceneToString(scene,text);
 }
 a(text,next,time);
 for(const auto& o:w.ScriptObjects())if(o.id>=kRuntimeEntityIdBase)a.Require(next>o.id,"saved allocation counter aliases live identity");
 a.Require(next>=kRuntimeEntityIdBase&&next<=uint64_t(std::numeric_limits<int64_t>::max())&&time>=0,"invalid saved world counters");
 if(a.reading){w.m_nextRuntimeId=next;w.m_simulationTime=time;}
 auto n=count(a,objects.size());a.Require(!a.reading||n==w.ScriptObjects().size(),"incomplete saved entity records");std::set<EntityId> seen;
 for(uint32_t i=0;i<n;++i){EntityId id=0;EntityPhysicalState state;bool hasState=false,authored=true,transient=false;SimulationFidelity fidelity=SimulationFidelity::Full;CoarseMotion coarse=CoarseMotion::Settled;double dormant=-1;bool forced=false;SimulationFidelity forcedValue=SimulationFidelity::Full;
  if(!a.reading){id=objects.at(i).id;hasState=w.GetEntityState(id,state);if(auto* e=w.FindEntity(id)){authored=e->authored;transient=e->transient;fidelity=e->fidelity;coarse=e->coarseMotion;dormant=e->dormantSinceSeconds;forced=e->forcedFidelity.has_value();if(forced)forcedValue=*e->forcedFidelity;}}
  a(id,hasState,authored,transient,fidelity,coarse,dormant,forced,forcedValue);physical(a,state);bool bodyPresent=!a.reading&&w.RuntimeBody(id).IsValid();a(bodyPresent);a.Require(seen.insert(id).second&&w.RuntimeDefinition(id),"unknown/duplicate saved entity");
  a.Require(int(fidelity)>=0&&int(fidelity)<=2&&int(coarse)>=0&&int(coarse)<=1,"invalid saved fidelity");
  if(a.reading&&hasState)a.Require(w.SetEntityState(id,state),"cannot restore physical state");
  if(bodyPresent){auto handle=w.RuntimeBody(id);a.Require(handle.IsValid(),"saved force body missing");w.Physics().PersistBodyForces(a,handle);}
  if(a.reading){if(auto* e=w.FindEntity(id)){e->authored=authored;e->transient=transient;e->coarseMotion=coarse;e->dormantSinceSeconds=dormant;if(forced){a.Require(int(forcedValue)>=0&&int(forcedValue)<=2,"invalid saved forced fidelity");e->forcedFidelity=forcedValue;}if(fidelity!=SimulationFidelity::Full){std::string error;a.Require(w.SetEntityFidelity(id,fidelity,&error),"cannot restore entity fidelity");}}}
 }
 bool view=bool(w.view);a(view);if(view){RuntimeWorld::RuntimeView v;if(!a.reading)v=*w.view;transform(a,v.pose);a(v.fov);if(a.reading)a.Require(w.SetRuntimeView(v.pose,v.fov),"invalid saved view");}
}
void WorldPersistence::Motors(RuntimeWorld& w,SaveArchive& a){
 auto n=count(a,w.m_characters.size());a.Require(n==w.m_characters.size(),"incomplete motor records");std::set<EntityId> seen;
 for(uint32_t i=0;i<n;++i){EntityId id=0,support=0;std::vector<EntityId> ignored;CharacterMotor* motor=nullptr;
  if(!a.reading){auto it=w.m_characters.begin();std::advance(it,i);id=it->first;motor=&it->second.motor;support=w.EntityIdOfBody(motor->result.support);for(auto b:motor->filter.ignoredBodies){auto entity=w.EntityIdOfBody(b);a.Require(entity!=0,"motor has unowned ignored body");ignored.push_back(entity);}}
  a(id,support,ignored);if(a.reading)motor=w.RuntimeCharacter(id);a.Require(motor&&seen.insert(id).second,"unknown/duplicate motor");motor->Persist(a);
  if(a.reading){motor->result.support=support?w.RuntimeBody(support):BodyHandle{};a.Require(!support||motor->result.support.IsValid(),"missing required support body");motor->filter.ignoredBodies.clear();for(auto e:ignored){auto body=w.RuntimeBody(e);a.Require(body.IsValid(),"missing ignored body reference");motor->filter.ignoredBodies.push_back(body);}auto& p=w.m_characters.at(id).previous;p.position=motor->position;p.rotation=motor->orientation;}
 }
}
// Identical instance payload for durable saves and bounded region suspension.
void WorldPersistence::InstanceAnimation(RuntimeWorld& w,EntityId id,SaveArchive& a){
 auto* instance=w.RuntimeAnimation(id);a.Require(instance&&instance->asset,"animation asset not ready");
 auto& v=*instance;const auto& skeleton=v.asset->skeleton;auto validClip=[&](const std::string& clip){return clip.empty()||std::any_of(v.asset->clips.begin(),v.asset->clips.end(),[&](const auto& c){return c.name==clip;});};
  playback(a,v.playback);a.Require(v.playback.clip.empty()||std::any_of(v.asset->clips.begin(),v.asset->clips.end(),[&](const auto& c){return c.name==v.playback.clip;}),"saved clip unavailable");a(v.mixer.elapsed,v.mixer.duration,v.mixer.paused);a.Require(v.mixer.elapsed>=0&&v.mixer.duration>=0,"invalid saved mixer time");auto sources=count(a,v.mixer.outgoing.size(),16);if(a.reading)v.mixer.outgoing.resize(sources);for(auto& source:v.mixer.outgoing){playback(a,source.playback);a.Require(validClip(source.playback.clip),"saved outgoing clip unavailable");a(source.weight);a.Require(source.weight>=0&&source.weight<=1,"invalid saved mixer weight");}
  auto layers=count(a,v.layers.size(),16);std::vector<RuntimeWorld::AnimationLayer> restored;
  for(uint32_t l=0;l<layers;++l){RuntimeWorld::AnimationLayer layer;if(!a.reading)layer=v.layers.at(l);layerSettings(a,layer.settings);playback(a,layer.playback);a.Require(validClip(layer.playback.clip)&&validClip(layer.settings.referenceClip),"saved layer clip unavailable");std::vector<std::string> mask;if(!a.reading)for(auto index:layer.mask)mask.push_back(SkeletonJointKey(skeleton,index));a(mask);if(a.reading){std::string e;a.Require(ResolveJointMask(skeleton,mask,layer.mask,e),"saved layer mask invalid");}pose(a,layer.reference);std::string e;a.Require(layer.reference.local.empty()||ValidPose(skeleton,layer.reference,e),"invalid saved layer reference pose");a.Require(std::none_of(restored.begin(),restored.end(),[&](const auto& v){return v.settings.id==layer.settings.id;}),"duplicate saved layer");restored.push_back(std::move(layer));}
  if(a.reading)v.layers=std::move(restored);
  pose(a,v.sourcePose);pose(a,v.finalPose);std::string error;a.Require(ValidPose(skeleton,v.sourcePose,error)&&ValidPose(skeleton,v.finalPose,error),"invalid saved skeletal pose");
  auto external=count(a,v.external.size(),16);std::map<std::string,PoseContribution> contributions;
  for(uint32_t j=0;j<external;++j){std::string key;PoseContribution c;if(!a.reading){auto it=v.external.begin();std::advance(it,j);key=it->first;c=it->second;}a(key);contribution(a,c,skeleton);a.Require(contributions.emplace(key,std::move(c)).second,"duplicate pose producer");}
  if(a.reading){v.external=std::move(contributions);v.skin=ResolveSkinMatrices(skeleton,v.finalPose);v.previousWorld.clear();v.recentWorld.clear();v.motionDt=0;v.previousFinalPose=v.finalPose;v.fullBodyIK.reset();v.preparedM70=false;
    // Durable target intent lives in the ordinary entity definition; recreate
    // only the mapping here, never an invented prior fixed sample or solve cache.
    const auto* definition=w.RuntimeDefinition(id);
    if(definition&&definition->animation&&definition->animation->fullBodyIK){
      FullBodyIKRuntime restoredIK;restoredIK.settings=*definition->animation->fullBodyIK;
      a.Require(PrepareFullBodyIK(skeleton,restoredIK.settings,restoredIK.mapping,error),"saved full-body IK mapping invalid: "+error);
      restoredIK.prepared=true;v.fullBodyIK=std::move(restoredIK);
    }}
}
bool WorldPersistence::CanSuspendAnimation(RuntimeWorld& w,EntityId id){auto p=w.m_physicalAnimations.find(id);return !w.RagdollActive(id)&&!w.m_ragdollReturns.count(id)&&(p==w.m_physicalAnimations.end()||(!p->second.pending&&p->second.mode==PhysicalAnimationMode::Animation));}
bool WorldPersistence::CaptureAnimation(RuntimeWorld& w,EntityId id,std::string& bytes,std::string& error){try{SaveArchive a;InstanceAnimation(w,id,a);a.Require(a.bytes.size()<=1024*1024,"region animation snapshot exceeds 1 MiB");bytes=std::move(a.bytes);return true;}catch(const std::exception& e){error=e.what();return false;}}
bool WorldPersistence::CaptureKinematic(RuntimeWorld& w,EntityId id,std::string& bytes,std::string& error){
 try{auto body=w.RuntimeBody(id);if(!w.Physics().IsKinematicBody(body)){bytes.clear();return true;}SaveArchive a;w.Physics().PersistKinematic(a,body);a.Require(a.bytes.size()<=1024,"kinematic snapshot bound");bytes=std::move(a.bytes);return true;}catch(const std::exception& e){error=e.what();return false;}
}
bool WorldPersistence::RestoreKinematic(RuntimeWorld& w,EntityId id,const std::string& bytes,std::string& error){
 // Region restoration uses a private, disabled actor; only native fixup may
 // resolve its body before publication. No callback or integration occurs here.
 const bool pending=w.m_regionPending.erase(id)!=0;bool ok=false;
 try{SaveArchive a(bytes);a.Require(bytes.size()<=1024&&w.Physics().IsKinematicBody(w.RuntimeBody(id)),"saved kinematic body unavailable");w.Physics().PersistKinematic(a,w.RuntimeBody(id));a.Finish();if(pending)w.m_regionKinematicCommands[id]=bytes;ok=true;}catch(const std::exception& e){error=e.what();}
 if(pending)w.m_regionPending.insert(id);
 return ok;
}
bool WorldPersistence::RestoreAnimation(RuntimeWorld& w,EntityId id,const std::string& bytes,std::string& error){
 // Private staged entities are exposed only to this owner-thread native fixup.
 // No simulation, callback or publication occurs in this lookup scope.
 bool pending=w.m_regionPending.erase(id)!=0;
 try{SaveArchive a(bytes);InstanceAnimation(w,id,a);a.Finish();if(pending)w.m_regionPending.insert(id);return true;}catch(const std::exception& e){if(pending)w.m_regionPending.insert(id);error=e.what();return false;}
}
void WorldPersistence::Animation(RuntimeWorld& w,SaveArchive& a){
 auto n=count(a,w.m_animationInstances.size());a.Require(n==w.m_animationInstances.size(),"incomplete skeletal records");std::set<EntityId> seen;
 for(uint32_t i=0;i<n;++i){EntityId id=0;RuntimeWorld::AnimationInstance* instance=nullptr;
  if(!a.reading){auto it=w.m_animationInstances.begin();std::advance(it,i);id=it->first;instance=&it->second;}
  a(id);if(a.reading)instance=w.RuntimeAnimation(id);a.Require(instance&&instance->asset&&seen.insert(id).second,"missing/duplicate skeletal participant");InstanceAnimation(w,id,a);
 }
 a(w.m_ragdollAutostarted);auto returning=count(a,w.m_ragdollReturns.size());std::map<EntityId,RuntimeWorld::RagdollReturn> returns;
 for(uint32_t i=0;i<returning;++i){EntityId id=0;RuntimeWorld::RagdollReturn r;if(!a.reading){auto it=w.m_ragdollReturns.begin();std::advance(it,i);id=it->first;r=it->second;}a(id,r.elapsed,r.duration);a.Require(r.duration>0&&r.elapsed>=0&&r.elapsed<=r.duration,"invalid saved articulation transition");a.Require(w.RuntimeAnimation(id)&&returns.emplace(id,r).second,"missing/duplicate articulation return");}if(a.reading)w.m_ragdollReturns=std::move(returns);
}
void WorldPersistence::Articulation(RuntimeWorld& w,SaveArchive& a){ArticulationExtended(w,a,1);}
void WorldPersistence::ArticulationExtended(RuntimeWorld& w,SaveArchive& a,unsigned version){
 std::set<EntityId> owners,bodies;auto n=count(a,w.m_ragdolls.size());for(uint32_t i=0;i<n;++i){EntityId id=0;RuntimeWorld::RagdollInstance r;
  if(!a.reading){auto it=w.m_ragdolls.begin();std::advance(it,i);id=it->first;r=it->second;}
  a(id);a.Require(owners.insert(id).second,"duplicate articulation owner");auto* animation=w.RuntimeAnimation(id);a.Require(animation&&animation->asset,"articulation missing skeleton");r.asset=animation->asset;
  transform(a,r.reference);a(r.rootLocalPosition);if(version>=3){a(r.physicalControlled,r.partial);auto countFlags=count(a,r.dynamic.size(),32);if(a.reading)r.dynamic.resize(countFlags);for(unsigned f=0;f<countFlags;++f){bool value=r.dynamic[f];a(value);r.dynamic[f]=value;}}auto bones=count(a,r.bodies.size(),1024);if(a.reading)r.bodies.resize(bones);
  for(auto& b:r.bodies){std::string key;if(!a.reading)key=SkeletonJointKey(r.asset->skeleton,b.node);a(key,b.parent,b.entity,b.offset,b.scale,b.orientation);if(a.reading){b.node=FindSkeletonJoint(r.asset->skeleton,key);b.body=w.RuntimeBody(b.entity);a.Require(b.node>=0&&b.body.IsValid()&&bodies.insert(b.entity).second,"invalid/duplicate saved articulation mapping");auto* e=w.FindEntity(b.entity);a.Require(e!=nullptr,"articulation body lacks entity record");e->transient=e->requiresFull=true;}}
  auto joints=count(a,r.joints.size(),1024);std::vector<JointHandle> restored;
  for(uint32_t j=0;j<joints;++j){JointSettings settings;EntityId bodyA=0,bodyB=0;if(!a.reading){JointState state;a.Require(w.Physics().GetJoint(r.joints.at(j),state),"stale articulated constraint");settings=state.settings;bodyA=w.EntityIdOfBody(settings.bodyA);bodyB=w.EntityIdOfBody(settings.bodyB);}a(bodyA,bodyB);constraint(a,settings,version);if(a.reading){settings.bodyA=w.RuntimeBody(bodyA);settings.bodyB=w.RuntimeBody(bodyB);a.Require(settings.bodyA.IsValid()&&settings.bodyB.IsValid(),"missing articulated joint body");auto joint=w.Physics().CreateJoint(settings);a.Require(joint.IsValid(),"articulated joint recreation failed");restored.push_back(joint);}}
  if(a.reading){a.Require(!r.physicalControlled||r.dynamic.size()==r.bodies.size(),"physical articulation flags mismatch");if(r.partial)for(size_t b=0;b<r.bodies.size();++b)if(!r.dynamic[b])w.Physics().SetBodyQueriesEnabled(r.bodies[b].body,false);r.joints=std::move(restored);const auto* d=w.RuntimeDefinition(id);a.Require(d&&d->ragdoll&&r.bodies.size()==d->ragdoll->bones.size(),"saved articulation definition mismatch");for(size_t b=0;b<r.bodies.size();++b){auto& mapped=r.bodies[b];a.Require(mapped.parent< int(b)&&mapped.parent>=-1,"invalid articulated parent");a.Require(FindSkeletonJoint(r.asset->skeleton,d->ragdoll->bones[b].joint)==mapped.node,"saved articulation joint mapping differs from definition");if(mapped.parent>=0&&d->ragdoll->bones[b].suppressParentCollision)w.Physics().SetPairCollisionEnabled(mapped.body,r.bodies[mapped.parent].body,false);}if(!d->ragdoll->selfCollision)for(auto& x:r.bodies)for(auto& y:r.bodies)if(x.entity<y.entity)w.Physics().SetPairCollisionEnabled(x.body,y.body,false);a.Require(w.m_ragdolls.emplace(id,std::move(r)).second,"duplicate articulation owner");}
 }
}
void WorldPersistence::PhysicalAnimation(RuntimeWorld& w,SaveArchive& a){
 auto records=count(a,w.m_physicalAnimations.size());std::set<EntityId> seen;
 for(unsigned i=0;i<records;++i){EntityId id=0;PhysicalAnimationState state;
  if(!a.reading){auto it=w.m_physicalAnimations.begin();std::advance(it,i);id=it->first;state=it->second;}
  a(id,state.mode,state.motorWasEnabled,state.rebuild);bool pending=state.pending.has_value();a(pending);
  a.Require(int(state.mode)>=0&&int(state.mode)<=3&&seen.insert(id).second,"invalid/duplicate physical animation state");
  if(pending){PhysicalAnimationRequest request;if(!a.reading)request=*state.pending;a(request.mode,request.fade,request.motorHandoff,request.resumeMotor);bool placement=request.placement.has_value();a(placement);if(placement){PhysicalAnimationPlacement p;if(!a.reading)p=*request.placement;a(p.position,p.rotation);request.placement=p;}std::string error;a.Require(ValidPhysicalAnimationRequest(request,error),"invalid physical transition: "+error);state.pending=request;}
  if(a.reading){auto* d=w.RuntimeDefinition(id);auto* animation=w.RuntimeAnimation(id);a.Require(d&&d->ragdoll&&animation&&animation->asset,"missing physical animation owner");if(state.mode!=PhysicalAnimationMode::Animation){auto rig=w.m_ragdolls.find(id);a.Require(rig!=w.m_ragdolls.end()&&rig->second.physicalControlled,"missing physical articulation");a.Require(rig->second.partial==(state.mode==PhysicalAnimationMode::Partial),"physical authority mismatch");}state.observedAsset=animation->asset;state.previousReferenceWorld.clear();state.recentReferenceWorld.clear();state.motionDt=0;w.m_physicalAnimations[id]=std::move(state);}
 }
}
void WorldPersistence::Navigation(RuntimeWorld& w,SaveArchive& a){
 std::vector<EntityId> ids;for(auto& o:w.ScriptObjects())if(o.navigationAgent&&o.navigationAgent->enabled)ids.push_back(o.id);auto expected=ids.size();a(ids);a.Require(ids.size()==expected,"incomplete saved navigation agents");
 std::set<EntityId> seen;for(auto id:ids){a.Require(seen.insert(id).second,"duplicate navigation agent");auto* state=w.Navigation().Agent(id);a.Require(state!=nullptr,"missing saved navigation agent");a(state->stopped,state->hasDestination,state->destination,state->onLink,state->elapsed);
  SceneObjectId link=0;glm::vec3 start{0},end{0};if(!a.reading&&state->onLink){a.Require(state->corner<state->path.corners.size(),"navigation link cursor invalid");auto& c=state->path.corners[state->corner];link=c.link;start=c.position;end=c.linkEnd;}a(link,start,end);
  if(a.reading){state->path={};state->corner=0;state->steering={};state->remaining=0;if(state->onLink){auto* d=w.RuntimeDefinition(link);a.Require(d&&d->navigationLink&&d->navigationLink->enabled,"required navigation link unavailable");state->path.status=NavigationPath::Status::Complete;state->path.corners.push_back({start,link,end});}else if(state->hasDestination)state->elapsed=1e10f;}
 }
}
void WorldPersistence::Audio(RuntimeWorld& w,SaveArchive& a){
 auto n=count(a,w.m_audioEmitters.size(),128);a.Require(n==w.m_audioEmitters.size(),"incomplete audio records");std::set<EntityId> seen;for(uint32_t i=0;i<n;++i){EntityId id=0;RuntimeWorld::AudioResume resume;bool hasVelocity=false;glm::vec3 velocity{0};
  if(!a.reading){auto& emitter=w.m_audioEmitters.at(i);id=emitter.id;resume.requested=emitter.wantPlay;hasVelocity=emitter.explicitVelocity.has_value();if(hasVelocity)velocity=*emitter.explicitVelocity;AudioVoiceSnapshot voice;bool available=w.m_audioSystem&&w.m_audioSystem->Snapshot(emitter.voice,voice);a.Require(!emitter.settings.enabled||!emitter.wantPlay||available,"entity "+std::to_string(id)+" audio source not ready; retry after prefill");if(available){a.Require(!voice.seeking,"entity "+std::to_string(id)+" audio seek pending; retry after seek commits");a.Require(voice.error.empty(),"required audio source failed");resume.time=voice.positionSeconds;resume.state=voice.state;resume.groupGain=voice.groupGain;resume.fadeFrames=voice.groupFadeFrames;}}
  a(id,resume.time,resume.state,resume.requested,resume.groupGain,resume.fadeFrames,hasVelocity,velocity);a.Require(seen.insert(id).second&&resume.groupGain>=0&&resume.groupGain<=1,"duplicate/invalid voice control state");a.Require(resume.time>=0&&int(resume.state)>=0&&int(resume.state)<=3,"invalid saved voice state");if(a.reading){a.Require(w.RuntimeDefinition(id)&&w.RuntimeDefinition(id)->audioEmitter,"unknown saved audio source");w.m_audioResume[id]=resume;w.SetAudioVelocity(id,hasVelocity?std::optional<glm::vec3>(velocity):std::nullopt);}
 }
 // Master/device gain is a current-machine preference, never overwritten.
 std::set<std::string> groupNames;auto groups=w.audioGroups.groups;uint32_t size=uint32_t(groups.size());a(size);a.Require(size<=16,"saved audio group limit");for(uint32_t i=0;i<size;++i){std::string name;AudioGroupSettings settings;if(!a.reading){auto it=groups.begin();std::advance(it,i);name=it->first;settings=it->second;if(w.m_audioSystem)w.m_audioSystem->GetGroup(name,settings);}a(name,settings.gain,settings.mute,settings.paused);a.Require(groupNames.insert(name).second&&groups.count(name)&&name!="master"&&settings.gain>=0&&settings.gain<=1,"invalid saved project audio group");if(a.reading)w.audioGroups.groups[name]=settings;}
}
bool WorldPersistence::Capture(RuntimeWorld& w,std::map<std::string,SaveChunk>& chunks,std::string& error){JUDAS_PROFILE_SCOPE("Save capture");if(!Prepare(w,error))return false;try{chunks.clear();const std::vector<std::pair<std::string,void(*)(RuntimeWorld&,SaveArchive&)>> participants={{"entities",Entities},{"motors",Motors},{"animation",Animation},{"articulation",Articulation},{"navigation",Navigation},{"audio",Audio}};for(auto& [name,fn]:participants){ProfileScope participant(PerformanceProfiler::Get().Intern("Save capture "+name));SaveArchive a;unsigned version=1;
 if(name=="articulation")for(const auto& [_,r]:w.m_ragdolls)for(auto joint:r.joints){JointState state;if(w.Physics().GetJoint(joint,state)&&state.settings.rotationalResistance)version=2;}
 if(name=="articulation"&&std::any_of(w.m_ragdolls.begin(),w.m_ragdolls.end(),[](const auto& r){return r.second.physicalControlled;}))version=3;
 if(name=="articulation")ArticulationExtended(w,a,version);else fn(w,a);chunks[name]={version,std::move(a.bytes)};}
 // Conditional participant leaves existing saves and default authored scenes
 // unchanged. Physical state is restored first; this record reinstates durable
 // intent without synthesizing a previous displacement or CCD history.
 std::vector<EntityId> kinematicIds;for(const auto& o:w.ScriptObjects())if(w.Physics().IsKinematicBody(w.RuntimeBody(o.id))&&!w.IsTransientEntity(o.id))kinematicIds.push_back(o.id);
 if(!kinematicIds.empty()){SaveArchive motions;auto n=count(motions,kinematicIds.size());for(unsigned i=0;i<n;++i){auto id=kinematicIds[i];motions(id);w.Physics().PersistKinematic(motions,w.RuntimeBody(id));}chunks["kinematic-motion"]={1,std::move(motions.bytes)};}
 if(!w.m_physicalAnimations.empty()){SaveArchive physicalState;PhysicalAnimation(w,physicalState);chunks["physical-animation"]={1,std::move(physicalState.bytes)};}if(!w.m_deformableOwners.empty()){SaveArchive deforms;Deformables(w,deforms);bool fracture=false;for(auto& [_,r]:w.m_deformables)fracture|=bool(r.simulation.asset->fracture);chunks["deformables"]={unsigned(fracture?2:1),std::move(deforms.bytes)};}SaveArchive liquid;w.Liquids().Persist(liquid);chunks["liquid"]={1,std::move(liquid.bytes)};SaveArchive contacts;w.Physics().PersistTouches(contacts,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);},[&](BodyHandle h){return !w.IsTransientEntity(w.EntityIdOfBody(h))||std::any_of(w.m_ragdolls.begin(),w.m_ragdolls.end(),[&](const auto& r){return std::any_of(r.second.bodies.begin(),r.second.bodies.end(),[&](const auto& b){return b.body.id==h.id;});});});chunks["contacts"]={1,std::move(contacts.bytes)};SaveArchive sleeping;std::vector<BodyHandle> sleepBodies;for(auto h:w.Physics().AliveBodies())if(w.Physics().IsDynamicBody(h)&&w.EntityIdOfBody(h))sleepBodies.push_back(h);auto sleepCount=count(sleeping,sleepBodies.size());for(uint32_t i=0;i<sleepCount;++i){auto id=w.EntityIdOfBody(sleepBodies[i]);sleeping(id);w.Physics().PersistSleep(sleeping,sleepBodies[i]);}chunks["sleep"]={1,std::move(sleeping.bytes)};SaveArchive scripts;auto records=w.Scripts()?w.Scripts()->Capture(true):std::vector<ScriptStateRecord>{};for(auto& o:w.ScriptObjects())if(!w.IsTransientEntity(o.id))for(auto& slot:o.scripts)if(slot.enabled){scripts.Require(std::any_of(records.begin(),records.end(),[&](const auto& r){return r.entity==o.id&&r.slot==slot.id;}),"required script instance not initialized: entity "+std::to_string(o.id)+" slot "+std::to_string(slot.id)+"; retry after lifecycle boundary");}records.erase(std::remove_if(records.begin(),records.end(),[&](const auto& r){return w.IsTransientEntity(r.entity);}),records.end());auto n=count(scripts,records.size(),4096);for(uint32_t i=0;i<n;++i){auto& r=records[i];scripts(r.entity,r.slot,r.json);std::string e;scripts.Require(ScriptSystem::ValidateJson(r.json,e),"required script state capture failed: entity "+std::to_string(r.entity)+" slot "+std::to_string(r.slot)+": "+e);}chunks["scripts"]={1,std::move(scripts.bytes)};if(w.view&&(w.view->nearPlane!=.1f||w.view->farPlane!=500.f)){SaveArchive v;v(w.view->nearPlane,w.view->farPlane);chunks["view-projection"]={1,std::move(v.bytes)};}return true;}catch(const std::exception& e){error=e.what();return false;}}
bool WorldPersistence::SceneFrom(const std::map<std::string,SaveChunk>& chunks,Scene& scene,std::string& error){try{auto it=chunks.find("entities");if(it==chunks.end()||it->second.version!=1)throw std::runtime_error("required entity participant missing/newer");SaveArchive a(it->second.data);std::string text;a(text);std::istringstream scan(text);std::string line;size_t objects=0;while(std::getline(scan,line)){std::istringstream tokens(line);std::string keyword;tokens>>keyword;if(keyword=="object"&&++objects>8192)throw std::runtime_error("snapshot entity limit before scene allocation");}if(!LoadSceneFromString(text,scene,error))return false;return true;}catch(const std::exception& e){error=e.what();return false;}}
bool WorldPersistence::Prepare(RuntimeWorld& w,std::string& error){
 error.clear();
 // A boot/menu save can precede the first fixed step. Materialize normal lazy
 // participants without advancing motion or rebuilding existing navigation.
 for(const auto& o:w.ScriptObjects()){
  if(!w.FindEntity(o.id)){
   EntityRecord record;record.id=o.id;record.name=o.name;record.definition=o;
   record.authored=true;record.requiresFull=true;record.state.position=o.transform.position;
   record.state.rotation=o.transform.rotation;record.slot=std::numeric_limits<std::size_t>::max();
   w.m_extraEntities.push_back(std::move(record));
  }
  if(o.characterMotor)w.RuntimeCharacter(o.id);
  if(o.deformable){auto* deform=w.RuntimeDeformable(o.id,error);if(!deform||!deform->error.empty()){if(deform)error="required deformable solve failed: "+deform->error;return false;}if(!w.m_restoreConstruction&&deform->asset->fracture&&deform->asset->fracture->rigid&&!w.PrepareRigidFracture(o.id,error))return false;}
  if(o.navigationAgent&&o.navigationAgent->enabled&&!w.Navigation().Agent(o.id))w.Navigation().RegisterAgent(o.id);
 }
 for(auto& o:w.ScriptObjects())if(w.m_restoreConstruction&&o.navigationSurface&&o.navigationSurface->enabled&&w.Resources()){
  auto data=w.Resources()->GetNavigation(o.navigationSurface->asset,error);if(!data){if(error.empty())error="loading";return false;}
  if(!w.Navigation().LoadSurface(o.id,o.transform,data,error))return false;
 }
 if(w.m_restoreConstruction)w.Navigation().Update(w,0);
 if(!w.m_restoreConstruction)for(const auto& source:w.m_audioEmitters)if(source.settings.enabled&&source.wantPlay){
  if(!w.m_audioSystem){error="entity "+std::to_string(source.id)+": persistent audio service unavailable";return false;}
  AudioVoiceSnapshot voice;bool available=w.m_audioSystem->Snapshot(source.voice,voice);
  if(!source.error.empty()&&source.error!="loading"){error="entity "+std::to_string(source.id)+" audio: "+source.error;return false;}
  if(!voice.error.empty()){error="entity "+std::to_string(source.id)+" audio: "+voice.error;return false;}
  // Region publication may introduce a voice between the request and capture.
  // Defer the ENTIRE snapshot until its ordinary prefill/seek has committed.
  if(!available||!voice.ready||voice.seeking){error="loading";return false;}
 }
 if(!w.Liquids().PreparePersistence(w,error))return false;
 for(auto id:w.m_animationOwners){auto* v=w.RuntimeAnimation(id);if(!v||!v->asset){auto* d=w.RuntimeDefinition(id);auto* resources=w.Resources();if(d&&d->render&&resources&&resources->StateOf(d->render->meshAsset)==ResourceState::Failed){error="required skeleton "+std::to_string(id)+": "+resources->ErrorOf(d->render->meshAsset);return false;}error="loading";return false;}}return true;}
bool WorldPersistence::Restore(RuntimeWorld& w,const std::map<std::string,SaveChunk>& chunks,std::string& error){JUDAS_PROFILE_SCOPE("Save restoration");try{const std::vector<std::pair<std::string,void(*)(RuntimeWorld&,SaveArchive&)>> participants={{"entities",Entities},{"motors",Motors},{"animation",Animation},{"articulation",Articulation},{"navigation",Navigation},{"audio",Audio}};for(auto& [name,fn]:participants){auto it=chunks.find(name);if(it==chunks.end()||(it->second.version!=1&&!(name=="articulation"&&(it->second.version==2||it->second.version==3))))throw std::runtime_error("required participant missing/newer: "+name);ProfileScope participant(PerformanceProfiler::Get().Intern("Save restore "+name));SaveArchive a(it->second.data);if(name=="articulation")ArticulationExtended(w,a,it->second.version);else fn(w,a);a.Finish();}
 std::set<EntityId> restoredKinematics;
 if(auto it=chunks.find("kinematic-motion");it!=chunks.end()){if(it->second.version!=1)throw std::runtime_error("newer kinematic motion participant");SaveArchive motions(it->second.data);auto n=count(motions,0);for(unsigned i=0;i<n;++i){EntityId id=0;motions(id);auto h=w.RuntimeBody(id);motions.Require(w.Physics().IsKinematicBody(h)&&restoredKinematics.insert(id).second,"saved kinematic body unavailable/duplicate");w.Physics().PersistKinematic(motions,h);}motions.Finish();}
 for(const auto& o:w.ScriptObjects())if(w.Physics().IsKinematicBody(w.RuntimeBody(o.id))&&!w.IsTransientEntity(o.id)&&!restoredKinematics.count(o.id))throw std::runtime_error("required kinematic command record missing");
 if(auto it=chunks.find("physical-animation");it!=chunks.end()){if(it->second.version!=1)throw std::runtime_error("newer physical animation participant");SaveArchive physicalState(it->second.data);PhysicalAnimation(w,physicalState);physicalState.Finish();}if(!w.m_deformableOwners.empty()){auto it=chunks.find("deformables");if(it==chunks.end()||(it->second.version!=1&&it->second.version!=2))throw std::runtime_error("required deformable participant missing/newer");bool fracture=false;for(auto id:w.m_deformableOwners){auto* sim=w.RuntimeDeformable(id,error);if(!sim)throw std::runtime_error(error);fracture|=bool(sim->asset->fracture);}if(it->second.version!=unsigned(fracture?2:1))throw std::runtime_error("deformable/fracture participant version mismatch");SaveArchive a(it->second.data);Deformables(w,a);a.Finish();}if(auto it=chunks.find("sleep");it!=chunks.end()){if(it->second.version!=1)throw std::runtime_error("newer sleep participant");SaveArchive sleeping(it->second.data);auto n=count(sleeping,0);std::set<EntityId> seen;for(uint32_t i=0;i<n;++i){EntityId id=0;sleeping(id);auto h=w.RuntimeBody(id);sleeping.Require(w.Physics().IsDynamicBody(h)&&seen.insert(id).second,"saved sleep body unavailable/duplicate");w.Physics().PersistSleep(sleeping,h);}sleeping.Finish();}
for(auto name:{"liquid","scripts","contacts"}){auto it=chunks.find(name);if(it==chunks.end()||it->second.version!=1)throw std::runtime_error("required participant missing/newer");SaveArchive a(it->second.data);if(std::string(name)=="liquid")w.Liquids().Persist(a);else if(std::string(name)=="contacts")w.Physics().PersistTouches(a,[&](BodyHandle h){return w.EntityIdOfBody(h);},[&](uint64_t id){return w.RuntimeBody(id);});else {std::vector<ScriptStateRecord> scripts;std::set<std::pair<EntityId,uint32_t>> seen;auto n=count(a,0,4096);for(uint32_t i=0;i<n;++i){ScriptStateRecord r;a(r.entity,r.slot,r.json);const auto* d=w.RuntimeDefinition(r.entity);a.Require(d&&std::any_of(d->scripts.begin(),d->scripts.end(),[&](const auto& s){return s.id==r.slot&&s.enabled;})&&seen.emplace(r.entity,r.slot).second,"saved script slot unavailable/duplicate");scripts.push_back(std::move(r));}for(const auto& o:w.ScriptObjects())if(!w.IsTransientEntity(o.id))for(const auto& slot:o.scripts)if(slot.enabled)a.Require(seen.count({o.id,slot.id}),"required restored script slot missing: entity "+std::to_string(o.id)+" slot "+std::to_string(slot.id));a.Require(w.RestoreScriptState(scripts,error,true),"saved script state invalid");}a.Finish();}if(auto it=chunks.find("view-projection");it!=chunks.end()){if(it->second.version!=1||!w.view)throw std::runtime_error("invalid view-projection participant");SaveArchive v(it->second.data);float nearPlane=0,farPlane=0;v(nearPlane,farPlane);v.Require(w.SetRuntimeView(w.view->pose,w.view->fov,nearPlane,farPlane),"invalid saved projection");v.Finish();}return true;}catch(const std::exception& e){error=e.what();return false;}}
void WorldPersistence::Published(RuntimeWorld& w){w.m_restoreConstruction=false;w.ResetAudioMotion();w.m_touchEntityHistory.clear();for(const auto& o:w.ScriptObjects()){auto b=w.RuntimeBody(o.id);if(b.IsValid())w.m_touchEntityHistory[b.id]=o.id;}}

bool WorldPersistence::PrepareAudio(RuntimeWorld& w,std::string& error){
 for(auto& source:w.m_audioEmitters){auto it=w.m_audioResume.find(source.id);if(it==w.m_audioResume.end()||!source.settings.enabled)continue;
  if(!w.m_audioSystem||!w.m_assets){error="saved audio service unavailable";return false;}auto& saved=it->second;
  if(!source.voice.IsValid()){
   if(source.settings.loading==AudioLoading::Streamed){auto path=w.m_assets->GetStreamAudioPath(source.settings.asset,error);if(!path.empty())source.voice=w.m_audioSystem->CreateStreamVoice(path,source.settings,error);}
   else{auto clip=w.m_assets->GetAudio(source.settings.asset,error);if(clip.IsValid())source.voice=w.m_audioSystem->CreateVoice(clip,source.settings,error);}
   if(!source.voice.IsValid()){if(error.empty())error="loading";return false;}
  }
  if(!saved.seekSubmitted){if(!w.m_audioSystem->Seek(source.voice,saved.time)){error="saved audio cursor cannot be sought";return false;}saved.seekSubmitted=true;}
  AudioVoiceSnapshot voice;if(!w.m_audioSystem->Snapshot(source.voice,voice)){error="saved audio voice lost";return false;}
  if(!voice.error.empty()){error=voice.error;return false;}if(!voice.ready||voice.seeking){error="loading";return false;}if(voice.durationKnown&&saved.time>voice.durationSeconds+1e-4){error="saved audio cursor beyond source duration";return false;}
 }
 return true;
}

// Explicit project exclusions are limited to ephemeral presentation/simple bodies.
// Required subsystem state cannot disappear behind an exclusion flag.
bool RuntimeWorld::SetTransientEntity(EntityId id,bool transient,std::string& error){
 auto* e=FindEntity(id);const auto* d=RuntimeDefinition(id);if(!e||!d){error="stale entity";return false;}
 if(d->socket||d->deformable||d->liquidBasin||d->liquidContainer||d->liquidConnection||d->characterMotor||d->animation||d->ragdoll||d->joint||d->navigationAgent||d->navigationSurface||d->navigationObstacle||d->navigationLink||d->audioEmitter||d->audioListener||d->gravity||!d->scripts.empty()){error="entity "+std::to_string(id)+": required component cannot be transient";return false;}
 e->transient=transient;return true;
}

void WorldPersistence::Deformables(RuntimeWorld& w,SaveArchive& a){auto n=count(a,w.m_deformableOwners.size());a.Require(n==w.m_deformableOwners.size(),"deformable participant count differs");std::set<EntityId> seen;for(uint32_t i=0;i<n;++i){EntityId id=0;if(!a.reading){auto it=w.m_deformableOwners.begin();std::advance(it,i);id=*it;}a(id);a.Require(seen.insert(id).second&&w.m_deformableOwners.count(id),"unknown/duplicate saved deformable");std::string error;auto* sim=w.RuntimeDeformable(id,error);a.Require(sim!=nullptr,"required deformable resource not ready: "+error);sim->Persist(a);if(sim->asset->fracture&&sim->asset->fracture->rigid)w.PersistRigidFracture(id,a);for(size_t j=0;j<sim->settings.attachments.size();++j){const auto& attachment=sim->settings.attachments[j];if(attachment.enabled&&!sim->released[j]&&attachment.target)a.Require(w.RuntimeDefinition(attachment.target)&&!w.IsTransientEntity(attachment.target),"required attachment target missing/transient");}if(a.reading){auto& record=w.m_deformables.at(id);record.targets.resize(sim->settings.attachments.size());for(auto& target:record.targets)target.valid=false;++record.revision;}}}
