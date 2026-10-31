#include "Project.h"
#include "ProjectStarter.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "Simulation.h"
#include "GameSession.h"
#include "ScriptSystem.h"
#include "SceneSerialization.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using Json=nlohmann::json;
unsigned checks=0;void Check(bool ok,const std::string& text){++checks;if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';}
Json State(RuntimeWorld& w){for(const auto& s:w.Scripts()->Capture())if(s.entity==1)return Json::parse(s.json);throw std::runtime_error("grid state absent");}
void Tick(RuntimeWorld& w,InputSystem& input,int count){GameSession session;std::string e;Check(session.Begin(w,e),"begin normal played session");for(int i=0;i<count;++i){input.BeginFrame();input.BeginFixedStep();StepAuthoritativeWorld(session,input,1.f/60);w.UpdateScripts(&input,1.f/60);}for(const auto& d:w.Scripts()->Diagnostics())throw std::runtime_error(d.message);}
int main(int argc,char** argv){try{
 if(argc!=2&&argc!=3&&argc!=4)return 2;
 const auto root=fs::absolute(argv[1]);const bool cold=argc==3,coldQueue=cold&&std::string(argv[2])=="--cold-queue";std::string error;
 Project project;Scene scene;
 if(!cold){Check(!fs::exists(root),"fresh grid project");Check(Project::CreateNew(root.string(),"Grid Movement",project,error)&&PopulateGridProjectStarter(project,scene,error),"same grid project preset as New Project");if(argc==4){Check(std::string(argv[2])=="--controller","explicit negative-control source selection");fs::copy_file(argv[3],fs::path(project.AssetsDir())/"game.js",fs::copy_options::overwrite_existing);}
  project.Settings().startupScene="Scenes/main.judas";
  // Test the actual untouched starter from the reported approach side. The
  // motion tests below intentionally replace its layout, so cannot catch this.
  {
   AssetDatabase starterAssets;starterAssets.Scan(project.RootDir(),project.AssetsDir());
   ResourceManager starterResources(nullptr,&starterAssets);starterResources.SetBlockingMode(true);
   for(bool legacy:{true,false}) {
    Scene authored=scene;authored.Find(1)->transform.position={5,2,0};
    SceneObject* obstacle=nullptr;
    for(auto& o:authored.Objects())if(o.name=="Blocked grid cell")obstacle=&o;
    Check(obstacle!=nullptr,"grid starter contains an authored blocking cell");
    Check(obstacle->render->sprite.size==glm::vec2(.9f)&&obstacle->body->planar->vertices[2]==glm::dvec2(.45),"starter visible obstacle and physical footprint occupy the same cell");
    if(legacy){obstacle->body->planar=PlanarBox({.7,1.5});obstacle->render->sprite.size={1.4f,3};}
    RuntimeWorld starter;starter.legacyGameplay=false;
    Check(starter.Build(authored,&starterResources,error),"build unmodified starter with real script and layout");
    InputSystem keys;Check(keys.SetMap(project.Settings().input,error),"starter uses normal project controls");
    keys.SetPhysical("key:A",1);Tick(starter,keys,7);keys.SetPhysical("key:A",0);Tick(starter,keys,25);
    auto position=starter.Physics().GetTransform(starter.RuntimeBody(1)).position;
    Check(glm::length(position-glm::vec3(legacy?5.f:4.f,2,0))<1e-5f,legacy?"negative control: inherited 1.4 m obstacle rejects the adjacent grid cell":"actual starter approaches within one cell of the visible edge");
    keys.SetPhysical("key:A",1);Tick(starter,keys,7);keys.SetPhysical("key:A",0);Tick(starter,keys,25);
    position=starter.Physics().GetTransform(starter.RuntimeBody(1)).position;
    Check(glm::length(position-glm::vec3(legacy?5.f:4.f,2,0))<1e-5f,"occupied obstacle cell still blocks native movement");
   }
   bool covered=false;for(const auto& o:scene.Objects())if(o.name=="Ground tile"&&o.transform.position==glm::vec3(11,6,-.2f))covered=true;
   Check(covered,"backdrop covers the previously exposed walkable room corner");
  }
  std::vector<SceneObjectId> remove;for(const auto& o:scene.Objects())if(o.body&&o.id!=1)remove.push_back(o.id);for(auto id:remove)scene.DestroyObject(id);scene.Find(1)->transform.position={0,0,0};
  auto& wall=scene.CreateObject("Blocked east cell");wall.transform.position={2,0,0};wall.body=SceneBodyComponent{};wall.body->shape=SceneShape::Planar;wall.body->planar=PlanarBox({.45,.45});
  Check(SaveSceneToFile(scene,project.StartupScenePath(),error)&&project.Save(error),"save editable grid fixture");
 }else Check(project.Load((root/"Grid_Movement.judasproj").string(),error),"cold process opens grid project");
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());ResourceManager resources(nullptr,&assets);resources.SetBlockingMode(true);InputSystem input;Check(input.SetMap(project.Settings().input,error),"ordinary project bindings");
 RuntimeWorld world;world.legacyGameplay=false;GameSnapshot snapshot;snapshot.project="grid-preset";snapshot.scene="Scenes/main.judas";snapshot.content=std::string(64,'c');SaveStorage storage((root/"Cold").string());
 if(cold){bool recovered=false;Check(storage.Read(coldQueue?"grid-queue":"grid",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"fresh process reads mid-step snapshot");Check(world.Build(scene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"cold restores pending cell and native kinematic state");Tick(world,input,coldQueue?60:30);
  Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(coldQueue?0.f:1.f,1,0))<1e-5f,"cold resumes remaining cell without snap or replaying input");Check(State(world)["moves"]==(coldQueue?3:2),"cold commits each traversed cell exactly once");std::cout<<"GRID_COLD checks="<<checks<<" failures=0\n";return 0;
 }
 Check(world.Build(scene,&resources,error),"build editable grid game");
 input.SetPhysical("key:D",1);Tick(world,input,1);input.SetPhysical("key:D",0);Tick(world,input,24);
 Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(1,0,0))<1e-5f&&State(world)["moves"]==1,"single input edge finishes exactly one cell through native motion");
 input.SetPhysical("key:D",1);Tick(world,input,30);input.SetPhysical("key:D",0);Tick(world,input,1);
 Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(1,0,0))<1e-5f&&State(world)["blocked"]==true,"blocked cell cannot be crossed or snapped into");
 input.SetPhysical("key:W",1);Tick(world,input,7);input.SetPhysical("key:W",0);
 const auto partial=world.Physics().GetTransform(world.RuntimeBody(1)).position;Check(partial.y>0&&partial.y<1&&State(world)["target"]!=nullptr,"save fixture is genuinely partway through a cell");
 Check(WorldPersistence::Capture(world,snapshot.participants,error)&&storage.Write("grid",snapshot,error),"publish real partial-step cold save");
 Scene queuedScene;Check(WorldPersistence::SceneFrom(snapshot.participants,queuedScene,error),"reconstruct independent queued-turn fixture");RuntimeWorld queued;queued.legacyGameplay=false;
 Check(queued.Build(queuedScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(queued,error)&&WorldPersistence::Restore(queued,snapshot.participants,error),"restore separate world before queuing a turn");InputSystem queuedInput;Check(queuedInput.SetMap(project.Settings().input,error),"isolated queued-turn input map");
 queuedInput.SetPhysical("key:A",1);queuedInput.SetPhysical("key:A",0);Tick(queued,queuedInput,1);Check(State(queued)["queued"]!=nullptr,"short turn is actually pending in saved script state");
 GameSnapshot queueSave;queueSave.project=snapshot.project;queueSave.scene=snapshot.scene;queueSave.content=snapshot.content;
 Check(WorldPersistence::Capture(queued,queueSave.participants,error)&&storage.Write("grid-queue",queueSave,error),"save pending step and buffered turn together");Tick(queued,queuedInput,60);
 Check(glm::length(queued.Physics().GetTransform(queued.RuntimeBody(1)).position-glm::vec3(0,1,0))<1e-5f&&State(queued)["moves"]==3,"uninterrupted buffered turn completes both actual cells");
 Tick(world,input,30);Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(1,1,0))<1e-5f&&State(world)["moves"]==2,"uninterrupted continuation completes pending step");
 input.SetPhysical("key:W",1);input.SetPhysical("key:D",1);Tick(world,input,1);input.SetPhysical("key:W",0);input.SetPhysical("key:D",0);Tick(world,input,24);
 Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(2,1,0))<1e-5f,"diagonal input resolves one cardinal cell without cutting corner");
 const auto moves=State(world)["moves"];
 input.SetPhysical("key:W",1);Tick(world,input,5);input.SetPhysical("key:W",0);
 SceneObjectId wallId=0;for(const auto& o:scene.Objects())if(o.name=="Blocked east cell")wallId=o.id;
 world.Physics().ResetBody(world.RuntimeBody(wallId),{2,1.9f,0},{1,0,0,0});Tick(world,input,8);
 Check(State(world)["blocked"]==true&&world.Physics().GetTransform(world.RuntimeBody(1)).position.y<1.5f,"obstacle entering an accepted step stops real motion without snapping into it");
 const auto escapeBlocker=world.Physics().CreateShape(Shape::Planar(PlanarBox({.3,.1})),{{2,.8f,0},{1,0,0,0}},false,1,.5f,0);
 input.SetPhysical("key:S",1);Tick(world,input,1);input.SetPhysical("key:S",0);Tick(world,input,8);
 Check(State(world)["blocked"]==true&&world.Physics().GetTransform(world.RuntimeBody(1)).position.y>1.1f,"escaping an overlap still detects a second blocker behind it");
 world.Physics().DestroyBody(escapeBlocker);Tick(world,input,30);
 std::cout<<"BACKOUT "<<State(world).dump()<<" y="<<world.Physics().GetTransform(world.RuntimeBody(1)).position.y<<'\n';
 Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(2,1,0))<1e-5f&&State(world)["moves"]==moves,"reverse input backs out physically without inventing another cell transition");
 input.SetPhysical("key:A",1);input.SetPhysical("key:A",0);Tick(world,input,25);
 Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(1,1,0))<1e-5f,"tap released before a fixed step still starts one cell");
 world.Physics().ResetBody(world.RuntimeBody(wallId),{10,10,0},{1,0,0,0});
 input.SetPhysical("key:D",1);Tick(world,input,6);input.SetPhysical("key:D",0);input.SetPhysical("key:W",1);input.SetPhysical("key:W",0);Tick(world,input,45);
 Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(1)).position-glm::vec3(2,2,0))<1e-5f,"one buffered short turn survives release while the preceding cell finishes");
 std::cout<<"GRID checks="<<checks<<" failures=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
