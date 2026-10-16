#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
using J=nlohmann::json;int checks=0,failures=0;void Check(bool b,const char* s){++checks;failures+=!b;std::printf("%s %s\n",b?"PASS":"FAIL",s);}
J State(RuntimeWorld& w){auto states=w.Scripts()->Capture();return states.empty()?J::object():J::parse(states[0].json);}
int main(){namespace fs=std::filesystem;const auto root=fs::absolute(".cache/m89/script-probe");fs::remove_all(root);fs::create_directories(root/"Assets");std::ofstream(root/"Assets/probe.js")<<R"JS(import {players,world,ui} from 'judas';
export default class{
 constructor(){this.state={steps:0};}
 reject(fn,kind){let ok=false;try{fn()}catch(e){ok=e instanceof kind}if(!ok)throw Error('expected '+kind.name)}
 start(){
  const map=key=>[{name:'move_x',axis:true,bindings:[{control:key,scale:1,deadzone:0}]}];this.a=players.create('A',map('key:W'));this.b=players.create('B',map('key:Up'));this.a.assign('keyboard',{controls:['key:W']});this.b.assign('keyboard',{controls:['key:Up']});this.a.entity=world.entity('2');this.b.entity=world.entity('3');
  this.reject(()=>this.b.assign('keyboard',{controls:['key:W']}),TypeError);this.reject(()=>this.a.setInputMap([{name:'bad',axis:false,bindings:[{control:'madeup',scale:1,deadzone:0}]}]),TypeError);
  this.a.viewport={x:0,y:0,width:.5,height:1};this.b.viewport={x:.5,y:0,width:.5,height:1};this.a.setView({position:{x:0,y:0,z:10}},70);this.b.setView({position:{x:10,y:0,z:0},rotation:{w:.70710678,x:0,y:.70710678,z:0}},70);
  const pa=this.a.project({x:0,y:0,z:0}),pb=this.b.project({x:0,y:0,z:0}),ra=this.a.ray(),rb=this.b.ray();
  if(!pa||!pb||!ra||!rb||Math.abs(pa.x-.25)>.001||Math.abs(pb.x-.75)>.001||ra.direction.z>-.99||rb.direction.x>-.99)throw Error('independent projection/ray');
  this.reject(()=>this.a.ray({x:2,y:0}),TypeError);this.reject(()=>this.a.viewport={x:0,y:0,width:0,height:1},TypeError);this.reject(()=>this.a.setView({},70,{near:1,far:.5}),TypeError);
  const doc=ui.get('probe');doc.player=this.a;if(doc.player.id!==this.a.id)throw Error('UI player binding');this.oldElement=doc.get('root');this.oldInput=this.a.input;this.old=this.a.id;
  this.state.a=this.a.id;this.state.b=this.b.id;this.state.started=true;
 }
 fixedUpdate(){this.state.steps++;if(this.a.valid)this.a.entity.character.velocity={x:this.a.input.axis('move_x')*3,y:0,z:0};if(this.b.entity)this.b.entity.character.velocity={x:this.b.input.axis('move_x')*3,y:0,z:0};
  if(this.state.steps===12){this.a.leave();this.reject(()=>this.oldInput.axis('move_x'),ReferenceError);this.reject(()=>this.oldElement.text,ReferenceError);if(this.a.valid||players.get(this.old)!==null||!world.entity('2').valid)throw Error('identity lifetime');const fresh=players.create('fresh');if(fresh.id===this.old)throw Error('revived token');world.entity('3').destroy();if(this.b.entity!==null||!this.b.valid)throw Error('entity lifetime');this.state.lifetime=true;}
 }
})JS";
 AssetDatabase db;std::string error;db.Scan(root.string(),(root/"Assets").string());AssetRecord script;Check(db.Track((root/"Assets/probe.js").string(),script,error),"public player probe registered as ordinary asset");if(failures){std::puts(error.c_str());return 1;}ResourceManager resources(nullptr,&db);resources.SetBlockingMode(true);Scene scene;auto& owner=scene.CreateObject("Owner");owner.scripts={{1,script.id,true,"{}"}};for(int i=0;i<2;++i){auto& motor=scene.CreateObject("Motor");motor.transform.position={float(i*5),0,0};motor.characterMotor=CharacterMotorSettings{};motor.characterMotor->gravityScale=0;}
 RuntimeWorld w;Check(w.Build(scene,&resources,error),"two ordinary motors in one authoritative world");if(failures){std::puts(error.c_str());return 1;}InputSystem input;w.viewportWidth=640;w.viewportHeight=480;UIDocument doc;UIElement element;element.id="root";element.kind=UIKind::Canvas;doc.elements={element};w.UI().Add(doc,"probe",1,error);w.UpdateScripts(&input,1.f/60);Check(w.Scripts()->Diagnostics().empty()&&State(w).value("started",false),"actual JS creates/routes players, views, UI and negative controls");if(!w.PlayersIfLoaded())return 1;auto hub=input.localDevices.lock();hub->BeginFrame();w.Players().Route(*hub);hub->BeginFrame();hub->Set("keyboard","key:W",1);w.Players().Route(*hub);
 for(int i=0;i<10;++i){w.Players().BeginFixedStep();w.FixedScripts(&input,1.f/60);w.UpdateCharacters(1.f/60);w.Physics().Step(1.f/60);}
 Check(w.RuntimeCharacter(2)&&w.RuntimeCharacter(2)->position.x>.3f&&w.RuntimeCharacter(3)&&w.RuntimeCharacter(3)->position.x>4.99f&&w.RuntimeCharacter(3)->position.x<5.01f,"one player's JS moves only its own actual CharacterMotor");
 hub->BeginFrame();hub->Set("keyboard","key:W",0);hub->Set("keyboard","key:Up",1);w.Players().Route(*hub);w.Players().BeginFixedStep();w.FixedScripts(&input,1.f/60);w.UpdateCharacters(1.f/60);Check(w.RuntimeCharacter(3)->position.x>5.04f,"other player can independently drive its motor");w.Players().BeginFixedStep();w.FixedScripts(&input,1.f/60);Check(w.Scripts()->Diagnostics().empty()&&State(w).value("lifetime",false),"stale handles, controlled-body destruction and player reuse fail safely");hub.reset();w.Destroy();Check(input.localDevices.expired(),"world teardown retires optional input hub");
 // Execute the exact published cookbook and API enumeration with the same VM.
 for(const auto& name:{"local-multiplayer","surface"}){
  fs::copy_file(fs::path("docs/judasjs/examples")/(std::string(name)+".js"),root/"Assets"/(std::string(name)+".js"),fs::copy_options::overwrite_existing);AssetRecord example;db.Track((root/"Assets"/(std::string(name)+".js")).string(),example,error);Scene s;auto& o=s.CreateObject("Cookbook motor");o.characterMotor=CharacterMotorSettings{};o.characterMotor->gravityScale=0;o.scripts={{1,example.id,true,"{}"}};RuntimeWorld world;Check(world.Build(s,&resources,error),"exact documented example world");world.UpdateScripts(&input,1.f/60);
  if(std::string(name)=="local-multiplayer"){auto devices=input.localDevices.lock();devices->BeginFrame();world.Players().Route(*devices);devices->BeginFrame();devices->Set("keyboard","key:D",1);world.Players().Route(*devices);for(int i=0;i<10;++i){world.Players().BeginFixedStep();world.FixedScripts(&input,1.f/60);world.UpdateCharacters(1.f/60);}Check(world.RuntimeCharacter(1)->position.x>.01f,"exact cookbook consumes per-player input and accelerates a real motor");}
  else{std::ofstream(root/"surface.json")<<State(world).dump(2)<<'\n';Check(!world.PlayersIfLoaded(),"API enumeration alone does not allocate multiplayer state");}
  Check(world.Scripts()->Diagnostics().empty(),"exact example executes without script faults");world.Destroy();
 }
 std::ofstream(root/"Assets/headless.js")<<R"JS(import {players} from 'judas';export default class{constructor(){this.state={}}start(){const p=players.create('Headless identity');if(!p.valid||p.entity!==null||players.devices.length)throw Error('device coupling');let rejected=false;try{p.assign('keyboard')}catch(e){rejected=e instanceof TypeError}if(!rejected)throw Error('absent adapter assignment');this.state.ok=true;}})JS";
 AssetRecord headlessAsset;db.Track((root/"Assets/headless.js").string(),headlessAsset,error);Scene headlessScene;headlessScene.CreateObject("Headless controller").scripts={{1,headlessAsset.id,true,"{}"}};RuntimeWorld headless;headless.Build(headlessScene,&resources,error);headless.UpdateScripts(nullptr,1.f/60);Check(headless.Scripts()->Diagnostics().empty()&&State(headless).value("ok",false),"identity works with no Window/input device; unavailable assignment rejects");headless.Destroy();
 resources.Shutdown();std::printf("%d checks %d failures\n",checks,failures);return failures?1:0;}
