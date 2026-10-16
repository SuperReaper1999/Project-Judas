#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Project.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

namespace {unsigned checks=0,failures=0;void Check(bool b,const char* name){++checks;failures+=!b;std::printf("%s %s\n",b?"PASS":"FAIL",name);}}
int main(){
 std::string error;Project project;
 Check(project.Load("projects/audio_lab/audio_lab.judasproj",error),"ordinary source audio project loads");
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());JobSystem jobs(1);AudioSystem audio;audio.SetJobSystem(&jobs);
 Check(audio.Init(error,true),"production audio backend initializes device-free");ResourceManager resources(nullptr,&assets,&jobs,&audio);
 Scene scene;auto& listener=scene.CreateObject("listener");listener.audioListener=SceneAudioListenerComponent{true,true};
 auto& emitter=scene.CreateObject("emitter");emitter.transform.position={2,1,-5};emitter.audioEmitter=SceneAudioEmitterComponent{};
 emitter.audioEmitter->asset="60606060606060606060606060606002";emitter.audioEmitter->loop=true;emitter.audioEmitter->playOnStart=true;emitter.audioEmitter->doppler=1;
 RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&resources,error),"ordinary emitter/listener world builds");if(failures)return 1;
 resources.WaitForAll();world.BeginAudio();world.UpdateAudio(glm::mat4(1),1,.02f);audio.AdvanceWithoutDevice(480);
 world.UpdateAudio(glm::translate(glm::mat4(1),glm::vec3(-.1f,0,0)),1,.02f);
 const auto voice=world.AudioEmitters()[0].voice;AudioVoiceSnapshot before,after;Check(audio.Snapshot(voice,before)&&before.positionSeconds>0,"actual persistent voice advances before translation");
 Check(world.PlayAudioOneShot(2,error)&&audio.VoiceCount()==2,"independent ordinary one-shot starts");
 const glm::dvec3 delta(512,-512,512);Check(world.RequestOrigin(delta,error)&&world.CommitOrigin(),"ordinary audio no longer blocks native chart publication");
 Check(audio.Snapshot(voice,after)&&after.positionSeconds==before.positionSeconds&&after.state==before.state&&glm::length(glm::dvec3(after.position)+delta-glm::dvec3(before.position))<.0001,"voice identity/cursor/state survives with translated backend position");
 const glm::vec3 eye=glm::vec3(.2f,0,0)-glm::vec3(delta);world.UpdateAudio(glm::translate(glm::mat4(1),-eye),1,.02f);
 const auto observation=audio.Listener();audio.Snapshot(voice,after);const auto expected=AudioSystem::DopplerRatio(after.position,eye,{0,0,0},{5,0,0},1);Check(glm::length(observation.position-eye)<.0001f&&std::abs(after.dopplerRatio-expected)<.00001f&&std::abs(expected-1)>.001f,"listener keeps actual camera motion without rebase-induced Doppler velocity");
 Check(audio.VoiceCount()==2,"origin change does not retire or duplicate the independent shot");
 world.Destroy();Check(audio.VoiceCount()==0,"normal teardown releases all translated audio ownership");resources.Shutdown();
 std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
