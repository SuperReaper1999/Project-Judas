// Execute the public module in the real QuickJS VM, including negative controls.
#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "InputSystem.h"
#include "SceneSerialization.h"
#include "Powertrain.h"
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
Json State(RuntimeWorld& world){const auto states=world.Scripts()->Capture();if(states.size()!=1)throw std::runtime_error("one probe script required");return Json::parse(states.front().json);}
void Step(RuntimeWorld& world,InputSystem& input){input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.UpdatePowertrains(1.f/60);world.UpdateWheels(1.f/60);world.ResolvePowertrainLoads(1.f/60);world.Physics().Step(1.f/60);world.PublishWheels();world.PublishPowertrains(1.f/60);}
}
int main(int argc,char** argv){try{
 const auto root=fs::absolute(argc>1?argv[1]:"build/m82-powertrain-script");fs::create_directories(root/"Assets");
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {world,Powertrain} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={steps:0};}
 rejects(fn,kind){let caught=false;try{fn();}catch(e){caught=e instanceof kind;}if(!caught)throw Error('expected '+kind.name);}
 start(){
  this.power=this.entity.powertrain;if(!(this.power instanceof Powertrain)||!this.power.valid)throw Error('powertrain handle');
  if(world.entity('2').powertrain!==null)throw Error('implicit powertrain');
  const s=this.power.settings;if(s.version!==1||s.shafts[1].target!=='3'||s.shafts[0].name!=='rotor')throw Error('settings');
  s.shafts[0].inertia=999;if(this.power.settings.shafts[0].inertia===999)throw Error('live settings memory');
  this.state.started=true;
 }
 update(){this.rejects(()=>this.power.set({}),TypeError);this.rejects(()=>{this.power.enabled=false;},TypeError);this.state.phase=true;}
 fixedUpdate(){
  this.state.steps++;
  if(this.state.steps===1){
   this.power.set({sources:[{name:'engine',customTorque:40}],links:[{name:'gear',ratio:.5}],brakes:[{name:'axleBrake',engagement:0}]});
   this.power.set({links:[{name:'gear',ratio:null}]});
   this.rejects(()=>this.power.set({sources:[{name:'engine',customTorque:99},{name:'missing',customTorque:1}]}),TypeError);
   this.rejects(()=>this.power.set({sources:[{name:'engine',customTorque:NaN}]}),TypeError);
   this.rejects(()=>this.power.set({sources:[{name:'engine',input:Infinity}]}),TypeError);
   this.rejects(()=>this.power.set({sources:[{name:'engine',customTorque:999999}]}),TypeError);
   this.rejects(()=>this.power.set({sources:[{name:'engine',customTorque:10},{name:'engine',customTorque:20}]}),TypeError);
   this.rejects(()=>this.power.set({links:[{name:'gear',ratio:0}]}),TypeError);
   this.rejects(()=>this.power.set({brakes:[{name:'axleBrake',engagement:-1}]}),TypeError);
   this.rejects(()=>this.power.set({sources:[{name:'engine',unknown:1}]}),TypeError);
   this.rejects(()=>this.power.set({unknown:1}),TypeError);
   this.rejects(()=>this.power.set([]),TypeError);
   this.rejects(()=>{this.power.enabled=1;},TypeError);
   this.power.enabled=false;if(this.power.enabled)throw Error('disable');this.power.enabled=true;
   this.state.negatives=true;
  }
  if(this.state.steps===2){
   const s=this.power.state;
   if(!s.available||!s.converged||!s.shafts[1].available||s.sources[0].torque!==40||!Number.isFinite(s.shafts[0].rpm))throw Error('physical snapshot');
   s.shafts[0].angularVelocity=999;if(this.power.state.shafts[0].angularVelocity===999)throw Error('live state memory');
   this.state.physical=true;
  }
 }
})JS";
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());std::string error;AssetRecord script;
 Check(db.Track((root/"Assets/probe.js").string(),script,error),"ordinary registered public powertrain probe");
 Scene scene;
 auto& ground=scene.CreateObject("Ground");ground.id=1;ground.body=SceneBodyComponent{};ground.body->halfExtents={10,.1f,10};ground.transform.position={0,-.1f,0};
 auto& chassis=scene.CreateObject("Chassis");chassis.id=2;chassis.body=SceneBodyComponent{};chassis.body->motion=SceneBodyMotion::Dynamic;chassis.body->mass=100;chassis.body->halfExtents={.4f,.1f,.4f};chassis.transform.position={0,.85f,0};
 auto& wheel=scene.CreateObject("Wheel");wheel.id=3;wheel.wheel=WheelSettings{};wheel.wheel->chassis=2;wheel.wheel->mount={0,-.2f,0};
 auto& owner=scene.CreateObject("Mechanical graph");owner.id=4;owner.powertrain=PowertrainSettings{};auto& settings=*owner.powertrain;
 PowertrainShaft rotor;rotor.name="rotor";rotor.inertia=.2;rotor.bearing=2;rotor.axis={-1,0,0};
 PowertrainShaft axle;axle.name="axle";axle.kind=PowertrainShaftKind::Wheel;axle.target=3;
 settings.shafts={rotor,axle};PowertrainSource source;source.name="engine";source.shaft=0;source.maxTorque=200;settings.sources={source};
 PowertrainLink gear;gear.name="gear";gear.input=0;gear.output=1;gear.ratio=.5;settings.links={gear};settings.brakes={{"axleBrake",1,200}};
 owner.scripts={{1,script.id,true,"{}"}};
 ResourceManager resources(nullptr,&db);RuntimeWorld world;world.legacyGameplay=false;
 Check(world.Build(scene,&resources,error),"normal runtime builds powertrain and wheel entities");
 InputSystem input;world.UpdateScripts(&input,1.f/60);Step(world,input);
 auto* power=world.RuntimePowertrain(4);
 Check(power&&power->command.sources[0].customTorque==40&&!power->command.links[0].overrideRatio,"partial named patches held and rejected requests atomic");
 Step(world,input);
 Check(world.Scripts()->Diagnostics().empty()&&State(world).value("started",false)&&State(world).value("phase",false)&&State(world).value("negatives",false)&&State(world).value("physical",false),"actual VM phase, finite-value, names, bounds and snapshot contracts");
 Check(world.RuntimeWheel(3)->state.contact&&world.Physics().GetLinearVelocity(world.RuntimeBody(2)).z<0,"public source drives ordinary wheel traction and real chassis motion");
 std::ofstream(root/"Assets/lifetime.js")<<R"JS(import {world} from 'judas';export default class {
 constructor(){this.power=world.entity('4').powertrain;this.state={};}
 fixedUpdate(){if(!this.power.valid){let read=false,write=false;try{this.power.state;}catch(e){read=e instanceof ReferenceError;}try{this.power.set({});}catch(e){write=e instanceof ReferenceError;}if(!read||!write)throw Error('stale handle');this.state.stale=true;}}
})JS";
 AssetRecord lifetime;Check(db.Track((root/"Assets/lifetime.js").string(),lifetime,error),"registered retained-handle negative probe");
 Scene lifeScene=scene;lifeScene.Find(4)->scripts.clear();lifeScene.Find(2)->scripts={{1,lifetime.id,true,"{}"}};RuntimeWorld life;life.legacyGameplay=false;
 Check(life.Build(lifeScene,&resources,error),"independent mechanical lifetime world");Step(life,input);
 Check(life.DestroyEntity(4),"normal destruction removes powertrain owner");Step(life,input);
 Check(life.Scripts()->Diagnostics().empty()&&State(life).value("stale",false),"stale public component read and write throw ReferenceError");life.Destroy();
 fs::copy_file("docs/judasjs/examples/powertrain.js",root/"Assets/example.js",fs::copy_options::overwrite_existing);AssetRecord example;
 Check(db.Track((root/"Assets/example.js").string(),example,error),"exact documented cookbook registered");
 Scene cookbookScene=scene;cookbookScene.Find(4)->scripts={{1,example.id,true,"{}"}};cookbookScene.Find(4)->powertrain->sources[0].kind=PowertrainSourceKind::Electric;RuntimeWorld cookbook;cookbook.legacyGameplay=false;
 Check(cookbook.Build(cookbookScene,&resources,error),"ordinary cookbook world");InputMap map;map.Add("drive",true);map.AddBinding("drive",{"key:W"});map.Add("brake",false);map.AddBinding("brake",{"key:Space"});
 Check(input.SetMap(map,error),"project logical input map");input.SetPhysical("key:W",.5f);Step(cookbook,input);
 Check(cookbook.Scripts()->Diagnostics().empty()&&cookbook.RuntimePowertrain(4)->command.sources[0].input==.5&&cookbook.RuntimePowertrain(4)->state.sourceTorque[0]==100&&cookbook.Physics().GetLinearVelocity(cookbook.RuntimeBody(2)).z<0,"exact cookbook controls actual source and traction through logical input");
 input.SetPhysical("key:Space",1);Step(cookbook,input);
 Check(cookbook.RuntimePowertrain(4)->command.brakes[0].engagement==1,"exact cookbook commands mechanical brake");cookbook.Destroy();
 fs::copy_file("docs/judasjs/examples/surface.js",root/"Assets/surface.js",fs::copy_options::overwrite_existing);AssetRecord enumeration;
 Check(db.Track((root/"Assets/surface.js").string(),enumeration,error),"live public API enumeration registered");Scene surfaceScene=scene;surfaceScene.Find(4)->scripts={{1,enumeration.id,true,"{}"}};
 RuntimeWorld surface;surface.legacyGameplay=false;Check(surface.Build(surfaceScene,&resources,error),"live public API enumeration world");surface.UpdateScripts(&input,1.f/60);
 const auto published=State(surface);std::ofstream(root/"surface.json")<<published.dump(2)<<'\n';const auto& names=published.at("surface");
 Check(surface.Scripts()->Diagnostics().empty()&&std::find(names.begin(),names.end(),"Powertrain.set")!=names.end()&&std::find(names.begin(),names.end(),"Entity.powertrain")!=names.end(),"real module exposes typed powertrain surface");
 surface.Destroy();world.Destroy();resources.Shutdown();std::printf("M82 powertrain JS %u PASS\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"M82 powertrain JS FAIL after %u: %s\n",checks,e.what());return 1;}}
