#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDependencies.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "SaveStorage.h"
#include "ScriptSystem.h"
#include "Simulation.h"
#include "GameSession.h"
#include "Prefab.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool ok,const std::string& label){
    ++checks;
    if(!ok)throw std::runtime_error(label);
    std::cout<<"PASS "<<label<<'\n';
}
void Tick(RuntimeWorld& w,InputSystem& input,int n){
    GameSession session;
    std::string error;
    if(!session.Begin(w,error))throw std::runtime_error(error);
    for(int i=0;i<n;++i){
        input.BeginFrame();
        input.BeginFixedStep();
        StepAuthoritativeWorld(session,input,1.f/60);
        w.UpdateScripts(&input,1.f/60);
    }
    if(w.Scripts())for(const auto& fault:w.Scripts()->Diagnostics())throw std::runtime_error(fault.message);
}
nlohmann::json State(RuntimeWorld& w){
    for(const auto& r:w.Scripts()->Capture())if(r.entity==1)return nlohmann::json::parse(r.json);
    throw std::runtime_error("player script state unavailable");
}
int main(int argc,char** argv){
    try{
        if(argc!=3&&argc!=4)return 2;
        const auto root=fs::absolute(argv[2]);
        std::string error;
        const bool cold=argc==4;
        if(!cold){
            Check(!fs::exists(root),"integration output is isolated/fresh");
            fs::copy(argv[1],root,fs::copy_options::recursive);
        }
        Project project;
        Check(project.Load((root/"Sprite_Animation_Workshop.judasproj").string(),error),"open real animation workshop: "+error);
        AssetDatabase assets;
        assets.Scan(project.RootDir(),project.AssetsDir());
        JobSystem jobs(2);
        ResourceManager resources(nullptr,&assets,&jobs);
        resources.SetBlockingMode(true);
        resources.SetHeadlessResidency(true);
        InputSystem input;
        Check(input.SetMap(project.Settings().input,error),"project input map includes interaction");
        if(cold){
            SaveStorage storage((root/"ColdSaves").string());
            GameSnapshot snapshot;
            bool recovered=false;
            Check(storage.Read("animation",snapshot,recovered,error),"fresh process reads real checksum-protected slot: "+error);
            Scene scene;
            Check(WorldPersistence::SceneFrom(snapshot.participants,scene,error),"fresh process reconstructs authored snapshot");
            RuntimeWorld restored;
            restored.legacyGameplay=false;
            Check(restored.Build(scene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,snapshot.participants,error),"fresh process restores physical world and sprite phase: "+error);
            nlohmann::json expected;
            {
                std::ifstream f(root/"expected.json");
                f>>expected;
            }
            auto before=*restored.RuntimeDefinition(1)->render->spritePlayback;
            Check(before.time==expected["time"]&&before.clip==expected["clip"]&&before.playing==expected["playing"],"restored named clip/double phase/pause match exact saved endpoint");
            Check(!restored.SpriteAnimationObservation(1),"restoration does not invent or replay a marker");
            Tick(restored,input,15);
            const auto after=*restored.RuntimeDefinition(1)->render->spritePlayback;
            Check(after.time==expected["afterTime"]&&after.clip==expected["afterClip"],"cold continuation equals uninterrupted actual playback endpoint");
            Check(State(restored).at("strikes")==expected["afterStrikes"],"cold continuation does not duplicate attack marker");
            std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";
            return 0;
        }
        Scene scene;
        Check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"load editable animation/physics scene");
        // Make the real strike target nearby; ordinary native body, unchanged controller.
        scene.Find(197)->transform.position={
            1.1f,-.2f,0
        };
        RuntimeWorld world;
        world.legacyGameplay=false;
        Check(world.Build(scene,&resources,error),"build real gameplay and sheet dependency");
        Tick(world,input,100);
        Check(State(world).value("grounded",false),"real controller is grounded before interaction");
        const auto target=world.RuntimeBody(197);
        const auto start=world.Physics().GetTransform(target).position;
        const auto stepsBefore=State(world).at("strikes").get<int>();
        input.SetPhysical("key:E",1);
        Tick(world,input,1);
        input.SetPhysical("key:E",0);
        Check(world.RuntimeDefinition(1)->render->spritePlayback->clip=="interact","ordinary JS starts nonloop interaction clip");
        Tick(world,input,8);
        Check(State(world).at("strikes")==stepsBefore,"wind-up does not apply premature strike");
        Tick(world,input,10);
        Check(State(world).at("strikes")==stepsBefore+1&&State(world).at("impacts").get<int>()>0,"timed animation event applies actual native-body impulse");
        Check(world.Physics().GetTransform(target).position.x>start.x+.05f,"event-driven physical action moves the target");
        Tick(world,input,30);
        Check(State(world).at("strikes")==stepsBefore+1&&world.RuntimeDefinition(1)->render->spritePlayback->clip=="idle","one strike, then ordinary JS transition back to idle");
        input.SetPhysical("key:D",1);
        Tick(world,input,20);
        input.SetPhysical("key:D",0);
        Check(State(world).at("footsteps").get<int>()>0,"real run playback emits script-observable footstep markers");
        input.SetPhysical("key:Space",1);
        Tick(world,input,1);
        input.SetPhysical("key:Space",0);
        Tick(world,input,3);
        Check(world.RuntimeDefinition(1)->render->spritePlayback->clip=="jump","native physical jump selects jump sprite clip");
        Tick(world,input,100);
        auto p=*world.RuntimeDefinition(1)->render->spritePlayback;
        p.clip="interact";
        p.time=.16;
        p.playing=true;
        p.loop=false;
        Check(world.SetSpritePlayback(1,p,error),"explicit phase seek emits no gameplay event");
        Check(!world.SpriteAnimationObservation(1),"seek clears preceding observations rather than dispatching markers");
        GameSnapshot snapshot;
        snapshot.project="stage3";
        snapshot.scene="main";
        snapshot.content=std::string(64,'c');
        Check(WorldPersistence::Capture(world,snapshot.participants,error),"capture real sprite and physical continuation: "+error);
        SaveStorage storage((root/"ColdSaves").string());
        Check(storage.Write("animation",snapshot,error),"write checksum-protected cold fixture");
        nlohmann::json expected={
            {
                "time",p.time
            },{
                "clip",p.clip
            },{
                "playing",p.playing
            }
        };
        Tick(world,input,15);
        const auto after=*world.RuntimeDefinition(1)->render->spritePlayback;
        expected["afterTime"]=after.time;
        expected["afterClip"]=after.clip;
        expected["afterStrikes"]=State(world).at("strikes");
        std::ofstream(root/"expected.json")<<expected;
        Scene saved;
        Check(WorldPersistence::SceneFrom(snapshot.participants,saved,error),"snapshot scene includes real playback configuration");
        RuntimeWorld restored;
        restored.legacyGameplay=false;
        Check(restored.Build(saved,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,snapshot.participants,error),"restore into independent world before next fixed step: "+error);
        Tick(restored,input,15);
        Check(EncodeSpritePlayback(*restored.RuntimeDefinition(1)->render->spritePlayback)==EncodeSpritePlayback(after)&&State(restored).at("strikes")==expected["afterStrikes"],"independent continuation matches uninterrupted phase and events");
        // Content changes cannot reinterpret a previous save phase silently.
        auto* record=assets.Find(p.sheet);
        SpriteSheet sheet;
        Check(record&&LoadSpriteSheet(record->path,sheet,error),"inspect real versioned sheet");
        const auto original=sheet;
        sheet.clips[0].frames[0].seconds+=.1;
        Check(SaveSpriteSheet(record->path,sheet,error),"author changed clip timing");
        resources.Invalidate(p.sheet);
        resources.GetSpriteSheet(p.sheet,error);
        Check(!WorldPersistence::Restore(restored,snapshot.participants,error)&&error.find("content changed")!=std::string::npos,"changed-content save rejects instead of silently reinterpreting time");
        Check(SaveSpriteSheet(record->path,original,error),"restore exact original fixture content");
        resources.Invalidate(p.sheet);
        resources.GetSpriteSheet(p.sheet,error);
        auto invalid=original;
        invalid.frames.erase(invalid.frames.begin());
        {
            std::ofstream f(record->path);
            f<<EncodeSpriteSheet(invalid);
        }
        resources.Invalidate(p.sheet);
        Check(!resources.GetSpriteSheet(p.sheet,error)&&error.find("missing frame")!=std::string::npos,"removed referenced frame fails resource publication with useful diagnostic");
        Check(SaveSpriteSheet(record->path,original,error),"repair fixture for repeated lifecycle checks");
        resources.Invalidate(p.sheet);
        for(int i=0;i<3;++i){
            RuntimeWorld repeat;
            repeat.legacyGameplay=false;
            Check(repeat.Build(scene,&resources,error),"repeated scene construction");
            Tick(repeat,input,3);
            repeat.Destroy();
            Check(repeat.SpriteAnimationObservation(1)==nullptr,"world clear retires sprite event ownership");
        }
        // Independent prefab instances share content, never playback clocks.
        Scene prefab;
        auto& actor=prefab.CreateObject("Animated prefab");
        actor.render=scene.Find(1)->render;
        const auto prefabPath=root/"Assets/animated.judasprefab";
        Check(SaveSceneToFile(prefab,prefabPath.string(),error),"write ordinary animated prefab");
        AssetRecord prefabAsset;
        Check(assets.Track(prefabPath.string(),prefabAsset,error),"track reusable animated prefab");
        SceneTransform placement;
        placement.position={
            20,5,0
        };
        auto a=world.SpawnPrefab(prefabAsset.id,placement,error);
        placement.position.x=22;
        auto b=world.SpawnPrefab(prefabAsset.id,placement,error);
        Check(a&&b&&a!=b,"two ordinary animated prefab instances instantiate");
        auto state=*world.RuntimeDefinition(a)->render->spritePlayback;
        state.clip="run";
        state.time=.12;
        state.playing=false;
        Check(world.SetSpritePlayback(a,state,error),"pause and seek one prefab instance");
        const auto otherTime=world.RuntimeDefinition(b)->render->spritePlayback->time;
        Tick(world,input,3);
        Check(world.RuntimeDefinition(a)->render->spritePlayback->time==.12&&world.RuntimeDefinition(b)->render->spritePlayback->time>otherTime,"prefab clocks are independent; no shared asset mutation");
        RuntimeWorld excluded;excluded.legacyGameplay=false;
    Check(excluded.Build(prefab,&resources,error)&&WorldPersistence::Prepare(excluded,error),"prepare transient-only animated fixture");
    Check(excluded.SetTransientEntity(1,true,error),"exclude optional animated actor from modern saves");
    std::map<std::string,SaveChunk> excludedChunks;
    Check(WorldPersistence::Capture(excluded,excludedChunks,error)&&!excludedChunks.count("sprite-content"),"excluded sprites do not manufacture required save participants");
    Scene region;
        auto& regional=region.CreateObject("Streamed animation");
        regional.render=actor.render;
        regional.render->spritePlayback->clip="run";
        regional.render->spritePlayback->time=.1;
        Check(SaveSceneToFile(region,(root/"Scenes/animated-region.judas").string(),error),"author genuine streamed animation region");
        RuntimeWorld streamed;
        streamed.legacyGameplay=false;
        Scene empty;
        Check(streamed.Build(empty,&resources,error),"build empty streaming root");
        WorldManifest manifest;
        WorldRegion entry;
        entry.id="animated";
        entry.scene="Scenes/animated-region.judas";
        manifest.regions.emplace(entry.id,entry);
        WorldStreaming stream(streamed,resources,project,manifest);
        const auto pump=[&](const std::function<bool()>& done){
            for(int i=0;i<2000;++i){
                resources.Pump();
                stream.Advance(false);
                if(done())return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
        };
        auto request=stream.Request("animated",false,error);
        Check(request&&pump([&]{
            return stream.Status(request)&&stream.Status(request)->state=="active";
        }),"region coordinator publishes animated sprite");
        auto owner=stream.Resolve("animated",1);
        Tick(streamed,input,13);
        const auto phase=streamed.RuntimeDefinition(owner)->render->spritePlayback->time;
        stream.Release(request);
        Check(pump([&]{
            return stream.Regions()[0].state=="unloaded";
        }),"inactive region really unloads animated owner");
        Tick(streamed,input,20);
        request=stream.Request("animated",false,error);
        Check(pump([&]{
            return stream.Status(request)&&stream.Status(request)->state=="active";
        }),"revisit reconstructs native animation owner");
        owner=stream.Resolve("animated",1);
        Check(streamed.RuntimeDefinition(owner)->render->spritePlayback->time==phase,"streamed phase was suspended, not advanced while absent or reset on return");
        Tick(streamed,input,1);
        Check(streamed.RuntimeDefinition(owner)->render->spritePlayback->time>phase,"restored sprite really resumes at next fixed boundary");
        std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";
        return 0;
    }
    catch(const std::exception& e){
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}
