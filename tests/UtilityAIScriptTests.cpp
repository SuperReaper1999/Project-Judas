#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldState.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "InputSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs=std::filesystem;
using J=nlohmann::json;
int checks=0,failures=0;
void Check(bool ok,const std::string& message){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
int main(int argc,char** argv) {
    const bool cold=argc==3;
    const fs::path root=cold?fs::absolute(argv[2]):fs::temp_directory_path()/("judas-ai-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const bool read=cold&&std::string(argv[1])=="read";
    fs::create_directories(root/"Assets");
    if(!read)std::ofstream(root/"Assets/agent.js")<<R"JS(
import {UtilityAgent,console} from 'judas';
const definition={version:1,name:'script-test',interval:.1,actions:[
 {id:'eat',considerations:[{fact:'hunger'}]},
 {id:'hide',considerations:[{fact:'danger'}]}]};
export default class {
 constructor(){this.state={starts:0,stops:0,updates:0,sensors:0,chosen:null};}
 make(){this.brain=new UtilityAgent(definition,{
  eat:{start:()=>this.state.starts++,update:()=>this.state.updates++,stop:()=>{this.state.stops++;console.log('AI_STOP_EAT')}},
  hide:{start:()=>this.state.starts++,update:()=>this.state.updates++,stop:()=>{this.state.stops++;console.log('AI_STOP_HIDE')}}
 },'test brain');}
 start(){this.make();
  let conversions=0,rejected=0;
  const hostile={valueOf:()=>{conversions++;this.brain.dispose();return .1;}};
  try{this.brain.tick(hostile,[]);}catch(e){rejected++;}
  // Use an independent agent: errors intentionally fail the active action.
  const other=new UtilityAgent(definition);
  try{other.finish({toString:()=>{conversions++;other.dispose();return 'success';}});}catch(e){rejected++;}
  other.dispose();this.state.coercions=conversions;this.state.rejected=rejected;
  const completing=new UtilityAgent(definition,{eat:{update:()=>completing.finish('success')}});
  const outcome=completing.tick(.02,[{action:'eat',target:'meal',facts:{hunger:1}}]);
  this.state.completed=outcome.selected===null&&outcome.status==='success'&&outcome.reason==='action success';
  completing.dispose();
  const disposing=new UtilityAgent(definition,{eat:{start:()=>disposing.dispose()}});
  const disposed=disposing.tick(.02,[{action:'eat',target:'meal',facts:{hunger:1}}]);
  this.state.disposed=disposed.selected===null&&disposed.status==='disposed';
 }
 restore(){this.make();this.brain.restore(this.state.saved);}
 fixedUpdate(dt){this.brain.tick(dt,()=>{this.state.sensors++;return [
 {action:'eat',target:'meal',facts:{hunger:.8}},
 {action:'hide',target:'cover',facts:{danger:.2}}];});
 this.state.saved=this.brain.snapshot();this.state.chosen=this.brain.inspection.selected;}
}
)JS";
    AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());
    std::string error;AssetRecord record;
    if(!read)Check(assets.Track((root/"Assets/agent.js").string(),record,error),"ordinary registered definition consumer");
    else {const auto* found=assets.FindByRelativePath("Assets/agent.js");Check(found!=nullptr,"cold process rediscovers exact script asset");if(!found)return 2;record=*found;}
    Scene scene;auto& npc=scene.CreateObject("NPC");npc.scripts.push_back({1,record.id,true,"{}"});const auto id=npc.id;
    ResourceManager resources(nullptr,&assets);RuntimeWorld world;ProjectSettings project;project.legacyGameplay=false;world.ConfigureProject(project);
    InputSystem input;
    if(read){
        std::ifstream file(root/"save.snapshot",std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(file),{}};
        GameSnapshot saved;Scene restored;
        const bool loaded=DecodeGameSnapshot(bytes,saved,error)&&WorldPersistence::SceneFrom(saved.participants,restored,error)&&
            world.Build(restored,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&
            WorldPersistence::Restore(world,saved.participants,error);
        Check(loaded,"fresh process accepts modern physical/script participants: "+error);
        if(!loaded)return 2;
    } else Check(world.Build(scene,&resources,error),"script-owned world builds: "+error);
    for(int i=0;i<(read?1:6);++i)world.FixedScripts(&input,.02f);
    auto* scripts=world.Scripts();Check(scripts&&scripts->Diagnostics().empty(),"real VM lifecycle and native calls remain fault-free");
    const auto state=scripts->Capture();Check(state.size()==1,"one script owner captured");
    if(state.empty())return 2;
    const auto values=J::parse(state[0].json);std::cout<<"STATE "<<values.dump()<<'\n';
    Check(values.value("disposed",false),"disposal from action start leaves no running result or dangling owner");
    Check(values.value("completed",false),"update-to-finish returns the actual completed outcome");
    Check(values.value("coercions",-1)==0&&values.value("rejected",0)==2,"coercion cannot dispose a native agent during a borrowed call");
    Check(values.at("chosen").at("action")=="eat"&&values.at("chosen").at("target")=="meal","real native choice retains target identity");
    Check(values.at("sensors").get<int>()==2,"lazy sensing cadence survives ordinary and restored ticks");
    const auto& continuation=values.at("saved");
    Check(std::abs(continuation.at("elapsed").get<double>()-(read?.14:.12))<1e-6 &&
          std::abs(continuation.at("activeAge").get<double>()-(read?.12:.10))<1e-6,
          "actual native decision clock and commitment continue rather than restarting");
    Check(values.at("starts").get<int>()==(read?2:1),"restored action reenters exactly once; ordinary action remains committed");
    const auto inspection=scripts->AIInspections();Check(inspection.size()==1&&inspection[0].entity==id&&inspection[0].slot==1,"editor sees owning entity/slot and live native decision");
    if(!read){
        GameSnapshot snapshot;snapshot.project="utility-ai-test";snapshot.scene="Scenes/main.judas";snapshot.content=std::string(64,'a');
        Check(WorldPersistence::Capture(world,snapshot.participants,error),"native AI snapshot travels in ordinary modern project state");
        std::ofstream(root/"save.snapshot",std::ios::binary)<<EncodeGameSnapshot(snapshot);
        const auto saved=scripts->Capture();Check(scripts->Reload(true,error),"deliberate script reload preserves declared state");world.FixedScripts(&input,.02f);
        Check(scripts->Diagnostics().empty()&&scripts->AIInspections().size()==1,"reload retires old agent rather than accumulating duplicate owners");
        world.DestroyEntity(id,&error);
        Check(!world.RuntimeDefinition(id),"destroyed owner is unpublished immediately");
        world.FixedScripts(&input,.02f);
        Check(scripts->AIInspections().empty(),"normal lifecycle synchronization retires native AI without an explicit script destroy callback");
    }
    world.Destroy();
    if(!cold) {
        std::ofstream(root/"Assets/owner.js")<<R"JS(
export default class {
 constructor(){this.state={};}
 start(){globalThis.retirements=0;globalThis.testAI=globalThis.__judas('aiCreate',
 JSON.stringify({actions:[{id:'idle'}]}),'ownership control',()=>globalThis.retirements++);}
})JS";
        std::ofstream(root/"Assets/foreign.js")<<R"JS(
export default class {
 constructor(){this.state={rejected:0,retirements:0};}
 start(){for(const op of ['aiInspect','aiSnapshot','aiDispose','aiDue','aiTick','aiRestore','aiFinish']) {
  try{globalThis.__judas(op,globalThis.testAI,op==='aiFinish'?'success':.1,'[]');}
  catch(error){this.state.rejected++;}
 }}
 fixedUpdate(){this.state.retirements=globalThis.retirements;}
})JS";
        assets.Scan(root.string(),(root/"Assets").string());
        AssetRecord ownerAsset,foreignAsset;
        Check(assets.Track((root/"Assets/owner.js").string(),ownerAsset,error)&&
              assets.Track((root/"Assets/foreign.js").string(),foreignAsset,error),"registered adversarial ownership fixtures");
        Scene isolated;auto ownerId=isolated.CreateObject("owner").id;
        isolated.Find(ownerId)->scripts.push_back({1,ownerAsset.id,true,"{}"});
        auto foreignId=isolated.CreateObject("foreign").id;
        isolated.Find(foreignId)->scripts.push_back({1,foreignAsset.id,true,"{}"});
        RuntimeWorld ownership;ownership.ConfigureProject(project);
        Check(ownership.Build(isolated,&resources,error),"independent owners construct");
        ownership.FixedScripts(&input,.02f);
        auto captures=ownership.Scripts()->Capture();
        Check(captures.size()==2&&J::parse(captures[1].json).at("rejected")==7&&
              ownership.Scripts()->AIInspections().size()==1,"all foreign observation/mutation/disposal operations reject before changing the live owner");
        ownership.DestroyEntity(ownerId,&error);ownership.FixedScripts(&input,.02f);
        captures=ownership.Scripts()->Capture();
        Check(captures.size()==1&&J::parse(captures[0].json).at("retirements")==1&&
              ownership.Scripts()->AIInspections().empty(),"unhandled owner removal runs cleanup exactly once at the normal lifecycle boundary");
        ownership.Destroy();
    }
    if(!cold)fs::remove_all(root);
    std::cout<<"Utility AI script: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
