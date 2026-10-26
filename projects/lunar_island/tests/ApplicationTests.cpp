// Run the shipped game in its ordinary application loop. Only player placement
// and camera aim are fixture operations; resource/inventory/physics edits must
// come from the actual project scripts, ordinary input and native UI.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "SceneSession.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "json.hpp"
#include "WorldStreaming.h"
#include <memory>
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <cmath>
using Json=nlohmann::json;
namespace {
unsigned checks=0,failed=0;int logicalLookX=0,logicalLookY=0;
void Check(bool ok,const std::string& text){++checks;failed+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<text<<std::endl;}
void Quit(){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
Json Read(const std::filesystem::path& p){std::ifstream f(p);return Json::parse(f);}
void Write(const std::filesystem::path& p,const Json& j){std::ofstream(p)<<j.dump(2)<<'\n';}
Json State(RuntimeWorld& w){for(auto& r:w.Scripts()->Capture())if(r.entity==100&&r.slot==1)return Json::parse(r.json);return Json::object();}
Json PersonState(RuntimeWorld& w,EntityId id){for(auto& r:w.Scripts()->Capture())if(r.entity==id&&r.slot==1)return Json::parse(r.json);return Json::object();}
Json DistrictState(RuntimeWorld& w,const std::string& region){auto* stream=w.SceneControl()->StreamingIfLoaded();for(auto& row:w.Scripts()->Capture())if(row.entity!=100&&stream->Owner(row.entity)==region){auto state=Json::parse(row.json);if(state.contains("housing"))return state;}return Json::object();}
void StateFixture(RuntimeWorld& w,std::function<void(Json&)> edit){auto rows=w.Scripts()->Capture();for(auto& r:rows)if(r.entity==100&&r.slot==1){auto j=Json::parse(r.json);edit(j);r.json=j.dump();}std::string e;if(!w.RestoreScriptState(rows,e))throw std::runtime_error(e);}
glm::vec3 V(const Json& j){return {j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};}
Json J(glm::vec3 v){return {v.x,v.y,v.z};}
unsigned Count(const Json& s,const std::string& item){unsigned n=0;for(auto& row:s["bag"]["slots"])if(!row.is_null()&&row["item"]==item)n+=row["count"].get<unsigned>();return n;}
void Aim(RuntimeWorld& w,glm::vec3 target,float ground,float distance=2,glm::vec3 backward={0,0,1}){auto t=w.RuntimeDefinition(100)->transform;PhysicsQueryFilter groundFilter;groundFilter.requiredTags=1;auto fixturePoint=target+backward*distance;auto groundHit=w.Physics().Raycast({fixturePoint.x,200,fixturePoint.z},{0,-1,0},400,groundFilter);if(groundHit.hit)ground=groundHit.point.y;t.position={fixturePoint.x,ground+.05f,fixturePoint.z};t.rotation={1,0,0,0};if(!w.SetRuntimeTransform(100,t))throw std::runtime_error("fixture placement failed");w.RuntimeCharacter(100)->velocity={};float pitch=std::atan2(target.y-t.position.y-1.6f,distance);StateFixture(w,[&](Json& s){s["yaw"]=std::atan2(backward.x,backward.z);s["pitch"]=pitch;s["third"]=false;});}
void Screenshot(EngineHost& h,const std::filesystem::path& p){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng(p.string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"capture "+p.filename().string());}
bool Healthy(RuntimeWorld& w){for(auto& d:w.Scripts()->Diagnostics()){std::cout<<"SCRIPT "<<d.callback<<' '<<d.message<<std::endl;return false;}return true;}
void AimEntityFixture(RuntimeWorld& w,EntityId id,float offset=0){
 auto body=w.RuntimeBody(id);if(!body.IsValid())throw std::runtime_error("aimed fixture missing "+std::to_string(id));auto target=w.Physics().GetTransform(body).position+glm::vec3(0,offset,0);PhysicsQueryFilter f;f.ignoredBodies.push_back(w.RuntimeBody(100));
 for(float distance:{2.f,2.5f,1.6f})for(int i=0;i<16;++i){float angle=i*6.2831853f/16;glm::vec3 side{std::sin(angle),0,std::cos(angle)},foot{target.x+side.x*distance,14.23f,target.z+side.z*distance};auto block=w.Physics().CapsuleCast({foot+glm::vec3(0,.9f,0),{1,0,0,0}},.3f,.6f,{0,1,0},0,f);if(block.hit)continue;auto eye=foot+glm::vec3(0,1.6f,0);auto d=target-eye;auto hit=w.Physics().Raycast(eye,glm::normalize(d),glm::length(d)+.05f,f);if(!hit.hit||w.EntityIdOfBody(hit.body)!=id)continue;auto t=w.RuntimeDefinition(100)->transform;t.position=foot;t.rotation={1,0,0,0};w.SetRuntimeTransform(100,t);w.RuntimeCharacter(100)->velocity={};StateFixture(w,[&](Json& s){s["yaw"]=angle;s["pitch"]=std::atan2(d.y,distance);s["third"]=false;});return;}
 throw std::runtime_error("No unobstructed physical test viewpoint for "+std::to_string(id));
}
void AimVehicleFixture(RuntimeWorld& w,EntityId id){
 auto body=w.RuntimeBody(id);auto target=w.Physics().GetTransform(body).position;PhysicsQueryFilter f;f.ignoredBodies.push_back(w.RuntimeBody(100));
 for(float distance:{2.5f,3.5f,4.5f,5.5f})for(int i=0;i<24;++i){float angle=i*6.2831853f/24;glm::vec3 side{std::sin(angle),0,std::cos(angle)},foot=target+side*distance;auto ground=w.Physics().Raycast(foot+glm::vec3(0,5,0),{0,-1,0},20,f);if(!ground.hit||w.EntityIdOfBody(ground.body)==id)continue;foot.y=ground.point.y+.05f;auto block=w.Physics().CapsuleCast({foot+glm::vec3(0,.9f,0),{1,0,0,0}},.3f,.6f,{0,1,0},0,f);if(block.hit)continue;auto eye=foot+glm::vec3(0,1.6f,0),d=target-eye;auto hit=w.Physics().Raycast(eye,glm::normalize(d),3.8f,f);if(!hit.hit||w.EntityIdOfBody(hit.body)!=id)continue;auto t=w.RuntimeDefinition(100)->transform;t.position=foot;t.rotation={1,0,0,0};w.SetRuntimeTransform(100,t);w.RuntimeCharacter(100)->velocity={};StateFixture(w,[&](Json& s){s["yaw"]=angle;s["pitch"]=std::atan2(d.y,distance);s["third"]=false;});return;}
 throw std::runtime_error("No usable vehicle interaction viewpoint "+std::to_string(id));
}
glm::vec3 ClearBuildPoint(RuntimeWorld& w){PhysicsQueryFilter ground;ground.requiredTags=1;for(float z:{38.f,44.f,50.f,56.f})for(float x:{2.f,8.f,-2.f,14.f}){auto hit=w.Physics().Raycast({x,100,z},{0,-1,0},200,ground);if(!hit.hit)continue;PhysicsQueryFilter clear;clear.ignoredBodies={hit.body,w.RuntimeBody(100)};glm::vec3 p=hit.point;auto overlap=w.Physics().BoxCast({p+glm::vec3(0,.218f,0),{1,0,0,0}},{1.41f,.188f,1.41f},{0,1,0},0,clear);if(!overlap.hit)return p;}throw std::runtime_error("No clear construction fixture area");}
EntityId FindNamed(RuntimeWorld& w,const std::string& name){for(auto& e:w.ScriptObjects())if(e.name==name&&w.RuntimeBody(e.id).IsValid())return e.id;return 0;}
unsigned StoredCount(const Json& s,const std::string& item){unsigned n=0;for(auto& bag:s["housing"]["containers"])for(auto& row:bag["slots"])if(!row.is_null()&&row["item"]==item)n+=row["count"].get<unsigned>();return n;}
int BarricadeHealth(const Json& s){for(auto& r:s["housing"]["objects"])if(r.value("barricade",false))return r["health"].get<int>();return -1;}
void AimLive(EngineHost& h,RuntimeWorld& w,glm::vec3 target,float ground,float distance){auto t=w.RuntimeDefinition(100)->transform;t.position={target.x,ground+.05f,target.z+distance};t.rotation={1,0,0,0};w.SetRuntimeTransform(100,t);w.RuntimeCharacter(100)->velocity={};auto state=State(w);float desired=std::atan2(target.y-t.position.y-1.6f,distance),sensitivity=state["sensitivity"].get<float>();logicalLookX=int(std::round(state["yaw"].get<float>()/sensitivity));logicalLookY=int(std::round((state["pitch"].get<float>()-desired)/sensitivity));(void)h;}

}
int main(int argc,char** argv){
 if(argc!=4){std::cerr<<"usage: observer project write|read|performance output\n";return 2;}
 auto project=std::filesystem::absolute(argv[1]);std::string mode=argv[2];auto out=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(out);
 auto data=Read(project.parent_path()/"Authoring/world_manifest.json");
 auto resources=data["resources"];Json tree,rock;for(auto& r:resources){if(r["kind"]=="tree"&&tree.is_null())tree=r;if(r["kind"]=="rock"&&rock.is_null())rock=r;}
 EntityId treeId=std::stoull(tree["id"].get<std::string>());glm::vec3 treeBase=V(tree["position"]),start{},pauseStart{};Json expected;
 bool reverseThrottle=false;
 Json stage5Expected;
 std::ofstream drivingTimes; if(mode=="roadtrip"){drivingTimes.open(out/"driving-timings.csv");drivingTimes<<"frame,frame_ms,physics_ms,bodies\n";}
 bool roadActive=false;EntityId roadVehicle=0;float roadMaxDeviation=0;
 EntityId restoredVehicle=0;unsigned restoredWood=0;
 EntityId metalBoard=0,brokenBoard=0,builtFoundation=0,builtWall=0,builtChest=0,builtRoof=0,builtDoor=0;glm::vec3 wallBefore{},buildPoint{};
 unsigned frame=0,due=12,index=0;std::vector<std::string> pulse,held;std::map<std::string,std::string> bindings;uint64_t request=0;bool capture=false,done=false,restoreObserved=false;unsigned restoreFrame=0;double measured=0;std::vector<double> frames,physics;std::ofstream times(out/(mode+"-timings.csv"));times<<"frame,frame_ms,physics_ms,bodies\n";
 using Job=std::pair<unsigned,std::function<void(EngineHost&,RuntimeWorld&)>>;std::vector<Job> jobs;
 auto job=[&](unsigned wait,std::function<void(EngineHost&,RuntimeWorld&)> fn){jobs.emplace_back(wait,fn);};
 auto press=[&](std::string a){pulse.push_back(a);};
 auto click=[&](const std::string& id){job(5,[&,id](EngineHost&,RuntimeWorld& w){Check(w.UI().Focus(w.UI().Find("lunar"),id),"focus UI "+id);press("ui_activate");});};
 auto select=[&](int row){job(3,[](EngineHost&,RuntimeWorld& w){auto h=w.UI().Find("lunar");w.UI().Select(h,"entries",-1);w.UI().Focus(h,"entries");});for(int i=0;i<=row;++i)job(3,[&](EngineHost&,RuntimeWorld&){press("ui_down");});};
 auto gather=[&](Json r){job(4,[&,r](EngineHost&,RuntimeWorld& w){auto p=V(r["position"]);Aim(w,p+glm::vec3(0,.15f,0),p.y);});job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});job(3,[&,r](EngineHost&,RuntimeWorld& w){Check(!w.RuntimeBody(std::stoull(r["id"].get<std::string>())).IsValid(),"actual E depletes "+r["kind"].get<std::string>()+" "+r["id"].get<std::string>());});};
 job(5,[&](EngineHost& h,RuntimeWorld& w){Check(w.UI().Paused(),"welcome pauses actual world");Check(w.Physics().AliveBodyCount()>100,"authored world collision loaded");Screenshot(h,out/"welcome.png");press("pause");});
 job(8,[&](EngineHost&,RuntimeWorld& w){Check(!w.UI().Paused(),"Escape begins real game");start=w.RuntimeDefinition(100)->transform.position;held={"move_y"};});
 job(50,[&](EngineHost&,RuntimeWorld& w){held.clear();Check(glm::distance(start,w.RuntimeDefinition(100)->transform.position)>1,"CharacterMotor moves via project input");press("camera_toggle");});
 job(5,[&](EngineHost&,RuntimeWorld& w){Check(State(w)["third"].get<bool>(),"same player toggles third person");pauseStart=w.RuntimeDefinition(100)->transform.position;press("pause");});
 job(5,[&](EngineHost&,RuntimeWorld& w){Check(w.UI().Paused(),"pause menu stops gameplay");pauseStart=w.RuntimeDefinition(100)->transform.position;held={"move_y","primary"};});
 job(35,[&](EngineHost&,RuntimeWorld& w){held.clear();Check(glm::distance(pauseStart,w.RuntimeDefinition(100)->transform.position)<1e-5f,"move/fire do not leak through modal");press("pause");});
 if(mode=="harvestwrite"||mode=="harvestread"){
#include "Stage5Harvest.inc"
 }else if(mode=="harbourskiff"){
#include "Stage5Skiff.inc"
 }else if(mode=="fieldmap"){
#include "Stage5Map.inc"
 }else if(mode=="claimhome"){
#include "Stage5Home.inc"
 }else if(mode=="worldwrite"||mode=="worldread"){
#include "Stage5World.inc"
 }else if(mode=="roadtrip"){
#include "Stage5Road.inc"
 }else if(mode=="islandtour"){
#include "Stage5Tour.inc"
 }else if(mode=="vehicleprefab"){
#include "Stage4Prefab.inc"
 }else if(mode.rfind("land-",0)==0){
#include "Stage4Landing.inc"
 }else if(mode=="garagestream"){
#include "Stage4Streaming.inc"
 }else if(mode=="cargo"){
#include "Stage4Cargo.inc"
 }else if(mode.rfind("drive-",0)==0){
#include "Stage4Drive.inc"
 }else if(mode=="pursuit"){
  job(6,[&](EngineHost&,RuntimeWorld& w){Aim(w,w.Physics().GetTransform(w.RuntimeBody(6210)).position,14,8);pauseStart=w.RuntimeCharacter(6210)->position;});
  job(100,[&](EngineHost& h,RuntimeWorld& w){auto a=w.Navigation().Agent(6210);auto start=w.RuntimeCharacter(6210)->position,goal=w.RuntimeCharacter(100)->position;auto definition=w.RuntimeDefinition(6900);std::cout<<"SURFACE DEFINITION "<<(definition?1:0)<<" enabled "<<(definition&&definition->navigationSurface?definition->navigationSurface->enabled:false)<<" asset "<<(definition&&definition->navigationSurface?definition->navigationSurface->asset:"none")<<std::endl;auto stats=w.Navigation().Statistics();for(auto& e:w.Navigation().Errors())std::cout<<"NAV ERROR "<<e.first<<" "<<e.second<<std::endl;std::cout<<"NAV STATS surfaces "<<stats.surfaces<<" tiles "<<stats.tiles<<" polygons "<<stats.polygons<<std::endl;auto far=w.Navigation().Sample(start,100);std::cout<<"FAR SAMPLE "<<(far?J(far->position).dump():"none")<<std::endl;for(float range:{2.f,10.f}){auto a=w.Navigation().Sample(start,range),b=w.Navigation().Sample(goal,range);std::cout<<"NAV SAMPLE "<<range<<" start "<<(a?J(a->position).dump():"none")<<" end "<<(b?J(b->position).dump():"none")<<std::endl;}std::cout<<"PURSUIT motor "<<J(w.RuntimeCharacter(6210)->position).dump()<<" definition "<<J(w.RuntimeDefinition(6210)->transform.position).dump()<<" start "<<J(pauseStart).dump()<<" velocity "<<J(w.RuntimeCharacter(6210)->velocity).dump()<<" status "<<(a?int(a->path.status):-1)<<" steering "<<(a?J(a->steering).dump():"none")<<" target "<<(a?J(a->destination).dump():"none")<<" state "<<PersonState(w,6210).dump()<<std::endl;Screenshot(h,out/"pursuit.png");Check(glm::distance(pauseStart,w.RuntimeCharacter(6210)->position)>.5f,"real motor pursuit advances");std::string navError;auto navData=h.Resources().GetNavigation(definition->navigationSurface->asset,navError);std::cout<<"NAV RESOURCE "<<bool(navData)<<" error "<<navError<<std::endl;w.Navigation().Update(w,1.f/60);std::cout<<"DIRECT UPDATE surfaces "<<w.Navigation().Statistics().surfaces<<std::endl;for(auto& e:w.Navigation().Errors())std::cout<<"DIRECT ERROR "<<e.first<<" "<<e.second<<std::endl;done=true;Quit();});
 }else if(mode=="barrier"){
#include "Stage3Barrier.inc"
 }else if(mode=="encounter"){
#include "Stage3Scenario.inc"
 }else if(mode=="write"){
  // Gather ingredients through the physical query and ordinary E input.
  int branches=0,stones=0,fibers=0,berries=0;
  for(auto& r:resources){auto k=r["kind"].get<std::string>();if((k=="branch"&&branches++<4)||(k=="loose_stone"&&stones++<3)||(k=="fiber"&&fibers++<2)||(k=="berries"&&berries++<2))gather(r);}
  job(4,[&](EngineHost&,RuntimeWorld& w){auto s=State(w);Check(Count(s,"wood")==8&&Count(s,"stone")==6&&Count(s,"fiber")==8&&Count(s,"berries")==8,"physical gathering credits exact inventory quantities");press("inventory");});
  click("tab_craft");select(0);click("primary");
  job(4,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"stone_axe")==1&&Count(State(w),"wood")==5,"native crafting makes axe and debits ingredients");});
  select(1);click("primary");
  job(4,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"stone_pickaxe")==1&&Count(State(w),"wood")==1,"native crafting makes pickaxe");press("pause");});
  job(5,[&](EngineHost&,RuntimeWorld& w){StateFixture(w,[](Json& s){s["third"]=false;});Aim(w,treeBase+glm::vec3(0,1.2f,0),treeBase.y);press("hotbar_2");});
  for(int i=0;i<3;++i)job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(180,[&](EngineHost& h,RuntimeWorld& w){auto s=State(w);auto b=w.RuntimeBody(treeId);Check(s["harvest"][tree["id"].get<std::string>()]["felled"]==true,"third real axe hit fells tree");Check(w.Physics().IsDynamicBody(b),"felled trunk is actual dynamic body");auto t=w.Physics().GetTransform(b);Check(glm::dot(t.rotation*glm::vec3(0,1,0),glm::vec3(0,1,0))<.95f,"tree physically tips under impulse/gravity");Screenshot(h,out/"felled-tree.png");Aim(w,t.position,treeBase.y);});
  // Process a different physical tree; the first remains for cold-save proof.
  Json timberTree;bool skipped=false;for(auto& r:resources)if(r["kind"]=="tree"){if(!skipped){skipped=true;continue;}timberTree=r;break;}
  EntityId timberId=std::stoull(timberTree["id"].get<std::string>());auto timberBase=V(timberTree["position"]);
  job(5,[&,timberBase](EngineHost&,RuntimeWorld& w){Aim(w,timberBase+glm::vec3(0,1.2f,0),timberBase.y);});
  for(int i=0;i<3;++i)job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(180,[](EngineHost&,RuntimeWorld&){});
  for(int i=0;i<3;++i){job(5,[&,timberId,timberBase](EngineHost&,RuntimeWorld& w){auto pose=w.Physics().GetTransform(w.RuntimeBody(timberId));auto side=glm::cross(pose.rotation*glm::vec3(0,1,0),glm::vec3(0,1,0));side=glm::length(side)>.1f?glm::normalize(side):glm::vec3(0,0,1);Aim(w,pose.position+glm::vec3(0,.1f,0),timberBase.y,2.2f,side);});job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});}
  job(5,[&,timberId](EngineHost&,RuntimeWorld& w){Check(State(w)["stats"]["timber"]==12&&Count(State(w),"wood")==13,"fallen timber processing produces exact building resource quantity");Check(!w.RuntimeBody(timberId).IsValid(),"processed physical trunk removed only after successful credit");});
  // Keep the first fallen tree unprocessed for exact cold physical persistence.
  job(5,[&](EngineHost&,RuntimeWorld& w){auto p=V(rock["position"]);Aim(w,p+glm::vec3(0,.2f,0),p.y);press("hotbar_3");});
  for(int i=0;i<6;++i)job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"stone")==7,"six real pickaxe hits yield six stone");Check(!w.RuntimeBody(std::stoull(rock["id"].get<std::string>())).IsValid(),"depleted deposit removed from physical world");auto c=V(data["campfire"]);Aim(w,c+glm::vec3(0,.5f,0),c.y);press("inventory");});
  click("tab_craft");select(3);click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"cooked_meal")==1&&Count(State(w),"berries")==3,"campfire crafting consumes real gathered berries/wood");press("pause");});
  job(5,[&](EngineHost&,RuntimeWorld& w){auto p=V(data["spring"]);Aim(w,p+glm::vec3(0,.9f,0),p.y);});
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(5,[&](EngineHost& h,RuntimeWorld& w){Screenshot(h,out/"well-materials.png");Check(Count(State(w),"water")==1&&Count(State(w),"empty_flask")==0,"well refills reusable flask through interaction");press("inventory");});
  job(5,[&](EngineHost&,RuntimeWorld&){press("pause");});
  job(5,[&](EngineHost&,RuntimeWorld&){press("hotbar_5");});
  job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"empty_flask")==1&&Count(State(w),"water")==0,"drinking returns reusable empty flask");Check(State(w)["vitals"]["thirst"].get<float>()>99,"consumable actually restores thirst");press("inventory");});
  // Pack's first slot is the starting coat. Dropping verifies real prefab creation.
  select(0);click("drop");
  job(5,[&](EngineHost&,RuntimeWorld& w){auto s=State(w);Check(s["drops"].size()==1&&Count(s,"work_coat")==0,"drop creates real bundle and debits only on success");if(s["drops"].size()){auto id=std::stoull(s["drops"].begin().key());Check(w.Physics().IsDynamicBody(w.RuntimeBody(id)),"dropped supplies have dynamic physical collision");}press("pause");});
  job(8,[&](EngineHost&,RuntimeWorld& w){auto drops=State(w)["drops"];if(drops.size()){auto p=w.Physics().GetTransform(w.RuntimeBody(std::stoull(drops.begin().key()))).position;Aim(w,p,p.y-.22f,1.5f);}});
  job(6,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(6,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"work_coat")==1&&State(w)["drops"].empty(),"E recovers actual dropped bundle and returns its item exactly once");});
