// Reproduce the operator's camera/model mismatch, then exercise the real
// project scripts at partial fixed-step presentation times. No solver changes.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "Simulation.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glm/gtc/quaternion.hpp>
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
double Number(const std::string& s,const char* key){auto p=s.find(std::string("\"")+key+"\":");return p==std::string::npos?-999:std::strtod(s.c_str()+p+std::strlen(key)+3,nullptr);}
int main(int argc,char** argv){const bool baseline=argc>1&&std::string(argv[1])=="--baseline";
 std::string error;EngineHost host;Check(host.Init("Script presentation",640,360,false,error),"real engine host");if(failures)return 1;host.Audio().Init(error,true);
 auto& window=host.GetWindow();window.SetTestInputMode(true);
 for(const bool flight:{false,true}){
  const std::string name=flight?"boundary_demo":"shooter_game";
  auto root=fs::absolute(fs::path("build/m52-presentation")/(name+(baseline?"-before":"-after")));
  fs::remove_all(root);fs::create_directories(root);fs::copy(fs::path("projects")/name,root,fs::copy_options::recursive);
  if(baseline){for(const auto* file:flight?std::initializer_list<const char*>{"controller.js","rules.js"}:std::initializer_list<const char*>{"player.js","camera.js"})
   fs::copy_file(fs::path("docs/evidence/m52/jitter-followup")/(std::string("original-")+file),root/"Assets/scripts"/file,fs::copy_options::overwrite_existing);}
  // Separate ordinary script slot observes public API semantics in both runs.
  std::ofstream(root/"Assets/probe.js")<<R"JS(import {time} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={calls:0};}
 update(){const a=this.entity.transform,b=this.entity.presentedTransform;this.state.outside=Math.hypot(a.position.x-b.position.x,a.position.y-b.position.y,a.position.z-b.position.z);}
 presentationUpdate(dt,alpha){const a=this.entity.transform,b=this.entity.presentedTransform;
  Object.assign(this.state,{calls:this.state.calls+1,alpha,fixed:time.fixed,rawX:a.position.x,rawZ:a.position.z,px:b.position.x,pz:b.position.z});
  try{this.entity.character.velocity={x:0,y:0,z:0};this.state.guarded=false;}catch{this.state.guarded=true;}
 }
})JS";
  Project project;Check(project.Load((root/(name+".judasproj")).string(),error),"registered project copy loads");
  host.OpenProjectAssets(root.string(),(root/"Assets").string());AssetRecord probe;
  Check(host.Assets().Track((root/"Assets/probe.js").string(),probe,error),"probe uses normal registered JS asset");
  Scene scene;Check(LoadSceneFromFile(flight?project.Resolve("Scenes/flight.judas"):project.StartupScenePath(),scene,error),"actual project scene loads");
  scene.Find(10)->scripts.push_back({99,probe.id,true,"{}"});window.Input().SetMap(project.Settings().input,error);
  RuntimeWorld world;
  world.SetSceneControl(std::make_shared<SceneSession>(project,flight?project.Resolve("Scenes/flight.judas"):project.StartupScenePath()));
  Check(world.Build(scene,&host.Resources(),error,&project.Settings().classification,&project.Settings().navigation)&&world.IsBuilt(),"ordinary runtime builds");if(failures){std::puts(error.c_str());return 1;}
  auto update=[&](){world.UpdateScripts(&window.Input(),1.f/60);};
  auto fixed=[&](){window.Input().BeginFixedStep();StepPlayedWorld(world,window,1.f/60);window.Input().BeginFrame();};
  auto probeState=[&](){for(const auto& s:world.Scripts()->Capture())if(s.entity==10&&s.slot==99)return s.json;return std::string{};};
  update();host.Resources().WaitForAll();update();
  if(flight){auto t=world.RuntimeDefinition(70)->transform;t.position={0,1.8f,3};world.SetRuntimeTransform(70,t);window.Input().SetPhysical("key:F",1);update();window.Input().SetPhysical("key:F",0);window.Input().BeginFrame();
   world.Physics().SetAngularVelocity(world.RuntimeBody(70),{.2f,.6f,.1f});}
  else {window.Input().SetPhysical("key:V",1);update();window.Input().SetPhysical("key:V",0);window.Input().BeginFrame();}
  window.Input().SetPhysical("key:W",1);for(int i=0;i<45;++i){update();fixed();}
  const EntityId id=flight?70:10;const auto* def=world.RuntimeDefinition(id);auto raw=world.PresentedTransform(id,def->transform,1);
  const auto velocity=flight?world.Physics().GetLinearVelocity(world.RuntimeBody(id)):world.RuntimeCharacter(id)->velocity;
  float worstCamera=0,worstAvatar=0;
  for(const float alpha:{.25f,.75f}){
   world.PresentationScripts(&window.Input(),1.f/144,alpha);auto presented=world.PresentedTransform(id,def->transform,alpha);
   const glm::vec3 expected=presented.position+presented.rotation*(flight?glm::vec3(0,1,0):glm::vec3(.7f,1.f,3.5f));
   const float cameraError=world.view?glm::length(world.view->pose.position-expected):100;
   worstCamera=std::max(worstCamera,cameraError);
   const float rotationError=world.view?1-std::abs(glm::dot(world.view->pose.rotation,presented.rotation)):1;
   float avatarError=0;
   if(!flight){auto avatar=world.PresentedTransform(11,world.RuntimeDefinition(11)->transform,alpha);avatarError=glm::length(avatar.position-presented.position);worstAvatar=std::max(worstAvatar,avatarError);}
   std::printf("POSE %s baseline=%d alpha=%.2f camera_error_m=%.9f avatar_error_m=%.9f camera_rotation_error=%.9f\n",name.c_str(),baseline,alpha,cameraError,avatarError,rotationError);
   if(!baseline){Check(cameraError<2e-5f,"script camera matches render-interpolated world position");Check(rotationError<2e-6f,"script camera matches interpolated orientation");if(!flight)Check(avatarError<2e-5f,"cosmetic model shares camera/motor presentation timeline");}
   auto state=probeState();auto motorPose=world.PresentedTransform(10,world.RuntimeDefinition(10)->transform,alpha);
   Check(std::abs(Number(state,"alpha")-alpha)<1e-6&&state.find("\"fixed\":false")!=std::string::npos,"presentation callback receives alpha in non-authoritative mode");
   Check(std::abs(Number(state,"px")-motorPose.position.x)<2e-5&&std::abs(Number(state,"pz")-motorPose.position.z)<2e-5,"public getter uses engine presentation pose");
   auto motorRaw=world.PresentedTransform(10,world.RuntimeDefinition(10)->transform,1);
   Check(std::abs(Number(state,"rawX")-motorRaw.position.x)<2e-5&&std::abs(Number(state,"rawZ")-motorRaw.position.z)<2e-5,"transform stays authoritative inside presentation callback");
   Check(state.find("\"guarded\":true")!=std::string::npos,"presentation cannot submit fixed motor velocity intent");
  }
  if(baseline){Check(worstCamera>.005f,"original scripts reproduce camera/renderer mismatch");if(!flight)Check(worstAvatar>.005f,"original script model is a fixed-step behind");}
  auto after=world.PresentedTransform(id,def->transform,1);auto afterVelocity=flight?world.Physics().GetLinearVelocity(world.RuntimeBody(id)):world.RuntimeCharacter(id)->velocity;
  Check(glm::length(after.position-raw.position)<1e-6f&&glm::length(afterVelocity-velocity)<1e-6f,"presentation does not modify authoritative motion");
  update();Check(Number(probeState(),"outside")<1e-6,"presented getter outside presentation is authoritative");
  if(!flight&&!baseline){auto beforeRotation=world.view->pose.rotation;window.Input().AddDelta("mouse:dx",18);update();world.PresentationScripts(&window.Input(),1.f/144,.75f);
   Check(std::abs(glm::dot(beforeRotation,world.view->pose.rotation))<.99999f,"mouse look updates even without a new physics step");}
  Check(world.Scripts()->Diagnostics().empty(),"project and probe callbacks are fault-free");window.Input().SetPhysical("key:W",0);window.Input().BeginFrame();world.Destroy();world.PresentationScripts(&window.Input(),0,.5f);
  Check(!world.Scripts()&&!world.view,"Stop removes presentation callbacks and view state");
 }
 host.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
