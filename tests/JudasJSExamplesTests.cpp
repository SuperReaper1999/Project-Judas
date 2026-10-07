// M50 executes the copyable documentation scripts through normal runtime APIs.
#include "EngineHost.h"
#include <thread>
#include <chrono>
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "Project.h"
#include "Prefab.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const std::string& label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label.c_str());}
int main(int argc,char** argv){
 fs::path out=argc>1?argv[1]:"build/m50-results";fs::create_directories(out);std::string only=argc>2?argv[2]:"";
 std::string error;EngineHost host;Check(host.Init("M50 cookbook",640,360,false,error),"real EngineHost");if(failures)return 1;host.Audio().Init(error,true);
 const char* names[]={"authoring-runtime","imported-model","developer-integration","fracture","deformable","saves","streaming","localization","materials","profiling","liquid-surface","liquid","navigation","surface","presentation","impulse-point","physical-control","minimal","input-motion","spawn","queries","contacts","trigger","audio","particles","ui","scene-session","joint","animation","ragdoll","character","stale-handle"};
 for(auto name:names){if(!only.empty()&&only!=name)continue;
  std::string n=name,project=n=="authoring-runtime"?"world_workshop":n=="imported-model"?"import_lab":n=="developer-integration"?"m65_integration":n=="fracture"?"fracture_lab":n=="deformable"?"deformable_lab":n=="streaming"?"streamed_range":n=="localization"?"text_lab":n=="liquid-surface"?"liquid_surface_demo":n=="liquid"?"liquid_reservoir_demo":n=="navigation"?"shooter_game":n=="audio"?"script_demo":n=="joint"?"joint_demo":n=="animation"||n=="ragdoll"?"ragdoll_demo":"character_demo";
  auto root=fs::absolute(fs::path("build/m50-examples")/n);fs::remove_all(root);fs::create_directories(root);fs::copy("projects/"+project,root,fs::copy_options::recursive);
  auto example=std::string(n=="trigger"?"contacts":name);fs::copy_file("docs/judasjs/examples/"+example+".js",root/"Assets/example.js",fs::copy_options::overwrite_existing);
  if(n=="deformable"){
   fs::rename(root/"Assets/example.js",root/"Assets/deformable-example.js");
   std::ofstream(root/"Assets/example.js")<<R"JS(import Base from './deformable-example.js';
import {world,session} from 'judas';
export default class extends Base {
 destroy(){if(this.entity.valid){const d=this.entity.deformable;session.set('m62DestroyEpoch',d.epoch);}}
 update(){if(this.fabric){try{this.fabric.force({x:0,y:0,z:1});}catch(e){this.state.phaseGuard=e instanceof TypeError;}}}
 fixedUpdate(){super.fixedUpdate();const d=this.entity.deformable;if(!d)return;this.fabric=d;
  if(!this.state.tested){const mass=d.state.mass;try{d.setMaterial({density:Number.MAX_VALUE});}catch(e){this.state.massGuard=e instanceof TypeError;}this.state.materialAtomic=d.state.mass===mass;
   d.enabled=false;this.state.disabled=!d.force({x:0,y:0,z:1});d.enabled=true;
   this.state.attached=d.attach('top',{kind:'body',target:world.entity('110'),offset:{x:0,y:0,z:0}})&&d.attach('top',{kind:'world'});
   this.state.reset=d.reset();this.spawn=world.spawnPrefab('6262626262626262626262626262000b',{position:{x:0,y:3,z:5}});this.state.tested=true;
  }
  if(this.spawn?.valid&&this.spawn.deformable){const old=this.spawn.deformable;this.spawn.destroy();this.state.invalid=!old.valid;try{old.state;}catch(e){this.state.staleThrows=e instanceof ReferenceError;}this.spawn=null;}
 }
})JS";
  }
  host.OpenProjectAssets(root.string(),(root/"Assets").string());AssetRecord script;
  if(n=="deformable"){AssetRecord dependency;Check(host.Assets().Track((root/"Assets/deformable-example.js").string(),dependency,error),"deformable relative module registered through normal assets");}
  Check(host.Assets().Track((root/"Assets/example.js").string(),script,error),n+" normal registered script");
  Project p;Check(p.Load((root/(project+".judasproj")).string(),error),n+" normal project");
  Scene scene;Check(LoadSceneFromFile(p.StartupScenePath(),scene,error),n+" ordinary scene");
  if(n=="navigation"){Scene resolved;if(!ResolvePrefabs(scene,&host.Assets(),resolved,error)){Check(false,error);continue;}scene=std::move(resolved);for(auto& o:scene.Objects()){o.prefabAsset.clear();o.prefabRoot=o.prefabSource=0;o.prefabIds.clear();o.prefabOverrides.clear();}}
  EntityId owner=n=="authoring-runtime"?3:n=="developer-integration"?101:n=="fracture"?100:n=="deformable"?100:n=="localization"?1:n=="liquid-surface"?10:n=="liquid"?10:n=="navigation"?1000:project=="character_demo"?40:10;
  for(auto& o:scene.Objects()){o.scripts.clear();o.ui.reset();if(n=="audio"&&o.audioEmitter)owner=o.id;}
  if(n=="joint")for(auto& o:scene.Objects())if(o.joint){owner=o.id;break;}
  if(n=="character"||n=="streaming")owner=10;
  auto* def=scene.Find(owner);if(!def){Check(false,n+" fixture owner");continue;}
  if(n=="contacts"||n=="trigger"){for(auto& o:scene.Objects()){o.joint.reset();if(o.id!=1&&o.id!=owner)o.body.reset();}def->transform.position={0,.4,0};def->body->halfExtents={.4,.4,.4};def->body->sensor=n=="trigger";}
  if(n=="materials"&&!def->render)def->render=SceneRenderComponent{};
  if(n=="particles")def->particleEmitter=ParticleEmitterSettings{};
  def->scripts.push_back({1,script.id,true,n=="saves"?"{\"cancelForFixture\":true}":"{}"});
  auto& resources=host.Resources();resources.WaitForAll();RuntimeWorld world;
  Check(world.Build(scene,&resources,error,&p.Settings().classification,&p.Settings().navigation),n+" runtime builds");if(!world.IsBuilt()){std::puts(error.c_str());continue;}
  auto session=std::make_shared<SceneSession>(p,p.StartupScenePath());world.SetSceneControl(session);world.viewportWidth=640;world.viewportHeight=360;
  if(n=="localization")world.UI().Load("cffe801f7c121dc6c96bbf4582e3f46c","text_lab",owner,error);
  if(n=="imported-model"){resources.GetMesh(def->render->meshAsset,error);resources.WaitForAll();world.UpdateAnimations(0);}
  if(n=="animation"||n=="ragdoll"){resources.GetMesh("46464646464646464646464646464601",error);resources.WaitForAll();world.UpdateAnimations(0);}
  auto& window=host.GetWindow();window.SetTestInputMode(true);window.Input().SetMap(p.Settings().input,error);window.Input().SetPhysical("key:D",1);
  if(n=="localization"){world.Localization().Refresh();resources.WaitForAll();world.Localization().Refresh();}
  GameSession game;Check(game.Begin(world,error),n+" ordinary fixed-step session");
  if(n=="audio"){resources.WaitForAll();world.BeginAudio();world.UpdateAudio(glm::mat4(1),1,1.f/60);}
  bool launched=false;
  for(int frame=0;frame<(n=="streaming"?500:100);++frame){
   if(n=="streaming")std::this_thread::sleep_for(std::chrono::milliseconds(1));
   if(n=="streaming")session->AdvanceStreaming(world,false);
   window.Input().BeginFixedStep();world.UpdateUIScripts(&window.Input(),1.f/60);world.UpdateScripts(&window.Input(),1.f/60);
   if(n=="character"&&!launched&&world.RuntimeCharacter(owner)->result.supported){window.Input().SetPhysical("key:Space",1);window.Input().BeginFixedStep();launched=true;}
   if((n=="contacts"||n=="trigger")&&frame==60){auto t=world.RuntimeDefinition(owner)->transform;t.position.y=5;world.SetRuntimeTransform(owner,t);}
   if(n=="ui"&&frame==2){auto h=world.UI().Find("example_hud");world.UI().Layout(640,360);auto* layout=world.UI().LayoutOf(h,"start");
    if(layout){auto point=layout->rect.position+layout->rect.size*.5f;window.Input().SetPhysical("mouse:Left",1);world.UI().Input(window.Input(),point,true,640,360);window.Input().BeginFrame();window.Input().SetPhysical("mouse:Left",0);world.UI().Input(window.Input(),point,true,640,360);world.DispatchUIEvents(&window.Input(),1.f/60);}}
   StepPlayedWorld(game,window,1.f/60);if(n=="character")window.Input().SetPhysical("key:Space",0);
   world.PresentationScripts(&window.Input(),1.f/60,.5f);world.UpdateAudio(glm::mat4(1));resources.WaitForAll();
  }
  Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),n+" real VM callbacks without faults");
  std::string state;for(const auto& s:world.Scripts()->Capture())if(s.entity==owner&&s.slot==1)state=s.json;
  std::printf("EXAMPLE %s %s\n",name,state.c_str());std::ofstream(out/(n+".json"))<<state;
  auto yes=[&](const char* key){return state.find(std::string("\"")+key+"\":true")!=std::string::npos;};
  bool ok=n=="authoring-runtime"?yes("projected")&&yes("behind")&&yes("viewRay")&&yes("layout")&&yes("invalidLayout")&&yes("spawned")&&yes("invalidInit")&&yes("invalidRange")&&yes("camera")&&yes("stale"):n=="imported-model"?yes("ready")&&yes("parts")&&yes("motion")&&yes("missing"):n=="developer-integration"?yes("pose")&&yes("ik")&&yes("socket")&&yes("gravity")&&yes("material")&&yes("joint")&&yes("stale"):n=="fracture"?yes("submitted")&&yes("staleRevision")&&yes("queued")&&yes("loaded")&&yes("ordinary")&&yes("complete")&&yes("invalid")&&yes("staleThrows"):n=="deformable"?yes("submitted")&&yes("disabled")&&yes("attached")&&yes("reset")&&yes("invalid")&&yes("staleThrows")&&yes("phaseGuard")&&yes("massGuard")&&yes("materialAtomic"):n=="saves"?yes("resolved")&&yes("queued")&&yes("cancelled")&&yes("invalidTokens")&&session->Get("save_example")!="null":n=="streaming"?yes("active")&&session->Get("streamExample")=="1":n=="localization"?yes("supplementary")&&yes("plural")&&yes("named")&&yes("invalid")&&yes("nul")&&yes("keyLimit")&&yes("textWins"):n=="materials"?yes("changed")&&yes("reverted")&&yes("appearance")&&yes("stale"):n=="profiling"?yes("error")&&yes("invalid")&&state.find("\"value\":42")!=std::string::npos&&state.find("\"calls\":102")!=std::string::npos:n=="liquid-surface"?yes("ready")&&yes("conserved")&&yes("impulse")&&yes("presented"):n=="liquid"?yes("ready")&&yes("transferred")&&yes("conserved")&&yes("sample"):n=="navigation"?yes("sample")&&yes("path")&&yes("controlled"):n=="surface"?state.find("Entity.destroy")!=std::string::npos:
   n=="presentation"?yes("synchronized")&&yes("frameMode")&&state.find("\"calls\":100")!=std::string::npos:
   n=="impulse-point"?yes("angular")&&yes("rejectsInvalid")&&yes("linear")&&yes("unchanged"):
   n=="physical-control"?yes("capture")&&state.find("\"mass\":40")!=std::string::npos:
   n=="minimal"?state.find("\"started\":1")!=std::string::npos&&state.find("\"steps\":100")!=std::string::npos:
   n=="input-motion"?yes("moved"):n=="spawn"?yes("spawned"):n=="queries"?yes("ray")&&yes("shape")&&yes("closest")&&yes("collider"):
   n=="contacts"||n=="trigger"?state.find("\"enters\":1")!=std::string::npos&&state.find("\"exits\":1")!=std::string::npos&&state.find("\"stays\":0")==std::string::npos:
   n=="audio"?yes("requested")&&yes("stopped")&&yes("settings")&&yes("seek")&&yes("oneShot")&&yes("group")&&yes("pausedDrop"):n=="particles"?yes("configured")&&yes("burst"):
   n=="ui"?yes("clicked"):n=="scene-session"?yes("queued")&&session->Pending()&&session->Get("example_visits")=="1":
   n=="joint"?yes("controlled"):n=="animation"?yes("ready")&&yes("fade")&&yes("layers"):
   n=="ragdoll"?yes("entered")&&yes("left"):n=="character"?yes("controlled")&&yes("launched"):
   yes("invalid")&&yes("throws")&&yes("characterThrows")&&yes("lookupNull")&&yes("staleLookup");
  Check(ok,n+" documented outcome");uint64_t stoppedEpoch=0;
  if(n=="deformable"){auto* d=world.RuntimeDeformable(owner,error);stoppedEpoch=d?d->generation:0;}
  game.End();
  if(n=="deformable"){
   world.RestoreAuthoredState();
   Check(stoppedEpoch&&session->Get("m62DestroyEpoch")=="\""+std::to_string(stoppedEpoch)+"\"","Stop destroy callback sees existing deformable before cleanup");
   auto* fresh=world.RuntimeDeformable(owner,error);Check(fresh&&fresh->generation!=stoppedEpoch&&session->Get("m62DestroyEpoch")!="\""+std::to_string(fresh->generation)+"\""&&fresh->velocities[0]==glm::dvec3(0),"authored reset creates a fresh rest instance after callbacks");
  }
  world.EndScripts();world.Destroy();Check(!world.Scripts(),n+" teardown");
 }
 host.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
