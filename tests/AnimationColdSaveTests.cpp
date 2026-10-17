#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SaveStorage.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <filesystem>
#include <cmath>
using Json=nlohmann::json;
int main(int argc,char** argv){if(argc!=3)return 64;std::string error;int checks=0,failures=0;auto check=[&](bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);if(!ok)std::puts(error.c_str());};
 EngineHost host;check(host.Init("M95 cold animation save",320,240,false,error),"native resource host");Project project;check(project.Load("projects/animation_studio/animation_studio.judasproj",error),"ordinary project");host.OpenProjectAssets(project.RootDir(),project.AssetsDir());auto& resources=host.Resources();RuntimeWorld world;GameSnapshot snapshot;SaveStorage storage(std::filesystem::absolute(argv[2]).string());const bool writing=std::string(argv[1])=="write";
 Scene scene;if(writing){check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"authored studio");for(auto& o:scene.Objects()){o.scripts.clear();o.ui.reset();}}
 else{bool recovered=false;check(storage.Read("animation",snapshot,recovered,error)&&!recovered,"fresh process reads checksum-protected save");check(WorldPersistence::SceneFrom(snapshot.participants,scene,error),"saved scene reconstruction");}
 check(world.Build(scene,&resources,error,nullptr,nullptr,!writing),"world construction");world.UpdateAnimations(0);resources.WaitForAll();world.UpdateAnimations(0);
 auto tick=[&]{auto* a=world.RuntimeAnimation(10);auto* m=world.RuntimeCharacter(10);if(!a||!m||!world.AdvanceAnimation(10,1.f/60,error))return false;m->displacementIntent=a->step.motion.translation;world.Physics().Step(1.f/60);world.UpdateCharacters(1.f/60);world.UpdateAnimations(1.f/60);return true;};
 auto endpoint=[&]{auto* a=world.RuntimeAnimation(10);auto* m=world.RuntimeCharacter(10);return Json{{"time",a->playback.time},{"position",{m->position.x,m->position.y,m->position.z}},{"velocity",{m->velocity.x,m->velocity.y,m->velocity.z}},{"events",a->step.events.size()},{"paused",a->mixer.paused}};};
 auto same=[&](const Json& expected){auto actual=endpoint();bool ok=std::abs(actual["time"].get<float>()-expected["time"].get<float>())<1e-6&&actual["events"]==expected["events"]&&actual["paused"]==expected["paused"];for(auto key:{"position","velocity"})for(int i=0;i<3;++i)ok&=std::abs(actual[key][i].get<float>()-expected[key][i].get<float>())<1e-5;return ok;};
 if(writing){auto* a=world.RuntimeAnimation(10);a->playback.clip="Walk";a->playback.loop=true;world.RuntimeCharacter(10)->velocity={1,0,0};bool ok=true;for(int i=0;i<18;++i)ok&=tick();check(ok,"uninterrupted authoritative animation/motor steps");auto before=endpoint();check(WorldPersistence::Capture(world,snapshot.participants,error),"capture phase, markers, motor and physical state");check(tick(),"uninterrupted next boundary");snapshot.metadata=Json{{"before",before},{"next",endpoint()}}.dump();snapshot.project="M95 animation studio";snapshot.scene="Scenes/studio.judas";snapshot.content=SceneFingerprintSha256("focused native continuation fixture");check(storage.Write("animation",snapshot,error),"ordinary atomic save storage");}
 else{check(WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"restore participants without a settling step");WorldPersistence::Published(world);auto metadata=Json::parse(snapshot.metadata);check(same(metadata["before"]),"fresh physical and animation endpoint equals saved endpoint");check(tick()&&same(metadata["next"]),"first resumed boundary matches uninterrupted motion, phase and event crossing");}
 world.Destroy();host.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
