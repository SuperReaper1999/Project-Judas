#include "AudioSystem.h"
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "InteractivePlay.h"
#include "editor/EditorDocument.h"
#include "EngineHost.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
namespace {
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
bool Near(glm::vec3 a,glm::vec3 b){return glm::length(a-b)<2e-5f;}
const char* loopId="34000000000000000000000000000001";
const char* shotId="34000000000000000000000000000002";
const char* musicId="34000000000000000000000000000003";
Scene Fixture(){
    Scene s;s.Settings().name="Audio fixture";
    auto& listener=s.CreateObject("Listener");listener.audioListener=SceneAudioListenerComponent{};
    listener.transform.position={2,3,4};listener.transform.rotation=glm::angleAxis(.6f,glm::normalize(glm::vec3(1,2,3)));
    auto& emitter=s.CreateObject("Source");emitter.audioEmitter=SceneAudioEmitterComponent{};
    emitter.audioEmitter->asset=loopId;emitter.audioEmitter->loop=true;emitter.audioEmitter->volume=.2f;
    emitter.body=SceneBodyComponent{};emitter.body->motion=SceneBodyMotion::Dynamic;
    emitter.transform.position={-2,1,-3};
    auto& ui=s.CreateObject("Non-spatial");ui.audioEmitter=SceneAudioEmitterComponent{};
    ui.audioEmitter->asset=musicId;ui.audioEmitter->spatial=false;ui.transform.position={10000,-20,55};
    auto& player=s.CreateObject("Start");
    return s;
}
}
int main(){
    std::string error;AudioSystem audio;
    Check(!audio.IsInitialized()&&audio.VoiceCount()==0,"unused service opens no device and owns no voices");
    Check(audio.Init(error,true),"actual miniaudio engine starts in explicit device-free test mode");
    AssetDatabase db;db.Scan(std::filesystem::current_path().string(),"tests/fixtures/engine-assets");
    const auto* record=db.Find(loopId);Check(record&&record->type==AssetType::Audio,"ordinary metadata discovers stable audio asset");
    AssetId id;AssetType type;std::string source;
    Check(AssetDatabase::ReadMeta("tests/fixtures/engine-assets/audio/loop.wav.judasmeta",id,type,source,error)&&id==loopId&&type==AssetType::Audio,"audio metadata round-trips normal type/id");
    AudioData pcm;Check(LoadAudioFromFile("tests/fixtures/engine-assets/audio/loop.wav",pcm,error)&&pcm.channels==1&&pcm.Frames()==96000,"real WAV decoder produces whole clip PCM");
    AudioData mp3;Check(LoadAudioFromFile("tests/fixtures/engine-assets/audio/centered.mp3",mp3,error)&&mp3.channels==1&&mp3.Frames()>30000,"compressed MP3 uses same backend decoder");
    AudioData unchanged=pcm;const unsigned char corrupt[8]={1,2,3,4,5,6,7,8};
    Check(!DecodeAudioFromMemory(corrupt,sizeof(corrupt),unchanged,error)&&unchanged.samples==pcm.samples,"failed decode leaves existing data intact");
    const auto project=std::filesystem::path(".cache/m34-asset-tests");std::filesystem::remove_all(project);std::filesystem::create_directories(project/"Assets");
    AssetDatabase imported;imported.Scan(project.string(),(project/"Assets").string());AssetRecord importedClip;
    Check(imported.Import("tests/fixtures/engine-assets/audio/one_shot.wav","cue.wav",importedClip,error),"ordinary import validates decoder and creates stable audio metadata");
    const auto importedId=importedClip.id;
    Check(imported.Move(importedId,"renamed/cue.wav",error)&&imported.Find(importedId)&&imported.Find(importedId)->type==AssetType::Audio,"audio rename/move preserves stable identity");
    const auto importedPath=imported.Find(importedId)->path;
    {std::ofstream bad(importedPath,std::ios::binary|std::ios::trunc);bad.write(reinterpret_cast<const char*>(corrupt),sizeof(corrupt));}
    ResourceManager broken(nullptr,&imported,nullptr,&audio);
    Check(!broken.GetAudio(importedId,error).IsValid()&&broken.StateOf(importedId)==ResourceState::Failed,"corrupt file fails through actual resource IO/decoder path");
    std::filesystem::copy_file("tests/fixtures/engine-assets/audio/one_shot.wav",importedPath,std::filesystem::copy_options::overwrite_existing);broken.Invalidate(importedId);
    Check(broken.GetAudio(importedId,error).IsValid(),"corrected input recovers through supported invalidation path");broken.Shutdown();
    JobSystem jobs(2);ResourceManager resources(nullptr,&db,&jobs,&audio);
    resources.AddRef(loopId);resources.RequestAudio(loopId);resources.RequestAudio(loopId);resources.WaitForAll();
    auto clip=resources.GetAudio(loopId,error);
    Check(clip.IsValid()&&resources.Stats().misses==1&&audio.ClipCount()==1,"shared requests install one clip through real resource pipeline");
    Check(resources.DecodeThreadOf(loopId)!=std::this_thread::get_id(),"actual IO/decode runs on existing worker pool");
    AudioSettings settings;settings.loop=true;settings.volume=.2f;
    auto voice=audio.CreateVoice(clip,settings,error);AudioVoiceSnapshot state;
    Check(voice.IsValid()&&audio.Play(voice)&&audio.Snapshot(voice,state)&&state.state==AudioPlaybackState::Playing,"voice starts through engine API");
    const auto rotation=glm::angleAxis(.6f,glm::normalize(glm::vec3(1,2,3)));
    audio.SetListener({2,3,4},rotation);audio.SetPosition(voice,{-2,1,-3});audio.Snapshot(voice,state);
    const auto listener=audio.Listener();
    Check(Near(state.position,{-2,1,-3})&&Near(listener.position,{2,3,4})&&Near(listener.forward,rotation*glm::vec3(0,0,-1))&&Near(listener.up,rotation*glm::vec3(0,1,0)),"full emitter/listener pose reaches real backend without world-up substitution");
    Check(audio.AdvanceWithoutDevice(1000)&&audio.Pause(voice)&&audio.Snapshot(voice,state)&&state.state==AudioPlaybackState::Paused,"pause preserves active voice");
    auto cursor=state.cursorFrames;audio.AdvanceWithoutDevice(1000);audio.Snapshot(voice,state);
    Check(state.cursorFrames==cursor&&audio.Resume(voice),"paused cursor stays fixed and resume retains it");
    audio.AdvanceWithoutDevice(100000);audio.Snapshot(voice,state);
    Check(state.loop&&state.state==AudioPlaybackState::Playing,"loop stays active after clip duration");
    Check(audio.Stop(voice)&&audio.Snapshot(voice,state)&&state.state==AudioPlaybackState::Stopped&&state.cursorFrames==0,"stop rewinds without orphan voice");
    audio.SetListener({0,0,0},glm::angleAxis(glm::half_pi<float>(),glm::vec3(1,0,0)));
    Check(Near(audio.Listener().forward,{0,1,0})&&Near(audio.Listener().up,{0,0,1}),"quarter-turn listener rotation has independently known forward/up axes");
    settings.spatial=false;Check(audio.SetSettings(voice,settings),"2D mode routes to backend non-spatial path");
    audio.Snapshot(voice,state);const auto oldPosition=state.position;audio.SetPosition(voice,{999,888,777});audio.Snapshot(voice,state);
    Check(!state.spatial&&state.position==oldPosition,"non-spatial position updates have no effect");
    settings.spatial=true;settings.referenceDistance=2;settings.maximumDistance=10;settings.rolloff=1;
    Check(std::abs(AudioSystem::DistanceGainForDiagnostics(settings,2)-1)<1e-6&&std::abs(AudioSystem::DistanceGainForDiagnostics(settings,4)-.5f)<1e-6,"actual inverse-distance backend gain matches independent reference values");
    settings.attenuation=AudioAttenuation::Linear;
    Check(std::abs(AudioSystem::DistanceGainForDiagnostics(settings,6)-.5f)<1e-6&&std::abs(AudioSystem::DistanceGainForDiagnostics(settings,20))<1e-6,"linear attenuation is finite and clamps beyond maximum distance");
    settings.attenuation=AudioAttenuation::None;audio.SetSettings(voice,settings);audio.Snapshot(voice,state);
    Check(state.spatialPanningEnabled&&state.attenuation==AudioAttenuation::None&&state.rolloff==0&&AudioSystem::DistanceGainForDiagnostics(settings,100)==1,"no distance attenuation retains actual backend spatial panning configuration");
    auto invalid=settings;invalid.pitch=0;Check(!audio.SetSettings(voice,invalid),"invalid audio settings fail without silent clamping");
    audio.DestroyVoice(voice);
    settings.loop=false;settings.spatial=false;
    auto once=audio.PlayOneShot(clip,settings,{0,0,0},error);Check(once.IsValid(),"generic one-shot starts");
    audio.AdvanceWithoutDevice(100000);audio.Update();Check(!audio.Snapshot(once,state)&&audio.VoiceCount()==0,"completed standalone one-shot auto-releases");
    auto completed=audio.CreateVoice(clip,settings,error);audio.Play(completed);audio.AdvanceWithoutDevice(100000);audio.Snapshot(completed,state);
    Check(state.state==AudioPlaybackState::Finished,"authored one-shot retains finished state without auto-retirement");
    Check(audio.Stop(completed)&&audio.Snapshot(completed,state)&&state.state==AudioPlaybackState::Stopped&&state.cursorFrames==0,"explicit Stop resets a completed one-shot state");audio.DestroyVoice(completed);
    Scene authored=Fixture(),round;std::string text;
    SaveSceneToString(authored,text);Check(LoadSceneFromString(text,round,error)&&ScenesEqual(authored,round),"emitter/listener authored values round-trip");
    std::string hash1,hash2;ComputeSceneFingerprint(authored,hash1,error);round.Objects()[1].audioEmitter->pitch=1.5f;ComputeSceneFingerprint(round,hash2,error);
    Check(hash1!=hash2,"authored audio changes strict baseline fingerprint");
    Scene duplicate=authored;duplicate.Objects()[2].audioListener=SceneAudioListenerComponent{};SaveSceneToString(duplicate,text);
    Check(!LoadSceneFromString(text,round,error),"multiple enabled authored listeners reject deterministically");
    RuntimeWorld world;Check(world.Build(authored,&resources,error),"ordinary runtime builds authored audio bindings");
    resources.WaitForAll();InteractivePlay play;Check(play.Begin(world,WorldCoordinates{},error),"real Play entry starts world audio lifecycle");
    world.UpdateAudio(glm::mat4(1),1);
    Check(audio.VoiceCount()==2,"Play creates two voices sharing decoded assets");
    auto body=world.AudioEmitters()[0].voice;
    EntityPhysicalState moved;world.GetEntityState(2,moved);moved.position={7,8,9};world.SetEntityState(2,moved);world.UpdateAudio(glm::mat4(1),1);audio.Snapshot(body,state);
    Check(Near(state.position,moved.position),"moving entity presentation updates its emitter position");
    Check(world.PauseAudio(2)&&world.ResumeAudio(2)&&world.StopAudio(2)&&world.PlayAudio(2),"world play/stop/pause/resume controls use same voices");
    Check(world.SetAudioEnabled(2,false)&&!audio.Snapshot(body,state),"disabling emitter releases its voice immediately");
    world.SetAudioEnabled(2,true);world.UpdateAudio(glm::mat4(1),1);body=world.AudioEmitters()[0].voice;
    Check(world.DestroyEntity(2,&error)&&!audio.Snapshot(body,state),"destroying dynamic emitter releases its voice immediately");
    play.End();Check(audio.VoiceCount()==0&&ScenesEqual(authored,Fixture()),"Stop leaves no voices and never changes authored scene");
    world.Destroy();
    EditorDocument doc;doc.GetScene()=authored;doc.BeginEdit();doc.GetScene().Objects()[1].audioEmitter->volume=.7f;doc.CommitEdit();doc.Undo();
    Check(ScenesEqual(doc.GetScene(),authored),"audio authoring undo restores original state");
    auto missing=resources.GetAudio("34000000000000000000000000000099",error);
    Check(!missing.IsValid()&&resources.StateOf("34000000000000000000000000000099")==ResourceState::Failed&&!error.empty(),"missing audio fails visibly without affecting valid clip");
    Check(resources.GetAudio(loopId,error).IsValid(),"valid audio remains usable after unrelated load failure");
    {
        EngineHost host;Check(host.Init("M34 integration",320,240,false,error),"normal EngineHost starts audio service independently of Renderer");
        host.GetWindow().SetTestInputMode(true);Check(host.Audio().Init(error,true),"explicit device-free mode uses host-owned actual backend");
        host.OpenProjectAssets(std::filesystem::current_path().string(),"tests/fixtures/engine-assets");
        Scene scene=authored;scene.Objects()[0].audioListener->followActiveView=true;
        const Scene baseline=scene;
        RuntimeWorld runtime;Check(runtime.Build(scene,&host.Resources(),error),"same authored bindings build through process services");
        host.Resources().WaitForAll();InteractivePlay game;Check(game.Begin(runtime,WorldCoordinates{},error),"interactive runtime entry uses shared audio lifecycle");
        game.Frame(host.GetWindow(),host.GetRenderer(),1.f/60,false);
        Check(host.Audio().VoiceCount()==2&&host.Audio().Listener().forward==glm::vec3(0,0,-1),"ordinary interactive frame creates voices and updates active-camera listener");
        game.End();Check(host.Audio().VoiceCount()==0&&ScenesEqual(scene,baseline)&&ScenesEqual(authored,Fixture()),"ordinary Stop releases host voices without authored mutations");
        runtime.Destroy();host.OpenProjectAssets(std::filesystem::current_path().string(),"tests/fixtures/engine-assets");
        Check(host.Audio().VoiceCount()==0&&host.Audio().ClipCount()==0,"project closure clears runtime clips and voices");
        host.Shutdown();Check(!host.Audio().IsInitialized(),"process shutdown closes audio backend");
    }
    for(int n:{0,4,32}){
        Scene stress;auto& l=stress.CreateObject("Listener");l.audioListener=SceneAudioListenerComponent{};
        for(int i=0;i<n;++i){auto& e=stress.CreateObject("Emitter");e.audioEmitter=SceneAudioEmitterComponent{};e.audioEmitter->asset=loopId;e.audioEmitter->loop=true;e.audioEmitter->volume=.02f;e.transform.position={float(i),0,-5};}
        RuntimeWorld runtime;Check(runtime.Build(stress,&resources,error),"performance fixture uses ordinary world bindings");runtime.BeginAudio();runtime.UpdateAudio(glm::mat4(1),1);
        Check(audio.VoiceCount()==static_cast<std::size_t>(n),"performance fixture creates requested actual backend voices");
        const auto begin=std::chrono::steady_clock::now();
        for(int frame=0;frame<1000;++frame)runtime.UpdateAudio(glm::mat4(1),1);
        const double update=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/1000;
        const auto mixBegin=std::chrono::steady_clock::now();audio.AdvanceWithoutDevice(48000);
        const double mix=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-mixBegin).count();
        std::printf("PERFORMANCE voices=%d world_audio_update_mean_us=%.6f mix_one_second_cpu_ms=%.6f (device-free backend DSP; no perceptual claim)\n",n,update,mix);
        runtime.Destroy();
    }
    resources.ReleaseRef(loopId);resources.Shutdown();jobs.Shutdown();audio.Shutdown();
    Check(!audio.IsInitialized()&&audio.VoiceCount()==0&&audio.ClipCount()==0,"shutdown releases every registered voice/clip and backend engine");
    std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
