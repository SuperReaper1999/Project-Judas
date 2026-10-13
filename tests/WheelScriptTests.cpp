// Real QuickJS wrapper/dispatcher checks with independently authored physics.
#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "InputSystem.h"
#include "SceneSerialization.h"
#include "Wheel.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
unsigned checks=0;
void Check(bool value,const char* label){++checks;std::printf("%s %s\n",value?"PASS":"FAIL",label);if(!value)throw std::runtime_error(label);}
Json State(RuntimeWorld& world){auto states=world.Scripts()->Capture();if(states.size()!=1)throw std::runtime_error("one wheel script required");return Json::parse(states.front().json);}
void Step(RuntimeWorld& world,InputSystem& input){input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.UpdateWheels(1.f/60);world.Physics().Step(1.f/60);world.PublishWheels();}
}
int main(int argc,char** argv){try{
 const auto root=fs::absolute(argc>1?argv[1]:"build/m81-wheel-script");fs::create_directories(root/"Assets");
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {world,Wheel} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={steps:0};}
 rejects(fn,kind){let caught=false;try{fn();}catch(e){caught=e instanceof kind;}if(!caught)throw Error('expected '+kind.name);}
 start(){
  this.wheel=this.entity.wheel;if(!(this.wheel instanceof Wheel)||!this.wheel.valid)throw Error('wheel handle');
  if(world.entity('2').wheel!==null)throw Error('no implicit wheel component');
  const s=this.wheel.settings;if(s.chassis.id!=='2'||Math.abs(s.radius-.35)>1e-6||s.collisionLayer!=='Default')throw Error('settings snapshot');
  s.radius=99;if(this.wheel.settings.radius===99)throw Error('mutable native snapshot');
  this.state.started=true;
 }
 update(){
  this.rejects(()=>this.wheel.set({driveTorque:1}),TypeError);
  this.rejects(()=>this.wheel.configure({friction:.5}),TypeError);
  this.rejects(()=>{this.wheel.enabled=false;},TypeError);
  this.state.phaseGuards=true;
 }
 fixedUpdate(){
  this.state.steps++;
  if(!this.state.configured){
   this.wheel.set({driveTorque:120,brakeTorque:0,steeringAngle:.12});
   this.wheel.set({brakeTorque:3});
   this.rejects(()=>this.wheel.set({driveTorque:999,brakeTorque:-1}),TypeError);
   this.rejects(()=>this.wheel.set({driveTorque:NaN}),TypeError);
   this.rejects(()=>this.wheel.set({driveTorque:Infinity}),TypeError);
   this.rejects(()=>this.wheel.set({gear:2}),TypeError);
   this.rejects(()=>this.wheel.set([]),TypeError);
   this.wheel.configure({friction:.7,damping:1800,rollingResistance:.03});
   this.rejects(()=>this.wheel.configure({friction:.2,springStiffness:-1}),TypeError);
   this.rejects(()=>this.wheel.configure({chassis:'2'}),TypeError);
   this.rejects(()=>this.wheel.configure([]),TypeError);
   const settings=this.wheel.settings;if(Math.abs(settings.friction-.7)>1e-6||settings.springStiffness!==20000)throw Error('atomic setting validation');
   this.wheel.enabled=false;if(this.wheel.enabled)throw Error('disable');
   this.wheel.enabled=true;if(!this.wheel.enabled)throw Error('enable');
   this.state.configured=true;
  }
  if(this.state.steps===2){
   const s=this.wheel.state;
   if(!s.available||!s.contact||s.contactEntity.id!=='1'||s.normal.y<.99||s.normalForce<=0||!Number.isFinite(s.angularVelocity))throw Error('physical contact snapshot');
   s.normal.y=99;if(this.wheel.state.normal.y===99)throw Error('mutable native state');
   this.state.contact=true;
  }
 }
})JS";
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());std::string error;AssetRecord script;
 Check(db.Track((root/"Assets/probe.js").string(),script,error),"registered wheel script");
 Scene scene;
 auto& ground=scene.CreateObject("ordinary ground");ground.id=1;ground.body=SceneBodyComponent{};ground.body->halfExtents={10,.1f,10};ground.transform.position={0,-.1f,0};
 auto& chassis=scene.CreateObject("ordinary chassis");chassis.id=2;chassis.body=SceneBodyComponent{};chassis.body->motion=SceneBodyMotion::Dynamic;chassis.body->mass=100;chassis.body->halfExtents={.4f,.1f,.4f};chassis.transform.position={0,.85f,0};
 auto& wheel=scene.CreateObject("authored wheel");wheel.id=3;wheel.wheel=WheelSettings{};wheel.wheel->chassis=2;wheel.wheel->mount={0,-.2f,0};wheel.scripts.push_back({1,script.id,true,"{}"});
 ResourceManager resources(nullptr,&db);RuntimeWorld world;Check(world.Build(scene,&resources,error),"ordinary world constructs wheel component");
 InputSystem input;world.UpdateScripts(&input,1.f/60);Step(world,input);
 auto* runtime=world.RuntimeWheel(3);Check(runtime&&runtime->command.driveTorque==120&&runtime->command.brakeTorque==3&&std::abs(runtime->command.steeringAngle-.12f)<1e-6f,"partial commands preserve values and failed command is atomic");
 Check(runtime&&std::abs(runtime->settings.friction-.7f)<1e-6f&&runtime->settings.springStiffness==20000,"runtime configuration matches actual physics component");
 Step(world,input);Check(world.Scripts()->Diagnostics().empty()&&State(world).value("started",false)&&State(world).value("phaseGuards",false)&&State(world).value("contact",false),"real VM reads contact and enforces phase/argument/snapshot contracts");
 Check(runtime->state.contact&&runtime->state.normalForce>0&&world.Physics().GetLinearVelocity(world.RuntimeBody(2)).z<0,"public command produces real chassis traction");
 std::ofstream(root/"Assets/lifetime.js")<<R"JS(import {world} from 'judas';
