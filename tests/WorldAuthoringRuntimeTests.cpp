#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "CameraProjection.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const std::string& label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 const auto out=fs::absolute(argv[1]);fs::create_directories(out);auto root=out/"project";
 if(fs::exists(root)){std::cerr<<"Use a fresh result directory\n";return 2;}
 fs::copy("projects/world_workshop",root,fs::copy_options::recursive);
 std::string error;EngineHost host;Check(host.Init("M67 authoring runtime",640,360,false,error),"actual GL EngineHost: "+error);if(failures)return 1;
 host.OpenProjectAssets(root.string(),(root/"Assets").string());
 fs::copy_file("docs/judasjs/examples/authoring-runtime.js",root/"Assets/example.js");AssetRecord script;Check(host.Assets().Track((root/"Assets/example.js").string(),script,error),"registered copyable example");
 Project project;Check(project.Load((root/"world_workshop.judasproj").string(),error),"normal named project loader");Scene authored;Check(LoadSceneFromFile(project.StartupScenePath(),authored,error),"normal named scene loader");
 Check(authored.Objects().size()>300,"substantial generated ordinary scene");
 auto original=authored;for(auto& o:authored.Objects())o.scripts.clear();authored.Find(3)->scripts={{1,script.id,true,"{}"}};
 auto& resources=host.Resources();resources.WaitForAll();RuntimeWorld world;
 Check(world.Build(authored,&resources,error,&project.Settings().classification,&project.Settings().navigation),"normal resource/physics build: "+error);if(failures)return 1;
 world.viewportWidth=640;world.viewportHeight=360;world.legacyGameplay=false;
 GameSession game;Check(game.Begin(world,error),"ordinary fixed-step session");
 auto& window=host.GetWindow();window.SetTestInputMode(true);window.Input().SetMap(project.Settings().input,error);
 bool prepared=false;for(int attempt=0;attempt<20&&!prepared;++attempt){prepared=WorldPersistence::Prepare(world,error);resources.WaitForAll();world.UpdateAnimations(0);}Check(prepared,"materialize existing lazy participants before count: "+error);auto beforeBodies=world.Physics().AliveBodyCount();
 for(int i=0;i<3;++i){world.UpdateScripts(&window.Input(),1.f/60);world.FixedScripts(&window.Input(),1.f/60);resources.WaitForAll();}
 Check(world.Scripts()->Diagnostics().empty(),"real JS example callbacks: no faults");for(auto& d:world.Scripts()->Diagnostics())std::cout<<d.message<<'\n';
 std::map<EntityId,nlohmann::json> states;for(auto& s:world.Scripts()->Capture())states[s.entity]=nlohmann::json::parse(s.json);
 std::ofstream(out/"states.json")<<nlohmann::json(states).dump(2);
 auto owner=states[3];if(!owner.is_object())owner=nlohmann::json::object();for(auto key:{"projected","behind","viewRay","layout","invalidLayout","spawned","invalidInit","invalidRange","camera","stale"})Check(owner.value(key,false),std::string("JS ")+key);
 if(owner.contains("first")&&owner.contains("second")){
  auto a=std::stoull(owner["first"].get<std::string>()),b=std::stoull(owner["second"].get<std::string>());
  Check(states[a].value("label","")=="Example 1"&&states[b].value("label","")=="Example 2","independent typed construction properties");
  Check(states[a]["payload"]["serial"]==1&&states[b]["payload"]["serial"]==2,"independent constructor initial state");
  Check(states[a]["constructedVelocity"]["x"]==3&&states[b]["constructedVelocity"]["x"]==3,"initial motion visible before first simulation");
  Check(states[a]["started"]==1&&states[b]["started"]==1,"start once after successful construction");
 }
 Check(world.Physics().AliveBodyCount()==beforeBodies+2,"invalid initialization publishes no partial body");
 auto& renderer=host.GetRenderer();resources.WaitForAll();world.UpdateAnimations(.2f);
 renderer.ResetStats();RenderWorldFrame(renderer,640,360,world,nullptr,glm::lookAt(glm::vec3(0,80,0),glm::vec3(0,80,-1),glm::vec3(0,1,0)),glm::perspective(glm::radians(70.f),640.f/360,.15f,2000.f),{0,80,0},1);
 std::vector<unsigned char> pixels;renderer.CaptureFrame(640,360,pixels);WriteRgbPng((out/"distant-marker.png").string(),640,360,pixels);
 bool turquoise=false;for(size_t i=0;i+2<pixels.size();i+=3)turquoise|=pixels[i+1]>pixels[i]+35&&pixels[i+2]>pixels[i]+35;
 Check(turquoise,"actual distant geometry survives render/culling beyond old 500 m cap");
 world.viewportWidth=1000;world.viewportHeight=1000;
 auto wide=ProjectViewport(glm::mat4(1),glm::perspective(glm::radians(70.f),640.f/360,.15f,2000.f),{100,0,-800});
 auto square=ProjectViewport(glm::mat4(1),glm::perspective(glm::radians(70.f),1.f,.15f,2000.f),{100,0,-800});
 Check(square.position.x>wide.position.x&&square.inside,"resize uses actual aspect in viewport projection");
 Check(world.view&&world.view->nearPlane==.15f&&world.view->farPlane==2000,"invalid range preserved last-good view");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"normal save participants capture new runtime state: "+error);
 Scene snapshot;RuntimeWorld resumed;
 bool restored=WorldPersistence::SceneFrom(chunks,snapshot,error)&&resumed.Build(snapshot,&resources,error,&project.Settings().classification,&project.Settings().navigation,true)&&WorldPersistence::Prepare(resumed,error)&&WorldPersistence::Restore(resumed,chunks,error);
 Check(restored,"cold runtime construction and participant restore: "+error);
 if(restored){WorldPersistence::Published(resumed);resumed.UpdateScripts(&window.Input(),.016f);Check(resumed.Scripts()->Diagnostics().empty(),"cold restoration callbacks safe");Check(resumed.view&&resumed.view->farPlane==2000,"projection range participant survives cold restore");
  for(auto& s:resumed.Scripts()->Capture())if(s.entity!=3){auto state=nlohmann::json::parse(s.json);Check(state.value("restored",false)&&state.value("started",0)==1,"restore does not replay fresh prefab start/init");}
 }
 game.End();world.Destroy();resumed.Destroy();Check(original.Objects().size()==authored.Objects().size()&&original.Find(3)->scripts[0].asset!=script.id,"runtime fixture leaves authored project untouched");host.Shutdown();
 std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
