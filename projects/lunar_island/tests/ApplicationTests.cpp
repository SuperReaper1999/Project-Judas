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
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <cmath>
using Json=nlohmann::json;
namespace {
unsigned checks=0,failed=0;
void Check(bool ok,const std::string& text){++checks;failed+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<text<<std::endl;}
void Quit(){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
Json Read(const std::filesystem::path& p){std::ifstream f(p);return Json::parse(f);}
void Write(const std::filesystem::path& p,const Json& j){std::ofstream(p)<<j.dump(2)<<'\n';}
Json State(RuntimeWorld& w){for(auto& r:w.Scripts()->Capture())if(r.entity==100&&r.slot==1)return Json::parse(r.json);return Json::object();}
void StateFixture(RuntimeWorld& w,std::function<void(Json&)> edit){auto rows=w.Scripts()->Capture();for(auto& r:rows)if(r.entity==100&&r.slot==1){auto j=Json::parse(r.json);edit(j);r.json=j.dump();}std::string e;if(!w.RestoreScriptState(rows,e))throw std::runtime_error(e);}
glm::vec3 V(const Json& j){return {j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};}
Json J(glm::vec3 v){return {v.x,v.y,v.z};}
unsigned Count(const Json& s,const std::string& item){unsigned n=0;for(auto& row:s["bag"]["slots"])if(!row.is_null()&&row["item"]==item)n+=row["count"].get<unsigned>();return n;}
void Aim(RuntimeWorld& w,glm::vec3 target,float ground,float distance=2,glm::vec3 backward={0,0,1}){auto t=w.RuntimeDefinition(100)->transform;PhysicsQueryFilter groundFilter;groundFilter.requiredTags=1;auto fixturePoint=target+backward*distance;auto groundHit=w.Physics().Raycast({fixturePoint.x,200,fixturePoint.z},{0,-1,0},400,groundFilter);if(groundHit.hit)ground=groundHit.point.y;t.position={fixturePoint.x,ground+.05f,fixturePoint.z};t.rotation={1,0,0,0};if(!w.SetRuntimeTransform(100,t))throw std::runtime_error("fixture placement failed");w.RuntimeCharacter(100)->velocity={};float pitch=std::atan2(target.y-t.position.y-1.6f,distance);StateFixture(w,[&](Json& s){s["yaw"]=std::atan2(backward.x,backward.z);s["pitch"]=pitch;s["third"]=false;});}
void Screenshot(EngineHost& h,const std::filesystem::path& p){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng(p.string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"capture "+p.filename().string());}
bool Healthy(RuntimeWorld& w){for(auto& d:w.Scripts()->Diagnostics()){std::cout<<"SCRIPT "<<d.callback<<' '<<d.message<<std::endl;return false;}return true;}
}
int main(int argc,char** argv){
 if(argc!=4){std::cerr<<"usage: observer project write|read|performance output\n";return 2;}
 auto project=std::filesystem::absolute(argv[1]);std::string mode=argv[2];auto out=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(out);
 auto data=Read(project.parent_path()/"Authoring/world_manifest.json");
 auto resources=data["resources"];Json tree,rock;for(auto& r:resources){if(r["kind"]=="tree"&&tree.is_null())tree=r;if(r["kind"]=="rock"&&rock.is_null())rock=r;}
 EntityId treeId=std::stoull(tree["id"].get<std::string>());glm::vec3 treeBase=V(tree["position"]),start{},pauseStart{};Json expected;
 unsigned frame=0,due=12,index=0;std::vector<std::string> pulse,held;std::map<std::string,std::string> bindings;uint64_t request=0;bool capture=false,done=false;double measured=0;std::vector<double> frames,physics;std::ofstream times(out/(mode+"-timings.csv"));times<<"frame,frame_ms,physics_ms,bodies\n";
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
 if(mode=="write"){
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
  job(5,[&](EngineHost&,RuntimeWorld& w){Aim(w,{90,9,-100},8.1f);held={"jump"};});
  job(65,[&](EngineHost&,RuntimeWorld& w){held.clear();std::cout<<"SWIM endpoint "<<J(w.RuntimeDefinition(100)->transform.position).dump()<<" sample "<<w.SampleWater({90,9,-100},{0,1,0},.55f,.3f).immersion<<std::endl;Check(!w.AdvancedFluidsEnabled()&&!w.HasFluid()&&w.BasicWaterEnabled()&&w.WaterSwimmingEnabled(),"prescribed water enabled independently of advanced fluid solvers");Check(w.RuntimeDefinition(100)->transform.position.y>8.4f,"basic-water swimming lifts real motor without advanced fluids");Check(State(w)["vitals"]["health"].get<float>()>0,"swimming remains survivable");held={"dive"};});
  job(180,[&](EngineHost&,RuntimeWorld& w){held.clear();Check(State(w)["vitals"]["breath"].get<float>()<22,"submerged head consumes actual survival air supply");Check(State(w)["vitals"]["hunger"].get<float>()<75,"survival hunger progresses in actual fixed steps");Aim(w,V(data["spawn"])+glm::vec3(0,1,0),data["spawn"][1].get<float>());press("save");});
  job(30,[&](EngineHost&,RuntimeWorld&){/* wait for game-issued save request */});
 }else if(mode=="read"){
  job(5,[&](EngineHost&,RuntimeWorld& w){std::string error;request=w.SceneControl()->Saves(*w.Resources())->Request("load","island","","{}",error);Check(request!=0,"fresh process requests ordinary save load: "+error);});
 }else{
  job(10,[&](EngineHost&,RuntimeWorld&){held={"move_y"};});
 }
 ApplicationControl control;control.hidden=true;
 control.hostReady=[&](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);PerformanceProfiler::Get().Enable(true);};
 control.frameSeconds=[&](float dt){measured=dt*1000;return 1.f/60;};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){try{
   if(mode=="write"&&!capture){auto* service=w.SceneControl()->Saves(h.Resources());for(uint64_t id=1;id<12;++id){auto* s=service->Status(id);if(s&&s->operation=="save"&&s->state=="writing"){expected={{"state",State(w)},{"treePosition",J(w.Physics().GetTransform(w.RuntimeBody(treeId)).position)},{"treeRotation",{w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.w,w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.x,w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.y,w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation.z}}};Write(out/"expected.json",expected);request=id;capture=true;}}}
   if(index<jobs.size()&&frame>=due){auto item=jobs[index++];item.second(h,w);due=frame+(index<jobs.size()?jobs[index].first:10);}
   for(auto& entry:h.GetWindow().Input().Map().entries){for(auto& b:entry.bindings)h.GetWindow().QueueTestPhysical(b.control,0);for(auto& b:entry.bindings)if(b.scale>0){bindings[entry.name]=b.control;break;}}
   for(auto& a:held)h.GetWindow().QueueTestPhysical(bindings.at(a),1);
   for(auto& a:pulse){h.GetWindow().QueueTestPhysical(bindings.at(a),1);}
   pulse.clear();
 }catch(const std::exception& e){Check(false,e.what());done=true;Quit();}};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){++frame;try{
   if(!Healthy(w))throw std::runtime_error("game script failure");
   times<<frame<<','<<measured<<','<<w.Physics().LastStepStats().totalMilliseconds<<','<<w.Physics().AliveBodyCount()<<'\n';
   if(index==jobs.size()&&frame>due){
    if(!done&&h.Resources().Stats().failed)throw std::runtime_error("native resource decoding/upload failure");
    if(mode=="write"){auto* s=w.SceneControl()->Saves(h.Resources())->Status(request);if(s&&(s->state=="completed"||s->state=="failed")){Check(capture,"independently observed actual save capture boundary");Check(s->state=="completed","game-issued durable save completes: "+s->error);Write(out/"final-state.json",State(w));done=true;Quit();}}
    if(mode=="read"){auto* s=w.SceneControl()->Saves(h.Resources())->Status(request);if(s&&(s->state=="completed"||s->state=="failed")){Check(s->state=="completed","fresh process restores save: "+s->error);auto saved=Read(out/"expected.json");Check(State(w)["bag"]==saved["state"]["bag"],"cold save restores exact inventory");Check(State(w)["harvest"]==saved["state"]["harvest"],"cold save restores exact depletion/felling state");Check(glm::distance(w.Physics().GetTransform(w.RuntimeBody(treeId)).position,V(saved["treePosition"]))<.01f,"cold save restores actual fallen tree endpoint");auto r=saved["treeRotation"];glm::quat savedRotation{r[0].get<float>(),r[1].get<float>(),r[2].get<float>(),r[3].get<float>()};Check(std::abs(glm::dot(w.Physics().GetTransform(w.RuntimeBody(treeId)).rotation,savedRotation))>1-1e-5f,"cold save restores actual fallen trunk orientation");Check(!w.RuntimeBody(200).IsValid(),"cold restored depleted resource stays absent");Check(w.Physics().IsDynamicBody(w.RuntimeBody(treeId)),"restored fallen tree remains dynamic");Screenshot(h,out/"cold-restored.png");done=true;Quit();}}
    if(mode=="performance"){frames.push_back(measured);physics.push_back(w.Physics().LastStepStats().totalMilliseconds);if(frames.size()>=600){auto stats=[](std::vector<double> v){std::sort(v.begin(),v.end());double sum=0;for(double d:v)sum+=d;return Json{{"meanMs",sum/v.size()},{"medianMs",v[v.size()/2]},{"p95Ms",v[size_t(v.size()*.95)]},{"worstMs",v.back()}};};Write(out/"PERFORMANCE.json",{{"frames",600},{"frame",stats(frames)},{"physics",stats(physics)},{"bodies",w.Physics().AliveBodyCount()},{"controlledStep",1./60},{"note","Real rendered application frames, fixed 60-Hz clock, hidden window, dummy audio; not human displayed FPS"}});std::string e;PerformanceProfiler::Get().Export((out/"profile.json").string(),e);Screenshot(h,out/"performance.png");done=true;Quit();}}
   }
   if(frame>3500)throw std::runtime_error("bounded application test timed out");
 }catch(const std::exception& e){Check(false,e.what());done=true;Quit();}};
 std::string exe="lunar_island_observer",p=project.string();char* args[]={exe.data(),p.data()};int result=Application{}.Run(2,args,&control);Check(result==0&&done,"ordinary application exits cleanly");std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}
