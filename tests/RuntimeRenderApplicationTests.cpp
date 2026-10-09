// M72 real VM, durable state and resource lifetime. Render pixels have a separate GL probe.
#include "EngineHost.h"
#include "Application.h"
#include "InteractivePlay.h"
#include "RuntimeUI.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "Prefab.h"
#include "NamedAuthoring.h"
#include "ScriptSystem.h"
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"
#include "editor/EditorDocument.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
namespace fs=std::filesystem;using J=nlohmann::json;
int checks=0,failures=0;void Check(bool ok,const std::string& name){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",name.c_str());}
std::string Read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
J State(RuntimeWorld& w){for(auto& s:w.Scripts()->Capture())if(s.entity==90)return J::parse(s.json);return J::object();}
int Live(const fs::path& out){
 // The ordinary application renders every frame. TestHarness screenshots only
 // render selected samples and must not be mistaken for runtime frame timings.
 ApplicationControl control;control.hidden=true;unsigned frames=0;std::string error;
 control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::printf("LIVE renderer=%s size=%dx%d\n",glGetString(GL_RENDERER),h.GetWindow().Width(),h.GetWindow().Height());};
 control.worldReady=[](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.Resources().WaitForAll();};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){
  h.GetRenderer().ResetStats();
  if(frames==40||frames==41)h.GetWindow().QueueTestPhysical("key:P",frames==40?1:0);
  if(frames==70||frames==71)h.GetWindow().QueueTestPhysical("key:C",frames==70?1:0);
  if(frames==95||frames==96)h.GetWindow().QueueTestPhysical("key:H",frames==95?1:0);
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){
  ++frames;if(frames==160)std::printf("LIVE bodies=%zu draws=%u triangles=%u fixed_ms=%.6f appearance_bytes=%zu\n",w.CountLifecycle().physicsBodies,h.GetRenderer().Stats().drawCalls,h.GetRenderer().Stats().triangles,play.LastFixedStepMilliseconds(),h.GetRenderer().AppearanceBytes());
  if(frames==180){Check(w.Scripts()&&w.Scripts()->Diagnostics().empty(),"normal application lab JS is fault-free");Check(!w.RenderVisible(60)&&w.Settings().sunIntensity!=1,"normal logical controls change visibility and animate light");w.UI().RequestQuit();}
 };
 auto project=fs::absolute("projects/render_control_lab/render_control_lab.judasproj").string();std::string program="judas";char* args[]={program.data(),project.data()};Application application;Check(application.Run(2,args,&control)==0&&frames==180,"ordinary startup, every-frame presentation and shutdown");Check(PerformanceProfiler::Get().Export((out/"lab-live-profile.json").string(),error),"normal application complete-frame M56 profile exported");
 std::printf("M72 live %d checks %d failures\n",checks,failures);return failures?1:0;
}
int main(int argc,char** argv){fs::path out=argc>1?argv[1]:".cache/m72/application";fs::create_directories(out);auto fixture=fs::absolute(out/"fixture");bool reading=argc>2&&std::string(argv[2])=="read";std::string error;
 if(argc>2&&std::string(argv[2])=="live")return Live(out);
 if(!reading){fs::remove_all(fixture);fs::copy("projects/render_control_lab",fixture,fs::copy_options::recursive);}
 EngineHost host;Check(host.Init("M72 runtime render",640,360,false,error),"ordinary host: "+error);if(failures)return 1;host.Audio().Init(error,true);Project project;Check(project.Load((fixture/"render_control_lab.judasproj").string(),error),"ordinary project: "+error);host.OpenProjectAssets(project.RootDir(),project.Resolve("Assets"));
 Scene authored;Check(LoadSceneFromFile(project.StartupScenePath(),authored,error),"registered scene: "+error);if(failures){std::puts(error.c_str());return 1;}
 GameSnapshot snapshot;snapshot.project="m72-render-lab";snapshot.scene="Scenes/lab.judas";snapshot.content=std::string(64,'a');RuntimeWorld world;world.legacyGameplay=false;
 if(reading){Check(DecodeGameSnapshot(Read(out/"saved.snapshot"),snapshot,error),"fresh process snapshot decoded");Scene saved;Check(WorldPersistence::SceneFrom(snapshot.participants,saved,error),"durable engine participant decodes");Check(world.Build(saved,&host.Resources(),error,nullptr,nullptr,true),"fresh world from saved rendering intent: "+error);host.Resources().WaitForAll();Check(WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"fresh resources and script state restored: "+error);world.UpdateScripts(nullptr,0);Check(!world.RenderVisible(60)&&world.RuntimeDefinition(40)->render->runtimeMaterials.count("*"),"fresh process restores visibility and instance material");Check(world.Settings().sunIntensity==.25f&&!world.Settings().sunEnabled,"fresh process restores actual sun controls");Check(State(world)["clock"]==12.5,"plain JS clock restored without replaying start");Check(world.ResetAppearance()&&world.Settings().sunEnabled&&world.Settings().sunIntensity==authored.Settings().sunIntensity,"fresh process reset restores original authored light");Check(world.Settings().environmentAsset==authored.Settings().environmentAsset&&host.Resources().RefCount(authored.Settings().environmentAsset)>0,"fresh saved empty environment resets to pinned authored resource");Check(world.ClearRuntimeMaterial(40,"*")&&world.RuntimeDefinition(40)->render->runtimeMaterials.count("*")==0&&world.RuntimeDefinition(40)->render->runtimeMaterials.size()==1,"fresh whole clear restores authored base while preserving the independent part patch");world.Destroy();host.Shutdown();std::printf("M72 fresh %d checks %d failures\n",checks,failures);return failures?1:0;}
 auto source=Read(fixture/"Assets/models/two-surfaces.glb");
 // A malformed registered image exists only in this owned failure fixture;
 // the ordinary project/package correctly refuses invalid export assets.
 std::ofstream(fixture/"Assets/textures/failed.png")<<"deliberate decode-failure fixture";
 std::ofstream(fixture/"Assets/textures/failed.png.judasmeta")<<"JudasAssetMeta 1\nid \"727272727272727272727272727272ba\"\ntype texture\nsource \"\"\n";std::ofstream(fixture/"Assets/scripts/lab.js")<<R"JS(
import {world,entity} from 'judas';
export default class {
 constructor(){this.state={clock:12.5};}
 start(){
  const a=entity('40'),b=entity('41'),m=a.material('*');
  this.state.initial=b.material('*').state;
  m.set({baseColor:{x:.1,y:.6,z:.2,a:.4},alphaMode:'blend',roughness:.3,normalStrength:.8,occlusionStrength:.7,doubleSided:true});
  this.state.changed=m.state;this.state.independent=b.material('*').state.baseColor.x===this.state.initial.baseColor.x;
  let rejected=0;for(const value of [{roughness:2,metallic:.3},{textures:{unknown:'bad'}},{roughness:.8,textures:{baseColor:'00000000000000000000000000000000'}}])try{m.set(value);}catch(e){rejected++;}
  this.state.rejected=rejected;this.state.atomic=Math.abs(m.state.roughness-.3)<1e-6;
  a.rendererVisible=false;a.renderVisible=false;a.renderVisible=true;this.state.componentPreserved=!a.rendererVisible;a.rendererVisible=true;
  const p=a.modelParts[0];a.setPartVisible(p.identity,false);a.renderVisible=false;a.renderVisible=true;this.state.partPreserved=!a.modelParts[0].visible;
  a.material(p.identity).set({emissive:{x:1,y:0,z:0},emissiveIntensity:.5});this.state.stable=a.material(p.identity).state.emissive.x===1;
  const figure=entity('35'),joint=figure.animation.info.joints.at(-1);entity('61').setSocket(figure,joint);entity('60').renderVisible=false;figure.renderVisible=false;this.state.hiddenJoint=!!figure.animation.jointTransform(joint,'world');this.state.hiddenSocket=entity('61').valid;
  world.setAppearance({sunDirection:{x:1,y:.2,z:.1},sunColor:{x:.8,y:.3,z:.1},sunIntensity:.25,sunEnabled:false,ambientColor:{x:.02,y:.03,z:.04},environmentAsset:''});
  try{world.setAppearance({sunIntensity:5,sunDirection:{x:0,y:0,z:0}});}catch(e){this.state.sunAtomic=world.appearance.sunIntensity===.25;}
  const replacement=b.material('*');replacement.set({textures:{baseColor:'727272727272727272727272727272ba'}});this.state.texturePending=replacement.state.status==='pending';
 }
 update(){const replacement=entity('41').material('*');if(!this.state.textureFailed&&replacement.state.status==='failed'){this.state.textureFailed=!!replacement.state.error;replacement.clearOverrides();this.state.textureReset=replacement.state.status==='ready';}}
 restore(){} // engine restores materials; JS restores its plain clock
}
)JS";host.Assets().Scan(project.RootDir(),project.Resolve("Assets"));
 Check(world.Build(authored,&host.Resources(),error,&project.Settings().classification,&project.Settings().navigation),"normal runtime build: "+error);host.Resources().WaitForAll();world.UpdateAnimations(1.f/60);world.UpdateScripts(nullptr,1.f/60);Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"actual public VM operations have no faults");if(!world.Scripts())return 1;
 auto state=State(world);for(const auto* key:{"independent","atomic","componentPreserved","partPreserved","stable","sunAtomic","hiddenJoint","hiddenSocket"})Check(state.value(key,false),std::string("VM ")+key);Check(state.value("rejected",0)==3,"invalid grouped material updates rejected atomically");Check(state.value("texturePending",false),"public VM reports pending registered replacement");host.Resources().WaitForAll();world.UpdateScripts(nullptr,0);state=State(world);Check(state.value("textureFailed",false)&&state.value("textureReset",false),"public VM exposes bounded decode failure then clears to ready authored source");
 auto hit=world.Physics().Raycast({0,1,10},{0,0,-1},3);Check(hit.hit&&world.EntityIdOfBody(hit.body)==60,"hidden solid retains authoritative collision/query geometry");Check(world.RuntimeAnimation(35)!=nullptr,"hidden animated figure retains pose producer");world.UpdateSockets();SceneTransform joint;const auto* socket=world.RuntimeDefinition(61);Check(socket&&socket->socket&&world.JointPose(35,socket->socket->joint,"world",1,joint)&&glm::length(world.RuntimeDefinition(61)->transform.position-joint.position)<1e-5f,"hidden animated socket resolves through the ordinary world pose path");
 world.UpdateScripts(nullptr,1.f/60);bool hiddenScript=false;for(auto& record:world.Scripts()->Capture())if(record.entity==60)hiddenScript=J::parse(record.json).value("ticks",0)>=2;Check(hiddenScript,"render-hidden solid script continues through ordinary frame callbacks");auto target=world.RuntimeDefinition(40);Scene durable=authored;*durable.Find(40)=*target;*durable.Find(60)=*world.RuntimeDefinition(60);durable.Settings()=world.Settings();std::string serialized;SaveSceneToString(durable,serialized);Scene parsed;Check(LoadSceneFromString(serialized,parsed,error)&&ScenesEqual(durable,parsed),"authored + durable overrides + visibility + sun round trip: "+error);
 Scene prefab,decoded;Check(CreatePrefab(durable,40,prefab,error),"prefab extraction");std::string encoded;SaveSceneToString(prefab,encoded);Check(LoadSceneFromString(encoded,decoded,error)&&SceneObjectsEqual(prefab.Objects()[0],decoded.Objects()[0]),"prefab retains per-instance and stable part bindings: "+error);
 EditorDocument doc;Check(doc.Load(project.StartupScenePath(),error),"editor document loads");doc.BeginEdit();doc.GetScene().Find(40)->renderVisible=false;doc.GetScene().Find(40)->render->hiddenParts.push_back("authored-test");doc.GetScene().Settings().backgroundColor={.3,.2,.1};doc.CommitEdit(false);Check(doc.CanUndo()&&doc.IsDirty(),"visibility/parts/background changes record ordinary undo");doc.Undo();Check(doc.GetScene().Find(40)->renderVisible&&doc.GetScene().Find(40)->render->hiddenParts.empty(),"authored undo restores render-only state");doc.Redo();Check(!doc.GetScene().Find(40)->renderVisible,"authored redo restores accepted changes");
 Check(Read(fixture/"Assets/models/two-surfaces.glb")==source,"shared imported source bytes untouched");
 // Latest durable intent is used at draw time. Superseded asynchronous requests have no publication callback.
 auto tex="42f619a708d0f7677faae196b78100d3";auto before=host.Resources().RefCount(tex);MaterialSlot texture;texture.overrides.textures[0]=tex;Check(world.SetRuntimeMaterial(41,"*",texture),"texture replacement accepted through ordinary manager");Check(host.Resources().RefCount(tex)==before+1,"one bounded instance demand acquired");Check(world.ClearRuntimeMaterial(41,"*")&&host.Resources().RefCount(tex)==before,"clear retires superseded demand before upload");host.Resources().WaitForAll();Check(world.RuntimeDefinition(41)->render->runtimeMaterials.empty(),"late completion cannot restore reset intent");
 SceneObject region;region.id=150;region.name="additive render";region.render.emplace();region.render->runtimeMaterials["*"]=texture;region.transform.position={8,1,5};auto sun=world.Settings().sunDirection;Check(world.StageRegionObject(region,region,error,"m72:region"),"ordinary additive entity stages: "+error);host.Resources().WaitForAll();world.PublishRegion({150});Check(world.Settings().sunDirection==sun,"additive publication preserves shared world environment");Check(world.RenderVisible(150)&&host.Resources().RefCount(tex)==before+1,"staged override demand rebuilt");world.RemoveRegionObject(150);Check(!world.RuntimeDefinition(150)&&host.Resources().RefCount(tex)==before,"region removal retires instance demand and handles");
 host.Resources().WaitForAll();Check(WorldPersistence::Prepare(world,error)&&WorldPersistence::Capture(world,snapshot.participants,error),"ordinary save participants capture rendering state: "+error);std::ofstream(out/"saved.snapshot",std::ios::binary)<<EncodeGameSnapshot(snapshot);
 auto view=glm::lookAt(glm::vec3(0,4,15),glm::vec3(0,1,0),glm::vec3(0,1,0)),projection=glm::perspective(glm::radians(60.f),640.f/360,.1f,100.f);RenderWorldFrame(host.GetRenderer(),640,360,world,nullptr,view,projection,{0,4,15},.5f);std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(640,360,pixels);WriteRgbPng((out/"public-vm-render.png").string(),640,360,pixels);Check(world.CameraTexture(50).IsValid(),"live auxiliary screen remains allocated");Check(glGetError()==GL_NO_ERROR,"main/shadow/live-screen state remains coherent");
 const auto start=std::chrono::steady_clock::now();for(int i=0;i<10000;++i){auto light=world.Settings();light.sunEnabled=bool(i&1);light.sunDirection={.4f,float(.5+i%10*.01),1};world.SetAppearance(light);MaterialSlot o;o.overrides.roughness=.1f+float(i%8)*.1f;world.SetRuntimeMaterial(41,"*",o);world.SetRenderVisible(41,true,bool(i&1));}auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::printf("PERF 10000 grouped lighting/material/visibility writes %.3f ms %.6f ms/update, no shader/model rebuild\n",ms,ms/10000);
 world.Destroy();Check(host.Resources().RefCount(tex)==0,"world teardown retires authored and instance references");host.Shutdown();std::printf("M72 application %d checks %d failures\n",checks,failures);return failures?1:0;
}
