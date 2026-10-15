#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include "WorldPersistence.h"
#include "InputSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
namespace {
using J=nlohmann::ordered_json;
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",label);}
J State(RuntimeWorld& world,EntityId id){if(world.Scripts())for(const auto& r:world.Scripts()->Capture())if(r.entity==id)return J::parse(r.json);return J::object();}
void Step(RuntimeWorld& world,InputSystem& input,float dt=.5f){input.BeginFixedStep();world.FixedScripts(&input,dt);world.UpdateWeather(dt);}
}
int main(){namespace fs=std::filesystem;const fs::path root=fs::absolute("build/m86-weather-script-tests");fs::remove_all(root);fs::create_directories(root/"Assets");std::string error;
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {world,weather,input,Weather} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={steps:0};}
 bind(){this.field=this.entity.weather;this.retained=new Weather('4');}
 rejects(fn,Type){let yes=false;try{fn();}catch(e){yes=e instanceof Type;}if(!yes)throw Error('negative request accepted');}
 start(){this.bind();this.state.valid=this.field?.valid===true;this.state.absent=world.entity('3').weather===null;this.state.invalid=!new Weather('999999').valid;
  this.rejects(()=>this.field.transition({rain:.5},0),TypeError);this.rejects(()=>{this.field.enabled=false;},TypeError);this.rejects(()=>this.field.strike(),TypeError);this.state.phase=true;}
 restore(){this.bind();}
 fixedUpdate(){
  if(!this.retained?.valid){this.rejects(()=>this.retained.state,ReferenceError);this.rejects(()=>this.retained.transition({},0),ReferenceError);this.rejects(()=>{this.retained.enabled=true;},ReferenceError);this.state.stale=true;}
  const step=this.state.steps++;
  if(step===0){this.state.input=input.pressed('interact');
   this.rejects(()=>this.field.transition({madeUpRain:1},0),TypeError);this.rejects(()=>this.field.transition({rain:NaN},0),TypeError);this.rejects(()=>this.field.transition({rain:2},0),TypeError);
   this.rejects(()=>this.field.transition({wind:[1,2,3]},0),TypeError);this.rejects(()=>this.field.transition({wind:{x:Infinity,y:0,z:0}},0),TypeError);this.rejects(()=>this.field.transition({},-1),TypeError);this.rejects(()=>this.field.transition({},NaN),TypeError);
   this.rejects(()=>{this.field.enabled=1;},TypeError);this.rejects(()=>weather.sample({x:NaN,y:0,z:0}),TypeError);this.rejects(()=>{weather.quality=3;},TypeError);this.rejects(()=>{weather.quality=NaN;},TypeError);this.state.negatives=true;
   this.field.transition({rain:.2,gust:0,wind:{x:0,y:0,z:7}},0);if(this.field.state.current.rain!==.2)throw Error('zero-duration transition');this.state.immediate=true;
   const before=weather.sample({x:2,y:0,z:0});weather.quality=0;const off=weather.sample({x:2,y:0,z:0});weather.quality=2;const after=weather.sample({x:2,y:0,z:0});
   if(JSON.stringify(before)!==JSON.stringify(off)||JSON.stringify(off)!==JSON.stringify(after))throw Error('graphics changed authoritative query');this.state.quality=true;
   if(before.up.x!==1||before.up.y!==0||Math.abs(before.altitude-2)>.0001||Math.abs(before.wind.z-7)>.0001)throw Error('local arbitrary-gravity sample');this.state.coordinates=true;
   this.field.enabled=false;if(weather.sample({x:2,y:0,z:0})!==null)throw Error('disabled region still visible');this.field.enabled=true;this.state.disable=true;
   const sequence=this.field.state.lightningSequence;this.field.strike({x:5,y:0,z:0});const strike=this.field.state;
   if(strike.lightningSequence===sequence||strike.lightningPosition.x!==5||strike.lightningPosition.y!==0||strike.lightningPosition.z!==0)throw Error('strike local point');this.state.strike=true;
   const current=this.field.state.current;current.rain=999;if(this.field.state.current.rain===999)throw Error('snapshot aliases native');this.state.detached=true;
   this.field.transition({rain:1},4);if(this.field.state.current.rain!==.2)throw Error('transition initial discontinuity');this.state.transition=true;
  }
  if(step===1){const before=this.field.state.current;this.field.transition({snow:.8,rain:0},1);if(this.field.state.current.rain!==before.rain||this.field.state.current.snow!==before.snow)throw Error('interruption discontinuity');this.state.interruption=true;}
  if(step===3){const p=this.field.state.current;if(p.snow!==.8||p.rain!==0)throw Error('transition endpoint');this.state.endpoint=true;}
 }
})JS";
 fs::copy_file("docs/judasjs/examples/weather.js",root/"Assets/example.js",fs::copy_options::overwrite_existing);
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());AssetRecord script,example;Check(db.Track((root/"Assets/probe.js").string(),script,error)&&db.Track((root/"Assets/example.js").string(),example,error),"public wrapper and exact cookbook assets registered in clean project");
 Scene scene;scene.Settings().worldOrigin={9e11,-9e11,9e11};
 auto& owner=scene.CreateObject("Script weather owner");owner.id=1;owner.weather=WeatherSettings{};owner.weather->frame.up={1,0,0};owner.weather->frame.north={0,0,-1};owner.weather->profile.wind={0,0,7};owner.weather->profile.gust=0;owner.scripts={{1,script.id,true,"{}"}};
 auto& second=scene.CreateObject("Independent cookbook owner");second.id=2;second.transform.position={20,0,0};second.weather=WeatherSettings{};second.weather->regionShape=WeatherRegionShape::Sphere;second.weather->regionRadius=4;second.weather->blendDistance=1;second.weather->priority=2;second.scripts={{1,example.id,true,"{}"}};
 auto& plain=scene.CreateObject("No weather component");plain.id=3;
 auto& retained=scene.CreateObject("Retained handle target");retained.id=4;retained.transform.position={100,0,0};retained.weather=WeatherSettings{};retained.weather->regionShape=WeatherRegionShape::Sphere;retained.weather->regionRadius=4;retained.weather->blendDistance=1;
 scene.SetNextId(5);ResourceManager resources(nullptr,&db);RuntimeWorld world;world.legacyGameplay=false;
 Check(world.Build(scene,&resources,error),"actual QuickJS world builds independent authored weather components");InputSystem input;InputMap bindings;bindings.Add("interact",false);bindings.AddBinding("interact",{"key:E"});Check(input.SetMap(bindings,error),"logical test input is explicitly project-authored");input.SetPhysical("key:E",1);world.UpdateScripts(&input,1.f/60);
 Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"frame-phase rejection remains caught and isolated in ordinary JS");
 for(unsigned i=0;i<12;++i)Step(world,input);
 auto state=State(world,1);for(const char* key:{"valid","absent","invalid","phase","input","negatives","immediate","quality","coordinates","disable","strike","detached","transition","interruption","endpoint"})Check(state.value(key,false),key);
 Check(world.Scripts()->Diagnostics().empty(),"actual public weather operations execute without uncaught diagnostic");
 auto cookbook=State(world,2);Check(cookbook.value("started",false)&&cookbook.value("steps",0)==12&&cookbook.value("windSpeed",0.)>0,"exact documented weather cookbook executes with real fixed input/callbacks");
 Check(world.RuntimeWeather(1)->Current().snow==.8&&world.RuntimeWeather(2)->State().to.rain==.8&&world.RuntimeWeather(2)->Current().snow==0,"two scripted weather instances keep independent native transition state");
 Check(world.DestroyEntity(4),"ordinary destruction retires retained weather target");Step(world,input);
 Check(world.Scripts()->Diagnostics().empty()&&State(world,1).value("stale",false),"retained public wrapper detects destruction and fails safely");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"composed save captures real script and weather continuation");Scene snapshot;Check(WorldPersistence::SceneFrom(chunks,snapshot,error),"saved composed scene decodes");RuntimeWorld cold;cold.legacyGameplay=false;bool built=cold.Build(snapshot,&resources,error,nullptr,nullptr,true);bool restored=built&&WorldPersistence::Prepare(cold,error)&&WorldPersistence::Restore(cold,chunks,error);if(!restored)std::printf("RESTORE %s\n",error.c_str());Check(restored,"fresh runtime restores public script state and weather before resuming");
 if(restored){const auto before1=world.RuntimeWeather(1)->Capture(),before2=world.RuntimeWeather(2)->Capture();Check(cold.RuntimeWeather(1)->Capture()==before1&&cold.RuntimeWeather(2)->Capture()==before2,"restoration reconstructs actual native fields rather than script-cached status");Step(world,input);Step(cold,input);Check(world.RuntimeWeather(1)->Capture()==cold.RuntimeWeather(1)->Capture()&&world.RuntimeWeather(2)->Capture()==cold.RuntimeWeather(2)->Capture(),"first resumed native weather boundary matches uninterrupted instance");Check(cold.Scripts()->Diagnostics().empty()&&State(cold,2).value("steps",0)==State(world,2).value("steps",0),"cookbook state and lifecycle continue without replaying initial transition");}
 std::printf("Weather JS %u checks %u failures\n",checks,failures);return failures?1:0;
}
