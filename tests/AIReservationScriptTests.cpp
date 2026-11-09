#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "InputSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
unsigned checks=0,failures=0;
void Check(bool condition,const std::string& message) {
    ++checks;if(!condition){++failures;std::cerr<<"FAIL "<<message<<'\n';}
}
Json State(RuntimeWorld& world,SceneObjectId id) {
    for(const auto& record:world.Scripts()->Capture())if(record.entity==id)return Json::parse(record.json);
    throw std::runtime_error("required script state missing");
}
// Commands represent project decisions and use the actual script/native bridge;
// replacing plain state does not manufacture native reservations or clock state.
void Command(RuntimeWorld& world,SceneObjectId id,const std::string& command) {
    auto records=world.Scripts()->Capture();
    for(auto& record:records)if(record.entity==id){auto state=Json::parse(record.json);state["command"]=command;record.json=state.dump();}
    std::string error;
    if(!world.RestoreScriptState(records,error))throw std::runtime_error(error);
}
void Tick(RuntimeWorld& world,InputSystem& input,double dt=0) {
    // FixedScripts alone deliberately does not advance the world's clock.
    // The same public clock is advanced by production StepPlayedWorld.
    world.AdvanceSimulationTime(dt);world.FixedScripts(&input,float(dt));
}
}
int main(int argc,char** argv) try {
    const bool cold=argc==3;
    const bool read=cold&&std::string(argv[1])=="read";
    if(cold&&std::string(argv[1])!="write"&&!read)throw std::runtime_error("mode must be write or read");
    const auto root=cold?fs::absolute(argv[2]):fs::temp_directory_path()/("judas-ai-reservations-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"Assets");
    if(!read) {
        std::ofstream(root/"Assets/npc.js")<<R"JS(
import {AITargetReservations} from 'judas';
export const properties={name:{type:'string',default:'NPC'}};
export default class {
 constructor({properties}){this.name=properties.name;this.state={command:'',initial:false,available:false,result:false,sameSlotDenied:false,restoreAvailable:false};}
 make(){this.targets=new AITargetReservations();}
 start(){this.make();this.state.initial=this.targets.acquire('last-meal',1);
  const other=new AITargetReservations();
  this.state.sameSlotDenied=this.state.initial&&!other.acquire('last-meal',1);
  other.dispose();
  let coercions=0,rejections=0;
  const hostileTarget={toString:()=>{coercions++;this.targets.dispose();return 'last-meal';}};
  const hostileDuration={valueOf:()=>{coercions++;this.targets.dispose();return 1;}};
  for(const operation of [()=>this.targets.acquire(hostileTarget,1),()=>this.targets.release(hostileTarget),()=>this.targets.acquire('last-meal',hostileDuration)]) {
   try{operation();}catch(error){rejections++;}
  }
  this.state.coercions=coercions;this.state.rejections=rejections;
  this.state.available=this.targets.available('last-meal');
 }
 restore(){this.make();const independent=new AITargetReservations();
  this.state.restoreAvailable=independent.available('last-meal');independent.dispose();
 }
 fixedUpdate(){
  if(this.state.command==='acquire')this.state.result=this.targets.acquire('last-meal',1);
  else if(this.state.command==='release')this.state.result=this.targets.release('last-meal');
  else if(this.state.command==='renew')this.state.result=this.targets.acquire('last-meal',1);
  else if(this.state.command==='dispose')this.targets.dispose();
  else if(this.state.command==='fault')throw new Error('intentional reservation owner fault');
  this.state.command='';
  this.state.available=this.targets.available('last-meal');
 }
}
)JS";
        std::ofstream(root/"Assets/observer.js")<<R"JS(
import {AITargetReservations} from 'judas';
export default class {
 constructor(){this.targets=new AITargetReservations();
  this.bootAvailable=this.targets.available('last-meal');this.state={available:false,bootAvailable:false};}
 start(){this.state.bootAvailable=this.bootAvailable;}
 restore(){this.state.bootAvailable=this.bootAvailable;}
 fixedUpdate(){this.state.available=this.targets.available('last-meal');}
}
)JS";
    }
    AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());
    AssetRecord npcAsset,observerAsset;std::string error;
    if(!read)Check(assets.Track((root/"Assets/npc.js").string(),npcAsset,error)&&assets.Track((root/"Assets/observer.js").string(),observerAsset,error),"registered real project NPC/observer scripts");
    else {
        const auto* a=assets.FindByRelativePath("Assets/npc.js");const auto* b=assets.FindByRelativePath("Assets/observer.js");
        Check(a&&b,"fresh process rediscovers exact registered assets");if(!a||!b)return 2;npcAsset=*a;observerAsset=*b;
    }
    Scene scene;
    const auto first=scene.CreateObject("first NPC").id;
    scene.Find(first)->scripts.push_back({1,npcAsset.id,true,"{\"name\":\"first\"}"});
    const auto second=scene.CreateObject("second NPC").id;
    scene.Find(second)->scripts.push_back({1,npcAsset.id,true,"{\"name\":\"second\"}"});
    const auto observer=scene.CreateObject("observer").id;
    scene.Find(observer)->scripts.push_back({1,observerAsset.id,true,"{}"});
    ResourceManager resources(nullptr,&assets);ProjectSettings project;
    RuntimeWorld world;world.ConfigureProject(project);InputSystem input;
    if(read) {
        std::ifstream file(root/"snapshot.save",std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(file),{}};
        GameSnapshot snapshot;Scene restored;
        const bool ready=DecodeGameSnapshot(bytes,snapshot,error)&&WorldPersistence::SceneFrom(snapshot.participants,restored,error)&&
            world.Build(restored,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error);
        Check(ready,"fresh process restores actual modern world/script participant save: "+error);if(!ready)return 2;
        // Constructor/restore callbacks are invoked normally without a physical
        // step, so the transient board can be observed before any reacquisition.
        world.UpdateScripts(&input,0);
        Check(State(world,first).at("restoreAvailable")&&State(world,second).at("restoreAvailable"),
              "no previous claimant token is resurrected by modern cold restoration");
        Command(world,second,"acquire");Tick(world,input);
        Check(State(world,second).at("result")&&!State(world,observer).at("available"),
              "a newly created second scope can acquire before old first scope reclaims");
        Command(world,first,"acquire");Tick(world,input);
        Check(!State(world,first).at("result"),"historical saved owner cannot steal a new runtime claim");
    } else {
        Check(world.Build(scene,&resources,error),"ordinary script-owned world constructs");Tick(world,input);
        Check(world.Scripts()->Diagnostics().empty(),"reservation constructors run through actual script lifecycle");
        Check(State(world,first).at("initial")&&!State(world,second).at("initial"),"first of two independently owned NPCs gets the exclusive last meal");
        Check(State(world,first).at("coercions")==0&&State(world,first).at("rejections")==3,"hostile target/duration coercion cannot dispose a borrowed native scope");
        Check(State(world,first).at("sameSlotDenied"),"second reservation object in the same script cannot self-share the meal");
        Check(!State(world,observer).at("available"),"third independent scope observes actual occupied native target");
        Command(world,second,"release");Tick(world,input);
        Check(!State(world,second).at("result")&&!State(world,observer).at("available"),"nonowner release is rejected without freeing the meal");
        Tick(world,input,.75);Command(world,first,"renew");Tick(world,input);
        Check(State(world,first).at("result"),"real owner can renew before expiry");
        Tick(world,input,.5);Command(world,second,"acquire");Tick(world,input);
        Check(!State(world,second).at("result"),"renewed lease survives another half simulated second");
        Tick(world,input,.5);Command(world,second,"acquire");Tick(world,input);
        Check(State(world,second).at("result")&&!State(world,observer).at("available"),"lease expires exactly in world simulation time and competitor acquires");
        Command(world,second,"release");Tick(world,input);Command(world,first,"acquire");Tick(world,input);
        Check(State(world,first).at("result"),"first claimant reacquires for persistence positive control");
        GameSnapshot saved;saved.project="ai-reservation-test";saved.scene="Scenes/main.judas";saved.content=std::string(64,'b');
        Check(WorldPersistence::Capture(world,saved.participants,error),"capture uses modern project/script persistence");
        std::ofstream(root/"snapshot.save",std::ios::binary)<<EncodeGameSnapshot(saved);
        if(!cold) {
            // Modern in-process restore must also create a new empty board.
            RuntimeWorld restored;restored.ConfigureProject(project);Scene savedScene;
            Check(WorldPersistence::SceneFrom(saved.participants,savedScene,error)&&restored.Build(savedScene,&resources,error,nullptr,nullptr,true)&&
                  WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,saved.participants,error),"modern restore constructs private replacement world");
            restored.UpdateScripts(&input,0);
            Check(State(restored,first).at("restoreAvailable")&&State(restored,second).at("restoreAvailable"),"modern world replacement does not revive transient claims");
            Check(!State(world,observer).at("available"),"independent old world's occupancy was not altered by replacement construction");
            restored.Destroy();
            Check(world.Scripts()->Reload(true,error),"deliberate script reload preserves project state while replacing reservation scopes");
            Tick(world,input);
            Check(State(world,observer).at("bootAvailable")&&State(world,first).at("initial")&&
                  !State(world,second).at("initial")&&!State(world,observer).at("available"),
                  "reload starts with no native claim before explicit start-policy reacquisition");
            Command(world,first,"acquire");Tick(world,input);
            Check(State(world,first).at("result"),"restored first NPC explicitly reacquires after reload");
            world.DestroyEntity(first,&error);Tick(world,input);
            Check(State(world,observer).at("available"),"owner destruction releases claims at the normal script synchronization boundary");
            Command(world,second,"acquire");Tick(world,input);
            Check(State(world,second).at("result"),"remaining NPC can acquire after owner removal");
            Command(world,second,"fault");Tick(world,input);
            Check(world.Scripts()->Diagnostics().size()==1&&State(world,observer).at("available"),"script fault automatically releases native claims despite no destroy callback");
        }
    }
    world.Destroy();
    if(!cold) {
        std::ofstream(root/"Assets/raw-owner.js")<<R"JS(
export default class {
 constructor(){this.state={claimed:false};}
 start(){globalThis.foreignReservation=globalThis.__judas('reservationCreate');
  this.state.claimed=globalThis.__judas('reservationAcquire',globalThis.foreignReservation,'raw-target',1);}
}
)JS";
        std::ofstream(root/"Assets/raw-foreign.js")<<R"JS(
