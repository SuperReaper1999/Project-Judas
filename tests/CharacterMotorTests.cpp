#include "CharacterMotor.h"
#include "UniformGravity.h"
#include "RadicalGravity.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "InputSystem.h"
#include "Prefab.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
bool Near(glm::vec3 a,glm::vec3 b,float eps=.02f){return glm::length(a-b)<eps;}
void Tick(CharacterMotor& m,PhysicsWorld& p,const GravityField& g,int n){for(int i=0;i<n;++i){p.Step(1.f/60);m.Step(p,g,1.f/60);}}

namespace {
void RegisterObserver(CharacterMotor& motor,PhysicsWorld& physics){
 motor.observationBody=physics.CreateQueryCapsule(motor.settings.radius,motor.settings.halfHeight,
     {motor.position+motor.orientation*motor.settings.offset,motor.orientation});
 physics.SetCollisionFilter(motor.observationBody,motor.settings.collisionLayer,motor.settings.collisionMask);
 motor.filter.ignoredBodies.push_back(motor.observationBody);
}
void QueryTick(CharacterMotor& motor,PhysicsWorld& physics,const GravityField& gravity,bool pre=false){
 constexpr float dt=1.f/60;
 if(pre)physics.BeginPreStepQueryContacts();else physics.Step(dt);
 motor.Step(physics,gravity,dt);
 physics.ResetBody(motor.observationBody,motor.position+motor.orientation*motor.settings.offset,motor.orientation);
 if(pre)physics.Step(dt);
 physics.FinishQueryTouches();
}
unsigned SensorEvents(const PhysicsWorld& world,BodyHandle sensor,BodyHandle observer){
 unsigned count=0;for(const auto& event:world.LastStepTouchEvents())
  if(event.sensor&&((event.a.id==sensor.id&&event.b.id==observer.id)||
                    (event.b.id==sensor.id&&event.a.id==observer.id)))++count;
 return count;
}
void PrintSensorPairs(const PhysicsWorld& world,BodyHandle sensor,BodyHandle observer,const char* label){
 for(const auto& event:world.LastStepTouchEvents())if(event.sensor&&(event.a.id==sensor.id||event.b.id==sensor.id))
  std::printf("QUERY_PAIR %s sensor=%u observer=%u a=%u b=%u phase=%u point=(%.9g,%.9g,%.9g)\n",
              label,sensor.id,observer.id,event.a.id,event.b.id,unsigned(event.phase),event.point.x,event.point.y,event.point.z);
}
BodyHandle Sensor(PhysicsWorld& world,glm::vec3 center,glm::vec3 half){
 auto body=world.CreateStaticBox(center,half,0,0);world.SetBodySensor(body,true);return body;
}
void QuerySensorPaths(){
 UniformGravity zero({0,0,0}),gravity({0,-9.81f,0});
 for(bool pre:{false,true})for(bool shift:{false,true}){
  PhysicsWorld world;world.Init();const float origin=shift?5000.f:0.f;
  auto a=Sensor(world,{origin+.5f,1,0},{.002f,1,1});
  auto b=Sensor(world,{origin+1.3f,1,0},{.002f,1,1});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({origin,1,0},{1,0,0,0});RegisterObserver(motor,world);
  if(shift){world.ShiftOrigin({5000,0,0});motor.ShiftOrigin({5000,0,0});}
  motor.velocity={120,0,0};QueryTick(motor,world,zero,pre);
  Check(SensorEvents(world,a,motor.observationBody)==2&&SensorEvents(world,b,motor.observationBody)==2,
        "accepted motor dash crosses every thin static sensor, including prephysics/origin-shift publication");
  bool meaningful=true;for(const auto& event:world.LastStepTouchEvents())if(event.sensor)
    meaningful&=event.point.x>.39f&&event.point.x<1.41f&&
                std::abs(event.relativeVelocity.x)>119.f&&!event.impulseAvailable&&event.normalImpulse==0;
  Check(meaningful&&Near(motor.position,{2,1,0},.001f),
        "accepted query path keeps real impact points/velocity without impulses or motion truncation");
  QueryTick(motor,world,zero,pre);
  Check(SensorEvents(world,a,motor.observationBody)==0&&SensorEvents(world,b,motor.observationBody)==0,"query path crossing does not leave a phantom overlap");
 }
 {
  PhysicsWorld world;world.Init();world.CreateStaticBox({.5f,1,1},{.05f,2,2},0,0);
  auto chord=Sensor(world,{.165f,1,1},{.005f,.2f,.005f});
  auto slide=Sensor(world,{.33f,1,1.5f},{.005f,.2f,.005f});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({0,1,0},{1,0,0,0});RegisterObserver(motor,world);motor.velocity={120,0,120};
  QueryTick(motor,world,zero);
  Check(motor.result.collided&&motor.position.x<.36f&&motor.position.z>1.9f&&
        SensorEvents(world,chord,motor.observationBody)==0&&SensorEvents(world,slide,motor.observationBody)==2,
        "wall-slide records its accepted legs and never a start-to-final chord through another sensor");
 }
 for(bool ceiling:{false,true}){
  PhysicsWorld world;world.Init();world.CreateStaticBox({0,-.5f,0},{10,.5f,10},.6f,0);
  const auto obstacle=world.CreateStaticBox({1,.2f,0},{.5f,.2f,2},.6f,0);
  if(ceiling)world.CreateStaticBox({0,2.1f,0},{2,.2f,2},.6f,0);
  auto apex=Sensor(world,ceiling?glm::vec3(0,1.88f,0):glm::vec3(.15f,2.32f,0),{.01f,.005f,.1f});
  CharacterMotor motor;motor.Reset({0,.92f,0},{1,0,0,0});RegisterObserver(motor,world);
  QueryTick(motor,world,gravity);
  BodyHandle startOverlap;
  if(!ceiling){
    // Create after settling/publication: this first real overlap has no prior
    // touch owner. A successful step must observe it before the capsule leaves.
    startOverlap=Sensor(world,{0,.025f,0},{.01f,.002f,.01f});
    auto query=motor.filter;query.includeSensors=true;
    const auto center=motor.position+motor.orientation*motor.settings.offset;
    const auto overlaps=[&](glm::vec3 p){
      bool truncated=false,found=false;
      const auto bodies=world.OverlapShape(Shape::Capsule(motor.settings.radius,motor.settings.halfHeight),
          {p,motor.orientation},query,32,truncated);
      for(const auto body:bodies)found|=body.id==startOverlap.id;
      return !truncated&&found;
    };
    Check(overlaps(center)&&!overlaps(center+motor.result.up*motor.settings.skin),
          "step-origin sensor overlaps the actual capsule but not its lifted preparation probe");
  }
  motor.velocity={12,0,0};QueryTick(motor,world,gravity);
  std::printf("QUERY_STEP ceiling=%d pos=(%.9g,%.9g,%.9g) stepped=%d supported=%d recovered=%d events=%u\n",ceiling,motor.position.x,motor.position.y,motor.position.z,motor.result.stepped,motor.result.supported,motor.result.recovered,SensorEvents(world,apex,motor.observationBody));
  PrintSensorPairs(world,apex,motor.observationBody,"step");
  if(ceiling)Check(!motor.result.stepped&&SensorEvents(world,apex,motor.observationBody)==0,
                   "a rejected step probe does not publish sensor travel");
  else {
    // The capsule can land on the step's front corner before its center is
    // above the flat top. Check actual capsule/box clearance and support, not
    // a height that assumes the capsule has already crossed the entire edge.
    const auto coreBottom=motor.position-glm::vec3(0,motor.settings.halfHeight,0);
    const auto nearest=glm::clamp(coreBottom,glm::vec3(.5f,0,-2),glm::vec3(1.5f,.4f,2));
    const auto gap=glm::length(coreBottom-nearest)-motor.settings.radius;
    Check(motor.result.stepped&&motor.result.supported&&motor.result.support.id==obstacle.id&&
          gap>-.002f&&gap<motor.settings.skin+.005f&&SensorEvents(world,apex,motor.observationBody)==2,
          "accepted step has real nonpenetrating support and publishes its up/forward/down path");
    Check(SensorEvents(world,startOverlap,motor.observationBody)==2,
          "accepted step publishes a real initial sensor overlap omitted by the lifted probe origin");
  }
 }
 for(bool filterActor:{false,true}){
  PhysicsWorld world;world.Init();auto filtered=Sensor(world,{.5f,1,0},{.002f,1,1});
  if(!filterActor)world.SetCollisionFilter(filtered,1,0);
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  if(filterActor)motor.settings.collisionMask=0;
  motor.Reset({0,1,0},{1,0,0,0});RegisterObserver(motor,world);motor.velocity={120,0,0};
  QueryTick(motor,world,zero);
  Check(SensorEvents(world,filtered,motor.observationBody)==0,"accepted query sensor sweeps respect bilateral collision masks");
 }

 {
  PhysicsWorld world;world.Init();auto gate=Sensor(world,{.5f,1,0},{.3f,1,1});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({0,1,0},{1,0,0,0});RegisterObserver(motor,world);motor.velocity={36,0,0};
  QueryTick(motor,world,zero);
  Check(SensorEvents(world,gate,motor.observationBody)==1&&world.LastStepTouchEvents()[0].phase==PhysicsWorld::TouchPhase::Enter&&
        std::abs(world.LastStepTouchEvents()[0].relativeVelocity.x)>35.f,
        "query traversal ending inside a sensor publishes persistent enter with its actual entry velocity");
  motor.velocity={0,0,0};QueryTick(motor,world,zero);
  Check(SensorEvents(world,gate,motor.observationBody)==1&&world.LastStepTouchEvents()[0].phase==PhysicsWorld::TouchPhase::Stay,
        "query endpoint overlap retains normal stay ownership");
  motor.velocity={120,0,0};QueryTick(motor,world,zero);
  Check(SensorEvents(world,gate,motor.observationBody)==1&&world.LastStepTouchEvents()[0].phase==PhysicsWorld::TouchPhase::Exit,
        "leaving a persistent query overlap publishes only its exit");
 }
 {
  PhysicsWorld world;world.Init();auto gate=Sensor(world,{1001,1,0},{.002f,1,1});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({1000,1,0},{1,0,0,0});RegisterObserver(motor,world);motor.velocity={120,0,0};
  world.Step(1.f/60);motor.Step(world,zero,1.f/60);
  world.ShiftOrigin({1000,0,0});motor.ShiftOrigin({1000,0,0});
  world.ResetBody(motor.observationBody,motor.position,motor.orientation);world.FinishQueryTouches();
  bool translated=SensorEvents(world,gate,motor.observationBody)==2;
  for(const auto& event:world.LastStepTouchEvents())if(event.sensor)translated&=event.point.x>.99f&&event.point.x<1.01f;
  Check(translated,"pending accepted query events translate with a completed-boundary origin shift");
 }
 {
  PhysicsWorld world;world.Init();auto gate=Sensor(world,{1,1,0},{.002f,1,1});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({0,1,0},{1,0,0,0});RegisterObserver(motor,world);
  motor.Reset({2,1,0},{1,0,0,0});
  world.ResetBody(motor.observationBody,motor.position,motor.orientation);
  QueryTick(motor,world,zero);
  Check(SensorEvents(world,gate,motor.observationBody)==0,"a controller teleport is not inferred as a sensor sweep");
 }

 {
  PhysicsWorld world;world.Init();world.CreateStaticBox({0,1,0},{1,2,2},0,0);
  auto gate=Sensor(world,{.6f,1,0},{.002f,1,1});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({.1f,1,0},{1,0,0,0});RegisterObserver(motor,world);
  QueryTick(motor,world,zero);
  std::printf("QUERY_RECOVER pos=(%.9g,%.9g,%.9g) recovered=%d supported=%d events=%u\n",motor.position.x,motor.position.y,motor.position.z,motor.result.recovered,motor.result.supported,SensorEvents(world,gate,motor.observationBody));
  PrintSensorPairs(world,gate,motor.observationBody,"recovery");
  Check(motor.result.recovered&&motor.position.x>1&&SensorEvents(world,gate,motor.observationBody)==0,
        "depenetration recovery does not invent travel through an intermediate sensor");
 }
 {
  PhysicsWorld world;world.Init();auto platform=world.CreateStaticBox({0,-.25f,0},{1,.25f,1},.6f,0);
  world.SetMotionType(platform,BodyMotionType::Kinematic);
  auto gate=Sensor(world,{1,1,0},{.002f,1,1});
  CharacterMotor motor;motor.Reset({0,.92f,0},{1,0,0,0});RegisterObserver(motor,world);
  QueryTick(motor,world,zero);world.SetKinematicVelocity(platform,{120,0,0},{0,0,0});
  QueryTick(motor,world,zero);
  const auto pp=world.GetTransform(platform).position,previous=world.GetPreviousTransform(platform).position;
  std::printf("QUERY_CARRY pos=(%.9g,%.9g,%.9g) supported=%d recovered=%d events=%u platform=(%.9g,%.9g,%.9g) previous=(%.9g,%.9g,%.9g) velocity=(%.9g,%.9g,%.9g)\n",motor.position.x,motor.position.y,motor.position.z,motor.result.supported,motor.result.recovered,SensorEvents(world,gate,motor.observationBody),pp.x,pp.y,pp.z,previous.x,previous.y,previous.z,motor.velocity.x,motor.velocity.y,motor.velocity.z);
  PrintSensorPairs(world,gate,motor.observationBody,"carry");
  Check(motor.result.supported&&motor.position.x>1.9f&&SensorEvents(world,gate,motor.observationBody)==2,
        "accepted support-carry segments publish static sensor crossings");
 }
 // Pending observations are generation-checked snapshots. A later target
 // move keeps a real historical crossing; removal/disable/mask changes retire
 // it. Moving a target before the accepted path must never use its old bound.
 for(int mutation=0;mutation<7;++mutation){
  PhysicsWorld world;world.Init();auto gate=Sensor(world,{1,1,0},{.002f,1,1});
  CharacterMotor motor;motor.settings.radius=.1f;motor.settings.halfHeight=0;
  motor.Reset({0,1,0},{1,0,0,0});RegisterObserver(motor,world);motor.velocity={120,0,0};
  world.Step(1.f/60);
  if(mutation==0)world.ResetBody(gate,{10,1,0},{1,0,0,0});
  motor.Step(world,zero,1.f/60);
  world.ResetBody(motor.observationBody,motor.position,motor.orientation);
  if(mutation==1)world.ResetBody(gate,{10,1,0},{1,0,0,0});
  if(mutation==2)world.SetBodyEnabled(gate,false);
  if(mutation==3)world.SetCollisionFilter(gate,0,0);
  if(mutation==4){world.DestroyBody(gate);auto replacement=Sensor(world,{10,1,0},{.002f,1,1});
    Check(replacement.id!=gate.id,"sensor slot reuse has a different generation");}
  if(mutation==5){world.DestroyBody(motor.observationBody);
    auto replacement=world.CreateQueryCapsule(.1f,0,{{10,1,0},{1,0,0,0}});
    Check(replacement.id!=motor.observationBody.id,"query observer slot reuse has a different generation");}
  if(mutation==6)world.SetCollisionFilter(motor.observationBody,0,0);
  world.FinishQueryTouches();
  const unsigned expected=mutation==1?2:0;
  std::printf("QUERY_LIFETIME mutation=%d events=%u expected=%u\n",mutation,
              SensorEvents(world,gate,motor.observationBody),expected);
  Check(SensorEvents(world,gate,motor.observationBody)==expected,
        "pending query crossings obey owner lifetime and current bilateral policy");
  if(mutation==1){bool snapshot=true;for(const auto& event:world.LastStepTouchEvents())if(event.sensor)
      snapshot&=event.point.x>.99f&&event.point.x<1.01f;
    Check(snapshot,"sensor moved after a real crossing retains its verified historical point");}
  world.FinishQueryTouches();
  Check(SensorEvents(world,gate,motor.observationBody)==expected,"repeated query publication does not append events");
 }
 for(bool pre:{false,true}){
  PhysicsWorld world;world.Init();auto gate=Sensor(world,{1,1,0},{.002f,1,1});
  CharacterMotor first,second;
  for(auto* motor:{&first,&second}){motor->settings.radius=.1f;motor->settings.halfHeight=0;}
  first.Reset({0,1,-.5f},{1,0,0,0});second.Reset({0,1,.5f},{1,0,0,0});
  RegisterObserver(first,world);RegisterObserver(second,world);
  for(auto* motor:{&first,&second})motor->velocity={120,0,0};
  if(pre)world.BeginPreStepQueryContacts();else world.Step(1.f/60);
  for(auto* motor:{&first,&second}){motor->Step(world,zero,1.f/60);
    world.ResetBody(motor->observationBody,motor->position,motor->orientation);}
  if(pre)world.Step(1.f/60);
  world.FinishQueryTouches();
  Check(SensorEvents(world,gate,first.observationBody)==2&&SensorEvents(world,gate,second.observationBody)==2,
        "multiple pre/postphysics controllers retain separate pairs on one shared sensor");
 }

}
}