export default class {
 constructor(){this.wheel=world.entity('3').wheel;this.state={};}
 fixedUpdate(){
  this.state.valid=this.wheel.valid;
  if(!this.state.valid){
   let threw=false;try{this.wheel.state;}catch(e){threw=e instanceof ReferenceError;}
   if(!threw)throw Error('stale wheel snapshot must fail');this.state.stale=true;
   let mutate=false;try{this.wheel.set({driveTorque:1});}catch(e){mutate=e instanceof ReferenceError;}
   if(!mutate)throw Error('stale wheel mutation must fail');this.state.staleMutation=true;
  }
 }
})JS";
 AssetRecord lifetime;Check(db.Track((root/"Assets/lifetime.js").string(),lifetime,error),"registered lifetime probe");
 Scene lifeScene=scene;lifeScene.Find(3)->scripts.clear();lifeScene.Find(2)->scripts.push_back({1,lifetime.id,true,"{}"});RuntimeWorld life;
 Check(life.Build(lifeScene,&resources,error),"independent wheel lifetime world");Step(life,input);
 Check(life.DestroyEntity(3),"normal destruction retires wheel owner");Step(life,input);
 Check(life.Scripts()->Diagnostics().empty()&&State(life).value("stale",false)&&State(life).value("staleMutation",false),"retained public wheel never aliases a retired component");
 life.Destroy();
 // Execute the exact documented file, not a rewritten equivalent.
 fs::copy_file("docs/judasjs/examples/wheel.js",root/"Assets/wheel-example.js",fs::copy_options::overwrite_existing);
 AssetRecord example;Check(db.Track((root/"Assets/wheel-example.js").string(),example,error),"registered exact cookbook wheel script");
 Scene exampleScene=scene;exampleScene.Find(3)->scripts={{1,example.id,true,"{}"}};RuntimeWorld cookbook;
 Check(cookbook.Build(exampleScene,&resources,error),"ordinary cookbook world");
 InputMap map;map.Add("drive",true);map.AddBinding("drive",{"key:W"});map.Add("steer",true);map.AddBinding("steer",{"key:A"});map.Add("brake",false);map.AddBinding("brake",{"key:Space"});
 Check(input.SetMap(map,error),"project-owned logical wheel input map");input.SetPhysical("key:W",1);input.SetPhysical("key:A",.5f);
 Step(cookbook,input);Step(cookbook,input);
 auto* controlled=cookbook.RuntimeWheel(3);
 Check(cookbook.Scripts()->Diagnostics().empty()&&controlled->command.driveTorque==140&&std::abs(controlled->command.steeringAngle-.2f)<1e-6f&&State(cookbook).value("contact",false)&&State(cookbook).value("spin",0.f)>0,"exact cookbook executes logical input, tyre contact and spin reads");
 input.SetPhysical("key:Space",1);Step(cookbook,input);Check(controlled->command.brakeTorque==240,"exact cookbook applies authored brake input");cookbook.Destroy();
 fs::copy_file("docs/judasjs/examples/surface.js",root/"Assets/surface.js",fs::copy_options::overwrite_existing);
 AssetRecord enumeration;Check(db.Track((root/"Assets/surface.js").string(),enumeration,error),"registered live API enumeration");
 Scene surfaceScene=scene;surfaceScene.Find(3)->scripts={{1,enumeration.id,true,"{}"}};RuntimeWorld surface;
 Check(surface.Build(surfaceScene,&resources,error),"ordinary live API world");surface.UpdateScripts(&input,1.f/60);
 const auto published=State(surface);std::ofstream(root/"surface.json")<<published.dump(2)<<'\n';
 const auto& names=published.at("surface");Check(surface.Scripts()->Diagnostics().empty()&&std::find(names.begin(),names.end(),"Wheel.set")!=names.end()&&std::find(names.begin(),names.end(),"Entity.wheel")!=names.end(),"actual QuickJS module exposes the declared wheel surface");
 surface.Destroy();world.Destroy();resources.Shutdown();std::printf("M81 wheel scripts: %u checks PASS\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"M81 wheel scripts FAIL after %u checks: %s\n",checks,e.what());return 1;}}
