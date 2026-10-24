#include "PhysicalBalance.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "SaveArchive.h"
namespace {
int checks=0,failures=0;
void Check(bool result,const char* name){++checks;if(!result){++failures;std::printf("FAIL %s\n",name);}}
struct Fixture {
 PhysicsWorld world;PhysicalBalanceController controller;BalanceSettings settings;
 std::vector<BalanceBody> mapping;std::vector<glm::vec3> gravity;
 glm::quat frame;BodyHandle ground;std::string error;
 Fixture(float fallen=0,glm::quat orientation=glm::quat(1,0,0,0),bool floor=true,bool ceiling=false,int variant=0):frame(orientation){
  const bool slender=variant==1;world.Init();
  if(floor)ground=world.CreateShape(Shape::Box({5,.2f,5}),{frame*glm::vec3(0,-.2f,0),frame},false,0,.8f,0);
  if(ceiling)world.CreateShape(Shape::Box({5,.2f,5}),{frame*glm::vec3(0,.8f,0),frame},false,0,.8f,0);
  glm::vec3 positions[]={{0,1.4f,0},{-.4f,1.05f,0},{-.4f,.65f,0},{-.4f,.3f,.08f},{.4f,1.05f,0},{.4f,.65f,0},{.4f,.3f,.08f}};
  glm::vec3 sizes[]={{.25f,.25f,.15f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.3f,.3f,.3f},{.09f,.2f,.09f},{.07f,.2f,.07f},{.3f,.3f,.3f}};
  const int parents[]={-1,0,1,2,0,4,5};float masses[]={4,1,1,10,1,1,10};
  if(slender){positions[0]={0,1.42f,0};positions[1]={-.4f,.87f,0};positions[2]={-.4f,.47f,0};positions[3]={-.4f,.12f,.08f};positions[4]={.4f,.87f,0};positions[5]={.4f,.47f,0};positions[6]={.4f,.12f,.08f};sizes[0]={.25f,.4f,.15f};sizes[3]=sizes[6]={.35f,.12f,.38f};const float lightMasses[]={10,3,2,2,3,2,2};std::copy(lightMasses,lightMasses+7,masses);}
  if(variant==2){positions[0]={0,1.35f,0};positions[1]={-.34f,1.0f,0};positions[2]={-.34f,.62f,0};positions[3]={-.34f,.28f,.06f};positions[4]={.34f,1.0f,0};positions[5]={.34f,.62f,0};positions[6]={.34f,.28f,.06f};sizes[0]={.28f,.24f,.18f};sizes[1]=sizes[4]={.1f,.18f,.1f};sizes[2]=sizes[5]={.08f,.2f,.08f};sizes[3]=sizes[6]={.28f,.28f,.33f};const float differentMasses[]={3.5f,1.5f,.75f,12,1.5f,.75f,9};std::copy(differentMasses,differentMasses+7,masses);}

  auto rotation=glm::angleAxis(fallen,glm::vec3(1,0,0));float minimum=100;
  for(int i=0;i<7;i++){auto centre=rotation*(positions[i]-positions[0]);auto basis=glm::mat3_cast(rotation);for(int k=0;k<3;k++)basis[k]=glm::abs(basis[k]);minimum=std::min(minimum,centre.y-(basis*sizes[i]).y);}
  for(int i=0;i<7;i++){auto body=world.CreateShape(Shape::Box(sizes[i]),{frame*(rotation*(positions[i]-positions[0])+glm::vec3(0,-minimum+.02f,0)),frame*rotation},true,masses[i],.8f,0);mapping.push_back({body,{},parents[i],glm::quat(1,0,0,0)});}
  for(int i=1;i<7;i++){glm::vec3 point=positions[i];point.y+=(i==3||i==6)?(variant==2?.14f:.15f):(variant==2&&(i==1||i==4)?.18f:.2f);JointSettings j;j.type=(i==2||i==5)?JointType::Hinge:JointType::Ball;j.limits=true;j.lower=-2.7f;j.upper=2.7f;j.bodyA=mapping[i].body;j.bodyB=mapping[parents[i]].body;j.anchorA=point-positions[i];j.anchorB=point-positions[parents[i]];mapping[i].joint=world.CreateJoint(j);world.SetPairCollisionEnabled(j.bodyA,j.bodyB,false);}
  settings.standingHeight=variant==2?1.35f:(slender?1.42f:1.4f);if(slender)settings.touchdownTolerance=.2f;
  
  for(int f=0;f<2;f++){auto& e=settings.effectors[f];e.upper=f?4:1;e.lower=f?5:2;e.foot=f?6:3;e.contactOffset={0,-sizes[e.foot].y,0};e.halfExtents=sizes[e.foot];for(int a=0;a<3;a++)for(float sign:{-1.f,1.f}){glm::vec3 normal(0);normal[a]=sign;e.patches.push_back({normal*e.halfExtents,normal,(slender||variant==2)?.52f:.44f});}}
  gravity.assign(7,frame*glm::vec3(0,-9.81f,0));
  Check(controller.Configure(world,mapping,settings,error),"explicit free-root fixture configures");
 }
 void Advance(int count,bool control=true,bool applyGravity=true){for(int k=0;k<count;k++){if(control)controller.Prepare(world,gravity,1.f/60);if(applyGravity)for(size_t i=0;i<mapping.size();++i)world.ApplyLinearAcceleration(mapping[i].body,gravity[i],1.f/60);world.Step(1.f/60);}}
 bool Standing(){auto pose=world.GetTransform(mapping[0].body);float upright=glm::dot(pose.rotation*settings.up,frame*glm::vec3(0,1,0));float height=glm::dot(pose.position,frame*glm::vec3(0,1,0));float speed=0,mass=0;for(const auto& b:mapping){speed+=glm::length(world.GetLinearVelocity(b.body))*world.GetMass(b.body);mass+=world.GetMass(b.body);}return upright>.97f&&height>1.2f&&speed/mass<.1f;}
};
void Recovery(float fallen,glm::quat frame,int variant=0){Fixture f(fallen,frame,true,false,variant);f.Advance(120,false);Check(f.controller.Request(f.world,BalanceTask::Recover,false,true,f.error),"recovery request accepted");f.Advance(1080);if(variant==2){auto p=f.world.GetTransform(f.mapping[0].body);auto& o=f.controller.Observation();std::printf("Second rig recovery: root %.4f up %.4f speed %.4f contacts %u/%u, reason %s\n",p.position.y,(p.rotation*glm::vec3(0,1,0)).y,o.speed,o.supports[0].contacts,o.supports[1].contacts,f.controller.History().reason.c_str());}Check(f.Standing(),"actual dynamic root and mass reach physical standing");Check(f.controller.History().progress==BalanceProgress::Succeeded,"success requires sustained measured physical standing");Check(f.world.IsDynamicBody(f.mapping[0].body),"physical root remains dynamic");auto& observation=f.controller.Observation();Check(observation.supports[0].contacts&&observation.supports[1].contacts,"both declared effectors have actual solver support");}
void CorrectiveStep(){
 Fixture denied(0,glm::quat(1,0,0,0),true,false,true),allowed(0,glm::quat(1,0,0,0),true,false,true);
 for(auto* f:{&denied,&allowed}){f->Advance(120,false);f->controller.Request(f->world,BalanceTask::Stand,f==&allowed,true,f->error);f->Advance(360);Check(f->Standing(),"equivalent stepping trial starts physically settled");f->world.ApplyLinearImpulse(f->mapping[0].body,{0,0,29});}
 glm::vec3 initial=allowed.world.GetTransform(allowed.mapping[3].body).position;float clearance=0,slip=0;bool airborne=false,choiceObserved=false,selectionVerified=false;
 for(int k=0;k<720;k++){denied.Advance(1);allowed.Advance(1);auto pose=allowed.world.GetTransform(allowed.mapping[3].body);clearance=std::max(clearance,pose.position.y-initial.y);auto& observation=allowed.controller.Observation();if(!choiceObserved&&allowed.controller.History().swing>=0){choiceObserved=true;auto a=observation.supports[0].load,b=observation.supports[1].load;selectionVerified=allowed.controller.History().swing==(b+.001f*(a+b)<a?1:0);std::printf("First planned foot %d, actual loads %.9g / %.9g N s\n",allowed.controller.History().swing,a,b);}if(clearance>.025f&&!observation.supports[0].contacts)airborne=true;slip=std::max(slip,glm::length(allowed.world.GetTransform(allowed.mapping[6].body).position-glm::vec3(.4f,.12f,.08f)));}
 Check(choiceObserved&&selectionVerified,"actual first step uses lighter support or stable authored order within the load tie band");Check(!denied.Standing(),"same real perturbation defeats in-place balance");Check(allowed.Standing(),"actual step extends measured physical recovery envelope");Check(airborne&&clearance>.025f,"selected foot lifts physically and loses actual supporting contact");Check(allowed.controller.History().landings>=1,"touchdown requires returned solver contact");Check(allowed.controller.History().progress==BalanceProgress::Succeeded,"post-step physical standing settles before success");Check(glm::length(allowed.world.GetTransform(allowed.mapping[3].body).position-initial)>.15f,"foot physically changes its placement");Check(allowed.controller.History().referenceTime==allowed.settings.referenceSeconds,"disturbance does not replay initial collapsed reference fade");
 std::printf("Step comparison: lift %.4f m, stance travel %.4f m, landings %u, denied root %.4f m, allowed root %.4f m\n",clearance,slip,allowed.controller.History().landings,denied.world.GetTransform(denied.mapping[0].body).position.y,allowed.world.GetTransform(allowed.mapping[0].body).position.y);
}
void Negatives(){
 Fixture off(1.5707963f);off.Advance(120,false);off.controller.Request(off.world,BalanceTask::Recover,false,false,off.error);off.Advance(1080);Check(!off.Standing(),"effort-disabled rig remains fallen");Check(off.controller.History().progress!=BalanceProgress::Succeeded,"effort-off reference deadline cannot report standing");
 Fixture blocked(1.5707963f,glm::quat(1,0,0,0),true,true);blocked.Advance(120,false);blocked.controller.Request(blocked.world,BalanceTask::Recover,false,true,blocked.error);blocked.Advance(1080);Check(!blocked.Standing(),"real low ceiling blocks physical mass elevation");Check(blocked.controller.History().progress!=BalanceProgress::Succeeded,"blocked recovery does not complete on a reference timer");
 Fixture zero;zero.controller.Request(zero.world,BalanceTask::Stand,false,true,zero.error);zero.gravity.assign(7,glm::vec3(0));zero.Advance(1);Check(zero.controller.History().progress==BalanceProgress::Failed&&!zero.controller.Observation().available,"zero gravity refuses finite standing task");
 Fixture stale;stale.controller.Request(stale.world,BalanceTask::Stand,false,true,stale.error);auto old=stale.mapping[3].body;stale.world.DestroyBody(old);auto replacement=stale.world.CreateDynamicBox({0,5,0},{.2f,.2f,.2f},1,.8f,0);stale.Advance(1);Check(stale.controller.History().progress==BalanceProgress::Failed,"destroyed mapped body fails safely");Check(glm::length(stale.world.GetLinearVelocity(replacement))<1e-6f,"replacement slot receives no stale drive or gravity");
 Fixture validation;auto bad=validation.settings;bad.up={0,0,0};Check(!validation.controller.Configure(validation.world,validation.mapping,bad,validation.error),"invalid root axes reject atomically");Check(validation.controller.Settings().up==validation.settings.up,"failed configuration preserves existing settings");validation.controller.Request(validation.world,BalanceTask::Stand,false,true,validation.error);Check(!validation.controller.Configure(validation.world,validation.mapping,validation.settings,validation.error),"active joint ownership cannot be reconfigured");validation.controller.Cancel(validation.world);bool disabled=true;for(size_t i=1;i<validation.mapping.size();++i){JointState state;validation.world.GetJoint(validation.mapping[i].joint,state);disabled&=!state.angularDrive.enabled;}Check(disabled,"cancellation releases every owned drive");
}
void Supports(){
 Fixture moving;moving.world.SetMotionType(moving.ground,BodyMotionType::Kinematic);moving.Advance(120,false);moving.controller.Request(moving.world,BalanceTask::Stand,false,true,moving.error);moving.Advance(360);
 auto initial=moving.world.GetTransform(moving.mapping[0].body).position;
 moving.world.SetKinematicVelocity(moving.ground,{.025f,0,0},{0,.025f,0});moving.Advance(720);
 const auto& mo=moving.controller.Observation();auto root=moving.world.GetTransform(moving.mapping[0].body).position;
 std::printf("Moving support: root travel %.5f m, relative speed %.5f m/s, height %.5f m, supports %u/%u\n",glm::length(root-initial),mo.speed,mo.height,mo.supports[0].contacts,mo.supports[1].contacts);
 Check(moving.Standing()&&mo.supports[0].body.id==moving.ground.id&&mo.supports[1].body.id==moving.ground.id,"ordinary translating and rotating support carries free dynamic rig through contacts");
 Check(glm::length(root-initial)>.1f&&mo.speed<.16f,"moving support produces actual displacement with bounded relative motion");
 Check(glm::length(mo.supports[0].velocity-moving.world.GetPointVelocity(moving.ground,mo.supports[0].point))<.002f,"observed support point velocity includes real angular motion");
 moving.world.DestroyBody(moving.ground);moving.Advance(930);Check(moving.controller.History().progress==BalanceProgress::Failed&&!moving.Standing(),"removed support cannot remain standing indefinitely through a cached reference");
 Fixture slope;slope.world.ResetBody(slope.ground,{0,-.2f,0},glm::angleAxis(.05235988f,glm::vec3(1,0,0)));slope.Advance(120,false);slope.controller.Request(slope.world,BalanceTask::Stand,false,true,slope.error);slope.Advance(1080);
 std::printf("Slope: height %.5f up %.5f speed %.5f contacts %u/%u\n",slope.controller.Observation().height,slope.controller.Observation().upright,slope.controller.Observation().speed,slope.controller.Observation().supports[0].contacts,slope.controller.Observation().supports[1].contacts);
 Check(slope.Standing()&&slope.controller.History().progress==BalanceProgress::Succeeded,"modest three-degree slope settles under distinct gravity and contact normal");
 Fixture radial;radial.settings.plantedOrientationWeight=1;radial.controller.Configure(radial.world,radial.mapping,radial.settings,radial.error);radial.world.DestroyBody(radial.ground);radial.ground=radial.world.CreateShape(Shape::Sphere(100),{{0,-100,0},{1,0,0,0}},false,0,.8f,0);
 radial.controller.Request(radial.world,BalanceTask::Stand,false,true,radial.error);
 for(int k=0;k<1200;k++){for(size_t i=0;i<7;i++)radial.gravity[i]=-glm::normalize(radial.world.GetCenterOfMass(radial.mapping[i].body)-glm::vec3(0,-100,0))*9.81f;radial.Advance(1);}
 std::printf("Radial: height %.5f up %.5f speed %.5f contacts %u/%u\n",radial.controller.Observation().height,radial.controller.Observation().upright,radial.controller.Observation().speed,radial.controller.Observation().supports[0].contacts,radial.controller.Observation().supports[1].contacts);
 std::printf("Radial status %d settled %.5f root speed %.5f\n",int(radial.controller.History().progress),radial.controller.History().settled,glm::length(radial.world.GetLinearVelocity(radial.mapping[0].body)));
 Check(radial.Standing()&&radial.controller.History().progress==BalanceProgress::Succeeded,"curved ground and independently sampled local radial accelerations physically support rig");
}
void History(){
 Fixture f(1.5707963f);f.Advance(120,false);f.controller.Request(f.world,BalanceTask::Recover,false,true,f.error);f.Advance(80);
 auto history=f.controller.History();auto root=f.world.GetTransform(f.mapping[0].body);auto velocity=f.world.GetLinearVelocity(f.mapping[0].body);
 SaveArchive archive;auto toId=[&](BodyHandle b){return b.id==f.ground.id?uint64_t(1):uint64_t(0);};auto fromId=[&](uint64_t id){return id==1?f.ground:BodyHandle{};};f.controller.PersistHistory(archive,toId,fromId);f.controller.Cancel(f.world);
 PhysicalBalanceController restored;Check(restored.Configure(f.world,f.mapping,f.settings,f.error),"fresh native controller rebuilds safe handle mapping");SaveArchive input(archive.bytes);restored.PersistHistory(input,toId,fromId);input.Finish();
 Check(restored.History().elapsed==history.elapsed&&restored.History().referenceTime==history.referenceTime&&restored.History().initialTargets==history.initialTargets,"in-progress recovery retains semantic clocks and reference poses");
 restored.Prepare(f.world,f.gravity,1.f/60);Check(!restored.Observation().available&&restored.History().elapsed==history.elapsed,"first restored sample is explicitly unavailable and advances no task clock");
 Check(f.world.GetTransform(f.mapping[0].body).position==root.position&&f.world.GetLinearVelocity(f.mapping[0].body)==velocity,"history restoration leaves real pose and velocity unchanged");
 for(int k=0;k<1080;k++){restored.Prepare(f.world,f.gravity,1.f/60);f.Advance(1,false);}
 Check(f.Standing()&&restored.History().progress==BalanceProgress::Succeeded,"restored in-progress physical recovery continues to actual standing");
}
void DisturbancesAndLosses(){
 for(auto direction:{glm::vec3(1,0,0),glm::vec3(-1,0,0),glm::vec3(0,0,1),glm::vec3(0,0,-1)}){
  Fixture f;f.Advance(120,false);f.controller.Request(f.world,BalanceTask::Stand,false,true,f.error);f.Advance(360);f.world.ApplyLinearImpulse(f.mapping[0].body,direction*2.f);float peak=0;for(int k=0;k<1080;k++){f.Advance(1);peak=std::max(peak,glm::length(f.world.GetLinearVelocity(f.mapping[0].body)));}
  Check(peak>.01f&&f.Standing()&&f.controller.History().landings==0,"bounded real disturbance settles on feet without corrective landing");
 }
 Fixture ice(0,glm::quat(1,0,0,0),true,false,1);ice.world.SetPhysicalMaterial(ice.ground,"",0,0);for(auto b:ice.mapping)ice.world.SetPhysicalMaterial(b.body,"",0,0);ice.Advance(120,false);ice.controller.Request(ice.world,BalanceTask::Stand,true,true,ice.error);ice.Advance(360);ice.world.ApplyLinearImpulse(ice.mapping[0].body,{0,0,29});ice.Advance(1080);Check(!ice.Standing()&&ice.controller.History().progress!=BalanceProgress::Succeeded,"inadequate authored grip cannot silently obtain external braking");
 Fixture shortStep(0,glm::quat(1,0,0,0),true,false,1);shortStep.settings.stepReach=.01f;shortStep.controller.Configure(shortStep.world,shortStep.mapping,shortStep.settings,shortStep.error);shortStep.Advance(120,false);shortStep.controller.Request(shortStep.world,BalanceTask::Stand,true,true,shortStep.error);shortStep.Advance(360);shortStep.world.ApplyLinearImpulse(shortStep.mapping[0].body,{0,0,29});shortStep.Advance(1080);Check(!shortStep.Standing()&&shortStep.controller.History().progress!=BalanceProgress::Succeeded,"insufficient authored foothold reach cannot claim recovered standing");
 Fixture filtered(0,glm::quat(1,0,0,0),true,false,1);filtered.settings.query.excludeLayers=1;filtered.controller.Configure(filtered.world,filtered.mapping,filtered.settings,filtered.error);filtered.Advance(120,false);filtered.controller.Request(filtered.world,BalanceTask::Stand,true,true,filtered.error);filtered.Advance(360);filtered.world.ApplyLinearImpulse(filtered.mapping[0].body,{0,0,29});filtered.Advance(1080);Check(filtered.controller.History().landings==0&&!filtered.Standing(),"normal M39 planning filter excludes an otherwise reachable ground body");
 Fixture uneven;uneven.world.CreateShape(Shape::Box({.5f,.01f,.6f}),{{-.4f,.01f,0},{1,0,0,0}},false,0,.8f,0);uneven.Advance(120,false);uneven.controller.Request(uneven.world,BalanceTask::Stand,false,true,uneven.error);uneven.Advance(1080);Check(uneven.Standing(),"two-centimetre uneven static support settles through normal contacts");
 Fixture dynamic;dynamic.world.SetMotionType(dynamic.ground,BodyMotionType::Dynamic,100);dynamic.world.CreateShape(Shape::Box({5,.2f,5}),{{0,-.6f,0},{1,0,0,0}},false,0,0,0);dynamic.Advance(120,false);dynamic.controller.Request(dynamic.world,BalanceTask::Stand,false,true,dynamic.error);dynamic.Advance(360);dynamic.world.ApplyLinearImpulse(dynamic.mapping[0].body,{2,0,0});float reaction=0;for(int k=0;k<120;k++){dynamic.Advance(1);reaction=std::max(reaction,glm::length(dynamic.world.GetLinearVelocity(dynamic.ground)));}std::printf("Dynamic support over frictionless bed: peak %.9g m/s\n",reaction);Check(dynamic.world.IsDynamicBody(dynamic.ground)&&reaction>.001f,"finite-mass support receives real contact reaction instead of becoming an anchor");
 Fixture interrupted(1.5707963f);interrupted.Advance(120,false);interrupted.controller.Request(interrupted.world,BalanceTask::Recover,false,true,interrupted.error);interrupted.Advance(60);auto p=interrupted.world.GetTransform(interrupted.mapping[0].body);auto v=interrupted.world.GetLinearVelocity(interrupted.mapping[0].body);interrupted.controller.Cancel(interrupted.world);Check(interrupted.world.GetTransform(interrupted.mapping[0].body).position==p.position&&interrupted.world.GetLinearVelocity(interrupted.mapping[0].body)==v,"interrupting get-up releases effort without physical pose or velocity repair");
 Fixture conflicting;conflicting.controller.Request(conflicting.world,BalanceTask::Stand,false,true,conflicting.error);conflicting.gravity[3]=-conflicting.gravity[3];conflicting.Advance(1);Check(conflicting.controller.History().progress==BalanceProgress::Failed,"conflicting per-body gravity refuses a fabricated whole-actor down vector");
}
void InternalEffort(){
 for(bool effort:{false,true}){
  Fixture f(1.2f,glm::quat(1,0,0,0),false);for(const auto& b:f.mapping){auto t=f.world.GetTransform(b.body);f.world.ResetBody(b.body,t.position+glm::vec3(0,4,0),t.rotation);}
  f.world.ApplyTorque(f.mapping[0].body,{0,0,10});f.world.ApplyTorque(f.mapping[1].body,{0,0,-10});
  f.controller.Request(f.world,BalanceTask::Stand,false,effort,f.error);float peak=0;
  for(int k=0;k<240;k++){f.controller.Prepare(f.world,f.gravity,1.f/60);f.world.Step(1.f/60);for(const auto& b:f.mapping)peak=std::max(peak,glm::length(f.world.GetAngularVelocity(b.body)));if(k==0||k==29||k==239){
   glm::vec3 linear(0),angular(0);for(const auto& b:f.mapping){auto com=f.world.GetCenterOfMass(b.body);auto momentum=f.world.GetLinearVelocity(b.body)*f.world.GetMass(b.body);linear+=momentum;angular+=f.world.GetInertiaWorld(b.body)*f.world.GetAngularVelocity(b.body)+glm::cross(com,momentum);}
   std::printf("Free-flight effort=%d interval=%d residual: linear %.9g kg m/s, angular %.9g kg m2/s, peak %.9g rad/s\n",effort,k+1,glm::length(linear),glm::length(angular),peak);
   Check(glm::length(linear)<.001f,"seven-body free-flight actuation has no external linear impulse");
  }}
  Check(peak>.01f,"free-flight momentum audit exercises real articulated angular motion");
  Check(f.controller.History().progress!=BalanceProgress::Succeeded,"internal actuation without external ground cannot claim standing");
 }
}
}
// Authored get-up keys round-trip, default keys are not written (existing
// content keeps its bytes) and out-of-envelope keys are rejected.
void GetUpAuthoring(){
    BalanceDefinition d;d.chains={{{"lu","ll","lf"},{"ru","rl","rf"}}};
    for(auto& e:d.settings.effectors)e.patches={BalanceContactPatch{}};
    std::string error;Check(ValidBalanceDefinition(d,error),"default balance definition with get-up defaults is valid");
    Check(SerializeBalanceDefinition(d).find("getUp")==std::string::npos,"default get-up settings are not serialized");
    d.settings.getUpFaceDown[1]={.5f,120,-90,30};d.settings.getUpPatience=4;d.settings.getUpAttempts=2;
    BalanceDefinition back;Check(ParseBalanceDefinition(SerializeBalanceDefinition(d),back,error),"authored get-up settings parse");
    Check(back.settings.getUpFaceDown[1].hip==120&&back.settings.getUpFaceDown[1].seconds==.5f&&back.settings.getUpPatience==4&&back.settings.getUpAttempts==2&&back.settings.getUpFaceUp[0].hip==d.settings.getUpFaceUp[0].hip,"authored get-up settings round-trip");
    auto bad=d;bad.settings.getUpFaceUp[2].knee=200;Check(!ValidBalanceDefinition(bad,error),"get-up joint angle beyond 180 degrees rejected");
    bad=d;bad.settings.getUpFaceUp[0].seconds=0;Check(!ValidBalanceDefinition(bad,error),"zero-length get-up key rejected");
    bad=d;bad.settings.getUpAttempts=0;Check(!ValidBalanceDefinition(bad,error),"zero get-up attempts rejected");
    Check(!ParseBalanceDefinition(R"({"getUpFaceUp":[[1,0,0,0]],"chains":[{},{}]})",back,error),"short get-up key list rejected");
}
int main(){Recovery(1.5707963f,glm::quat(1,0,0,0));Recovery(-1.5707963f,glm::quat(1,0,0,0));Recovery(1.4f,glm::quat(1,0,0,0));Recovery(-1.4f,glm::quat(1,0,0,0));Recovery(1.5707963f,glm::angleAxis(.9f,glm::normalize(glm::vec3(1,2,3))));Recovery(1.5707963f,glm::quat(1,0,0,0),2);Recovery(-1.5707963f,glm::quat(1,0,0,0),2);CorrectiveStep();Negatives();Supports();History();DisturbancesAndLosses();InternalEffort();GetUpAuthoring();std::printf("Physical balance controller development: %d checks, %d failures\n",checks,failures);return failures?1:0;}