int main(){
 QuerySensorPaths();
 UniformGravity g({0,-9.81f,0}),zero({0,0,0});PhysicsWorld p;p.Init();auto floor=p.CreateStaticBox({0,-.5,0},{20,.5,20},.6,0);
 CharacterMotor m;m.Reset({0,.92,0},{1,0,0,0});Tick(m,p,g,3);Check(m.result.supported&&m.result.support.id==floor.id&&Near(m.result.supportNormal,{0,1,0}),"flat support / true geometry normal");
 auto q=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));PhysicsWorld rotated;rotated.Init();rotated.CreateStaticBox(q*glm::vec3(0,-.5,0),q,{20,.5,20},.6,0);UniformGravity rg(q*glm::vec3(0,-9.81f,0));CharacterMotor r;r.Reset(q*glm::vec3(0,.92,0),q);Tick(r,rotated,rg,3);Check(r.result.supported&&Near(glm::inverse(q)*r.position,m.position)&&Near(r.result.supportNormal,q*glm::vec3(0,1,0)),"arbitrarily rotated equivalent support");
 p.CreateStaticBox({2,2,0},{.2,2,4},.6,0);m.velocity={4,0,2};Tick(m,p,g,45);Check(m.position.x<1.51f&&m.position.z>.5f&&m.result.velocity.x<.01f,"wall sweep and slide resolves inward velocity");
 p.CreateStaticBox({0,2,2},{4,2,.2},.6,0);for(int i=0;i<30;++i){m.velocity={4,0,4};Tick(m,p,g,1);}Check(m.position.x<1.51f&&m.position.z<1.51f,"corner remains outside both walls");
 m.Reset({0,.5,0},{1,0,0,0});Tick(m,p,g,1);Check(m.position.y>.9f&&m.result.recovered,"signed-depth penetration recovery");
 PhysicsWorld stairs;stairs.Init();stairs.CreateStaticBox({0,-.5,0},{10,.5,10},.6,0);stairs.CreateStaticBox({1,.2,0},{.5,.2,2},.6,0);CharacterMotor step;step.Reset({0,.92,0},{1,0,0,0});Tick(step,stairs,g,2);
 for(int i=0;i<30;++i){step.velocity={3,0,0};Tick(step,stairs,g,1);}std::printf("STEP %.6f %.6f %.6f support=%d\n",step.position.x,step.position.y,step.position.z,step.result.supported);Check(step.position.x>1&&step.position.y>1.28f,"step up reuses proven up-forward-down sweeps");
 for(int i=0;i<25;++i){step.velocity={3,0,0};Tick(step,stairs,g,1);}std::printf("DOWN x=%g y=%g supported=%d\n",step.position.x,step.position.y,step.result.supported);Check(step.result.supported&&step.position.y<1.f,"step down/support following");
 PhysicsWorld slope;slope.Init();auto tilt=glm::angleAxis(glm::radians(65.f),glm::vec3(0,0,1));slope.CreateStaticBox({0,0,0},tilt,{4,.2,4},.6,0);CharacterMotor steep;steep.Reset(tilt*glm::vec3(0,1.12,0),{1,0,0,0});Tick(steep,slope,g,1);Check(!steep.result.supported,"steep contact is not accepted as support");
 PhysicsWorld platform;platform.Init();auto support=platform.CreateDynamicBox({0,-.25,0},{2,.25,2},500,.6,0);CharacterMotor passenger;passenger.Reset({1,.92,0},{1,0,0,0});Tick(passenger,platform,zero,1);platform.SetLinearVelocity(support,{1,0,0});platform.SetAngularVelocity(support,{0,1,0});auto old=passenger.position;Tick(passenger,platform,zero,1);
 Check(passenger.result.supported&&passenger.position.x>old.x&&passenger.position.z<old.z,"translating/rotating actual support carry");
 Check(passenger.result.supportVelocity.x>.9f&&passenger.result.supportVelocity.z<-.9f,"support point velocity includes angular motion");
 auto inherited=passenger.velocity;passenger.velocity+=glm::vec3(0,3,0);Tick(passenger,platform,zero,1);Check(!passenger.result.supported&&Near(glm::vec3(passenger.velocity.x,0,passenger.velocity.z),glm::vec3(inherited.x,0,inherited.z)),"departing retains support world momentum");
 platform.DestroyBody(support);auto replacement=platform.CreateDynamicBox({0,-.25,0},{2,.25,2},500,.6,0);Tick(passenger,platform,zero,1);Check(replacement.id!=support.id&&passenger.result.support.id!=support.id,"destroyed support generation never aliases reused slot");
 CharacterMotor air;air.Reset({0,5,0},q);air.velocity={1,2,3};Tick(air,platform,zero,1);Check(Near(air.result.displacement,glm::vec3(1,2,3)/60.f,.0001f)&&Near(air.orientation*glm::vec3(0,1,0),q*glm::vec3(0,1,0)),"zero-gravity airborne motion retains explicit orientation");
 PhysicsWorld planet;planet.Init();planet.CreateStaticSphere({0,0,0},10,.6,0);RadicalGravity radial({0,0,0},9.81f);CharacterMotor orbital;orbital.Reset({0,10.92f,0},{1,0,0,0});Tick(orbital,planet,radial,2);for(int i=0;i<60;++i){auto up=orbital.orientation*glm::vec3(0,1,0);orbital.velocity=glm::normalize(glm::cross(glm::vec3(0,0,1),up))*2.f;Tick(orbital,planet,radial,1);}std::printf("RADIAL %.6f %.6f %.6f support=%d r=%.6f\n",orbital.position.x,orbital.position.y,orbital.position.z,orbital.result.supported,glm::length(orbital.position));Check(orbital.result.supported&&glm::length(orbital.position)>10.85f&&std::abs(orbital.position.x)>1,"radial gravity tangent movement follows real sphere");
 PhysicsWorld push;push.Init();auto body=push.CreateDynamicBox({1,1,0},{.4,.4,.4},1000,.6,0);CharacterMotor driver;driver.settings.gravityScale=0;driver.settings.maxPushImpulse=2;driver.Reset({0,1,0},{1,0,0,0});driver.velocity={5,0,0};Tick(driver,push,zero,10);Check(push.GetLinearVelocity(body).x>0&&push.GetLinearVelocity(body).x<=.021f,"dynamic push is finite bounded impulse, not velocity seeding");
 driver.Reset({0,1,0},{1,0,0,0});driver.settings.collisionMask=0;driver.velocity={5,0,0};Tick(driver,push,zero,20);Check(driver.position.x>1.5f,"M39 bilateral mask excludes body");
 driver.settings.collisionMask=kAllCategories;driver.filter.ignoredBodies={body};driver.Reset({0,1,0},{1,0,0,0});driver.velocity={5,0,0};Tick(driver,push,zero,20);Check(driver.position.x>1.5f,"generation-aware ignored body");
 driver.filter.ignoredBodies.clear();push.SetBodyTags(body,CategoryBit(1));driver.settings.excludedTags=CategoryBit(1);driver.Reset({0,1,0},{1,0,0,0});driver.velocity={5,0,0};Tick(driver,push,zero,20);Check(driver.position.x>1.5f,"project tag exclusion uses same authoritative body tags");
 Scene scene;auto id=scene.CreateObject("motor").id;scene.Find(id)->characterMotor=CharacterMotorSettings{};scene.Find(id)->transform.position={0,2,0};scene.Find(id)->render=SceneRenderComponent{};
 std::string error,text,hash,changed;Scene copy;SaveSceneToString(scene,text);Check(LoadSceneFromString(text,copy,error)&&ScenesEqual(scene,copy),"motor component serialization round-trip");ComputeSceneFingerprint(scene,hash,error);copy.Find(id)->characterMotor->stepHeight=.3f;ComputeSceneFingerprint(copy,changed,error);Check(changed!=hash,"motor authored settings change strict baseline fingerprint");
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"headless motor world needs no PlayerController/input/camera");auto* a=world.RuntimeCharacter(id);a->settings.gravityScale=0;world.SetCharacterSettings(id,a->settings);a->velocity={1,0,0};world.UpdateCharacters(1.f/60);Check(world.RuntimeDefinition(id)->transform.position.x>0&&world.RuntimeCharacter(id)->result.displacement.x>0,"ordinary runtime entity publishes resolved motion");
 world.SetCharacterSettings(id,CharacterMotorSettings{});world.SetCharacterSettings(id,CharacterMotorSettings{false});auto position=world.RuntimeDefinition(id)->transform.position;world.UpdateCharacters(1.f/60);Check(world.RuntimeDefinition(id)->transform.position==position&&!world.RuntimeCharacter(id)->result.supported,"disable retains pose and clears support");world.RestoreAuthoredState();Check(Near(world.RuntimeCharacter(id)->position,{0,2,0})&&world.RuntimeCharacter(id)->velocity==glm::vec3(0),"authored reset discards stale motor state");world.Destroy();Check(!world.RuntimeCharacter(id),"world destruction invalidates character access");
 namespace fs=std::filesystem;auto root=fs::absolute("build/m49-focused");fs::remove_all(root);fs::create_directories(root/"Assets");std::ofstream(root/"Assets/motor.js")<<R"JS(import {input,world} from 'judas';
 export default class {constructor({entity}){this.entity=entity;this.state={};}fixedUpdate(){const m=this.entity.character;m.configure({gravityScale:0,collisionLayer:"Default",collisionMask:["Default"],requiredTags:[],excludedTags:[]});m.velocity={x:input.axis('move_x')+1,y:0,z:0};m.accelerate({x:0,y:1,z:0});this.state.safe=!!m;this.state.support=m.state.supportEntity;this.state.gravity=m.gravity;} })JS";
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());AssetRecord script,prefab;Check(db.Track((root/"Assets/motor.js").string(),script,error),"registered script asset");scene.Find(id)->scripts.push_back({1,script.id,true,"{}"});SaveSceneToFile(scene,(root/"Assets/motor.judasprefab").string(),error);db.Track((root/"Assets/motor.judasprefab").string(),prefab,error);ResourceManager resources(nullptr,&db);Check(world.Build(scene,&resources,error),"scriptable world build");InputSystem input;input.SetPhysical("key:D",1);input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.UpdateCharacters(1.f/60);Check(world.Scripts()->Diagnostics().empty()&&world.RuntimeCharacter(id)->position.x>.03f,"JS fixedUpdate reads logical input and drives motor");
 SceneTransform placement;placement.position={5,2,0};auto spawned=world.SpawnPrefab(prefab.id,placement,error);Check(spawned&&world.RuntimeCharacter(spawned),"prefab runtime spawn contains normal motor");world.FixedScripts(&input,1.f/60);world.UpdateCharacters(1.f/60);Check(world.RuntimeCharacter(id)!=world.RuntimeCharacter(spawned)&&world.RuntimeCharacter(spawned)->position.x>5,"independent motor/script instances");Check(world.DestroyHierarchy(spawned,error)&&!world.RuntimeCharacter(spawned),"destroy hierarchy safely retires motor entity");world.UpdateCharacters(1.f/60);world.Destroy();Check(world.Build(scene,&resources,error)&&world.RuntimeCharacter(id)->velocity==glm::vec3(0),"reload starts fresh runtime state");world.Destroy();
 for(int count:{1,10,100}){PhysicsWorld perf;perf.Init();perf.CreateStaticBox({0,-.5,0},{200,.5,200},.6,0);std::vector<CharacterMotor> motors(count);for(int i=0;i<count;++i)motors[i].Reset({float(i*2),.92f,0},{1,0,0,0});auto start=std::chrono::steady_clock::now();for(int f=0;f<120;++f){perf.Step(1.f/60);for(auto& motor:motors)motor.Step(perf,g,1.f/60);}std::printf("PERF %d motors whole_motor_batch_us %.3f samples=120\n",count,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/120);}
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