#include "Stage2Scenario.inc"
  job(5,[&](EngineHost&,RuntimeWorld& w){Aim(w,{90,9,-100},8.1f);held={"jump"};});
  job(65,[&](EngineHost&,RuntimeWorld& w){held.clear();std::cout<<"SWIM endpoint "<<J(w.RuntimeDefinition(100)->transform.position).dump()<<" sample "<<w.SampleWater({90,9,-100},{0,1,0},.55f,.3f).immersion<<std::endl;Check(!w.AdvancedFluidsEnabled()&&!w.HasFluid()&&w.BasicWaterEnabled()&&w.WaterSwimmingEnabled(),"prescribed water enabled independently of advanced fluid solvers");Check(w.RuntimeDefinition(100)->transform.position.y>8.4f,"basic-water swimming lifts real motor without advanced fluids");Check(State(w)["vitals"]["health"].get<float>()>0,"swimming remains survivable");held={"dive"};});
  job(180,[&](EngineHost&,RuntimeWorld& w){held.clear();Check(State(w)["vitals"]["breath"].get<float>()<22,"submerged head consumes actual survival air supply");Check(State(w)["vitals"]["hunger"].get<float>()<75,"survival hunger progresses in actual fixed steps");Aim(w,V(data["spawn"])+glm::vec3(0,1,0),data["spawn"][1].get<float>());press("save");});
  job(30,[&](EngineHost&,RuntimeWorld&){/* wait for game-issued save request */});
 }else if(mode=="read"||mode=="encounterread"||mode=="cargoread"||mode=="garageread"){
  job(5,[&](EngineHost&,RuntimeWorld& w){std::string error;request=w.SceneControl()->Saves(*w.Resources())->Request("load","island","","{}",error);Check(request!=0,"fresh process requests ordinary save load: "+error);});
 }else if(mode=="build"){
  for(auto& r:resources)if((r["kind"]=="branch"&&std::stoull(r["id"].get<std::string>())<212)||(r["kind"]=="loose_stone"&&std::stoull(r["id"].get<std::string>())<256))gather(r);
  job(5,[&](EngineHost&,RuntimeWorld&){press("build_menu");});select(0);click("primary");job(6,[&](EngineHost& h,RuntimeWorld& w){buildPoint=ClearBuildPoint(w);AimLive(h,w,buildPoint,buildPoint.y,3.5f);});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost&,RuntimeWorld& w){builtFoundation=FindNamed(w,"Stone foundation");Check(builtFoundation!=0,"focused foundation placement succeeds");press("build_menu");});select(2);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){if(builtFoundation){auto p=w.Physics().GetTransform(w.RuntimeBody(builtFoundation)).position;AimLive(h,w,p+glm::vec3(0,.2f,0),buildPoint.y,3.5f);}});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost& h,RuntimeWorld& w){builtWall=FindNamed(w,"Timber wall");std::cout<<"WALL FEEDBACK "<<w.UI().Element(w.UI().Find("lunar"),"notice")->text<<" context "<<w.UI().Element(w.UI().Find("lunar"),"context")->text<<" state "<<State(w)["housing"]["stats"].dump()<<std::endl;Check(builtWall&&w.RuntimeJoint(builtWall).IsValid(),"focused wall snap creates actual body and joint");Screenshot(h,out/"build.png");done=true;Quit();});
 }else if(mode=="door"){
  job(6,[&](EngineHost&,RuntimeWorld& w){AimEntityFixture(w,3061);});job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(120,[&](EngineHost& h,RuntimeWorld& w){JointState s;bool joint=w.Physics().GetJoint(w.RuntimeJoint(3061),s);auto b=w.RuntimeBody(3061);std::cout<<"DOOR DETAIL valid "<<joint<<" active "<<s.active<<" rest "<<s.settings.rest<<" coordinate "<<s.coordinate<<" stiffness "<<s.settings.stiffness<<" limits "<<s.settings.lower<<' '<<s.settings.upper<<" velocity "<<J(w.Physics().GetAngularVelocity(b)).dump()<<" pose "<<J(w.Physics().GetTransform(b).position).dump()<<std::endl;
  std::vector<PhysicsWorld::BodyContactSample> contacts;w.Physics().ReadBodyContactSample(b,contacts);for(auto& c:contacts)std::cout<<"DOOR CONTACT entity "<<w.EntityIdOfBody(c.other)<<" point "<<J(c.point).dump()<<" normal "<<J(c.normal).dump()<<" separation "<<c.separation<<std::endl;Screenshot(h,out/"door.png");Check(joint&&s.coordinate< -1,"actual opened door reaches intended inward angle");done=true;Quit();});
 }else{
  job(10,[&](EngineHost&,RuntimeWorld&){held={"move_y"};});
 }
 ApplicationControl control;control.hidden=true;
 control.hostReady=[&](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);PerformanceProfiler::Get().Enable(true);};
 control.frameSeconds=[&](float dt){measured=dt*1000;return 1.f/60;};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){try{
   if((mode=="write"||mode=="encounter"||mode=="cargo"||mode=="garagestream"||mode=="worldwrite"||mode=="harvestwrite")&&!capture){auto* service=w.SceneControl()->Saves(h.Resources());for(uint64_t id=1;id<12;++id){auto* s=service->Status(id);if(s&&s->operation=="save"&&s->state=="writing"){expected={{"state",State(w)},{"treePosition",J(w.Physics().GetTransform(w.RuntimeBody(treeId)).position)},{"treeRotation",{w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.w,w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.x,w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.y,w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.z}}};expected["physical"]=Json::array();for(auto entity:{EntityId(3120),builtWall,builtChest,builtRoof,builtDoor,metalBoard})if(entity&&w.RuntimeBody(entity).IsValid()){auto pose=w.Physics().GetTransform(w.RuntimeBody(entity));expected["physical"].push_back({{"entity",entity},{"position",J(pose.position)},{"rotation",{pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z}},{"joint",w.RuntimeJoint(entity).IsValid()}});}if(mode=="encounter"){expected["people"]=Json::array();for(EntityId npc:{6000,6100,6200,6210,6300,6310}){auto state=PersonState(w,npc);Json row={{"entity",npc},{"state",state}};if(state.contains("corpse")&&!state["corpse"].is_null()){EntityId corpse=std::stoull(state["corpse"].get<std::string>().substr(7));auto handle=w.RuntimeBody(corpse);row["corpse"]=corpse;row["position"]=J(w.Physics().GetTransform(handle).position);}expected["people"].push_back(row);}}if(mode=="cargo"||mode=="garagestream"){expected["vehicles"]=Json::array();for(EntityId entity:{7000,7100,7200,7300,7400,7500,7800}){auto b=w.RuntimeBody(entity);auto t=w.Physics().GetTransform(b);expected["vehicles"].push_back({{"entity",entity},{"state",PersonState(w,entity)},{"position",J(t.position)},{"rotation",{t.rotation.w,t.rotation.x,t.rotation.y,t.rotation.z}}});}}if(mode=="garagestream"){auto* stream=w.SceneControl()->StreamingIfLoaded();auto ref=State(w)["transport"]["driving"].get<std::string>();auto entity=stream->ResolvePersistentKey(ref);auto pose=w.Physics().GetTransform(w.RuntimeBody(entity));expected["vehicles"].push_back({{"entity",entity},{"reference",ref},{"state",PersonState(w,entity)},{"position",J(pose.position)}});}if(mode=="worldwrite")expected["island"]=stage5Expected;if(mode=="harvestwrite")expected["regionalHarvest"]=stage5Expected;Write(out/"expected.json",expected);request=id;capture=true;}}}
   if(index<jobs.size()&&frame>=due&&!(index==0&&w.RuntimeDefinition(6900)&&!w.UI().Paused())){auto item=jobs[index++];item.second(h,w);due=frame+(index<jobs.size()?jobs[index].first:10);}
   for(auto& entry:h.GetWindow().Input().Map().entries){for(auto& b:entry.bindings)h.GetWindow().QueueTestPhysical(b.control,0);for(auto& b:entry.bindings)if(b.scale>0){bindings[entry.name]=b.control;break;}}
   for(auto& a:held)h.GetWindow().QueueTestPhysical(bindings.at(a),1);
   for(auto& a:pulse){h.GetWindow().QueueTestPhysical(bindings.at(a),1);}
   pulse.clear();
   if(roadActive){auto t=w.Physics().GetTransform(w.RuntimeBody(roadVehicle));auto pos=glm::vec2(t.position.x,t.position.z);const glm::vec2 a{-500,180},goal{-1400,200};auto dir=glm::normalize(goal-a);float distance=glm::dot(pos-a,dir),cross=glm::length(pos-(a+dir*distance));roadMaxDeviation=std::max(roadMaxDeviation,cross);auto vel=w.Physics().GetLinearVelocity(w.RuntimeBody(roadVehicle));float speed=glm::length(vel);auto look=a+dir*std::min(glm::length(goal-a),distance+std::max(8.f,speed*.6f));auto delta=glm::normalize(glm::vec3(look.x-t.position.x,0,look.y-t.position.z));auto forward=t.rotation*glm::vec3(0,0,-1),right=t.rotation*glm::vec3(1,0,0);float angle=std::atan2(glm::dot(delta,right),glm::dot(delta,forward));h.GetWindow().QueueTestPhysical("key:D",std::max(0.f,std::min(1.f,angle*1.5f)));h.GetWindow().QueueTestPhysical("key:A",std::max(0.f,std::min(1.f,-angle*1.5f)));h.GetWindow().QueueTestPhysical("key:W",speed<14?1:0);if(cross>8)throw std::runtime_error("physical road vehicle departed the surfaced corridor");}
   if(reverseThrottle)h.GetWindow().QueueTestPhysical("key:S",1);
   h.GetWindow().QueueTestPhysical("mouse:dx",float(logicalLookX));h.GetWindow().QueueTestPhysical("mouse:dy",float(logicalLookY));logicalLookX=logicalLookY=0;
 }catch(const std::exception& e){Check(false,e.what());done=true;Quit();}};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){++frame;try{
   if(!Healthy(w))throw std::runtime_error("game script failure");
   if(roadActive)drivingTimes<<frame<<','<<measured<<','<<w.Physics().LastStepStats().totalMilliseconds<<','<<w.Physics().AliveBodyCount()<<'\n';
   times<<frame<<','<<measured<<','<<w.Physics().LastStepStats().totalMilliseconds<<','<<w.Physics().AliveBodyCount()<<'\n';
   if(index==jobs.size()&&frame>due){
    if(!done&&h.Resources().Stats().failed)throw std::runtime_error("native resource decoding/upload failure");
    if(mode=="write"||mode=="encounter"||mode=="cargo"||mode=="garagestream"||mode=="worldwrite"||mode=="harvestwrite"){auto* s=w.SceneControl()->Saves(h.Resources())->Status(request);if(s&&(s->state=="completed"||s->state=="failed")){Check(capture,"independently observed actual save capture boundary");Check(s->state=="completed","game-issued durable save completes: "+s->error);Write(out/"final-state.json",State(w));done=true;Quit();}}
    if(mode=="read"&&!restoreObserved){auto* s=w.SceneControl()->Saves(h.Resources())->Status(request);if(s&&(s->state=="completed"||s->state=="failed")){Check(s->state=="completed","fresh process restores save: "+s->error);auto saved=Read(out/"expected.json");Check(State(w)["bag"]==saved["state"]["bag"],"cold save restores exact inventory");Check(State(w)["harvest"]==saved["state"]["harvest"],"cold save restores exact depletion/felling state");Check(glm::distance(w.Physics().GetTransform(w.RuntimeBody(treeId)).position,V(saved["treePosition"]))<.01f,"cold save restores actual fallen tree endpoint");auto r=saved["treeRotation"];glm::quat savedRotation{r[0].get<float>(),r[1].get<float>(),r[2].get<float>(),r[3].get<float>()};Check(std::abs(glm::dot(w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation,savedRotation))>1-1e-5f,"cold save restores actual fallen trunk orientation");Check(!w.RuntimeBody(200).IsValid(),"cold restored depleted resource stays absent");Check(w.Physics().IsDynamicBody(w.RuntimeBody(treeId)),"restored fallen tree remains dynamic");Check(State(w)["housing"]==saved["state"]["housing"],"cold save restores exact storage, ownership, integrity, home and door ledgers");for(auto& observation:saved["physical"]){EntityId id=observation["entity"].get<EntityId>();auto body=w.RuntimeBody(id);Check(body.IsValid(),"cold restore retains actual Stage 2 body "+std::to_string(id));if(body.IsValid()){Check(glm::distance(w.Physics().GetTransform(body).position,V(observation["position"]))<.03f,"cold restore physical endpoint "+std::to_string(id));Check(w.RuntimeJoint(id).IsValid()==observation["joint"].get<bool>(),"cold restore actual attachment state "+std::to_string(id));}}bool off=false;for(auto& l:w.StaticLights())if(l.id==3108)off=!l.light.enabled;Check(off,"cold save restores actual native lamp-off state");Screenshot(h,out/"cold-restored.png");restoreObserved=true;restoreFrame=frame;}}
    if(mode=="read"&&restoreObserved&&frame>=restoreFrame+120){auto saved=Read(out/"expected.json");for(auto& entry:saved["state"]["housing"]["doors"].items()){if(!entry.value().value("opened",false))continue;EntityId id=std::stoull(entry.key().substr(7));JointState joint;bool found=w.Physics().GetJoint(w.RuntimeJoint(id),joint);std::cout<<"RESTORED HINGE "<<id<<" found "<<found<<" rest "<<joint.settings.rest<<" coordinate "<<joint.coordinate<<" stiffness "<<joint.settings.stiffness<<std::endl;if(found&&joint.coordinate>= -1){std::vector<PhysicsWorld::BodyContactSample> rows;w.Physics().ReadBodyContactSample(w.RuntimeBody(id),rows);for(auto& row:rows)std::cout<<"RESTORED DOOR CONTACT "<<w.EntityIdOfBody(row.other)<<" separation "<<row.separation<<std::endl;}float physicalAngle=0;if(found){auto fa=w.Physics().GetTransform(joint.settings.bodyA).rotation*joint.settings.frameA,fb=w.Physics().GetTransform(joint.settings.bodyB).rotation*joint.settings.frameB;auto x=fa*glm::vec3(1,0,0),y=fa*glm::vec3(0,1,0),by=fb*glm::vec3(0,1,0);physicalAngle=std::atan2(glm::dot(x,glm::cross(y,by)),glm::dot(y,by));}std::cout<<"ACTUAL HINGE ANGLE "<<id<<" "<<physicalAngle<<std::endl;Check(found&&physicalAngle< -1,"cold restored open hinge remains physically open after 120 steps "+std::to_string(id));}Check(!w.UI().Paused(),"restored world actually advances outside modal pause");Screenshot(h,out/"cold-continued.png");done=true;Quit();}
    if(mode=="encounterread"&&!restoreObserved){auto* status=w.SceneControl()->Saves(h.Resources())->Status(request);if(status&&(status->state=="completed"||status->state=="failed")){Check(status->state=="completed","Stage 3 cold restore publishes: "+status->error);auto saved=Read(out/"expected.json");Check(State(w)["bag"]==saved["state"]["bag"],"cold restore retains exact player loot, money and ammunition");Check(State(w)["combat"]==saved["state"]["combat"],"cold restore retains magazines, quests and kill accounting");for(auto& row:saved["people"]){auto id=row["entity"].get<EntityId>();auto state=PersonState(w,id);Check(state["health"]==row["state"]["health"]&&state["dead"]==row["state"]["dead"]&&state["bag"]==row["state"]["bag"],"cold restore retains independent actor health/death/stock "+std::to_string(id));if(row.contains("corpse")){auto body=w.RuntimeBody(row["corpse"].get<EntityId>());Check(body.IsValid()&&w.Physics().IsDynamicBody(body),"cold corpse is an actual dynamic body");if(body.IsValid())Check(glm::distance(w.Physics().GetTransform(body).position,V(row["position"]))<.03f,"cold corpse physical endpoint is preserved");}}Screenshot(h,out/"cold-restored.png");restoreObserved=true;restoreFrame=frame;}}
    if(mode=="encounterread"&&restoreObserved&&frame>=restoreFrame+120){for(EntityId id:{6200,6210,6300,6310})Check(PersonState(w,id)["dead"]==true&&!w.RuntimeCharacter(id)->settings.enabled,"dead actors remain disabled after 120 continued steps");Check(State(w)["vitals"]["health"].get<float>()>0,"restored expedition remains playable");done=true;Quit();}
    if((mode=="cargoread"||mode=="garageread")&&!restoreObserved){auto* status=w.SceneControl()->Saves(h.Resources())->Status(request);if(status&&(status->state=="completed"||status->state=="failed")){Check(status->state=="completed","vehicle cold restore publishes: "+status->error);auto saved=Read(out/"expected.json");Check(State(w)["bag"]==saved["state"]["bag"],"cold save restores exact personal inventory");auto driverReference=saved["state"]["transport"]["driving"].get<std::string>();restoredVehicle=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(driverReference);restoredWood=mode=="garageread"?3:2;Check(State(w)["transport"]["driving"]==driverReference&&restoredVehicle&&w.RuntimeBody(restoredVehicle).IsValid()&&!w.RuntimeCharacter(100)->settings.enabled,"cold restore retains actual disabled pilot motor and driver lease");std::cout<<"COLD PAUSED "<<w.UI().Paused()<<std::endl;for(auto& row:saved["vehicles"]){auto entity=row.contains("reference")?w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(row["reference"].get<std::string>()):row["entity"].get<EntityId>();auto state=PersonState(w,entity);Check(state["bag"]==row["state"]["bag"],"exact cold cargo "+std::to_string(entity));Check(state["health"]==row["state"]["health"],"exact cold vehicle condition "+std::to_string(entity));Check(std::abs(state["fuel"].get<double>()-row["state"]["fuel"].get<double>())<.005,"cold fuel within at most one resumed consumption step "+std::to_string(entity));Check(glm::distance(w.Physics().GetTransform(w.RuntimeBody(entity)).position,V(row["position"]))<.03f,"actual cold physical endpoint "+std::to_string(entity));}Screenshot(h,out/"cold-vehicles.png");start=w.Physics().GetTransform(w.RuntimeBody(restoredVehicle)).position;if(w.UI().Paused())press("pause");restoreObserved=true;restoreFrame=frame;}}
    if((mode=="cargoread"||mode=="garageread")&&restoreObserved){if(frame==restoreFrame+5)held={"move_y"};if(frame>=restoreFrame+150){held={};Check(glm::distance(start,w.Physics().GetTransform(w.RuntimeBody(restoredVehicle)).position)>1,"restored native vehicle responds physically to real input");Check(Count(PersonState(w,restoredVehicle),"wood")==restoredWood,"cargo persists through continued restored driving");done=true;Quit();}}
    if(mode=="performance"){frames.push_back(measured);physics.push_back(w.Physics().LastStepStats().totalMilliseconds);if(frames.size()>=600){auto stats=[](std::vector<double> v){std::sort(v.begin(),v.end());double sum=0;for(double d:v)sum+=d;return Json{{"meanMs",sum/v.size()},{"medianMs",v[v.size()/2]},{"p95Ms",v[size_t(v.size()*.95)]},{"worstMs",v.back()}};};Write(out/"PERFORMANCE.json",{{"frames",600},{"frame",stats(frames)},{"physics",stats(physics)},{"bodies",w.Physics().AliveBodyCount()},{"controlledStep",1./60},{"note","Real rendered application frames, fixed 60-Hz clock, hidden window, dummy audio; not human displayed FPS"}});std::string e;PerformanceProfiler::Get().Export((out/"profile.json").string(),e);Screenshot(h,out/"performance.png");done=true;Quit();}}
   }
   if(frame>7500)throw std::runtime_error("bounded application test timed out");
 }catch(const std::exception& e){Check(false,e.what());done=true;Quit();}};
 std::string exe="lunar_island_observer",p=project.string();char* args[]={exe.data(),p.data()};int result=Application{}.Run(2,args,&control);Check(result==0&&done,"ordinary application exits cleanly");std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}
