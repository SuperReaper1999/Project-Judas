#include "WorldPersistence.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "Simulation.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <filesystem>
#include <cstdio>
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=2e-4f){return glm::length(a-b)<tolerance;}
int main(int argc,char** argv){
 namespace fs=std::filesystem;fs::path out=argc==3?argv[2]:"build/m48-focused";fs::create_directories(out);std::string error;Scene scene,round;
 Check(LoadSceneFromFile("tests/fixtures/animation/ragdoll.judas",scene,error),"ordinary authored ragdoll scene loads");
 if(failures){std::puts(error.c_str());return 1;}std::string text;SaveSceneToString(scene,text);Check(LoadSceneFromString(text,round,error)&&ScenesEqual(scene,round),"mapping/passive constraints serialize normally");
 std::string h1,h2;ComputeSceneFingerprint(scene,h1,error);round.Find(10)->ragdoll->bones[1].mass=3;ComputeSceneFingerprint(round,h2,error);Check(h1!=h2,"mapping contributes to baseline fingerprint");
 EngineHost host;Check(host.Init("M48 focused",320,240,false,error),"real EngineHost context/resources");if(failures)return 1;
 host.OpenProjectAssets(fs::absolute("tests/fixtures/animation").string(),fs::absolute("tests/fixtures/animation/Assets").string());auto& resources=host.Resources();
 resources.GetMesh("46464646464646464646464646464601",error);resources.WaitForAll();
 // Core fixture has no behaviour scripts; all physics steps below use the ordinary game fixed-step function.
 for(auto& o:scene.Objects())o.scripts.clear();scene.Find(11)->animation->clip="Wave";
 RuntimeWorld world;Check(world.Build(scene,&resources,error),"normal runtime builds shared animation and ragdoll authoring");world.UpdateAnimations(0);
 auto* a=world.RuntimeAnimation(10);a->playback.Seek(*a->asset,.5);world.ResolveAnimationPose(*a,0);auto entryPose=a->finalPose;
 auto globals=PoseGlobalMatrices(a->asset->skeleton,entryPose);auto transform=world.PresentedTransform(10,scene.Find(10)->transform,1);auto model=glm::translate(glm::mat4(1),transform.position)*glm::mat4_cast(transform.rotation);
 size_t baseline=world.Physics().AliveBodyCount();Check(world.EnterRagdoll(10,error),"enter from current mid-Wave pose");if(!world.RagdollActive(10)){std::puts(error.c_str());return 1;}
 auto rootId=world.RagdollBody(10,"Root"),elbowId=world.RagdollBody(10,"Elbow"),tipId=world.RagdollBody(10,"Tip");auto root=world.RuntimeBody(rootId),elbow=world.RuntimeBody(elbowId),tip=world.RuntimeBody(tipId);
 bool positions=true;for(int i=0;i<3;++i){const char* keys[]={"Root","Elbow","Tip"};auto body=world.RuntimeBody(world.RagdollBody(10,keys[i]));auto expected=model*globals[i]*glm::vec4(0,.5,0,1);positions&=Near(world.Physics().GetTransform(body).position,glm::vec3(expected));}
 Check(positions&&world.Physics().AliveBodyCount()==baseline+3,"bone/body offsets produce current resolved world transforms");
 Check(Near(a->finalPose.local[1].translation,entryPose.local[1].translation)&&std::abs(glm::dot(a->finalPose.local[1].rotation,entryPose.local[1].rotation))>1-1e-5f,"physics source preserves activation pose without bind flash");
 PhysicsQueryFilter query;query.ignoredBodies={world.RuntimeBody(1)};auto centre=world.Physics().GetTransform(root).position;auto hit=world.Physics().Raycast(centre+glm::vec3(0,0,3),{0,0,-1},6,query);Check(hit.hit&&world.EntityIdOfBody(hit.body)==rootId,"M44 query sees mapped ordinary entity/body");
 std::map<std::string,SaveChunk> saved;Scene savedScene;Check(WorldPersistence::Capture(world,saved,error)&&WorldPersistence::SceneFrom(saved,savedScene,error),"current snapshot captures articulation ownership");RuntimeWorld continued;
 const bool continuedOkay=continued.Build(savedScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(continued,error)&&WorldPersistence::Restore(continued,saved,error);
 Check(continuedOkay&&continued.RagdollActive(10)&&continued.RagdollBody(10,"Root")&&continued.IsTransientEntity(continued.RagdollBody(10,"Root")),"modern snapshot restores internal bodies under their articulation owner");continued.Destroy();
 auto pairs=world.Physics().FindCollidingPairs();bool adjacent=false;for(auto p:pairs)adjacent|=(p.a.id==root.id&&p.b.id==elbow.id)||(p.a.id==elbow.id&&p.b.id==root.id);Check(!adjacent,"adjacent collision suppressed by generation-aware physical filter");
 world.Physics().ApplyLinearImpulse(tip,{3,0,0});Check(world.Physics().GetLinearVelocity(tip).x>1,"ordinary impulse changes mapped body momentum");
 Check(world.IsBuilt(),"ordinary game fixed-step path starts");auto start=std::chrono::steady_clock::now();
 for(int i=0;i<120;++i)StepPlayedWorld(world,host.GetWindow(),1.f/60);
 double one=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120;
 Check(world.RagdollActive(10)&&world.Physics().GetTransform(root).position.y<centre.y&&std::isfinite(world.Physics().GetLinearVelocity(root).x),"normal Judas gravity/contact path advances finite ragdoll");
 float maxAnchorError=0,maxAngle=0;auto bodies=std::vector<BodyHandle>{root,elbow,tip};
 for(size_t i=1;i<bodies.size();++i){auto child=world.Physics().GetTransform(bodies[i]);auto parent=world.Physics().GetTransform(bodies[i-1]);auto local=glm::normalize(glm::inverse(parent.rotation)*child.rotation);maxAngle=std::max(maxAngle,std::abs(2*std::atan2(local.z,local.w)));}
 // Observe the named constraints via their mapped skeleton pivot positions.
 auto finalGlobals=PoseGlobalMatrices(a->asset->skeleton,a->finalPose);auto current=world.PresentedTransform(10,transform,1);auto currentModel=glm::translate(glm::mat4(1),current.position)*glm::mat4_cast(current.rotation);
 for(int i=0;i<3;++i){auto expected=currentModel*finalGlobals[i]*glm::vec4(0,.5,0,1);maxAnchorError=std::max(maxAnchorError,glm::length(world.Physics().GetTransform(bodies[i]).position-glm::vec3(expected)));}
 Check(maxAngle<1.25f&&maxAnchorError<.002f,"M45 hinge limits and physics-to-local hierarchy mapping remain coherent");
 auto captured=a->finalPose;Check(world.LeaveRagdoll(10,.4,error)&&!world.RagdollActive(10)&&world.Physics().AliveBodyCount()==baseline,"leave releases bodies/constraints immediately");
 Check(std::abs(glm::dot(a->finalPose.local[1].rotation,captured.local[1].rotation))>1-1e-5f,"return contribution captures physical pose without one-frame reset");
 for(int i=0;i<30;++i)StepPlayedWorld(world,host.GetWindow(),1.f/60);Check(!a->external.count("ragdollReturn"),"visual return retires cleanly through M47");
 Check(!world.Physics().IsDynamicBody(root),"retired generation handle is invalid");
 Check(world.EnterRagdoll(11,error),"oblique instance maps through arbitrary reference basis");auto rotatedRoot=world.RuntimeBody(world.RagdollBody(11,"Root"));Check(rotatedRoot.id!=root.id&&!world.Physics().SetPairCollisionEnabled(root,rotatedRoot,false),"slot reuse does not resurrect old handles or exclusions");
 auto dead=world.RagdollBody(11,"Tip");world.DestroyEntity(dead);world.UpdateRagdolls(0);Check(!world.RagdollActive(11)&&world.Physics().AliveBodyCount()==baseline,"mapped body destruction tears down remaining articulation");
 SceneTransform placement;placement.position={6,3,0};auto spawned=world.SpawnPrefab("46464646464646464646464646464604",placement,error);world.UpdateAnimations(0);Check(spawned&&world.EnterRagdoll(spawned,error),"prefab-spawned ordinary instance supports ragdoll");world.SetRagdollEnabled(spawned,false,error);Check(!world.RagdollActive(spawned)&&world.DestroyHierarchy(spawned,error),"disable and hierarchy destruction clean physics");
 std::vector<EntityId> group;for(int i=0;i<5;++i){placement.position={float(i*4-8),5,5};auto id=world.SpawnPrefab("46464646464646464646464646464604",placement,error);world.UpdateAnimations(0);if(id&&world.EnterRagdoll(id,error))group.push_back(id);}
 start=std::chrono::steady_clock::now();for(int i=0;i<120;++i)StepPlayedWorld(world,host.GetWindow(),1.f/60);double five=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120;
 Check(group.size()==5,"small group shares one immutable skeleton with independent articulations");for(auto id:group)world.DestroyHierarchy(id,error);Check(world.Physics().AliveBodyCount()==baseline,"group destruction leaves no orphaned physics");
 world.EnterRagdoll(10,error);world.LeaveRagdoll(10,.4f,error);world.RestoreAuthoredState();world.UpdateAnimations(0);
 Check(world.RuntimeAnimation(10)->external.empty()&&world.RuntimeAnimation(10)->playback.time==0,"authored reset clears return contribution and mixer state");
 world.UpdateAnimations(.1f);world.EnterRagdoll(10,error);auto replacedBody=world.RuntimeBody(world.RagdollBody(10,"Root"));
 resources.Invalidate("46464646464646464646464646464601");resources.GetMesh("46464646464646464646464646464601",error);resources.WaitForAll();
 auto* reloaded=world.RuntimeAnimation(10);Check(reloaded->previousWorld.empty()&&reloaded->recentWorld.empty()&&reloaded->motionDt==0,"asset replacement invalidates pose motion observations");
 world.UpdateRagdolls(0);Check(!world.RagdollActive(10)&&!world.Physics().IsDynamicBody(replacedBody)&&world.Physics().AliveBodyCount()==baseline,"asset replacement safely retires old articulation");
 world.Destroy();Check(world.Build(scene,&resources,error),"Stop/reload reconstructs authored world");world.UpdateAnimations(0);Check(!world.RagdollActive(10)&&world.Physics().AliveBodyCount()==baseline,"fresh Play has no stale bodies/constraints");world.Destroy();
 auto freeScene=scene;for(auto& o:freeScene.Objects()){if(o.body)o.body.reset();if(o.ragdoll){o.animation->clip="Wave";o.animation->playOnStart=false;}}
 std::vector<glm::vec3> endpoints;auto rotation=glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3)));
 for(int trial=0;trial<2;++trial){auto equivalent=freeScene;if(trial)for(auto& o:equivalent.Objects()){o.transform.position=rotation*o.transform.position;o.transform.rotation=rotation*o.transform.rotation;}
  Check(world.Build(equivalent,&resources,error),"equivalent free-fall authored scene builds");world.UpdateAnimations(0);Check(world.EnterRagdoll(10,error),"equivalent current-pose articulation enters");world.IsBuilt();
  auto body=world.RuntimeBody(world.RagdollBody(10,"Root"));auto before=world.Physics().GetTransform(body).position;
  for(int i=0;i<30;++i)StepPlayedWorld(world,host.GetWindow(),1.f/60);auto change=world.Physics().GetTransform(body).position-before;endpoints.push_back(trial?glm::inverse(rotation)*change:change);world.Destroy();
 }
 Check(Near(endpoints[0],endpoints[1],.002f),"rotated gravity/world articulation produces equivalent physical displacement");
 host.Shutdown();
 std::printf("PERFORMANCE one_articulation_whole_fixed_step_ms=%.6f five_articulations_whole_fixed_step_ms=%.6f samples=120 mapped_bodies_each=3\n",one,five);std::printf("OBSERVATIONS mapped_center_residual=%.9g max_hinge_angle=%.9g\n",maxAnchorError,maxAngle);
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
