#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "Prefab.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include <glm/gtc/matrix_transform.hpp>
int checks=0,failures=0;
void Check(bool value,const char* text){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",text);}
int main(){std::string error;JobSystem jobs(2);AudioSystem audio;audio.SetJobSystem(&jobs);Check(audio.Init(error,true),"production audio without device");Project p;Check(p.Load("projects/audio_lab/audio_lab.judasproj",error),"normal current lab project");if(p.RootDir().empty()){std::puts(error.c_str());return 1;}AssetDatabase db;db.Scan(p.RootDir(),p.AssetsDir());ResourceManager resources(nullptr,&db,&jobs,&audio);
 Scene s;auto& listener=s.CreateObject("Listener");listener.audioListener=SceneAudioListenerComponent{};listener.transform.position={0,0,5};auto& source=s.CreateObject("Source");source.transform.position={0,0,-5};source.audioEmitter=SceneAudioEmitterComponent{};source.audioEmitter->asset="60606060606060606060606060606002";source.audioEmitter->playOnStart=true;source.audioEmitter->loop=true;source.audioEmitter->occlusion=true;source.audioEmitter->doppler=1;source.audioEmitter->send=1;
 auto& wall=s.CreateObject("Physics-only wall");wall.body=SceneBodyComponent{};wall.body->halfExtents={2,2,.2};
 RuntimeWorld world;Check(world.Build(s,&resources,error),"normal audio fixture builds using authoritative colliders");resources.WaitForAll();world.BeginAudio();world.UpdateAudio(glm::mat4(1),1,.06f);auto h=world.AudioEmitters()[0].voice;AudioVoiceSnapshot snap;audio.Snapshot(h,snap);Check(snap.occlusionGain<.3&&snap.cutoff<1500,"real resident collider sets obstruction gain/filter");Check(world.AudioQueryCount()==1,"bounded query diagnostics from actual PhysicsWorld");
 auto pcmRms=[&]{std::vector<float> warm(48000),pcm(48000);audio.ReadWithoutDevice(warm.data(),24000);audio.ReadWithoutDevice(pcm.data(),24000);double energy=0;for(float v:pcm)energy+=v*v;return std::sqrt(energy/pcm.size());};const auto blockedRms=pcmRms();
 const auto quietQueries=world.AudioQueryCount();audio.SetGroup("master",{1,true,false});world.UpdateAudio(glm::mat4(1),1,.06);Check(world.AudioQueryCount()==quietQueries,"muted master does not spend obstruction queries");audio.SetGroup("master",{});
 auto collider=world.RuntimeBody(3);world.Physics().SetBodyEnabled(collider,false);world.UpdateAudio(glm::mat4(1),1,.06);audio.Snapshot(h,snap);Check(snap.occlusionGain==1&&snap.cutoff==24000,"disabled/open collider restores unblocked targets");const auto openRms=pcmRms();Check(blockedRms>0&&blockedRms<openRms*.6,"real collider drives audible PCM attenuation/filter rather than just a flag");std::printf("OCCLUSION blocked_rms=%.9f open_rms=%.9f\n",blockedRms,openRms);world.Physics().SetBodyEnabled(collider,true);world.Physics().SetCollisionFilter(collider,1,kAllCategories);auto settings=static_cast<AudioSettings>(*s.Find(2)->audioEmitter);settings.occlusionMask=1;world.SetAudioSettings(2,settings);world.UpdateAudio(glm::mat4(1),1,.06);audio.Snapshot(h,snap);Check(snap.occlusionGain==1,"existing collision include mask filters wall");world.Physics().SetCollisionFilter(collider,0,kAllCategories);world.Physics().SetBodySensor(collider,true);world.UpdateAudio(glm::mat4(1),1,.06);audio.Snapshot(h,snap);Check(snap.occlusionGain==1,"normal sensor geometry excluded from audio ray");
 // Physics-only occluder has no entity wrapper; body observations still matter.
 auto standalone=world.Physics().CreateStaticBox({0,0,0},{1,1,.2},.5,0);world.UpdateAudio(glm::mat4(1),1,.06);audio.Snapshot(h,snap);Check(snap.occlusionGain<1,"wall without entity wrapper still obstructs");world.Physics().DestroyBody(standalone);
 Check(world.PlayAudioOneShot(2,error)&&world.PlayAudioOneShot(2,error)&&audio.VoiceCount()==3,"repeated buffered shots have independent voices");world.SetAudioEnabled(2,false);Check(audio.VoiceCount()==0,"component disable releases every owned one-shot");world.SetAudioEnabled(2,true);world.UpdateAudio(glm::mat4(1),1,.06);world.EndAudio();world.Destroy();Check(audio.VoiceCount()==0,"full Stop cleans normal voices and effects");
 const std::string alternate="60606060606060606060606060606004";
 const std::string unknown="ffffffffffffffffffffffffffffffff";
 Scene overrideScene;
 auto& emitter=overrideScene.CreateObject("Clip override source");
 emitter.audioEmitter=*s.Find(2)->audioEmitter;
 emitter.audioEmitter->playOnStart=false;
 emitter.audioEmitter->spatial=false;
 RuntimeWorld overrideWorld;
 Check(overrideWorld.Build(overrideScene,&resources,error),"clip override fixture builds");
 resources.RequestAudio(alternate);
 resources.WaitForAll();
 overrideWorld.BeginAudio();
 Check(!overrideWorld.PlayAudioOneShot(1,error,&unknown)&&!error.empty()&&audio.VoiceCount()==0,
       "invalid clip override does not allocate a voice");
 Check(overrideWorld.PlayAudioOneShot(1,error,&alternate)&&audio.VoiceCount()==1&&
       overrideWorld.AudioEmitters().front().settings.asset!=alternate,
       "registered override starts a voice without changing the authored clip");
 audio.AdvanceWithoutDevice(60000);
 audio.Update();
 Check(audio.VoiceCount()==0,"one-second override retires before the two-second authored clip would");
 overrideWorld.Destroy();
 // Manual one-shots are audible query demand even when the persistent source is stopped.
 Scene once;auto& ol=once.CreateObject("Listener");ol.audioListener=SceneAudioListenerComponent{};auto& oe=once.CreateObject("Manual emitter");oe.transform.position={0,0,-5};oe.audioEmitter=*s.Find(2)->audioEmitter;oe.audioEmitter->playOnStart=false;auto& ow=once.CreateObject("Wall");ow.transform.position={0,0,-2};ow.body=SceneBodyComponent{};ow.body->halfExtents={2,2,.2};RuntimeWorld one;Check(one.Build(once,&resources,error),"manual shot world");resources.WaitForAll();one.BeginAudio();one.UpdateAudio(glm::mat4(1),1,.06);Check(one.AudioQueryCount()==0&&one.PlayAudioOneShot(2,error),"stopped emitter has no ray until independent shot plays");one.UpdateAudio(glm::mat4(1),1,.06);Check(one.AudioQueryCount()==1,"independent shot participates in bounded obstruction demand");one.Destroy();
 // Point velocity of offset child under rotating parent, then discontinuity reset.
 Scene moving;auto& l=moving.CreateObject("View");l.audioListener=SceneAudioListenerComponent{};l.transform.position={0,0,10};auto& b=moving.CreateObject("Rotating parent");b.body=SceneBodyComponent{};b.body->motion=SceneBodyMotion::Dynamic;b.body->managed=true;auto& child=moving.CreateObject("Offset emitter");child.parent=2;child.transform.position={2,0,0};child.audioEmitter=*s.Find(2)->audioEmitter;child.audioEmitter->occlusion=false;RuntimeWorld motion;Check(motion.Build(moving,&resources,error),"attached emitter hierarchy");resources.WaitForAll();motion.BeginAudio();motion.UpdateAudio(glm::mat4(1),1,.016);motion.Physics().SetAngularVelocity(motion.RuntimeBody(2),{0,-5,0});motion.UpdateAudio(glm::mat4(1),1,.016);audio.Snapshot(motion.AudioEmitters()[0].voice,snap);Check(snap.dopplerRatio>1.02,"rotating attached offset uses angular point velocity");auto t=motion.RuntimeDefinition(2)->transform;t.position={100,0,0};motion.SetRuntimeTransform(2,t);motion.UpdateAudio(glm::mat4(1),1,.016);audio.Snapshot(motion.AudioEmitters()[0].voice,snap);Check(snap.dopplerRatio==1,"teleport resets motion instead of making pitch spike");motion.Destroy();
 // Zones use world orientation and stable identity, independent of authored traversal order.
 auto zonePcm=[&](bool reversed){Scene zones;auto& l=zones.CreateObject("Listener");l.audioListener=SceneAudioListenerComponent{};l.transform.position={3,4,5};auto& e=zones.CreateObject("Impulse");e.audioEmitter=SceneAudioEmitterComponent{};e.audioEmitter->asset="60606060606060606060606060606004";e.audioEmitter->spatial=false;e.audioEmitter->send=1;
  for(int i=0;i<2;++i){auto& z=zones.CreateObject("Authored zone");z.audioZone=SceneAudioZoneComponent{};z.transform.position={3,4,5};z.transform.rotation=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));z.audioZone->shape=SceneRegionShape::Box;z.audioZone->halfExtents={3,1,2};z.audioZone->asset=i?"60606060606060606060606060606006":"60606060606060606060606060606005";}
  if(reversed)std::reverse(zones.Objects().begin(),zones.Objects().end());
  RuntimeWorld w;Check(w.Build(zones,&resources,error),"oriented equal-priority authored zones");resources.WaitForAll();w.BeginAudio();w.UpdateAudio(glm::mat4(1),1,.016);w.PlayAudio(2);std::vector<float> output(96000);Check(audio.ReadWithoutDevice(output.data(),48000),"zone impulse actual PCM");Check(audio.Diagnostics().reverbProcessors==1,"overlapping zones share one bounded processor");w.Destroy();return output;};
 auto first=zonePcm(false),second=zonePcm(true);double difference=0;for(size_t i=0;i<first.size();++i)difference=std::max(difference,double(std::abs(first[i]-second[i])));Check(difference<1e-6,"equal-priority zone selection independent from authored traversal order");
 // Small CPU submission/DSP workload comparison; no fake device-latency claim.
 for(int mode=0;mode<4;++mode){int count=mode==0?0:mode==3?32:8;Scene fixture;auto& listener=fixture.CreateObject("Listener");listener.audioListener=SceneAudioListenerComponent{};
  auto& block=fixture.CreateObject("Resident wall");block.body=SceneBodyComponent{};block.body->halfExtents={8,2,.1};block.transform.position={0,0,-2};
  if(mode>=2){auto& zone=fixture.CreateObject("Zone");zone.audioZone=SceneAudioZoneComponent{};zone.audioZone->asset="60606060606060606060606060606005";zone.audioZone->radius=50;}
  for(int i=0;i<count;++i){auto& e=fixture.CreateObject("Emitter");e.transform.position={float(i%8)-4,0,-5};e.audioEmitter=SceneAudioEmitterComponent{};auto& a=*e.audioEmitter;a.asset="60606060606060606060606060606002";a.playOnStart=true;a.loop=true;a.volume=.01f;a.attenuation=AudioAttenuation::None;
   if(mode>=2){a.send=.3;a.occlusion=true;if(i==0){a.loading=AudioLoading::Streamed;a.asset="60606060606060606060606060606001";a.spatial=false;}}}
  RuntimeWorld runtime;Check(runtime.Build(fixture,&resources,error),"normal bounded performance world");resources.WaitForAll();runtime.BeginAudio();std::vector<double> frames,control,dsp;std::vector<float> pcm(1600);
  for(int frame=0;frame<180;++frame){auto start=std::chrono::steady_clock::now();runtime.UpdateAudio(glm::mat4(1),1,1.f/60);auto middle=std::chrono::steady_clock::now();audio.ReadWithoutDevice(pcm.data(),800);auto end=std::chrono::steady_clock::now();if(frame>30){control.push_back(std::chrono::duration<double,std::milli>(middle-start).count());dsp.push_back(std::chrono::duration<double,std::milli>(end-middle).count());frames.push_back(std::chrono::duration<double,std::milli>(end-start).count());}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  auto stats=audio.Diagnostics();auto summary=[&](std::vector<double> values,const char* key){std::sort(values.begin(),values.end());std::printf(" %s_median_ms=%.5f %s_p95_ms=%.5f %s_max_ms=%.5f",key,values[values.size()/2],key,values[values.size()*95/100],key,values.back());};
  std::printf("WORKLOAD mode=%d voices=%zu streams=%zu PCM=%zu buffered=%zu reverb=%zu rays=%llu retirements=%zu underruns=%llu",mode,stats.voices,stats.streams,stats.streamBytes,stats.bufferedBytes,stats.reverbProcessors,(unsigned long long)runtime.AudioQueryCount(),stats.pendingRetirements,(unsigned long long)stats.underruns);summary(frames,"audio_work");summary(control,"control");summary(dsp,"DSP");std::puts("");Check(stats.voices==size_t(count),"requested modest workload voices created");runtime.Destroy();for(int n=0;n<20&&audio.Diagnostics().pendingRetirements;++n){audio.Update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }
 // Real ordinary M59 registration/adoption/removal, root cursor persists.
 Project range;Check(range.Load("projects/streamed_range/streamed_range.judasproj",error),"current streamed game project");resources.Shutdown();db.Scan(range.RootDir(),range.AssetsDir());ResourceManager regionResources(nullptr,&db,&jobs,&audio);regionResources.SetHeadlessResidency(true);Scene root;Check(LoadSceneFromFile(range.StartupScenePath(),root,error),"registered streamed root scene");for(auto& o:root.Objects()){o.scripts.clear();o.ui.reset();}RuntimeWorld streamed;streamed.audioGroups=range.Settings().audio;Check(streamed.Build(root,&regionResources,error,&range.Settings().classification,&range.Settings().navigation),"audio root through ordinary world path");auto session=std::make_shared<SceneSession>(range,range.StartupScenePath());streamed.SetSceneControl(session);auto* stream=session->Streaming(streamed,error);Check(stream!=nullptr,"normal world manifest coordinator");if(!stream){std::puts(error.c_str());return 1;}streamed.BeginAudio();
 // This isolated audio clock still initializes region script metadata through
 // normal lifecycle. Otherwise conservative unpublished-schema pins cannot retire.
 auto frame=[&]{regionResources.Pump();streamed.UpdateScripts(nullptr,0);stream->Advance(false);streamed.UpdateAudio(glm::mat4(1),1,.016);audio.AdvanceWithoutDevice(512);std::this_thread::sleep_for(std::chrono::milliseconds(1));};
 auto state=[&](const char* id){for(auto& r:stream->Regions())if(r.id==id)return r.state;return std::string();};
 auto wait=[&](const char* id,const char* expected){for(int i=0;i<3000;++i){frame();if(state(id)==expected)return true;}return false;};
 for(int i=0;i<30;++i)frame();
 AudioVoiceHandle music;for(auto& e:streamed.AudioEmitters())if(e.id==610)music=e.voice;audio.Snapshot(music,snap);double cursor=snap.positionSeconds;Check(music.IsValid()&&cursor>0,"root music streaming cursor advances");
 AudioVoiceHandle adopted;EntityId prop=0;
 for(int trip=0;trip<3;++trip){auto token=stream->Request("gallery-0",false,error);auto duplicate=stream->Request("gallery-0",false,error);Check(token&&duplicate&&wait("gallery-0","active"),"independent demands activate one gallery");
  if(!prop){prop=stream->Resolve("gallery-0",30);Check(prop&&stream->Adopt(prop,"root",error),"ordinary carry ownership adoption");for(int n=0;n<200&&!adopted.IsValid();++n){frame();for(auto& e:streamed.AudioEmitters())if(e.id==prop)adopted=e.voice;}Check(adopted.IsValid(),"adopted buffered source finishes independent readiness");}
  AudioVoiceSnapshot during;audio.Snapshot(music,during);Check(std::any_of(streamed.AudioEmitters().begin(),streamed.AudioEmitters().end(),[&](const auto& e){return e.id==610&&e.voice.id==music.id;}),"root voice identity remains unchanged");
  stream->Release(token);frame();Check(state("gallery-0")=="active","second demand keeps gallery active");stream->Release(duplicate);Check(wait("gallery-0","unloaded"),"gallery unload reaches unloaded without decoder wait");for(int i=0;i<15;++i)frame();
  audio.Snapshot(music,during);Check(during.positionSeconds>cursor&&during.state==AudioPlaybackState::Playing,"root music continues without restart/duplication");cursor=during.positionSeconds;Check(audio.Snapshot(adopted,during)&&streamed.RuntimeDefinition(prop),"adopted emitter keeps original voice after birthplace removal");Check(audio.Diagnostics().streams==1&&audio.Diagnostics().pendingRetirements==0,"local stream resources retire after each trip");
 }
 auto cancel=stream->Request("gallery-1",false,error);stream->Release(cancel);for(int i=0;i<50;++i)frame();Check(state("gallery-1")=="unloaded"&&audio.Diagnostics().streams==1,"cancelled preparation cannot resurrect stream");streamed.Destroy();session.reset();for(int i=0;i<30;++i){audio.Update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}Check(audio.VoiceCount()==0&&audio.Diagnostics().pendingRetirements==0&&audio.Diagnostics().reverbProcessors==0,"whole-world teardown clears voices/retirements/zones");regionResources.Shutdown();audio.Shutdown();audio.SetJobSystem(nullptr);jobs.Shutdown();std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
