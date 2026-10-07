#include "PoseComposition.h"
#include "editor/EditorDocument.h"
#include "PerformanceProfiler.h"
#include "SaveArchive.h"
#include "Ragdoll.h"
#include "Material.h"
#include "PhysicalMaterial.h"
#include "SceneSerialization.h"
#include "CharacterMotor.h"
#include "UniformGravity.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "InputSystem.h"
#include "ScriptSystem.h"
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
int n=0,bad=0;void Check(bool ok,const char* s){++n;bad+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",s);}
constexpr float dt=1.f/60;
void Slope(glm::quat world){
 PhysicsWorld p;p.Init();auto ramp=glm::angleAxis(glm::radians(11.f),glm::vec3(0,0,1));auto q=world*ramp;
 p.CreateStaticBox({},q,{5,.2,3},.6,0);UniformGravity gravity(world*glm::vec3(0,-9.81,0));CharacterMotor m;
 m.Reset(q*glm::vec3(-2,1.13,0),world);for(int i=0;i<3;++i){p.Step(dt);m.Step(p,gravity,dt);}
 int supports=0;for(int i=0;i<45;++i){m.velocity=q*glm::vec3(3,0,0);p.Step(dt);m.Step(p,gravity,dt);supports+=m.result.supported;}
 printf("uphill supports=%d/45\n",supports);Check(supports>=43,"inclined tangent remains supported");
 int down=0;for(int i=0;i<45;++i){m.velocity=q*glm::vec3(-3,0,0);p.Step(dt);m.Step(p,gravity,dt);down+=m.result.supported;}Check(down>=43,"downhill tangent support follows ramp");
 m.velocity=world*glm::vec3(0,5,0);p.Step(dt);m.Step(p,gravity,dt);Check(!m.result.supported,"outward jump releases inclined support");
}
void EdgeChecks(){
 PhysicsWorld p;p.Init();auto floor=p.CreateStaticBox({},glm::vec3(1,.2,2),.6,0);UniformGravity g({0,-9.81,0});CharacterMotor m;m.Reset({0,1.1,0},{1,0,0,0});for(int i=0;i<5;++i){p.Step(dt);m.Step(p,g,dt);}for(int i=0;i<55;++i){m.velocity={3,0,0};p.Step(dt);m.Step(p,g,dt);}Check(!m.result.supported&&m.position.x>2,"walking off roof releases actual support");
 m.Reset({0,1.1,0},{1,0,0,0});for(int i=0;i<5;++i){p.Step(dt);m.Step(p,g,dt);}p.ResetBody(floor,{.02,0,0},{1,0,0,0});p.Step(dt);m.velocity={0,4,0};m.Step(p,g,dt);Check(!m.result.supported&&m.result.velocity.y>3,"moving support genuine outward departure");
 m.Reset({0,1.1,0},{1,0,0,0});for(int i=0;i<5;++i){p.Step(dt);m.Step(p,g,dt);}
 m.velocity={0,0,0};m.acceleration={0,10.01f,0};auto before=m.position;
 p.Step(dt);m.Step(p,g,dt);
 Check(!m.result.supported&&m.position.y>before.y&&m.result.velocity.y>0,
       "gentle net outward acceleration departs instead of being erased by support");
}
void PoseChecks(){
 Skeleton s;s.names={"Root","Mid","End"};s.parents={-1,0,1};s.order={0,1,2};s.rest.local.resize(3);s.rest.local[1].translation=s.rest.local[2].translation={1,0,0};
 LimbIKSettings k;k.root="Root";k.middle="Mid";k.end="End";k.target={1,1,0};k.pole={0,0,2};SkeletalPose pose;float residual=0;std::string error;
 Check(SolveLimbIK(s,s.rest,k,pose,residual,error)&&residual<1e-4,"two-bone reaches known reachable target");
 auto global=PoseGlobalMatrices(s,pose);Check(std::abs(glm::length(glm::vec3(global[1][3])-glm::vec3(global[0][3]))-1)<1e-5&&std::abs(glm::length(glm::vec3(global[2][3])-glm::vec3(global[1][3]))-1)<1e-5,"IK preserves actual segment lengths");
 k.target={4,0,0};Check(SolveLimbIK(s,s.rest,k,pose,residual,error)&&residual>1.99&&residual<2.01,"unreachable target reports actual residual without stretch");
 k.target={0,0,0};k.pole={0,0,0};Check(SolveLimbIK(s,s.rest,k,pose,residual,error)&&std::isfinite(residual),"folded/degenerate pole remains finite");
 k.weight=0;k.target={1,1,0};Check(SolveLimbIK(s,s.rest,k,pose,residual,error)&&glm::length(glm::vec3(PoseGlobalMatrices(s,pose)[2][3])-glm::vec3(2,0,0))<1e-6,"zero weight preserves incoming resolved pose");
 MaterialSlot slot;slot.overrides.uvScale=glm::vec2(4,5);slot.overrides.uvOffset=glm::vec2(.2,.3);std::vector<MaterialSlot> decoded;Check(DecodeMaterialSlots(EncodeMaterialSlots({slot}),decoded,error)&&ApplyMaterialOverride(MaterialDefinition{},decoded[0].overrides).uvScale==glm::vec2(4,5),"UV instance overrides serializer round-trip");
 PhysicalMaterial material;Check(ParsePhysicalMaterial("JudasPhysicalMaterial 1\nfriction 0.1\nrestitution 0.8\n",material,error)&&material.friction==.1f&&material.restitution==.8f,"shared physical resource uses existing coefficient limits");
 std::vector<ScriptProperty> properties;Check(ScriptSystem::ReadProperties(R"({"target":{"type":"entity","default":null}})",R"({"target":{"entity":"3"}})",properties,error)&&properties[0].text=="3","typed entity inspector schema");
 auto mapped=ScriptSystem::RemapPropertyEntities(ScriptSystem::WriteProperties(properties),{{3,8}});Check(ScriptSystem::PropertyEntities(mapped)==std::vector<SceneObjectId>{8},"authored entity references remap, numeric properties untouched");
}
void WorkflowChecks(){
 EditorDocument doc;auto& scene=doc.GetScene();auto a=scene.CreateObject("a").id,b=scene.CreateObject("b").id;scene.Find(a)->body=SceneBodyComponent{};scene.Find(b)->body=SceneBodyComponent{};scene.Find(a)->body->friction=.1;scene.Find(b)->body->friction=.9;scene.Find(b)->parent=a;
 doc.Select(a);doc.Select(b,true);std::string error;Check(doc.BatchProperties({{"body.restitution","0.3"}},error)&&scene.Find(a)->body->friction==.1f&&scene.Find(b)->body->friction==.9f,"batch edits preserve unchanged mixed fields");doc.Undo();Check(scene.Find(a)->body->restitution==.1f&&scene.Find(b)->body->restitution==.1f,"one undo restores complete batch");doc.Redo();
 Check(!doc.ReparentSelection(b,error)&&scene.Find(a)->parent==0,"cyclic batch parenting rejected without partial mutation");doc.Select(a);doc.CopyComponent("body");doc.Select(b);Check(doc.PasteComponent(error)&&scene.Find(b)->body->friction==.1f,"component copy/paste uses real property records");doc.Undo();Check(scene.Find(b)->body->friction==.9f,"component paste undo restores whole state");
 doc.Select(a);doc.Select(b,true);Check(doc.DuplicateSelection(error)&&scene.Objects().size()==4,"multi duplicate includes subtree exactly once");auto copy=doc.Selected();Check(scene.Find(copy)->parent!=a&&scene.Find(copy)->parent!=0,"duplicated hierarchy references remapped");doc.Undo();Check(scene.Objects().size()==2,"one duplicate action undo");
}
void SleepChecks(){
 PhysicsWorld w;w.Init();auto a=w.CreateDynamicBox({0,1,0},{.3,.3,.3},1,.6,0),b=w.CreateDynamicBox({0,2,0},{.3,.3,.3},1,.6,0);JointSettings fixed;fixed.bodyA=a;fixed.anchorB={0,1,0};auto root=w.CreateJoint(fixed);fixed.bodyB=b;fixed.anchorA={0,1,0};fixed.anchorB={0,0,0};auto link=w.CreateJoint(fixed);(void)root;
 for(int i=0;i<180;++i){w.ApplyLinearAcceleration(a,{0,-9.81,0},dt);w.ApplyLinearAcceleration(b,{0,-9.81,0},dt);w.Step(dt);}Check(w.IsSleeping(a)&&w.IsSleeping(b),"joint connected articulation settles coherently");w.ApplyTorque(b,{0,1,0});Check(!w.IsSleeping(a)&&!w.IsSleeping(b),"torque wakes entire articulation");w.Step(dt);
 fixed.motor=true;fixed.speed=1;w.SetJoint(link,fixed);for(int i=0;i<180;++i)w.Step(dt);Check(!w.IsSleeping(a)&&!w.IsSleeping(b),"active joint drive prevents silent sleeping");fixed.motor=false;w.SetJoint(link,fixed);for(int i=0;i<180;++i)w.Step(dt);
 w.ApplyLinearImpulse(b,{1,0,0});Check(!w.IsSleeping(a),"impulse wakes neighbour through constraint");w.DestroyBody(b);auto reused=w.CreateDynamicBox({4,1,0},{.3,.3,.3},1,.6,0);Check(!w.IsSleeping(reused)&&reused.id!=b.id,"destroy/reuse cannot alias sleep generation");
 PhysicsWorld fluid;fluid.Init();auto solid=fluid.CreateDynamicBox({0,0,0},{.5,.5,.5},1,.6,0);for(int i=0;i<60;++i)fluid.Step(dt);Check(fluid.IsSleeping(solid),"unforced still zero-gravity body sleeps");std::vector<PhysicsWorld::ContactParticle> particles={{{.5,0,0},{-2,0,0},1}};PhysicsWorld::ParticleBoundaryContact c;c.body=solid;c.point={.5,0,0};c.normal={1,0,0};c.twoWay=true;std::vector<glm::vec3> impulses;fluid.SolveParticleContacts(particles,{c},impulses);Check(!fluid.IsSleeping(solid)&&fluid.GetLinearVelocity(solid).x<0,"existing two-way fluid contact wakes and transfers momentum");
 PhysicsWorld pile;pile.Init();pile.CreateStaticBox({0,-.5,0},{5,.5,5},.6,0);auto lower=pile.CreateDynamicBox({0,.5,0},{.5,.5,.5},1,.6,0),upper=pile.CreateDynamicBox({0,1.5,0},{.5,.5,.5},1,.6,0);
 for(int i=0;i<600;++i){for(auto h:{lower,upper})pile.ApplyLinearAcceleration(h,{0,-9.81,0},dt);pile.Step(dt);}Check(pile.IsSleeping(lower)&&pile.IsSleeping(upper),"ordinary stacked contact island sleeps");pile.ApplyLinearImpulse(upper,{.5,0,0});Check(!pile.IsSleeping(lower)&&!pile.IsSleeping(upper),"cached speculative contact wakes connected resting pile");
 pile.ResetBody(lower,{0,.5,0},{1,0,0,0});pile.DestroyBody(upper);for(int i=0;i<180;++i){pile.ApplyLinearAcceleration(lower,{0,-9.81,0},dt);pile.Step(dt);}pile.ApplyLinearAcceleration(lower,{2,-9.81,0},dt);Check(!pile.IsSleeping(lower),"meaningful changed gravity wakes resting body");
 PhysicsWorld impact;impact.Init();impact.CreateStaticBox({0,-.5,0},{5,.5,5},.6,0);
 auto crate=impact.CreateDynamicBox({0,.5,0},{.5,.5,.5},1,.6,0);
 for(int i=0;i<180;++i){impact.ApplyLinearAcceleration(crate,{0,-9.81,0},dt);impact.Step(dt);}
 Check(impact.IsSleeping(crate),"impact regression starts with sleeping support");
 auto falling=impact.CreateDynamicSphere({0,1.202f,0},.2f,1,.6f,0);
 impact.ApplyLinearAcceleration(crate,{0,-9.81,0},dt);
 impact.ApplyLinearAcceleration(falling,{0,-9.81,0},dt);impact.Step(dt);
 Check(!impact.IsSleeping(crate)&&impact.LastStepStats().impactEvents>0&&
       std::isfinite(impact.GetTransform(crate).position.y),
       "new gravity-driven TOI wakes sleeper with an anchored motion ledger");
}
int main(int argc,char** argv){
 std::setvbuf(stdout,nullptr,_IOLBF,0);PoseChecks();WorkflowChecks();SleepChecks();EdgeChecks();auto out=std::filesystem::absolute(argc>1?argv[1]:"build/m65-core");std::filesystem::create_directories(out/"Assets");Slope({1,0,0,0});Slope(glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3))));
 PhysicsWorld p;p.Init();auto floor=p.CreateStaticBox({0,-.5,0},{5,.5,5},.6,0);auto b=p.CreateDynamicBox({0,.5,0},{.5,.5,.5},1,.6,0);
 for(int i=0;i<180;++i){p.ApplyLinearAcceleration(b,{0,-9.81,0},dt);p.Step(dt);}printf("sleep=%d v=%g\n",p.IsSleeping(b),glm::length(p.GetLinearVelocity(b)));Check(p.IsSleeping(b),"balanced gravity-supported crate sleeps");
 auto hit=p.Raycast({0,3,0},{0,-1,0},5);Check(hit.hit&&hit.body.id==b.id,"sleeping geometry remains queryable");
 p.ApplyLinearImpulse(b,{1,0,0});Check(!p.IsSleeping(b),"impulse wakes resting crate");p.Step(dt);Check(p.GetTransform(b).position.x>0,"woken crate physically moves");
 p.ResetBody(b,{0,.5,0},{1,0,0,0});for(int i=0;i<180;++i){p.ApplyLinearAcceleration(b,{0,-9.81,0},dt);p.Step(dt);}p.ResetBody(floor,{0,-1,0},{1,0,0,0});Check(!p.IsSleeping(b),"moving support wakes contact island");
 Scene scene;auto mid=scene.CreateObject("motor").id;scene.Find(mid)->characterMotor=CharacterMotorSettings{};scene.Find(mid)->characterMotor->gravityScale=0;
 auto sid=scene.CreateObject("sensor").id;scene.Find(sid)->body=SceneBodyComponent{};scene.Find(sid)->body->sensor=true;scene.Find(sid)->body->halfExtents={1,1,1};
 // Reruns: import refuses a file that already has metadata, so start from a fresh script asset.
 auto script=out/"Assets/checkpoint.js";std::filesystem::remove(script);std::filesystem::remove(script.string()+".judasmeta");std::ofstream(script)<<R"JS(export default class {constructor({entity}){this.entity=entity;this.state={enter:0,stay:0,exit:0};}onTriggerEnter(e){this.state.enter++;this.state.safe=e.other.valid;this.state.impulse=e.normalImpulse;}onTriggerStay(){this.state.stay++;}onTriggerExit(){this.state.exit++;}})JS";
 AssetDatabase db;db.Scan(out.string(),(out/"Assets").string());AssetRecord asset;std::string error;Check(db.Track(script.string(),asset,error),"checkpoint script registered");scene.Find(sid)->scripts.push_back({1,asset.id,true,"{}"});ResourceManager resources(nullptr,&db);RuntimeWorld world;const bool built=world.Build(scene,&resources,error);Check(built,"ordinary sensor/motor runtime");if(!built){printf("ABORT %s\nSUMMARY %d checks %d failures\n",error.c_str(),n,bad);return 1;}InputSystem input;
 auto step=[&]{input.BeginFixedStep();world.FixedScripts(&input,dt);world.Physics().Step(dt);world.UpdateCharacters(dt);world.DispatchPhysicsEvents(&input,dt);};step();step();auto t=world.RuntimeDefinition(mid)->transform;t.position={4,0,0};world.SetRuntimeTransform(mid,t);step();
 auto records=world.Scripts()->Capture();std::string state;for(auto r:records)if(r.entity==sid)state=r.json;printf("motor events %s\n",state.c_str());Check(state.find("\"enter\":1")!=std::string::npos&&state.find("\"stay\":1")!=std::string::npos&&state.find("\"exit\":1")!=std::string::npos,"motor sensor enter/stay/exit coalesced");Check(state.find("\"impulse\":null")!=std::string::npos&&state.find("\"safe\":true")!=std::string::npos,"massless motor event safe entity, absent solved impulse");Check(world.Scripts()->Diagnostics().empty(),"sensor callbacks fault-free");
 t.position={0,0,0};world.SetRuntimeTransform(mid,t);auto settings=*world.RuntimeDefinition(mid)->characterMotor;settings.collisionMask=0;world.SetCharacterSettings(mid,settings);step();records=world.Scripts()->Capture();for(auto r:records)if(r.entity==sid)state=r.json;Check(state.find("\"enter\":1")!=std::string::npos,"M39 rejected motor/sensor pair never enters");
 settings.collisionMask=kAllCategories;world.SetCharacterSettings(mid,settings);step();Check(world.SetColliderEnabled(mid,false),"generic collider disabling disables motor proxy");step();Check(!world.RuntimeCharacter(mid)->settings.enabled,"disabled proxy cannot be reenabled by motor update");world.DestroyHierarchy(mid,error);step();Check(world.Scripts()->Diagnostics().empty(),"destroyed motor retires sensor without stale callback");
 world.Destroy();
 Scene gravityScene;auto& zone=gravityScene.CreateObject("uniform override");zone.gravity=SceneGravityComponent{};zone.gravity->kind=SceneGravityKind::Uniform;zone.gravity->magnitude=4;zone.gravity->regionRadius=2;zone.transform.rotation=glm::angleAxis(glm::half_pi<float>(),glm::vec3(0,0,1));auto& radial=gravityScene.CreateObject("radial surrounding");radial.gravity=SceneGravityComponent{};radial.gravity->regionRadius=100;
 RuntimeWorld gravityWorld;Check(gravityWorld.Build(gravityScene,nullptr,error),"ordinary uniform/radial resolver world");Check(glm::length(gravityWorld.Gravity().Sample({1,0,0})-glm::vec3(4,0,0))<1e-5,"oblique uniform overriding region selected");Check(glm::length(gravityWorld.Gravity().Sample({0,10,0})-glm::vec3(0,-9.81,0))<1e-5,"radial outside override restored");Check(glm::length(gravityWorld.Gravity().Sample({1,0,0})-glm::vec3(4,0,0))<1e-5,"resolver transition back is position dependent");gravityWorld.Destroy();
 for(unsigned motors:{1u,10u,100u}){Scene workload;auto& floor=workload.CreateObject("support");floor.body=SceneBodyComponent{};floor.body->halfExtents={100,.5,100};floor.transform.position={0,-.5,0};for(unsigned i=0;i<motors;++i){auto& o=workload.CreateObject("motor");o.characterMotor=CharacterMotorSettings{};o.characterMotor->gravityScale=0;o.transform.position={float(i%10)*2,1,float(i/10)*2};auto position=o.transform.position;auto& sensor=workload.CreateObject("sensor");sensor.body=SceneBodyComponent{};sensor.body->sensor=true;sensor.body->halfExtents={.5,1,.5};sensor.transform.position=position;}RuntimeWorld bench;Check(bench.Build(workload,nullptr,error),"motor/event benchmark ordinary world");PerformanceProfiler::Get().Enable(true);PerformanceProfiler::Get().Clear();for(int i=0;i<360;++i){ProfileFrame f("M65 motor/event");ProfileFixedStep fixed(dt);bench.FixedScripts(&input,dt);bench.Physics().Step(dt);bench.UpdateCharacters(dt);bench.DispatchPhysicsEvents(&input,dt);}Check(PerformanceProfiler::Get().Export((out/("motor-"+std::to_string(motors)+".json")).string(),error),"M56 motor/event profile");bench.Destroy();PerformanceProfiler::Get().Enable(false);}
 printf("SUMMARY %d checks %d failures\n",n,bad);return bad?1:0;
}
