#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldStreaming.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
namespace {
bool Pose(const RuntimeWorld& world,SceneObjectId id,const SceneTransform& authored,float alpha,glm::vec3& p,glm::quat& q){
 if(const auto* entity=world.FindEntity(id))if(entity->lifecycle!=EntityLifecycle::Active)return false;
 auto t=world.PresentedTransform(id,authored,alpha);p=t.position;q=glm::normalize(t.rotation);return true;
}
BodyHandle MotionBody(RuntimeWorld& w,EntityId id){
 for(unsigned depth=0;id&&depth<64;++depth){auto b=w.RuntimeBody(id);if(b.IsValid())return b;auto* d=w.RuntimeDefinition(id);id=d?d->parent:0;}return {};
}
glm::vec3 Motion(RuntimeWorld& w,EntityId id,glm::vec3 point,glm::vec3 previous,float dt,bool valid){
 if(!valid||dt<=0||dt>.25f||glm::distance(point,previous)>std::max(1.f,dt*60.f))return glm::vec3(0);
 auto b=MotionBody(w,id);if(b.IsValid())return w.Physics().GetLinearVelocity(b)+glm::cross(w.Physics().GetAngularVelocity(b),point-w.Physics().GetTransform(b).position);
 for(unsigned depth=0;id&&depth<64;++depth){if(auto* motor=w.RuntimeCharacter(id))return motor->result.velocity;auto* d=w.RuntimeDefinition(id);id=d?d->parent:0;}
 return (point-previous)/dt;
}
std::string StableKey(RuntimeWorld& w,EntityId id){return w.AudioStableIdentity(id);}
}
void RuntimeWorld::ResetAudioMotion(){m_listenerMotionValid=false;for(auto& e:m_audioEmitters)e.motionValid=false;}
void RuntimeWorld::BeginAudio(){if(m_audioResume.empty())EndAudio();m_audioRunning=true;if(m_audioSystem){AudioGroupSettings master;bool retained=m_audioSystem->GetGroup("master",master);m_audioSystem->ConfigureGroups(audioGroups);if(m_restoreConstruction&&retained)m_audioSystem->SetGroup("master",master);}for(auto& e:m_audioEmitters){if(!IsPublished(e.id))continue;e.wantPlay=m_audioResume.count(e.id)?m_audioResume.at(e.id).requested:e.settings.playOnStart;e.error.clear();}ResetAudioMotion();}
void RuntimeWorld::ReleaseEntityAudio(SceneObjectId id){
 for(auto& e:m_audioEmitters)if(e.id==id){if(m_audioSystem)m_audioSystem->DestroyVoice(e.voice);e.voice={};e.wantPlay=false;}
 for(auto it=m_audioOneShots.begin();it!=m_audioOneShots.end();)if(it->first==id){if(m_audioSystem)m_audioSystem->DestroyVoice(it->second);it=m_audioOneShots.erase(it);}else ++it;
}
bool RuntimeWorld::PlayAudioOneShot(SceneObjectId id,std::string& error){
 if(!m_audioRunning||!m_audioSystem||!m_assets||!IsPublished(id)){error="audio world is inactive";return false;}
 for(auto& e:m_audioEmitters)if(e.id==id){if(!e.settings.enabled||e.settings.loading!=AudioLoading::Buffered){error="one-shots require an enabled buffered emitter";return false;}
  AudioGroupSettings group,master;if(m_audioSystem->GetGroup(e.settings.group,group)&&m_audioSystem->GetGroup("master",master)&&(group.paused||master.paused)){error.clear();return false;}
  if(m_audioOneShots.size()>=32){error="32 world one-shots already active";return false;}
  auto clip=m_assets->GetAudio(e.settings.asset,error);if(!clip.IsValid())return false;glm::vec3 p;glm::quat q;if(!Pose(*this,id,e.transform,1,p,q))return false;
  auto h=m_audioSystem->PlayOneShot(clip,e.settings,p,error);if(!h.IsValid())return false;m_audioOneShots.push_back({id,h});return true;}
 error="entity has no audio emitter";return false;
}
void RuntimeWorld::EndAudio(){EndWeatherAudio();for(auto& shot:m_audioOneShots)if(m_audioSystem)m_audioSystem->DestroyVoice(shot.second);m_audioOneShots.clear();for(auto& e:m_audioEmitters){if(m_audioSystem)m_audioSystem->DestroyVoice(e.voice);e.voice={};}if(m_audioSystem&&m_audioRunning)m_audioSystem->ResetEnvironment();m_audioRunning=false;ResetAudioMotion();}
void RuntimeWorld::UpdateAudio(const glm::mat4& activeView,float alpha,float dt){
 JUDAS_PROFILE_SCOPE("Audio main update");
 if(!m_audioRunning||!m_audioSystem||!m_assets)return;
 glm::vec3 lp{0};glm::quat lq{1,0,0,0};bool listener=false;
 if(m_audioListener&&m_audioListener->settings.enabled){auto& l=*m_audioListener;listener=Pose(*this,l.id,l.transform,alpha,lp,lq);if(listener&&l.settings.followActiveView){auto t=glm::inverse(activeView);lp=glm::vec3(t[3]);lq=glm::normalize(glm::quat_cast(glm::mat3(t)));}}
 glm::vec3 lv{0};if(listener){if(m_audioListener->settings.followActiveView)lv=Motion(*this,0,lp,m_lastListenerPosition,dt,m_listenerMotionValid);else lv=Motion(*this,m_audioListener->id,lp,m_lastListenerPosition,dt,m_listenerMotionValid);m_lastListenerPosition=lp;}m_listenerMotionValid=listener;
 if(listener){m_audioSystem->SetListener(lp,lq);m_audioSystem->SetListenerMotion(lv);}
 UpdateWeatherAudio(m_settings.worldOrigin+glm::dvec3(lp),listener);
 std::vector<AudioEmitter*> eligible;
 for(auto& e:m_audioEmitters){if(!IsPublished(e.id))continue;
  glm::vec3 p;glm::quat q;const bool active=e.settings.enabled&&Pose(*this,e.id,e.transform,alpha,p,q)&&(!e.settings.spatial||listener);
  if(!active){m_audioSystem->DestroyVoice(e.voice);e.voice={};e.motionValid=false;continue;}
  AudioVoiceSnapshot existing;if(e.voice.IsValid()&&!m_audioSystem->Snapshot(e.voice,existing))e.voice={};
  if(!e.voice.IsValid()){
   std::string error;
   if(e.settings.loading==AudioLoading::Streamed){auto path=m_assets->GetStreamAudioPath(e.settings.asset,error);if(!path.empty())e.voice=m_audioSystem->CreateStreamVoice(path,e.settings,error);}
   else {auto clip=m_assets->GetAudio(e.settings.asset,error);if(clip.IsValid())e.voice=m_audioSystem->CreateVoice(clip,e.settings,error);}
   if(e.voice.IsValid()){m_audioSystem->SetPosition(e.voice,p);
    if(!m_audioResume.count(e.id)&&e.wantPlay)m_audioSystem->Play(e.voice);
   }
   if(!error.empty()&&error!="loading"&&error!=e.error)std::fprintf(stderr,"audio emitter %llu: %s\n",static_cast<unsigned long long>(e.id),error.c_str());
   e.error=error;
  }
  if(e.voice.IsValid()){
   auto restored=m_audioResume.find(e.id);if(restored!=m_audioResume.end()){
    auto saved=restored->second;if(!m_audioSystem->RestorePlayback(e.voice,saved.time,saved.state,saved.groupGain,saved.fadeFrames)){e.error="saved audio state incompatible";continue;}m_audioResume.erase(restored);
   }
   m_audioSystem->SetSettings(e.voice,e.settings);m_audioSystem->SetPosition(e.voice,p);
   auto velocity=e.explicitVelocity?*e.explicitVelocity:Motion(*this,e.id,p,e.lastPosition,dt,e.motionValid);
   m_audioSystem->SetMotion(e.voice,velocity);e.lastVelocity=velocity;e.lastPosition=p;e.motionValid=true;
   m_audioSystem->Snapshot(e.voice,existing);if(!existing.error.empty()&&existing.error!=e.error)std::fprintf(stderr,"audio emitter: %s\n",existing.error.c_str());e.error=existing.error;
   bool playing=existing.state==AudioPlaybackState::Playing;
   if(!playing)for(auto& shot:m_audioOneShots)if(shot.first==e.id){AudioVoiceSnapshot once;if(m_audioSystem->Snapshot(shot.second,once)&&once.state==AudioPlaybackState::Playing){playing=true;break;}}
   AudioGroupSettings group,master;
   const bool audible=e.settings.volume>1e-4f && existing.distanceGain>1e-4f &&
       m_audioSystem->GetGroup(e.settings.group,group) && !group.mute && !group.paused && group.gain>1e-4f &&
       m_audioSystem->GetGroup("master",master) && !master.mute && !master.paused && master.gain>1e-4f;
   if(e.settings.occlusion&&e.settings.spatial&&!e.settings.bypass&&playing&&audible)eligible.push_back(&e);
  }
 }
 if(listener){m_audioSystem->SetListener(lp,lq);m_audioSystem->SetListenerMotion(lv);}
 for(auto it=m_audioOneShots.begin();it!=m_audioOneShots.end();){AudioVoiceSnapshot state;
  if(!m_audioSystem->Snapshot(it->second,state)){it=m_audioOneShots.erase(it);continue;}
  for(auto& e:m_audioEmitters)if(e.id==it->first){glm::vec3 p;glm::quat q;if(!e.settings.enabled||!Pose(*this,e.id,e.transform,alpha,p,q)){m_audioSystem->DestroyVoice(it->second);break;}auto once=static_cast<AudioSettings>(e.settings);once.loop=false;m_audioSystem->SetSettings(it->second,once);m_audioSystem->SetPosition(it->second,p);m_audioSystem->SetMotion(it->second,e.lastVelocity);m_audioSystem->SetOcclusion(it->second,e.obstruction);break;}
  ++it;
 }
 // Resident authoritative geometry; at most 8 queries every 50 ms, fair stable rotation.
 m_audioQueryClock+=std::max(0.f,dt);
 if(listener&&!eligible.empty()&&m_audioQueryClock>=.05f){
  JUDAS_PROFILE_SCOPE("Audio obstruction queries");m_audioQueryClock=0;
  std::sort(eligible.begin(),eligible.end(),[&](auto* a,auto* b){return StableKey(*this,a->id)<StableKey(*this,b->id);});
  unsigned n=std::min<std::size_t>(8,eligible.size());for(unsigned i=0;i<n;++i){auto& e=*eligible[(m_audioQueryCursor++)%eligible.size()];AudioVoiceSnapshot v;m_audioSystem->Snapshot(e.voice,v);
   PhysicsQueryFilter filter;filter.includeLayers=e.settings.occlusionMask;auto source=MotionBody(*this,e.id),target=m_audioListener?MotionBody(*this,m_audioListener->id):BodyHandle{};if(source.IsValid())filter.ignoredBodies.push_back(source);if(target.IsValid())filter.ignoredBodies.push_back(target);
   const auto d=lp-v.position;const float distance=glm::length(d);bool hit=false;if(distance>1e-4f)hit=m_physics.Raycast(v.position,d,distance,filter).hit;
   e.obstruction=hit?1:0;m_audioSystem->SetOcclusion(e.voice,e.obstruction);++m_audioQueries;
  }
 }
 // Listener weighted, oriented zones. Highest authored priority; stable owner/source order ties.
 AudioEnvironmentSettings environment;float weight=0;int priority=std::numeric_limits<int>::min();float total=0;bool zonePending=false;
 std::vector<AudioZone*> zones;for(auto& z:m_audioZones)if(IsPublished(z.id)&&z.settings.enabled)zones.push_back(&z);
 std::sort(zones.begin(),zones.end(),[&](auto* a,auto* b){return StableKey(*this,a->id)<StableKey(*this,b->id);});
 for(auto* zone:zones){auto& z=*zone;glm::vec3 p;glm::quat q;if(!listener||!Pose(*this,z.id,z.transform,alpha,p,q))continue;
  auto local=glm::inverse(q)*(lp-p);float inside=z.settings.shape==SceneRegionShape::Sphere?z.settings.radius-glm::length(local):std::min({z.settings.halfExtents.x-std::abs(local.x),z.settings.halfExtents.y-std::abs(local.y),z.settings.halfExtents.z-std::abs(local.z)});
  if(inside<=0)continue;
  float contribution=z.settings.amount*(z.settings.blendDistance>0?glm::clamp(inside/z.settings.blendDistance,0.f,1.f):1.f);
  if(contribution<=0||z.settings.priority<priority)continue;
  if(z.settings.priority>priority){priority=z.settings.priority;total=0;environment={0,0,0,0};zonePending=false;}
  std::string error;auto settings=m_assets->GetAudioEnvironment(z.settings.asset,error);if(!settings){zonePending|=error=="loading";continue;}
  environment.roomSize+=settings->roomSize*contribution;environment.damping+=settings->damping*contribution;environment.width+=settings->width*contribution;environment.wet+=settings->wet*contribution;total+=contribution;
 }
 if(total>0){environment.roomSize/=total;environment.damping/=total;environment.width/=total;environment.wet/=total;weight=std::min(1.f,total);}
 if(!zonePending)m_audioSystem->SetEnvironment(environment,weight);
 m_audioSystem->Update();
 JUDAS_PROFILE_COUNTER("Audio occlusion queries",double(m_audioQueries),ProfileCounterMode::Latest);
}
bool RuntimeWorld::PlayAudio(SceneObjectId id){for(auto& e:m_audioEmitters)if(e.id==id){e.wantPlay=true;return !e.voice.IsValid()||(m_audioSystem&&m_audioSystem->Play(e.voice));}return false;}
bool RuntimeWorld::StopAudio(SceneObjectId id){for(auto& e:m_audioEmitters)if(e.id==id){e.wantPlay=false;return !e.voice.IsValid()||(m_audioSystem&&m_audioSystem->Stop(e.voice));}return false;}
bool RuntimeWorld::PauseAudio(SceneObjectId id){for(auto& e:m_audioEmitters)if(e.id==id)return m_audioSystem&&m_audioSystem->Pause(e.voice);return false;}
bool RuntimeWorld::ResumeAudio(SceneObjectId id){for(auto& e:m_audioEmitters)if(e.id==id)return m_audioSystem&&m_audioSystem->Resume(e.voice);return false;}
bool RuntimeWorld::SeekAudio(SceneObjectId id,double seconds){for(auto& e:m_audioEmitters)if(e.id==id)return m_audioSystem&&m_audioSystem->Seek(e.voice,seconds);return false;}
bool RuntimeWorld::SetAudioSettings(SceneObjectId id,const AudioSettings& settings){if(!ValidAudioSettings(settings))return false;for(auto& e:m_audioEmitters)if(e.id==id){const bool replace=e.settings.loading!=settings.loading||e.settings.streamPageFrames!=settings.streamPageFrames;static_cast<AudioSettings&>(e.settings)=settings;if(replace&&m_audioSystem){m_audioSystem->DestroyVoice(e.voice);e.voice={};e.motionValid=false;}return !e.voice.IsValid()||(m_audioSystem&&m_audioSystem->SetSettings(e.voice,settings));}return false;}
bool RuntimeWorld::SetAudioVelocity(SceneObjectId id,std::optional<glm::vec3> velocity){for(auto& e:m_audioEmitters)if(e.id==id){e.explicitVelocity=velocity;return true;}return false;}
bool RuntimeWorld::SetAudioEnabled(SceneObjectId id,bool enabled){for(auto& e:m_audioEmitters)if(e.id==id){e.settings.enabled=enabled;if(!enabled){const bool requested=e.wantPlay;ReleaseEntityAudio(id);e.wantPlay=requested;e.motionValid=false;}return true;}return false;}
