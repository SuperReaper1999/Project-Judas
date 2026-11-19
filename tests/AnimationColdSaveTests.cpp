#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "Simulation.h"
#include "SaveStorage.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "ToolProcess.h"
#include "EnginePaths.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <filesystem>
#include <cmath>
using Json=nlohmann::json;
int main(int argc,char** argv){
 if(argc==2){
  const auto executable=std::filesystem::absolute(argv[0]);
  const auto output=std::filesystem::absolute(argv[1]).string();
  for(const auto* mode:{"write","read"}){
   std::string log,error;
   const bool ok=CaptureToolOutput(executable,{mode,output},log,error,std::chrono::seconds(60),65536);
   std::printf("%s",log.c_str());
   if(!ok){std::fprintf(stderr,"Cold animation/IK %s failed: %s\n",mode,error.c_str());return 1;}
  }
  return 0;
 }
 if(argc!=3)return 64;
 std::string error;int checks=0,failures=0;auto check=[&](bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);if(!ok)std::puts(error.c_str());};
 EngineHost host;const bool initialized=host.Init("M95 cold animation and IK save",320,240,false,error);check(initialized,"native resource host");if(!initialized)return 1;
 Project project;const bool loaded=project.Load(ResolveEngineDataPath("projects/animation_studio/animation_studio.judasproj"),error);check(loaded,"ordinary project");if(!loaded){host.Shutdown();return 1;}host.OpenProjectAssets(project.RootDir(),project.AssetsDir());auto& resources=host.Resources();RuntimeWorld world;GameSnapshot snapshot;SaveStorage storage(std::filesystem::absolute(argv[2]).string());const bool writing=std::string(argv[1])=="write";
 Scene scene;if(writing){check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"authored studio");for(auto& o:scene.Objects()){o.scripts.clear();o.ui.reset();}}
 else{bool recovered=false;check(storage.Read("animation",snapshot,recovered,error)&&!recovered,"fresh process reads checksum-protected save");check(WorldPersistence::SceneFrom(snapshot.participants,scene,error),"saved scene reconstruction");}
 check(world.Build(scene,&resources,error,nullptr,nullptr,!writing),"world construction");world.UpdateAnimations(0);resources.WaitForAll();world.UpdateAnimations(0);
 if(writing){
  Scene fixture;check(LoadSceneFromFile(ResolveEngineDataPath("tests/fixtures/ik/configuration.judas"),fixture,error),"authored IK configuration");
  if(!fixture.Find(10)||!fixture.Find(10)->animation||!fixture.Find(10)->animation->fullBodyIK)return 1;
  auto settings=*fixture.Find(10)->animation->fullBodyIK;settings.targets.clear();
  const char* chains[]={"palmA","palmB","soleA","soleB"};
  const char* joints[]={"PalmA","PalmB","SoleA","SoleB"};
  for(unsigned i=0;i<4;++i){
   SceneTransform joint;if(!world.JointPose(10,joints[i],"model",1,joint))return 1;
   FullBodyIKTarget target;target.id=chains[i];target.chain=chains[i];target.space=IKTargetSpace::Model;
   target.position=glm::dvec3(joint.position)+glm::dvec3(i<2?(i==0?.06:-.06):0,i<2?.04:0,0);
   target.positionWeight=1;target.orientationWeight=0;settings.targets.push_back(target);
  }
  check(world.ConfigureFullBodyIK(10,settings,error),"live four-target IK config through runtime owner");
  auto* animation=world.RuntimeAnimation(10);
  AnimationLayerSettings layer;
  layer.id="mask-update";
  layer.clip="Walk";
  layer.mask={"PalmA"};
  layer.time=.125f;
  check(world.SetAnimationLayer(10,layer,false,error),"ordinary runtime layer resolves its first joint mask");
  layer.weight=.7f;
  const auto first=FindSkeletonJoint(animation->asset->skeleton,"PalmA");
  const auto second=FindSkeletonJoint(animation->asset->skeleton,"PalmB");
  check(world.SetAnimationLayer(10,layer,false,error)&&animation->layers.back().mask==std::vector<int>{first}&&
        animation->layers.back().playback.time==.125f,"weight update retains resolved mask and playback time");
  layer.mask={"PalmB"};
  check(world.SetAnimationLayer(10,layer,false,error)&&animation->layers.back().mask==std::vector<int>{second},
        "edited joint keys resolve a replacement mask");
  layer.mask.push_back("PalmB");
  check(!world.SetAnimationLayer(10,layer,false,error)&&animation->layers.back().mask==std::vector<int>{second},
        "duplicate edited keys fail without changing the live mask");
  check(world.SetAnimationLayer(10,layer,true,error),"remove fixture layer before continuation");
 }

 // Manual clip advancement is gameplay intent; production fixed stepping owns IK and physics.
 auto tick=[&]{auto* a=world.RuntimeAnimation(10);auto* m=world.RuntimeCharacter(10);if(!a||!m||!world.AdvanceAnimation(10,1.f/60,error))return false;m->displacementIntent=a->step.motion.translation;StepAuthoritativeWorld(world,host.GetWindow().Input(),1.f/60);return true;};
 auto endpoint=[&]{auto* a=world.RuntimeAnimation(10);auto* m=world.RuntimeCharacter(10);SceneTransform hand;world.JointPose(10,"PalmA","model",1,hand);return Json{{"hand",{hand.position.x,hand.position.y,hand.position.z}},{"ikTargets",a->fullBodyIK?a->fullBodyIK->settings.targets.size():0},{"time",a->playback.time},{"position",{m->position.x,m->position.y,m->position.z}},{"velocity",{m->velocity.x,m->velocity.y,m->velocity.z}},{"events",a->step.events.size()},{"paused",a->mixer.paused}};};
 auto same=[&](const Json& expected){auto actual=endpoint();bool ok=std::abs(actual["time"].get<float>()-expected["time"].get<float>())<1e-6&&actual["events"]==expected["events"]&&actual["paused"]==expected["paused"];ok&=actual["ikTargets"]==expected["ikTargets"];for(auto key:{"position","velocity","hand"})for(int i=0;i<3;++i)ok&=std::abs(actual[key][i].get<float>()-expected[key][i].get<float>())<1e-5;return ok;};
 if(writing){auto* a=world.RuntimeAnimation(10);a->playback.clip="Walk";a->playback.loop=true;world.RuntimeCharacter(10)->velocity={1,0,0};bool ok=true;for(int i=0;i<18;++i)ok&=tick();check(ok,"uninterrupted authoritative animation/motor steps");auto before=endpoint();check(WorldPersistence::Capture(world,snapshot.participants,error),"capture phase, markers, motor and physical state");check(tick(),"uninterrupted next boundary");check(world.RuntimeAnimation(10)->fullBodyIK&&world.RuntimeAnimation(10)->fullBodyIK->result.targets.size()==4,"actual uninterrupted IK solve publishes four residuals");snapshot.metadata=Json{{"before",before},{"next",endpoint()}}.dump();snapshot.project="M95 animation studio";snapshot.scene="Scenes/studio.judas";snapshot.content=SceneFingerprintSha256("focused native continuation fixture");check(storage.Write("animation",snapshot,error),"ordinary atomic save storage");}
 else{check(WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"restore participants without a settling step");WorldPersistence::Published(world);auto metadata=Json::parse(snapshot.metadata);check(same(metadata["before"]),"fresh physical and animation endpoint equals saved endpoint");check(tick()&&same(metadata["next"]),"first resumed boundary matches uninterrupted motion, hand pose, phase and event crossing");check(world.RuntimeAnimation(10)->fullBodyIK&&world.RuntimeAnimation(10)->fullBodyIK->result.targets.size()==4,"actual resumed IK boundary publishes four residuals");}
 world.Destroy();host.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
