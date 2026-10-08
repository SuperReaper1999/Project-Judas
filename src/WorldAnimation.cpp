#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include <set>
#include "Ragdoll.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
namespace {
bool PrepareLayer(const SkeletalAsset& asset,const AnimationLayerSettings& settings,RuntimeWorld::AnimationLayer& layer,std::string& error){
 if(!ValidAnimationLayers({settings},error))return false;
 auto clip=std::find_if(asset.clips.begin(),asset.clips.end(),[&](const auto& c){return c.name==settings.clip;});
 if(clip==asset.clips.end()){error="unknown layer clip: "+settings.clip;return false;}
 layer.settings=settings;layer.playback.clip=settings.clip;layer.playback.time=settings.time;layer.playback.speed=settings.speed;
 if(!ResolveJointMask(asset.skeleton,settings.mask,layer.mask,error))return false;
 layer.reference=asset.skeleton.rest;
 if(!settings.referenceClip.empty()){auto ref=std::find_if(asset.clips.begin(),asset.clips.end(),[&](const auto& c){return c.name==settings.referenceClip;});if(ref==asset.clips.end()){error="unknown additive reference clip";return false;}layer.reference=SampleClip(asset.skeleton,*ref,settings.referenceTime);}
 if(settings.additive)for(const auto& p:layer.reference.local)for(int k=0;k<3;++k)if(std::abs(p.scale[k])<1e-8f){error="additive reference scale is zero";return false;}
 return true;
}
}
RuntimeWorld::AnimationInstance* RuntimeWorld::RuntimeAnimation(EntityId id){
 const auto* definition=RuntimeDefinition(id);if(!definition||!definition->animation||!definition->render)return nullptr;
 auto [it,fresh]=m_animationInstances.try_emplace(id);auto& instance=it->second;instance.owner=id;
 if(fresh){const auto& a=*definition->animation;instance.playback.clip=a.clip;instance.playback.playing=a.playOnStart;instance.playback.loop=a.loop;instance.playback.speed=a.speed;instance.playback.time=a.time;instance.mixer.paused=!a.playOnStart;}
 if(m_assets){auto asset=m_assets->TryGetSkeletal(definition->render->meshAsset);if(asset!=instance.asset){instance.asset=asset;instance.external.clear();instance.mixer.Clear();instance.layers.clear();instance.error.clear();instance.previousWorld.clear();instance.recentWorld.clear();instance.motionDt=0;instance.preparedM70=false;instance.previousFinalPose={};instance.fullBodyIK.reset();
  if(definition->animation->fullBodyIK){FullBodyIKRuntime k;k.settings=*definition->animation->fullBodyIK;instance.fullBodyIK=std::move(k);}
  if(asset){if(instance.fullBodyIK)instance.fullBodyIK->prepared=PrepareFullBodyIK(asset->skeleton,instance.fullBodyIK->settings,instance.fullBodyIK->mapping,instance.error);for(const auto& setting:definition->animation->layers){AnimationLayer layer;if(PrepareLayer(*asset,setting,layer,instance.error))instance.layers.push_back(std::move(layer));}ResolveAnimationPose(instance,0);}else instance.skin.clear();}}
 return &instance;
}
SkeletalPose RuntimeWorld::PresentedAnimationPose(EntityId id,float alpha)const{
 auto it=m_animationInstances.find(id);if(it==m_animationInstances.end())return {};const auto& a=it->second;
 if(!a.preparedM70||a.previousFinalPose.local.size()!=a.finalPose.local.size())return a.finalPose;
 return BlendPoses(a.previousFinalPose,a.finalPose,std::clamp(alpha,0.f,1.f));
}
const std::vector<glm::mat4>* RuntimeWorld::AnimationSkin(EntityId id,float alpha)const{
 auto it=m_animationInstances.find(id);if(it==m_animationInstances.end())return nullptr;const auto& a=it->second;
 if(alpha>=1||!a.preparedM70||!a.asset||a.previousFinalPose.local.size()!=a.finalPose.local.size())return &a.skin;
 a.presentedSkin=ResolveSkinMatrices(a.asset->skeleton,PresentedAnimationPose(id,alpha));return &a.presentedSkin;
}
void RuntimeWorld::ResolveAnimationPose(AnimationInstance& instance,float dt){
 JUDAS_PROFILE_SCOPE("Final pose resolution");if(!instance.asset)return;
 const auto& skeleton=instance.asset->skeleton;
 // M70 references are evaluated exactly once at the fixed boundary. Physical
 // contributions can be replaced afterward without resampling clips or IK.
 if(!instance.preparedM70||dt>0){
  instance.sourcePose=instance.mixer.Sample(*instance.asset,instance.playback,dt);std::vector<PoseContribution> contributions;
  for(auto& layer:instance.layers){if(!layer.settings.enabled)continue;auto pose=layer.playback.Evaluate(*instance.asset,instance.mixer.paused?0:dt);PoseContribution c;c.pose=std::move(pose);c.reference=layer.reference;c.weight=layer.settings.weight;c.mask=layer.mask;c.additive=layer.settings.additive;contributions.push_back(std::move(c));}
  instance.referencePose=ResolvePose(skeleton,instance.sourcePose,contributions);
  struct Producer {int order;std::string id;const PoseContribution* external;const LimbIKSettings* limb;};
  std::vector<Producer> sources;for(const auto& [key,c]:instance.external)if(c.order<1000)sources.push_back({c.order,key,&c,nullptr});
  if(const auto* d=RuntimeDefinition(instance.owner);d&&d->animation)for(const auto& k:d->animation->limbs)if(k.enabled)sources.push_back({k.order,k.id,nullptr,&k});
  std::stable_sort(sources.begin(),sources.end(),[](const auto& a,const auto& b){return std::tie(a.order,a.id)<std::tie(b.order,b.id);});
  for(const auto& source:sources){if(source.external)instance.referencePose=ResolvePose(skeleton,instance.referencePose,{*source.external});else {auto k=*source.limb;const auto* d=RuntimeDefinition(instance.owner);if(!d)continue;auto transform=PresentedTransform(instance.owner,d->transform,1);auto model=glm::translate(glm::mat4(1),transform.position)*glm::mat4_cast(transform.rotation)*glm::scale(glm::mat4(1),transform.scale);auto inverse=glm::inverse(model);k.target=glm::vec3(inverse*glm::vec4(k.target,1));k.pole=glm::vec3(inverse*glm::vec4(k.pole,1));SkeletalPose pose;float error=0;std::string diagnostic;if(SolveLimbIK(skeleton,instance.referencePose,k,pose,error,diagnostic))instance.referencePose=std::move(pose);else instance.error=diagnostic;}}
  instance.solvedReferencePose=instance.referencePose;
  if(instance.fullBodyIK&&instance.fullBodyIK->settings.enabled){auto& ik=*instance.fullBodyIK;if(!ik.prepared)ik.prepared=PrepareFullBodyIK(skeleton,ik.settings,ik.mapping,instance.error);
   if(ik.prepared&&ik.hasSample){const auto* d=RuntimeDefinition(instance.owner);if(d){auto transform=PresentedTransform(instance.owner,d->transform,1);std::vector<FullBodyIKTarget> modelTargets;
    if(FullBodyIKTargetsToModel(ik.sampledTargets,glm::dvec3(transform.position),transform.rotation,transform.scale,modelTargets,instance.error)&&SolveFullBodyIK(skeleton,instance.referencePose,ik.mapping,modelTargets,ik.result,instance.error))instance.solvedReferencePose=ik.result.pose;
   }}
  }
 }
 // Physical authority is a final contributor. A target solve never overrides
 // the actual dynamic-body result, and never feeds it back into its reference.
 instance.finalPose=instance.solvedReferencePose;
 std::vector<std::pair<std::string,const PoseContribution*>> physical;for(const auto& [key,c]:instance.external)if(c.order>=1000)physical.push_back({key,&c});
 std::stable_sort(physical.begin(),physical.end(),[](const auto& a,const auto& b){return std::tie(a.second->order,a.first)<std::tie(b.second->order,b.first);});
 for(const auto& source:physical)instance.finalPose=ResolvePose(skeleton,instance.finalPose,{*source.second});
 instance.skin=ResolveSkinMatrices(skeleton,instance.finalPose);
}
void RuntimeWorld::PrepareAnimationReferences(float dt){
 for(auto id:m_animationOwners){const auto* d=RuntimeDefinition(id);if(!d||!d->animation||!d->animation->enabled)continue;
  bool enabled=(d->animation->fullBodyIK&&d->animation->fullBodyIK->enabled)||(d->ragdoll&&d->ragdoll->physicalAnimation&&d->ragdoll->physicalAnimation->enabled);if(!enabled){auto found=m_animationInstances.find(id);if(found!=m_animationInstances.end())found->second.preparedM70=false;continue;}
  auto* a=RuntimeAnimation(id);if(!a||!a->asset)continue;a->previousFinalPose=a->finalPose;if(a->fullBodyIK){a->fullBodyIK->sampledTargets=a->fullBodyIK->settings.targets;a->fullBodyIK->hasSample=true;}a->preparedM70=false;ResolveAnimationPose(*a,dt);a->preparedM70=true;
 }
}
void RuntimeWorld::UpdateAnimations(float dt){
    JUDAS_PROFILE_SCOPE("Animation instances");
 for(auto id:m_animationOwners){const auto* definition=RuntimeDefinition(id);if(!definition){m_animationInstances.erase(id);continue;}
  auto* instance=RuntimeAnimation(id);if(instance&&instance->asset&&definition->animation->enabled){ResolveAnimationPose(*instance,instance->preparedM70?0:dt);
   if(dt>0){instance->previousWorld=instance->recentWorld;auto transform=PresentedTransform(id,definition->transform,1);auto model=glm::translate(glm::mat4(1),transform.position)*glm::mat4_cast(transform.rotation)*glm::scale(glm::mat4(1),transform.scale);instance->recentWorld=PoseGlobalMatrices(instance->asset->skeleton,instance->finalPose);for(auto& m:instance->recentWorld)m=model*m;instance->motionDt=dt;}
  }
 }
}
bool RuntimeWorld::SetPoseContribution(EntityId id,const std::string& name,const PoseContribution& contribution,std::string& error){
 auto* instance=RuntimeAnimation(id);if(!instance||!instance->asset){error="animation asset is not ready";return false;}
 const auto& skeleton=instance->asset->skeleton;
 if(name.empty()||(!instance->external.count(name)&&instance->external.size()>=16)||!std::isfinite(contribution.weight)||contribution.weight<0||contribution.weight>1){error="invalid pose contribution";return false;}
 if(!ValidPose(skeleton,contribution.pose,error)||(!contribution.reference.local.empty()&&!ValidPose(skeleton,contribution.reference,error)))return false;
 for(int node:contribution.mask)if(node<0||size_t(node)>=skeleton.rest.local.size()){error="invalid pose mask";return false;}
 if(contribution.additive){const auto& ref=contribution.reference.local.empty()?skeleton.rest:contribution.reference;for(const auto& p:ref.local)for(int k=0;k<3;++k)if(std::abs(p.scale[k])<1e-8f){error="additive reference scale is zero";return false;}}
 auto copy=contribution;for(auto& p:copy.pose.local)p.rotation=glm::normalize(p.rotation);for(auto& p:copy.reference.local)p.rotation=glm::normalize(p.rotation);
 if(copy.order<1000)instance->preparedM70=false;
 instance->external[name]=std::move(copy);ResolveAnimationPose(*instance,0);return true;
}
void RuntimeWorld::RemovePoseContribution(EntityId id,const std::string& name){auto* instance=RuntimeAnimation(id);if(instance){auto found=instance->external.find(name);if(found!=instance->external.end()&&found->second.order<1000)instance->preparedM70=false;instance->external.erase(name);ResolveAnimationPose(*instance,0);}}
bool RuntimeWorld::SetFinalPose(EntityId id,const SkeletalPose& pose,std::string& error){PoseContribution c;c.pose=pose;return SetPoseContribution(id,"externalOverride",c,error);}
bool RuntimeWorld::SetAnimationLayer(EntityId id,const AnimationLayerSettings& settings,bool remove,std::string& error){
 auto* instance=RuntimeAnimation(id);if(!instance||!instance->asset){error="animation asset is not ready";return false;}
 auto it=std::find_if(instance->layers.begin(),instance->layers.end(),[&](const auto& l){return l.settings.id==settings.id;});
 if(remove){if(it!=instance->layers.end())instance->layers.erase(it);instance->preparedM70=false;ResolveAnimationPose(*instance,0);return true;}
 if(it==instance->layers.end()&&instance->layers.size()>=16){error="too many layers";return false;}AnimationLayer layer;if(!PrepareLayer(*instance->asset,settings,layer,error))return false;
 if(it!=instance->layers.end()){if(it->settings.clip==settings.clip){layer.playback=it->playback;layer.playback.speed=settings.speed;}*it=std::move(layer);}else instance->layers.push_back(std::move(layer));instance->preparedM70=false;ResolveAnimationPose(*instance,0);return true;
}

