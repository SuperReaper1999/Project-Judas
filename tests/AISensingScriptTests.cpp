#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Project.h"
#include "InputSystem.h"
#include "Simulation.h"
#include "Window.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
namespace fs=std::filesystem;
using J=nlohmann::json;
int checks=0,failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
int main() try {
 const auto root=fs::temp_directory_path()/("judas-ai-sense-script-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root/"Assets");
 std::ofstream(root/"Assets/npc.js")<<R"JS(
import {AISenses,AITargetReservations,UtilityAgent,entity} from 'judas';
const definition={interval:.1,actions:[{id:'eat',weight:2},{id:'idle',weight:.1}]};
export default class {
 constructor({entity}){this.entity=entity;this.state={eaten:0,observed:0,unknown:0,decisions:0};}
 start(){this.senses=new AISenses({interval:.2,maxResults:4,maxRaycasts:2});
 this.budget=new AISenses({interval:.2,maxResults:4,maxRaycasts:0});
 this.claims=new AITargetReservations();this.hasClaim=false;
 this.brain=new UtilityAgent(definition,{
 eat:{start:(ctx,s)=>{this.hasClaim=this.claims.acquire(s.target,.8);},
 update:(dt,ctx,s)=>{
  const food=entity(s.target);
  if(!this.hasClaim||!food?.valid||!this.claims.acquire(s.target,.8)){this.brain.finish('failure');return;}
  const p=this.entity.transform.position,q=food.transform.position;
  const d=Math.hypot(q.x-p.x,q.y-p.y,q.z-p.z);
  if(d<.7){food.destroy();this.state.eaten++;this.brain.finish('success');return;}
  this.entity.setKinematicVelocity({x:(q.x-p.x)/d,y:(q.y-p.y)/d,z:(q.z-p.z)/d},{x:0,y:0,z:0});
 },stop:(ctx,s)=>{this.claims.release(s.target);this.hasClaim=false;if(this.entity.valid)this.entity.setKinematicVelocity({x:0,y:0,z:0},{x:0,y:0,z:0});}},
 idle:{update:()=>this.entity.setKinematicVelocity({x:0,y:0,z:0},{x:0,y:0,z:0})}
 });}
 destroy(){
  let denied=0;
  for(const operation of [()=>this.claims.acquire('during-destroy',1),()=>new AITargetReservations(),()=>new AISenses(),()=>new UtilityAgent(definition)]) {
   try{operation();}catch(error){if(String(error).includes('retiring'))denied++;}
  }
  globalThis.retirementGuards=denied;
  this.senses.dispose();this.budget.dispose();this.brain.dispose();this.claims.dispose();globalThis.senseCleanups=(globalThis.senseCleanups||0)+1;}
 fixedUpdate(dt){this.state.cleanups=globalThis.senseCleanups||0;this.state.retirementGuards=globalThis.retirementGuards||0;this.brain.tick(dt,()=>{
  this.state.decisions++;
  const options={requiredTags:['food'],visibility:true};
  const sensed=this.senses.sample(this.entity.transform.position,10,options);
  this.state.observed+=sensed.targets.length;
  const unknown=this.budget.sample(this.entity.transform.position,10,options);
  this.state.unknown+=unknown.targets.filter(t=>t.visible===null).length;
  return [{action:'idle',target:'',facts:{}},...sensed.targets.map(t=>({action:'eat',target:t.entityId,facts:{},
   eligible:t.visible===true&&this.claims.available(t.entityId),reason:'Occluded, unknown or reserved'}))];
 });}
}
)JS";
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord script;std::string error;
 Check(assets.Track((root/"Assets/npc.js").string(),script,error),"ordinary registered consumer script");
 ProjectSettings project;unsigned tag=0;project.classification.tags.Add("food",tag);
 Scene scene;std::vector<SceneObjectId> actors;
 for(float x:{-1.f,1.f}){auto& npc=scene.CreateObject("NPC");actors.push_back(npc.id);npc.transform.position={x,0,0};
  npc.body=SceneBodyComponent{};npc.body->motion=SceneBodyMotion::Kinematic;npc.body->halfExtents={.2f,.2f,.2f};npc.scripts.push_back({1,script.id,true,"{}"});}
 auto& meal=scene.CreateObject("last meal");const auto mealId=meal.id;meal.transform.position={0,0,3};meal.tags=CategoryBit(tag);meal.body=SceneBodyComponent{};meal.body->halfExtents={.2f,.2f,.2f};
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.ConfigureProject(project);
 Check(world.Build(scene,&resources,error,&project.classification),"two physical NPCs and one tagged food target build");InputSystem input;
 Window window;window.SetTestInputMode(true);
 Check(world.IsBuilt(),"ordinary played-world session starts");
 for(int i=0;i<180;++i)StepPlayedWorld(world,window,.05f);
 Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"sensing, decisions, leases and physical movement execute in real VM without faults");
 unsigned eaten=0,observed=0,unknown=0;for(const auto& r:world.Scripts()->Capture()){
  const auto s=J::parse(r.json);std::cout<<"NPC "<<r.entity<<" "<<s.dump()<<'\n';eaten+=s.at("eaten").get<unsigned>();observed+=s.at("observed").get<unsigned>();unknown+=s.at("unknown").get<unsigned>();
 }
 Check(observed>0&&unknown>0,"real spatial observations include explicit unknown LOS when the ray budget is zero");
 Check(eaten==1&&!world.RuntimeDefinition(mealId),"two independently owned agents cannot both consume the same actual physical target");
 const auto a=world.Physics().GetTransform(world.RuntimeBody(actors[0])).position;
 const auto b=world.Physics().GetTransform(world.RuntimeBody(actors[1])).position;
 Check(a.z>1||b.z>1,"winning action causes actual MK2 movement, not a cached action-name assertion");
 world.DestroyEntity(actors[0],&error);world.FixedScripts(&input,.05f);
 Check(world.Scripts()->Diagnostics().empty(),"owned sensors can dispose after unpublication without faulting later project cleanup");
 const auto remaining=world.Scripts()->Capture();
 Check(remaining.size()==1&&J::parse(remaining[0].json).value("retirementGuards",0)==4,"retiring project callbacks cannot create agents/sensors/scopes or acquire new leases");
 Check(remaining.size()==1&&J::parse(remaining[0].json).value("cleanups",0)==1,"project cleanup after sensor disposal executes exactly once");
 world.Destroy();fs::remove_all(root);std::cout<<checks<<" AI sensing/script checks, "<<failures<<" failures\n";return failures?1:0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
