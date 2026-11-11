#include "EngineHost.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "JobSystem.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <atomic>
#include <vector>

namespace {
int failures=0,checks=0;
void Check(bool ok,const char* message){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",message);}
bool SamePose(const SkeletalPose& a,const SkeletalPose& b){
 if(a.local.size()!=b.local.size())return false;
 for(size_t i=0;i<a.local.size();++i){const auto& x=a.local[i];const auto& y=b.local[i];
  if(std::memcmp(&x.translation,&y.translation,3*sizeof(float))||
     std::memcmp(&x.rotation,&y.rotation,4*sizeof(float))||
     std::memcmp(&x.scale,&y.scale,3*sizeof(float)))return false;
 }return true;
}
struct Endpoint {SkeletalPose reference,solved,final;FullBodyIKResult ik;float clock;std::string error;};
}
int main(){
 JobSystem jobs(3);
 for(unsigned lanes:{1u,2u,4u}){std::vector<unsigned> seen(10003);
  auto stats=jobs.ParallelFor(seen.size(),7,lanes,[&](size_t i){++seen[i];},"Independent outputs");
  bool once=stats.ownerItems+stats.workerItems==seen.size();for(auto n:seen)once&=n==1;
  Check(once,"one/two/four lanes produce every data output exactly once");
 }
 std::atomic<int> active{0};bool caught=false;
 try{jobs.ParallelFor(1000,1,4,[&](size_t i){struct Guard{std::atomic<int>& a;Guard(std::atomic<int>& x):a(x){++a;}~Guard(){--a;}} guard(active);if(i==17)throw std::runtime_error("item failure");},"Failing batch");}
 catch(const std::runtime_error&){caught=true;}
 Check(caught&&active==0&&jobs.Stats().running==0,"failed item joins helpers before caller data expires");
 {JobSystem single(1);std::vector<unsigned> output(257);auto parent=single.Submit([&](JobContext&){single.ParallelFor(output.size(),3,4,[&](size_t i){output[i]=unsigned(i+1);},"Nested batch");});
  bool correct=single.Wait(parent)==JobState::Completed;for(size_t i=0;i<output.size();++i)correct&=output[i]==i+1;
  Check(correct,"one-worker nested batch completes without worker starvation");single.Shutdown();
  auto stats=single.ParallelFor(output.size(),3,4,[&](size_t i){output[i]=9;},"Stopped pool");
  Check(stats.ownerItems==output.size()&&stats.workerItems==0,"stopped pool completes data on owner");
 }
 EngineHost host;std::string error;Check(host.Init("Independent IK exactness",320,240,false,error),"normal resource host");
 Project project;Check(project.Load("projects/animation_studio/animation_studio.judasproj",error),"current animation project");
 host.OpenProjectAssets(project.RootDir(),project.AssetsDir());Scene source,configuration;
 Check(LoadSceneFromFile(project.StartupScenePath(),source,error)&&LoadSceneFromFile("tests/fixtures/ik/configuration.judas",configuration,error),"current performer and IK configuration load");
 if(!source.Find(10)||!configuration.Find(10)||!configuration.Find(10)->animation->fullBodyIK)return 1;
 Scene scene;auto actor=*source.Find(10);actor.characterMotor.reset();actor.ragdoll.reset();actor.scripts.clear();actor.socket.reset();actor.parent=0;
 for(int i=0;i<40;++i){auto copy=actor;copy.id=100+i;copy.transform.position={float(i)*3,0,0};copy.animation->fullBodyIK=configuration.Find(10)->animation->fullBodyIK;copy.animation->fullBodyIK->targets.clear();scene.InsertObject(copy);}
 // A socket consumer after four queued producers, and a child of a socket,
 // expose stale ancestor poses if the owner publication boundary is wrong.
 auto socket=*scene.Find(104);socket.id=104;socket.socket=SceneSocketComponent{};socket.socket->enabled=true;socket.socket->target=100;socket.socket->joint="PalmB";*scene.Find(104)=socket;
 auto child=*scene.Find(105);child.parent=104;*scene.Find(105)=child;
 std::vector<std::vector<Endpoint>> reference;
 for(unsigned lanes:{1u,2u,4u}){
  SDL_setenv("JUDAS_ANIMATION_LANES",std::to_string(lanes).c_str(),1);
  RuntimeWorld world;Check(world.Build(scene,&host.Resources(),error),"world with independent, socket and nested child actors");
  world.UpdateAnimations(0);host.Resources().WaitForAll();world.UpdateAnimations(0);
  for(int i=0;i<40;++i){auto* a=world.RuntimeAnimation(100+i);if(!a||!a->asset){std::puts("missing performer");return 1;}
   auto settings=*configuration.Find(10)->animation->fullBodyIK;FullBodyIKTarget target;target.id="hand";target.chain="palmB";target.space=IKTargetSpace::World;
   SceneTransform hand;world.JointPose(100+i,"PalmB","world",1,hand);target.position=glm::dvec3(hand.position+glm::vec3(-.06,.04,0));settings.targets={target};
   Check(world.ConfigureFullBodyIK(100+i,settings,error),"owner captures authored world target");
  }
  // A stale mapping must reject without publishing a partial pose/result,
  // independent of which lane evaluates it.
  world.RuntimeAnimation(116)->fullBodyIK->mapping.parents.clear();
  bool exact=true;
  for(int step=0;step<12;++step){
   // Both automatic and manually advanced clocks must publish once.
   if(step%2)for(int i=0;i<40;i+=3)world.AdvanceAnimation(100+i,1.f/60,error);
   world.PrepareAnimationReferences(1.f/60);world.UpdateAnimations(1.f/60);
   std::vector<Endpoint> endpoints;
   for(int i=0;i<40;++i){auto* a=world.RuntimeAnimation(100+i);Endpoint e{a->referencePose,a->solvedReferencePose,a->finalPose,a->fullBodyIK->result,a->playback.time,a->error};
    if(lanes>1){const auto& r=reference[step][i];exact&=SamePose(e.reference,r.reference)&&SamePose(e.solved,r.solved)&&SamePose(e.final,r.final)&&e.clock==r.clock&&e.error==r.error&&e.ik.iterations==r.ik.iterations&&e.ik.converged==r.ik.converged&&e.ik.targets.size()==r.ik.targets.size();
     for(size_t k=0;k<e.ik.targets.size()&&k<r.ik.targets.size();++k)exact&=e.ik.targets[k].status==r.ik.targets[k].status&&e.ik.targets[k].positionError==r.ik.targets[k].positionError;
    }endpoints.push_back(std::move(e));
   }if(lanes==1)reference.push_back(std::move(endpoints));
  }
  Check(exact,"batched full-body IK preserves exact poses, clocks, residuals and parent/socket dependencies");world.Destroy();
 }
 SDL_setenv("JUDAS_ANIMATION_LANES","4",1);host.Shutdown();
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