bool RuntimeWorld::JointPose(EntityId id,const std::string& key,const std::string& space,float alpha,SceneTransform& out){
 auto* a=RuntimeAnimation(id);if(!a||!a->asset)return false;const auto& s=a->asset->skeleton;int n=FindSkeletonJoint(s,key);if(n<0||size_t(n)>=a->finalPose.local.size())return false;
 auto presented=PresentedAnimationPose(id,alpha);
 if(space=="local"){const auto& t=presented.local[n];out={t.translation,t.rotation,t.scale};return true;}
 if(space!="model"&&space!="world")return false;
 auto matrix=PoseGlobalMatrices(s,presented)[n];
 if(space=="world"){auto* d=RuntimeDefinition(id);auto t=PresentedTransform(id,d->transform,alpha);matrix=glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale)*matrix;}
 JointTransform t;std::string error;if(!DecomposeRigidPose(matrix,t,error))return false;out={t.translation,t.rotation,t.scale};return true;
}
bool RuntimeWorld::SetLimbIK(EntityId id,const LimbIKSettings& k,bool remove,std::string& error){
 auto* d=RuntimeDefinition(id);if(!d||!d->animation){error="entity has no animation";return false;}auto* a=RuntimeAnimation(id);if(!a||!a->asset){error="skeleton is not ready";return false;}
 auto settings=*d->animation;auto it=std::find_if(settings.limbs.begin(),settings.limbs.end(),[&](const auto& v){return v.id==k.id;});
 if(remove){if(it!=settings.limbs.end())settings.limbs.erase(it);}else{SkeletalPose tested;float residual=0;if(!SolveLimbIK(a->asset->skeleton,a->finalPose,k,tested,residual,error))return false;if(it==settings.limbs.end()){if(settings.limbs.size()>=16){error="16 limb contributor limit";return false;}settings.limbs.push_back(k);}else *it=k;}
 m_scriptDefinitions.at(id).animation=settings;a->preparedM70=false;ResolveAnimationPose(*a,0);return true;
}
bool RuntimeWorld::ConfigureFullBodyIK(EntityId id,const std::optional<FullBodyIKSettings>& settings,std::string& error){
 auto* d=RuntimeDefinition(id);if(!d||!d->animation){error="entity has no animation";return false;}auto* a=RuntimeAnimation(id);if(!a||!a->asset){error="IK skeleton is not ready";return false;}
 std::optional<FullBodyIKRuntime> pending;
 if(settings){FullBodyIKRuntime candidate;candidate.settings=*settings;if(!PrepareFullBodyIK(a->asset->skeleton,candidate.settings,candidate.mapping,error))return false;candidate.prepared=true;
  auto transform=PresentedTransform(id,d->transform,1);std::vector<FullBodyIKTarget> converted;if(!FullBodyIKTargetsToModel(candidate.settings.targets,glm::dvec3(transform.position),transform.rotation,transform.scale,converted,error))return false;
  if(!SolveFullBodyIK(a->asset->skeleton,a->referencePose.local.empty()?a->finalPose:a->referencePose,candidate.mapping,converted,candidate.result,error))return false;
  candidate.result={};pending=std::move(candidate);
 }
 m_scriptDefinitions.at(id).animation->fullBodyIK=settings;a->fullBodyIK=std::move(pending);a->preparedM70=false;a->error.clear();return true;
}
bool RuntimeWorld::SetFullBodyIKTargets(EntityId id,const std::vector<FullBodyIKTarget>& targets,std::string& error){
 auto* d=RuntimeDefinition(id);auto* a=RuntimeAnimation(id);if(!d||!d->animation||!a||!a->asset||!a->fullBodyIK){error="IK configuration/skeleton is unavailable";return false;}auto settings=a->fullBodyIK->settings;settings.targets=targets;
 if(!ValidFullBodyIKSettings(settings,error))return false;
 auto transform=PresentedTransform(id,d->transform,1);std::vector<FullBodyIKTarget> converted;if(!FullBodyIKTargetsToModel(targets,glm::dvec3(transform.position),transform.rotation,transform.scale,converted,error))return false;
 // Submit atomically. The next fixed reference phase samples the targets; reads
 // during the calling callback still describe the previous resolved sample.
 a->fullBodyIK->settings.targets=targets;m_scriptDefinitions.at(id).animation->fullBodyIK=settings;return true;
}
bool RuntimeWorld::SetSocket(EntityId id,const SceneSocketComponent& s,bool remove,std::string& error){
 auto* d=RuntimeDefinition(id);if(!d){error="stale socket owner";return false;}if(remove){m_scriptDefinitions.at(id).socket.reset();return true;}
 if(!s.target||s.joint.empty()||d->body.has_value()||d->characterMotor||d->ragdoll||d->deformable){error="visual socket cannot own physical motion";return false;}
 auto* skeleton=RuntimeAnimation(s.target);if(!skeleton||!skeleton->asset||FindSkeletonJoint(skeleton->asset->skeleton,s.joint)<0){error="socket target skeleton/joint is unavailable";return false;}
 std::set<EntityId> seen{id};EntityId target=s.target;while(target){if(!seen.insert(target).second){error="socket attachment cycle";return false;}auto* t=RuntimeDefinition(target);if(!t){error="missing socket target";return false;}target=t->socket?t->socket->target:t->parent;}
 auto finite=[](glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};if(!finite(s.offset.position)||!finite(s.offset.scale)){error="invalid socket transform";return false;}
 auto q=glm::dot(s.offset.rotation,s.offset.rotation);if(!std::isfinite(q)||q<1e-12f){error="invalid socket orientation";return false;}
 m_scriptDefinitions.at(id).socket=s;return true;
}
void RuntimeWorld::UpdateSockets(){
 std::set<EntityId> done,visiting;std::function<void(EntityId)> visit=[&](EntityId id){if(done.count(id)||!visiting.insert(id).second)return;const auto* d=RuntimeDefinition(id);if(d&&d->socket&&d->socket->enabled){const auto s=*d->socket;visit(s.target);SceneTransform joint;if(JointPose(s.target,s.joint,"world",1,joint)){SceneTransform pose;pose.position=joint.position+joint.rotation*(joint.scale*s.offset.position);pose.rotation=glm::normalize(joint.rotation*s.offset.rotation);pose.scale=joint.scale*s.offset.scale;SetRuntimeTransform(id,pose);}}visiting.erase(id);done.insert(id);};
 for(const auto& [id,d]:m_scriptDefinitions)if(d.socket)visit(id);
}
