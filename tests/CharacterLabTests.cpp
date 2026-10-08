// M70 exercises the ordinary project and published examples through the real VM.
// The copied project is private test output; approved source art stays unchanged.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSession.h"
#include "Prefab.h"
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
int checks=0,failures=0;
void Check(bool ok,const std::string& label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';}
Json Read(const fs::path& path){std::ifstream input(path);Json result;input>>result;return result;}
Json State(RuntimeWorld& world,EntityId id){if(world.Scripts())for(const auto& state:world.Scripts()->Capture())if(state.entity==id)return Json::parse(state.json);return Json::object();}
void Faults(RuntimeWorld& world,const std::string& label){Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),label);if(world.Scripts())for(const auto& d:world.Scripts()->Diagnostics())std::cout<<d.message<<'\n';}
bool Prepare(RuntimeWorld& world,ResourceManager& resources,std::string& error){
 for(int i=0;i<30;++i){resources.WaitForAll();world.UpdateAnimations(0);if(WorldPersistence::Prepare(world,error))return true;}return false;
}
bool Build(RuntimeWorld& world,const Scene& scene,ResourceManager& resources,Project& project,std::string& error,bool deferred=false){
 for(int attempt=0;attempt<30;++attempt){if(world.Build(scene,&resources,error,&project.Settings().classification,&project.Settings().navigation,deferred))return true;if(error!="loading")return false;resources.WaitForAll();}return false;
}
void Step(RuntimeWorld& world,GameSession& session,Window& window,ResourceManager& resources,int count=1,const char* control=nullptr,float value=0){
 for(int i=0;i<count;++i){window.Input().BeginFrame();if(i==0&&control)window.Input().SetPhysical(control,value);world.UpdateUIScripts(&window.Input(),1.f/60);world.UpdateScripts(&window.Input(),1.f/60);window.Input().BeginFixedStep();StepPlayedWorld(session,window,1.f/60);world.PresentationScripts(&window.Input(),1.f/60,.5f);resources.WaitForAll();}
}
void Pulse(RuntimeWorld& world,GameSession& session,Window& window,ResourceManager& resources,const char* control){
 Step(world,session,window,resources,1,control,1);Step(world,session,window,resources,1,control,0);
}
bool FinitePose(const SkeletalPose& pose){for(const auto& p:pose.local)for(int i=0;i<3;++i)if(!std::isfinite(p.translation[i])||!std::isfinite(p.scale[i]))return false;return true;}
bool ObserveContacts(RuntimeWorld::AnimationInstance* animation,const std::string& scenario,EntityId id){
 if(!animation||!animation->fullBodyIK)return false;
 bool good=true;
 for(const auto& target:animation->fullBodyIK->result.targets){std::cout<<"CONTACT "<<scenario<<" actor="<<id<<" target="<<target.id<<" position_m="<<target.positionError<<" orientation_rad="<<target.orientationError<<" status="<<target.status<<'\n';good&=target.positionError<=.01f&&target.orientationError<=glm::radians(2.f);}
 return good&&!animation->fullBodyIK->result.targets.empty();
}
FullBodyIKSettings KnownContacts(const SkeletalAsset& asset){
 FullBodyIKSettings settings;settings.bodyRoot="Pivot";settings.spine={"Vertebra0","Vertebra1"};settings.rootMin=glm::vec3(-.15f);settings.rootMax=glm::vec3(.15f);settings.chains={{"palmA",{"ShoulderA","ElbowA","PalmA"}},{"palmB",{"ShoulderB","ElbowB","PalmB"}},{"soleA",{"ThighA","ShinA","SoleA"}},{"soleB",{"ThighB","ShinB","SoleB"}}};
 auto globals=PoseGlobalMatrices(asset.skeleton,asset.skeleton.rest);for(const auto& chain:settings.chains){JointTransform transform;std::string error;DecomposeRigidPose(globals[size_t(FindSkeletonJoint(asset.skeleton,chain.joints.back()))],transform,error);FullBodyIKTarget target;target.id=target.chain=chain.id;target.position=glm::dvec3(transform.translation)+glm::dvec3(0,.025,-.015);target.orientation=transform.rotation;target.orientationWeight=1;settings.targets.push_back(target);}return settings;
}
void OwnerContactChecks(EngineHost& host,Project& project,const fs::path& root,const fs::path& out){
 auto& resources=host.Resources();auto& input=host.GetWindow().Input();std::string error;
 const auto path=root/"Assets/owner-contact-probe.js";
 std::ofstream(path)<<R"JS(import {time} from 'judas';
export const properties={optIn:{type:'boolean',default:false},destroyOther:{type:'boolean',default:false},disableBody:{type:'boolean',default:false},destroySelf:{type:'boolean',default:false}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={enter:0,stay:0,exit:0,safe:true,fixed:true};}
 start(){if(this.props.optIn)this.entity.ragdoll.receiveContactEvents=true;this.state.subscribed=this.entity.ragdoll.receiveContactEvents;}
 observe(e){this.state.safe&&=!!e.selfBody?.valid&&!!e.other?.valid&&typeof e.selfJoint==='string'&&Number.isFinite(e.point.x);this.state.fixed&&=time.fixed;this.state.joint=e.selfJoint;this.state.body=e.selfBody?.id;}
 onCollisionEnter(e){this.state.enter++;this.observe(e);if(this.props.destroyOther)e.other.destroy();if(this.props.disableBody)e.selfBody.setColliderEnabled(false);if(this.props.destroySelf)this.entity.destroy();}
 onCollisionStay(e){this.state.stay++;this.observe(e);}
 onCollisionExit(e){this.state.exit++;this.state.exitBodyValid=!!e.selfBody?.valid;this.state.exitOtherValid=!!e.other?.valid;this.state.exitJoint=e.selfJoint;}
 onTriggerEnter(e){this.onCollisionEnter(e);this.state.sensor=true;}
 onTriggerStay(e){this.onCollisionStay(e);}
 onTriggerExit(e){this.onCollisionExit(e);}
})JS";
 AssetRecord probe;Check(host.Assets().Track(path.string(),probe,error),"owner-contact script is a normal registered asset");if(probe.id.empty())return;
 Scene source;Check(LoadSceneFromFile((root/"Scenes/physical.judas").string(),source,error),"owner-contact fixture reads the ordinary imported rig");if(!source.Find(10))return;
 Scene fixture;auto actor=*source.Find(10);actor.characterMotor.reset();actor.socket.reset();actor.ui.reset();actor.animation->playOnStart=false;actor.animation->clip.clear();actor.animation->layers.clear();actor.animation->limbs.clear();actor.animation->fullBodyIK.reset();actor.transform.position={0,0,0};actor.transform.rotation={1,0,0,0};actor.transform.scale=glm::vec3(1);
 actor.ragdoll->physicalAnimation.reset();actor.ragdoll->playOnStart=false;actor.ragdoll->receiveContactEvents=true;auto bone=actor.ragdoll->bones.front();bone.parent.clear();bone.shape=RagdollShape::Sphere;bone.radius=.25f;bone.offset={0,0,0};bone.orientation={1,0,0,0};bone.collisionLayer=0;bone.collisionMask=kAllCategories;actor.ragdoll->bones={bone};actor.scripts={{1,probe.id,true,"{}"}};fixture.InsertObject(actor);
 SceneObject ground;ground.id=20;ground.name="Unscripted ordinary ground";ground.body=SceneBodyComponent{};ground.body->halfExtents={3,.5f,3};ground.transform.position={0,-.5f,0};fixture.InsertObject(ground);
 std::string bytes,enabledHash,legacyHash;Scene round;SaveSceneToString(fixture,bytes);
 Check(LoadSceneFromString(bytes,round,error)&&ScenesEqual(fixture,round)&&round.Find(10)->ragdoll->receiveContactEvents,"opt-in owner contact authoring round-trips");
 ComputeSceneFingerprint(fixture,enabledHash,error);round.Find(10)->ragdoll->receiveContactEvents=false;ComputeSceneFingerprint(round,legacyHash,error);Check(enabledHash!=legacyHash,"owner contact subscription affects authored compatibility without changing legacy defaults");
 SaveSceneToString(round,bytes);Check(bytes.find("ragdoll.receive-contact-events")==std::string::npos,"default false emits no legacy authored extension");
 auto start=[&](RuntimeWorld& world,const Scene& scene){const bool ready=Build(world,scene,resources,project,error)&&Prepare(world,resources,error)&&world.EnterRagdoll(10,error);Check(ready,"ordinary unscripted mapped body/contact world: "+error);if(!ready)return BodyHandle{};auto body=world.RuntimeBody(world.RagdollBody(10,bone.joint));world.Physics().ResetBody(body,{0,.25f,0},{1,0,0,0});return body;};
 auto step=[&](RuntimeWorld& world){input.BeginFrame();input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.Physics().Step(1.f/60);world.DispatchPhysicsEvents(&input,1.f/60);};
 {
  RuntimeWorld world;auto body=start(world,fixture);if(body.IsValid()){
   Check(world.RuntimeDefinition(world.RagdollBody(10,bone.joint))->scripts.empty()&&world.RuntimeDefinition(20)->scripts.empty(),"both actual collision participants are unscripted");
   step(world);step(world);world.Physics().ResetBody(body,{0,3,0},{1,0,0,0});step(world);step(world);auto state=State(world,10);
   Check(state.value("enter",0)==1&&state.value("stay",0)==1&&state.value("exit",0)==1,"unscripted bone/ground enter-stay-exit reaches owner once per pair");
   Check(state.value("safe",false)&&state.value("fixed",false)&&state.value("joint","")==SkeletonJointKey(world.RuntimeAnimation(10)->asset->skeleton,FindSkeletonJoint(world.RuntimeAnimation(10)->asset->skeleton,bone.joint)),"owner callback reports safe body, stable joint identity and actual fixed contact data");
   std::ofstream(out/"owner-contact-sequence.json")<<state.dump(2)<<'\n';Faults(world,"unscripted owner contact sequence has no VM faults");
  }
 }
 {
  RuntimeWorld world;auto scene=fixture;scene.Find(10)->ragdoll->receiveContactEvents=false;auto body=start(world,scene);if(body.IsValid()){step(world);step(world);auto state=State(world,10);Check(state.value("enter",-1)==0&&state.value("stay",-1)==0&&!state.value("subscribed",true),"legacy default leaves owner callbacks unsubscribed");}
 }
 {
  RuntimeWorld world;auto scene=fixture;scene.Find(10)->ragdoll->receiveContactEvents=false;scene.Find(10)->scripts[0].properties="{\"optIn\":true}";auto body=start(world,scene);if(body.IsValid()){step(world);Check(State(world,10).value("enter",0)==1&&world.RuntimeDefinition(10)->ragdoll->receiveContactEvents,"JudasJS explicitly opts into owner contact delivery");world.SetRagdollContactEvents(10,false);step(world);Check(State(world,10).value("stay",0)==0,"unsubscription prevents subsequent owner callbacks");}
 }
 {
  RuntimeWorld world;auto body=start(world,fixture);if(body.IsValid()){world.Physics().SetCollisionFilter(body,0,0);step(world);Check(State(world,10).value("enter",-1)==0,"owner subscription preserves M39 rejection");}
 }
 {
  RuntimeWorld world;auto scene=fixture;scene.Find(20)->body->sensor=true;auto body=start(world,scene);if(body.IsValid()){step(world);step(world);world.Physics().ResetBody(body,{0,3,0},{1,0,0,0});step(world);auto state=State(world,10);Check(state.value("sensor",false)&&state.value("enter",0)==1&&state.value("stay",0)==1&&state.value("exit",0)==1,"normal sensor observations use the same opted-in owner path");}
 }
 for(const char* behaviour:{"destroyOther","disableBody","destroySelf"}){
  RuntimeWorld world;auto scene=fixture;scene.Find(10)->scripts[0].properties=Json{{behaviour,true}}.dump();auto body=start(world,scene);if(!body.IsValid())continue;const auto oldGround=world.RuntimeBody(20);step(world);
  if(std::string(behaviour)=="destroySelf"){Check(!world.RuntimeDefinition(10)&&!world.Physics().IsBodyEnabled(body),"owner destruction inside contact callback retires the articulation safely");step(world);}
  else {if(std::string(behaviour)=="destroyOther"){SceneObject replacement=ground;replacement.id=0;replacement.transform.position={20,-.5f,0};const auto created=world.CreateEntity(replacement,nullptr,&error);Check(created!=0,"destroyed counterpart slot can be reused before exit: "+error);if(created){const auto reused=world.RuntimeBody(created);Check(reused.IsValid()&&reused.id!=oldGround.id&&!world.Physics().IsBodyEnabled(oldGround),"replacement has a fresh generation while original handle is stale");}}step(world);step(world);auto state=State(world,10);Check(state.value("enter",0)==1&&state.value("exit",0)==1&&state.value("exitJoint","")!="","callback destruction/disable produces one attributed exit");if(std::string(behaviour)=="destroyOther")Check(!state.value("exitOtherValid",true),"destroyed counterpart cannot alias the replacement entity handle");}
  Faults(world,std::string(behaviour)+" owner callback preserves VM lifetime");
 }
 // The normal prefab property system uses scene serialization/equality rather
 // than a special articulated event override path.
 auto properties=ObjectProperties(actor);auto overridden=actor;properties["ragdoll.receive-contact-events"]="false";
 Check(ApplyObjectProperties(overridden,properties,error)&&!overridden.ragdoll->receiveContactEvents,"ordinary prefab property overrides include owner contact subscription");
}
}
int main(int argc,char** argv){
 if(argc<2||argc>3){std::cerr<<"usage: "<<argv[0]<<" <fresh-output-directory> [cold-write|cold-read]\n";return 2;}
 const std::string mode=argc==3?argv[2]:"";if(!mode.empty()&&mode!="cold-write"&&mode!="cold-read")return 2;
 const auto out=fs::absolute(argv[1]),root=out/"project";if(fs::exists(root)&&mode!="cold-read"){std::cerr<<"Use a fresh output directory\n";return 2;}
 fs::create_directories(out);if(mode!="cold-read")fs::copy("projects/character_lab",root,fs::copy_options::recursive);
 std::string error;EngineHost host;Check(host.Init("M70 character lab",800,450,false,error),"actual GL host: "+error);if(failures)return 1;
 host.OpenProjectAssets(root.string(),(root/"Assets").string());auto& resources=host.Resources();auto& window=host.GetWindow();window.SetTestInputMode(true);
 Project project;Check(project.Load((root/"character_lab.judasproj").string(),error),"ordinary registered project: "+error);window.Input().SetMap(project.Settings().input,error);
 const auto source=Read(root/"Sources/figure.gltf"),alternateSource=Read(root/"Sources/alternate.gltf");
 Check(source["skins"].size()==2&&source["skins"][0]["joints"].size()==65&&source["meshes"].size()==6,"approved fixture is a full 65-joint six-part two-skin source");
 Check(source["nodes"][1]["name"]!=alternateSource["nodes"][1]["name"]&&source["nodes"][1]["translation"]!=alternateSource["nodes"][1]["translation"],"variation has different names and proportions");
 if(!mode.empty()){
  // Two explicit executable invocations exercise real storage and a brand-new
  // process/GL/resource/QuickJS lifetime. This is not a same-VM reload claim.
  SaveStorage slots((out/"slots").string());GameSnapshot snapshot;Scene physical;bool recovered=false;
  if(mode=="cold-read")Check(slots.Read("m70",snapshot,recovered,error)&&!recovered&&snapshot.project==project.Settings().saveIdentity,"new process reads committed M61 slot: "+error);
  bool loaded=mode=="cold-read"?WorldPersistence::SceneFrom(snapshot.participants,physical,error):LoadSceneFromFile((root/"Scenes/physical.judas").string(),physical,error);Check(loaded,"fresh process gets ordinary captured scene: "+error);
  for(const auto& object:physical.Objects()){
   if(object.render&&!object.render->meshAsset.empty())resources.GetMesh(object.render->meshAsset,error);
  }
  resources.WaitForAll();RuntimeWorld world;
  Check(loaded&&Build(world,physical,resources,project,error,mode=="cold-read")&&Prepare(world,resources,error),"new process constructs ordinary participants: "+error);if(!world.IsBuilt())return 1;
  world.legacyGameplay=false;world.viewportWidth=800;world.viewportHeight=450;world.SetSceneControl(std::make_shared<SceneSession>(project,(root/"Scenes/physical.judas").string()));GameSession game;
  if(mode=="cold-read"){
   Check(WorldPersistence::Restore(world,snapshot.participants,error),"new process restores complete native records: "+error);WorldPersistence::Published(world);auto* animation=world.RuntimeAnimation(10);auto physicalState=world.PhysicalAnimationSnapshot(10);
   Check(animation&&animation->fullBodyIK&&animation->fullBodyIK->settings.targets.size()==4&&animation->layers.size()==2,"fresh process restores multi-target mapping, crossfade and masked layers");
   Check(world.RagdollActive(10)&&physicalState.mode==PhysicalAnimationMode::Partial&&physicalState.observations.empty(),"fresh process restores actual partial authority without previous drive observations");
   auto savedMetadata=Json::parse(snapshot.metadata);std::cout<<"COLD deferred_script_state="<<State(world,90).dump()<<'\n';
   // Restore queues durable records before script instances exist. Normal
   // synchronization constructs them and applies records before callbacks run.
   if(world.Scripts())world.Scripts()->Synchronize(world.ScriptSlots());
   auto state=State(world,90);std::cout<<"COLD synchronized_script_state="<<state.dump()<<'\n';Check(state.value("elapsed",0.)==savedMetadata.value("elapsed",-1.),"loaded JS instance materializes durable state before its first callback");
   game.Begin(world,error);Step(world,game,window,resources,3);state=State(world,90);Check(state.value("elapsed",0.)>savedMetadata.value("elapsed",0.)&&state.value("hidden",false)&&state.value("carry",false),"fresh restore advances without replaying default initialization or hidden/layer toggles");
   Check(world.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Partial&&FinitePose(world.RuntimeAnimation(10)->finalPose),"first resumed steps preserve physical authority and finite final pose");Faults(world,"fresh process restore callbacks safe");
  }else{
   game.Begin(world,error);Step(world,game,window,resources,40);Pulse(world,game,window,resources,"key:C");Pulse(world,game,window,resources,"key:L");Pulse(world,game,window,resources,"key:H");
   auto* animation=world.RuntimeAnimation(10);Check(animation&&animation->asset&&world.ConfigureFullBodyIK(10,KnownContacts(*animation->asset),error),"normal imported fixture accepts four durable contacts beside partial physics: "+error);Step(world,game,window,resources,10);
   snapshot.project=project.Settings().saveIdentity;snapshot.scene="Scenes/physical.judas";snapshot.displayName="M70 full-rig partial state";snapshot.diagnosticBuild="M70 accepted-base candidate";snapshot.metadata=Json{{"elapsed",State(world,90).value("elapsed",0.)}}.dump();Check(ComputeSceneFingerprint(physical,snapshot.content,error),"canonical authored fingerprint for cold slot");
   Check(WorldPersistence::Capture(world,snapshot.participants,error)&&slots.Write("m70",snapshot,error),"M61 atomic storage commits multi-target/partial/layer/script participants: "+error);Faults(world,"writer actual project callbacks safe");
  }
  std::ofstream(out/(mode+"-state.json"))<<State(world,90).dump(2)<<'\n';game.End();world.EndScripts();world.Destroy();std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
 }
 Scene scene;Check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"registered wall scene loads: "+error);
 for(const auto& object:scene.Objects())if(object.render&&!object.render->meshAsset.empty())resources.GetMesh(object.render->meshAsset,error);
 resources.WaitForAll();RuntimeWorld world;Check(Build(world,scene,resources,project,error),"ordinary wall world: "+error);if(!world.IsBuilt())return 1;
 world.viewportWidth=800;world.viewportHeight=450;world.legacyGameplay=false;world.SetSceneControl(std::make_shared<SceneSession>(project,project.StartupScenePath()));
 Check(Prepare(world,resources,error),"async multipart resources published: "+error);
 auto* a=world.RuntimeAnimation(10);auto* b=world.RuntimeAnimation(11);
 if(a&&a->asset)std::cout<<"IMPORT actor=10 hierarchy="<<a->asset->skeleton.names.size()<<" palette="<<a->asset->skeleton.skinNodes.size()<<'\n';
 if(b&&b->asset)std::cout<<"IMPORT actor=11 hierarchy="<<b->asset->skeleton.names.size()<<" palette="<<b->asset->skeleton.skinNodes.size()<<'\n';
 Check(a&&a->asset&&b&&b->asset&&a->asset->skeleton.names.size()==72&&a->asset->skeleton.skinNodes.size()==130&&FindSkeletonJoint(a->asset->skeleton,"__JudasMotionRoot")>=0,"71 source hierarchy nodes plus normal motion root and both palettes retained");
 if(!a||!a->asset||!b||!b->asset)return 1;
 auto* parts=resources.TryGetModelParts(scene.Find(10)->render->meshAsset);Check(parts&&parts->size()==6,"resource publication retains all imported mesh parts");
 GameSession session;Check(session.Begin(world,error),"normal fixed-step session: "+error);Step(world,session,window,resources,90);Faults(world,"lab script executes using public APIs");
 auto independent=[](RuntimeWorld::AnimationInstance* instance){if(!instance||!instance->fullBodyIK)return false;const auto& result=instance->fullBodyIK->result;return result.targets.size()==4&&result.iterations<=64&&FinitePose(result.pose);};
 Check(independent(a)&&independent(b),"four contacts resolved independently on both imported rig variations");
 const bool wallA=ObserveContacts(a,"wall",10),wallB=ObserveContacts(b,"wall",11);Check(wallA&&wallB,"ordinary feasible wall contact residuals within 1 cm / 2 degrees");
 Check(a->fullBodyIK->mapping.root!=b->fullBodyIK->mapping.root||a->asset->skeleton.names!=b->asset->skeleton.names,"mapping uses each instance's actual skeleton identity");
 const auto rootBefore=world.RuntimeDefinition(10)->transform;Pulse(world,session,window,resources,"key:I");
 bool limited=false;for(const auto& result:a->fullBodyIK->result.targets)limited|=result.status!="reached";
 Check(limited&&FinitePose(a->finalPose)&&a->fullBodyIK->result.iterations<=64,"unreachable target remains a bounded finite compromise");
 Check(glm::length(world.RuntimeDefinition(10)->transform.position-rootBefore.position)<1e-4f,"IK root correction does not move the entity or motor root");Pulse(world,session,window,resources,"key:I");
 Pulse(world,session,window,resources,"key:C");Pulse(world,session,window,resources,"key:L");Pulse(world,session,window,resources,"key:H");Step(world,session,window,resources,30);
 int head=FindSkeletonJoint(a->asset->skeleton,"Cranial");Check(head>=0&&a->referencePose.local[size_t(head)].scale==glm::vec3(0)&&FinitePose(a->finalPose),"existing head-hidden layer remains safe beside shared IK");
 Check(a->layers.size()==2&&b->layers.size()==2&&independent(a)&&independent(b),"Carry and hidden-head masks remain per-instance with crossfade playback");
 SceneTransform palm;Check(world.JointPose(10,"PalmB","world",1,palm),"resolved joint world pose remains available");world.UpdateSockets();
 const auto* socket=world.RuntimeDefinition(30);Check(socket&&glm::length(socket->transform.position-palm.position)<.3f,"socket follows final resolved hierarchy");
 auto currentTime=a->playback.time;auto currentReference=a->referencePose;for(int i=0;i<4;++i){world.PresentationScripts(&window.Input(),1.f/60,.2f+i*.2f);world.AnimationSkin(10,.2f+i*.2f);}
 Check(a->playback.time==currentTime&&a->referencePose.local.size()==currentReference.local.size(),"extra presentation queries do not advance the authoritative clip");
 // Pending target values must not leak through an unrelated frame-side layer
 // update. The disposable sampled batch describes the last fixed result.
 if(a->fullBodyIK&&!a->layers.empty()){
  auto savedTargets=a->fullBodyIK->settings.targets;auto pending=savedTargets;
  pending.resize(1);pending[0].id="pending-fixed-frame";
  const auto sampledCount=a->fullBodyIK->result.targets.size();const auto sampledId=a->fullBodyIK->result.targets.front().id;
  Check(world.SetFullBodyIKTargets(10,pending,error)&&world.SetAnimationLayer(10,a->layers.front().settings,false,error),"atomic targets plus frame-side existing-layer update: "+error);
  Check(a->fullBodyIK->result.targets.size()==sampledCount&&a->fullBodyIK->result.targets.front().id==sampledId,"frame-side layer update cannot pull pending targets into the fixed sample");
  world.PrepareAnimationReferences(1.f/60);
  Check(a->fullBodyIK->result.targets.size()==1&&a->fullBodyIK->result.targets.front().id=="pending-fixed-frame","next fixed reference boundary publishes the submitted target batch");
  Check(world.SetFullBodyIKTargets(10,savedTargets,error),"restore ordinary authored four-target intent before save checks");world.PrepareAnimationReferences(0);
 }
 Check(world.view.has_value()&&world.view->farPlane==150,"lab uses ordinary script-owned projection range");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"save captures IK, layers, clip, sockets and script state: "+error);
 Scene saved;RuntimeWorld resumed;bool restored=WorldPersistence::SceneFrom(chunks,saved,error)&&Build(resumed,saved,resources,project,error,true);
 if(restored){resumed.viewportWidth=800;resumed.viewportHeight=450;resumed.SetSceneControl(std::make_shared<SceneSession>(project,project.StartupScenePath()));restored=Prepare(resumed,resources,error)&&WorldPersistence::Restore(resumed,chunks,error);}
 Check(restored,"cold normal participant restoration: "+error);
 if(restored){WorldPersistence::Published(resumed);resumed.UpdateScripts(&window.Input(),1.f/60);auto* restoredA=resumed.RuntimeAnimation(10);Check(restoredA&&restoredA->fullBodyIK&&restoredA->fullBodyIK->settings.targets.size()==4&&restoredA->layers.size()==2,"restored targets/layers preserved without fresh activation");Faults(resumed,"cold restore callback is safe");}
 Check(WorldPersistence::CanSuspendAnimation(world,10),"nonphysical shared IK can suspend through normal streaming participant");std::string suspended;
 bool streamed=WorldPersistence::CaptureAnimation(world,10,suspended,error)&&WorldPersistence::RestoreAnimation(world,10,suspended,error);Check(streamed&&a->fullBodyIK&&a->layers.size()==2,"stream suspension keeps durable target/layer configuration: "+error);
 // Resource replacement gets a new immutable asset and no old mapping/history.
 auto oldAsset=a->asset;const auto mesh=scene.Find(10)->render->meshAsset;resources.Invalidate(mesh);resources.GetMesh(mesh,error);resources.WaitForAll();world.UpdateAnimations(0);world.PrepareAnimationReferences(1.f/60);
 Check(a->asset&&a->asset!=oldAsset&&independent(a),"reimport publication rebuilds mapping against replacement immutable asset");
 session.End();world.EndScripts();world.Destroy();resumed.Destroy();Check(!world.Scripts()&&!world.IsBuilt(),"Stop/transition cleans the current lab world");
 Scene board;Check(LoadSceneFromFile((root/"Scenes/board.judas").string(),board,error),"ordinary registered board scene loads");RuntimeWorld boardWorld;bool boardBuilt=Build(boardWorld,board,resources,project,error)&&Prepare(boardWorld,resources,error);Check(boardBuilt,"board async resources and world ready: "+error);
 if(boardBuilt){boardWorld.SetSceneControl(std::make_shared<SceneSession>(project,(root/"Scenes/board.judas").string()));boardWorld.viewportWidth=800;boardWorld.viewportHeight=450;boardWorld.legacyGameplay=false;GameSession game;game.Begin(boardWorld,error);Step(boardWorld,game,window,resources,120);Faults(boardWorld,"board targets and separate visual samples use public APIs");const bool boardA=ObserveContacts(boardWorld.RuntimeAnimation(10),"board",10),boardB=ObserveContacts(boardWorld.RuntimeAnimation(11),"board",11);Check(boardA&&boardB,"tilting support foot contacts within 1 cm / 2 degrees");
  auto actual=boardWorld.RuntimeDefinition(20)->transform;boardWorld.PresentationScripts(&window.Input(),1.f/60,0);auto previousVisual=boardWorld.RuntimeDefinition(22)->transform;boardWorld.PresentationScripts(&window.Input(),1.f/60,1);auto currentVisual=boardWorld.RuntimeDefinition(22)->transform;
  Check(glm::length(currentVisual.position-actual.position)<1e-6f&&glm::length(previousVisual.position-currentVisual.position)>1e-6f&&glm::length(boardWorld.RuntimeDefinition(20)->transform.position-actual.position)<1e-6f,"visible board samples fixed history without moving the real collider");
  Pulse(boardWorld,game,window,resources,"key:T");Step(boardWorld,game,window,resources,30);const bool stepA=ObserveContacts(boardWorld.RuntimeAnimation(10),"board-step",10),stepB=ObserveContacts(boardWorld.RuntimeAnimation(11),"board-step",11);Check(stepA&&stepB,"same shared solver handles board stepping variant");game.End();boardWorld.EndScripts();boardWorld.Destroy();
 }
 // Run each copyable cookbook file unchanged on the actual full-rig fixture.
 for(const auto* name:{"multi-target-ik","physical-animation"}){
  const auto examplePath=root/"Assets"/(std::string(name)+"-example.js");fs::copy_file(fs::path("docs/judasjs/examples")/(std::string(name)+".js"),examplePath,fs::copy_options::overwrite_existing);AssetRecord script;Check(host.Assets().Track(examplePath.string(),script,error),std::string(name)+" registered example");
  resources.Invalidate(script.id);Scene fixture;LoadSceneFromFile((root/"Scenes/physical.judas").string(),fixture,error);for(auto& object:fixture.Objects()){object.scripts.clear();object.ui.reset();object.socket.reset();}fixture.Find(10)->scripts={{1,script.id,true,"{}"}};
  RuntimeWorld example;bool built=Build(example,fixture,resources,project,error);Check(built&&Prepare(example,resources,error),std::string(name)+" normal fixture world: "+error);if(!built)continue;
  example.viewportWidth=800;example.viewportHeight=450;example.legacyGameplay=false;GameSession game;game.Begin(example,error);Step(example,game,window,resources,100);Faults(example,std::string(name)+" unmodified example executes");const auto state=State(example,10);std::ofstream(out/(std::string(name)+".json"))<<state.dump(2)<<'\n';
  if(std::string(name)=="multi-target-ik")Check(state.value("configured",false)&&state.value("changed",false)&&state.value("cleared",false),"copyable shared-target configuration/update/clear outcomes");
  else Check(state.value("configured",false)&&state.value("partial",false)&&state.value("impulse",false)&&state.value("active",false)&&state.value("passive",false)&&state.value("returned",false),"copyable partial/impact/full/passive/animation transition outcomes");
  game.End();example.EndScripts();example.Destroy();
 }
 // This is the ordinary physical scene with its own controller, not a test-only
 // producer. Z constructs a prefab; save/load then retains the live articulation.
 Scene physical;LoadSceneFromFile((root/"Scenes/physical.judas").string(),physical,error);
 std::ofstream(root/"Assets/contact-probe.js")<<R"JS(export default class {
 constructor({entity}){this.entity=entity;this.state={contacts:0,attribution:false,other:false};}
 onCollisionEnter(event){this.state.contacts++;this.state.attribution ||= !!event.selfBody?.valid&&typeof event.selfJoint==='string';this.state.other ||= !!event.other?.valid;}
})JS";
 AssetRecord probe;Check(host.Assets().Track((root/"Assets/contact-probe.js").string(),probe,error),"ordinary articulated-owner callback probe registered");physical.Find(10)->scripts={{1,probe.id,true,"{}"}};
 physical.Find(10)->ragdoll->receiveContactEvents=true;
 RuntimeWorld realPhysical;bool built=Build(realPhysical,physical,resources,project,error);Check(built&&Prepare(realPhysical,resources,error),"physical scene ordinary startup: "+error);
 if(built){realPhysical.SetSceneControl(std::make_shared<SceneSession>(project,(root/"Scenes/physical.judas").string()));realPhysical.viewportWidth=800;realPhysical.viewportHeight=450;realPhysical.legacyGameplay=false;GameSession game;game.Begin(realPhysical,error);Step(realPhysical,game,window,resources,40);Faults(realPhysical,"partial scene public API startup");Check(realPhysical.PhysicalAnimationSnapshot(10).mode==PhysicalAnimationMode::Partial&&realPhysical.PhysicalAnimationSnapshot(11).mode==PhysicalAnimationMode::Partial,"two independent complete mappings enter partial mode");
  auto rootBody=realPhysical.RuntimeBody(realPhysical.RagdollBody(10,"Pivot"));auto armBody=realPhysical.RuntimeBody(realPhysical.RagdollBody(10,"ElbowA"));Check(!realPhysical.Physics().IsDynamicBody(rootBody)&&realPhysical.Physics().IsDynamicBody(armBody),"partial keeps explicit anchors while selected physical region is dynamic");
  Check(!WorldPersistence::CanSuspendAnimation(realPhysical,10),"active articulation is not silently demoted by streaming");Pulse(realPhysical,game,window,resources,"key:Z");Step(realPhysical,game,window,resources,10);auto controller=State(realPhysical,90);Check(controller.contains("spawned")&&controller["spawned"].size()==1,"public prefab spawn is included in lab controller state");
  auto before=realPhysical.Physics().GetTransform(armBody);Pulse(realPhysical,game,window,resources,"key:J");Step(realPhysical,game,window,resources,10);Check(glm::length(realPhysical.Physics().GetTransform(armBody).position-before.position)>.005f,"public ordinary hit reaches the actual arm body");
  Pulse(realPhysical,game,window,resources,"key:P");Step(realPhysical,game,window,resources,100);Faults(realPhysical,"ordinary owner contact callbacks safe after full release");auto contact=State(realPhysical,10);Check(contact.value("contacts",0)>0&&contact.value("attribution",false)&&contact.value("other",false),"actual M42 contact reaches articulated owner with safe selfBody/selfJoint/other");
  PhysicalAnimationRequest pending;pending.mode=PhysicalAnimationMode::Active;pending.motorHandoff=true;Check(realPhysical.RequestPhysicalAnimation(10,pending,error)&&!WorldPersistence::CanSuspendAnimation(realPhysical,10),"queued authority transition remains pinned for streaming");
  std::map<std::string,SaveChunk> physicalChunks;bool captured=WorldPersistence::Capture(realPhysical,physicalChunks,error);Check(captured,"save captures passive articulation, pending transition and spawned owner: "+error);RuntimeWorld loaded;Scene restoredScene;bool ok=captured&&WorldPersistence::SceneFrom(physicalChunks,restoredScene,error)&&Build(loaded,restoredScene,resources,project,error,true)&&Prepare(loaded,resources,error)&&WorldPersistence::Restore(loaded,physicalChunks,error);Check(ok,"partial/full-body participant cold restoration: "+error);if(ok){WorldPersistence::Published(loaded);auto restoredPhysical=loaded.PhysicalAnimationSnapshot(10);Check(restoredPhysical.mode==PhysicalAnimationMode::Passive&&restoredPhysical.pending&&restoredPhysical.pending->mode==PhysicalAnimationMode::Active&&loaded.RagdollActive(10),"restored actual authority and queued request remain distinct");loaded.Destroy();}
  auto staleArm=realPhysical.RuntimeBody(realPhysical.RagdollBody(10,"ElbowA"));auto bodyCount=realPhysical.Physics().AliveBodyCount();Check(realPhysical.DestroyHierarchy(10,error)&&!realPhysical.RagdollActive(10)&&realPhysical.Physics().AliveBodyCount()<bodyCount,"destroy removes owned bodies, constraints and physical state");Check(!realPhysical.Physics().IsDynamicBody(staleArm),"destroyed mapped generation cannot alias a surviving body");
  PhysicalAnimationRequest finish;finish.mode=PhysicalAnimationMode::Animation;finish.fade=.1f;Check(realPhysical.RequestPhysicalAnimation(11,finish,error),"remaining independent owner queues normal retirement");Step(realPhysical,game,window,resources,20);Check(WorldPersistence::CanSuspendAnimation(realPhysical,11),"completed return retires physics/pose contributor and restores stream eligibility");game.End();realPhysical.EndScripts();realPhysical.Destroy();
 }
 OwnerContactChecks(host,project,root,out);
 // EngineHost was declared before the worlds. Let normal reverse-scope RAII
 // destroy their SceneSession/SaveService users before host-owned workers.
 std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