import {AITargetReservations} from 'judas';
export default class {
 constructor(){this.state={rejected:0,available:true};}
 start(){this.targets=new AITargetReservations();
  for(const op of ['reservationAvailable','reservationAcquire','reservationRelease','reservationReleaseAll','reservationDispose']) {
   try{globalThis.__judas(op,globalThis.foreignReservation,'raw-target',1);}catch(error){this.state.rejected++;}
  }}
 fixedUpdate(){this.state.available=this.targets.available('raw-target');}
}
)JS";
        assets.Scan(root.string(),(root/"Assets").string());AssetRecord ownerAsset,foreignAsset;
        Check(assets.Track((root/"Assets/raw-owner.js").string(),ownerAsset,error)&&assets.Track((root/"Assets/raw-foreign.js").string(),foreignAsset,error),"register adversarial shared-handle fixtures");
        Scene isolated;const auto owner=isolated.CreateObject("raw owner").id;
        isolated.Find(owner)->scripts.push_back({1,ownerAsset.id,true,"{}"});
        const auto foreign=isolated.CreateObject("raw foreign").id;
        isolated.Find(foreign)->scripts.push_back({1,foreignAsset.id,true,"{}"});
        RuntimeWorld ownership;ownership.ConfigureProject(project);
        Check(ownership.Build(isolated,&resources,error),"independent reservation scope owners build");Tick(ownership,input);
        Check(ownership.Scripts()->Diagnostics().empty()&&State(ownership,owner).at("claimed")&&
              State(ownership,foreign).at("rejected")==5&&!State(ownership,foreign).at("available"),
              "foreign raw handle cannot inspect/acquire/release/dispose the actual native claim");
        ownership.DestroyEntity(owner,&error);Tick(ownership,input);
        Check(State(ownership,foreign).at("available"),"automatic raw-scope owner retirement actually frees the target");
        ownership.Destroy();
        fs::remove_all(root);
    }
    std::cout<<"AI reservation scripts: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
} catch(const std::exception& error) {std::cerr<<"AI reservation script test: "<<error.what()<<'\n';return 2;}
