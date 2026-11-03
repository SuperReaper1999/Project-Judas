#include "EngineHost.h"
#include "Application.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ToolProcess.h"
#include <SDL2/SDL.h>
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include "ScriptSystem.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace {
using J=nlohmann::json;namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const std::string& label){++checks;if(!ok){++failures;std::cout<<"FAIL "<<label<<'\n';}}
J State(RuntimeWorld& world){for(const auto& record:world.Scripts()->Capture()) {auto j=J::parse(record.json);if(j.contains("bag"))return j;}throw std::runtime_error("menu state missing");}
J Totals(const J& state){J result=J::object();auto add=[&](const J& item){if(!item.is_null()){auto id=item.at("id").get<std::string>();result[id]=result.value(id,0)+item.at("n").get<int>();}};for(const auto& x:state.at("bag"))add(x);for(const auto& x:state.at("near"))add(x);for(const auto& x:state.at("equipment").items())add(x.value());return result;}
SceneObjectId Id(const Scene& scene,const char* prefix){for(const auto& object:scene.Objects())if(object.name.rfind(prefix,0)==0)return object.id;throw std::runtime_error(std::string("missing ")+prefix);}
void ScriptPolicy(Renderer& renderer,const fs::path& output){
 const auto root=output/"binding-project";fs::create_directories(root/"Assets");std::string error;
 UIDocument doc;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;UIElement image;image.id="preview";image.parent="canvas";image.kind=UIKind::Image;UIElement text=image;text.id="label";text.kind=UIKind::Text;doc.elements={canvas,image,text};
 Check(SaveUIDocument((root/"Assets/probe.judasui").string(),doc,error),"binding fixture uses ordinary UI authoring");
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {ui,Entity} from 'judas';
 export const properties={camera:{type:'entity'},other:{type:'entity'}};
 export default class {constructor({properties}){this.p=properties;this.state={rejects:0,ticks:0,stale:false,font:false};}
 start(){this.doc=ui.get('probe');this.image=this.doc.get('preview');this.image.setPreviewCamera(this.p.camera);
  for(const attempt of [()=>this.image.setPreviewCamera(this.p.other),()=>this.doc.get('label').setPreviewCamera(this.p.camera),()=>this.image.setPreviewCamera(new Entity('9999999999999')),()=>this.image.setPreviewCamera(42),()=>this.image.setPreviewCamera({id:this.p.camera.id})])try{attempt();}catch(e){this.state.rejects++;}
  this.image.setPreviewCamera(null);this.image.setPreviewCamera(this.p.camera);
  this.image.configure({fontSize:28});this.state.font=this.image.fontSize===28;
 }
 uiUpdate(){if(++this.state.ticks===2){try{this.image.setPreviewCamera(this.p.camera);}catch(e){this.state.stale=e.name==='ReferenceError';}}}
 })JS";
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord uiAsset,script;Check(assets.Track((root/"Assets/probe.judasui").string(),uiAsset,error)&&assets.Track((root/"Assets/probe.js").string(),script,error),"binding fixture tracks actual script/UI assets");
 Scene scene;auto& camera=scene.CreateObject("Camera");const auto cameraId=camera.id;camera.renderCamera=SceneRenderCameraComponent{};
 auto& other=scene.CreateObject("Not a camera");const auto otherId=other.id;other.render=SceneRenderComponent{};
 auto& owner=scene.CreateObject("Script owner");const auto ownerId=owner.id;owner.ui=SceneUIComponent{uiAsset.id,"probe",true};owner.scripts.push_back({1,script.id,true,"{}"});ScriptSystem::SetPropertyEntity(owner.scripts[0].properties,"camera",cameraId);ScriptSystem::SetPropertyEntity(owner.scripts[0].properties,"other",otherId);
 ResourceManager resources(&renderer,&assets);RuntimeWorld world;ProjectSettings settings;settings.legacyGameplay=false;world.ConfigureProject(settings);Check(world.Build(scene,&resources,error),"binding world builds");InputSystem input;world.UpdateUIScripts(&input,.016f);
 J state;for(auto& r:world.Scripts()->Capture())if(r.entity==ownerId)state=J::parse(r.json);
 Check(state.value("rejects",0)==5&&state.value("font",false),"public script bridge rejects noncamera/nonimage/missing sources and reads actual font style");
 const auto h=world.UI().Find("probe");Check(world.UI().PreviewCamera(h,"preview")==cameraId,"invalid replacement preserves previous valid binding");world.UI().Unload(h);world.UpdateUIScripts(&input,.016f);
 for(auto& r:world.Scripts()->Capture())if(r.entity==ownerId)state=J::parse(r.json);
 Check(state.value("stale",false)&&world.Scripts()->Diagnostics().empty(),"unloaded preview handles throw ReferenceError without script fault");world.Destroy();resources.Shutdown();
}

int ApplicationCase(const fs::path& projectFile,const fs::path& output){
 const auto clone=output/"application-project";fs::create_directories(output);fs::copy(projectFile.parent_path(),clone,fs::copy_options::recursive);Project project;std::string error;Check(project.Load((clone/projectFile.filename()).string(),error),"application clone loads");project.Settings().saveIdentity=MintAssetId()+MintAssetId();Check(project.Save(error),"save workflow owns a fresh isolated identity");
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};control.hostReady=[](EngineHost& host){std::string e;host.Audio().Init(e,true);};
 int phase=0,frames=0;bool done=false;RuntimeWorld* original=nullptr;J expected;std::uint64_t request=0;
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){
  ++frames;const auto h=world.UI().Find("satchel");if(!h)return;
  if(world.Scripts()&&!world.Scripts()->Diagnostics().empty()){for(const auto& d:world.Scripts()->Diagnostics())std::cout<<"SCRIPT "<<d.message<<'\n';Check(false,"real application is script-fault-free");phase=99;}
  auto click=[&](const char* id){auto& ui=world.UI();ui.Layout(host.GetWindow().Width(),host.GetWindow().Height());const auto r=ui.LayoutOf(h,id)->rect;const auto p=r.position+r.size*.5f;InputSystem input;input.SetPhysical("mouse:Left",1);ui.Input(input,p,true,host.GetWindow().Width(),host.GetWindow().Height());input.BeginFrame();input.SetPhysical("mouse:Left",0);ui.Input(input,p,true,host.GetWindow().Width(),host.GetWindow().Height());world.DispatchUIEvents(&input,.016f);};
  if(phase==0){click("bag0");click("use");expected=State(world);Check(expected["equipment"]["weapon"]["id"]=="sword","application equips through ordinary UI action");original=&world;click("save");request=State(world)["request"].get<std::uint64_t>();Check(request!=0,"actual Save button queues public SaveService request");phase=1;}
  else if(phase==1){auto* service=world.SceneControl()->Saves(host.Resources());const auto* status=service->Status(request);if(status&&status->state=="failed"){Check(false,"save: "+status->error);phase=99;}else if(status&&status->state=="completed"){
    Check(true,"normal asynchronous save commits while paused");click("bag1");click("use");Check(State(world)["bag"]!=expected["bag"],"post-save change creates meaningful load negative control");click("load");phase=2;
  }}else if(phase==2&& &world!=original){const auto restored=State(world);Check(restored["bag"]==expected["bag"]&&restored["equipment"]==expected["equipment"]&&restored["health"]==expected["health"],"actual Load button replaces world and restores pre-mutation progress");
   Scene scene;LoadSceneFromFile(project.StartupScenePath(),scene,error);const auto camera=Id(scene,"Menu preview camera");Check(world.UI().PreviewCamera(h,"preview")==camera&&world.CameraTexture(camera).IsValid(),"loaded application reconstructs live UI preview binding");Check(world.RenderVisible(Id(scene,"Equipped blade")),"restored equipment controls actual rendered attachment");done=true;phase=99;
  }
  if(phase==1||phase==2)SDL_Delay(1);
  if(frames>1500){Check(false,"application save/load completes within bounded frame budget");phase=99;}
  if(phase==99){SDL_Event quit{};quit.type=SDL_QUIT;SDL_PushEvent(&quit);}
 };
 std::string executable="judas-ui-menu-test",path=project.ProjectFile();char* args[]={executable.data(),path.data()};Application app;Check(app.Run(2,args,&control)==0&&done,"ordinary application startup, save/load and shutdown complete");std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}

}
int main(int argc,char** argv)try{
 if(argc!=4)throw std::runtime_error("usage: ui-menu-tests write|read project output-directory");
 const std::string mode=argv[1];
 if(mode=="roundtrip"){
  const auto output=fs::absolute(argv[3]);fs::create_directories(output);
  for(const char* child:{"write","read","application"}){std::string text,error;const bool ok=CaptureToolOutput(fs::absolute(argv[0]),{child,fs::absolute(argv[2]).string(),output.string()},text,error,std::chrono::milliseconds(60000),1024*1024);std::ofstream(output/(std::string(child)+".log"))<<text<<error;std::cout<<text;Check(ok,std::string("fresh process ")+child+": "+error);if(!ok)return 1;}
  return failures?1:0;
 }
 if(mode=="application")return ApplicationCase(fs::absolute(argv[2]),fs::absolute(argv[3]));
 const bool read=std::string(argv[1])=="read";if(!read&&std::string(argv[1])!="write")return 2;
 const auto output=fs::absolute(argv[3]);fs::create_directories(output);std::string error;
 Project project;Check(project.Load(argv[2],error),"actual menu project loads: "+error);
 EngineHost host;Check(host.Init("UI menu qualification",1280,720,false,error),"native GL host: "+error);if(failures)return 2;
 host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
 Scene authored;Check(LoadSceneFromFile(project.StartupScenePath(),authored,error),"authored scene loads");
 const auto camera=Id(authored,"Menu preview camera"),witness=Id(authored,"Pause witness");
 RuntimeWorld world;world.ConfigureProject(project.Settings());
 if(read){
  std::ifstream f(output/"snapshot.save",std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(f),{}};GameSnapshot snapshot;Scene saved;
  Check(DecodeGameSnapshot(bytes,snapshot,error)&&WorldPersistence::SceneFrom(snapshot.participants,saved,error)&&world.Build(saved,&host.Resources(),error,&project.Settings().classification,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"fresh process restores real scene/physics/script participants: "+error);
 }else Check(world.Build(authored,&host.Resources(),error,&project.Settings().classification),"ordinary runtime builds scene");
 if(!world.IsBuilt())return 2;
 InputSystem input;input.SetMap(project.Settings().input,error);world.UpdateUIScripts(&input,1.f/60);
 const auto h=world.UI().Find("satchel");Check(h&&world.UI().Paused(),"project menu is genuinely modal");if(!h)return 2;
 Check(world.UI().PreviewCamera(h,"preview")==camera,"script binds actual render camera");
 auto frame=[&](glm::vec2 point,float button){input.BeginFrame();input.SetPhysical("mouse:Left",button);world.UpdateUIScripts(&input,1.f/60);world.UI().Input(input,point,true,1280,720);world.DispatchUIEvents(&input,1.f/60);};
 auto point=[&](const char* id){world.UI().Layout(1280,720);auto rect=world.UI().LayoutOf(h,id)->rect;return rect.position+rect.size*.5f;};
 auto click=[&](const char* id){const auto p=point(id);frame(p,1);frame(p,0);};
 auto drag=[&](const char* from,const char* to){const auto a=point(from),b=point(to);frame(a,1);frame(a+glm::vec2(20,0),1);frame(b,1);frame(b,0);};
 if(read){
  std::ifstream poseFile(output/"expected-body.json");J bodyExpected;poseFile>>bodyExpected;const auto pose=world.Physics().GetTransform(world.RuntimeBody(witness));const auto velocity=world.Physics().GetLinearVelocity(world.RuntimeBody(witness));
  Check(J::array({pose.position.x,pose.position.y,pose.position.z})==bodyExpected["position"]&&J::array({velocity.x,velocity.y,velocity.z})==bodyExpected["velocity"],"cold load restores actual physical endpoint, not just UI state");
  Check(!world.CameraTexture(camera).IsValid(),"cold load does not revive an old GPU target");
  std::ifstream file(output/"expected.json");J expected;file>>expected;Check(State(world)==expected,"cold restoration retains actual inventory, counts, equipment, health and UI preferences");
 }else{
  const auto original=Totals(State(world));drag("bag0","weapon");auto state=State(world);Check(state["bag"][0].is_null()&&state["equipment"]["weapon"]["id"]=="sword","actual pointer drop equips item through JavaScript");Check(Totals(state)==original,"equipping conserves all item counts");
  const auto before=State(world);drag("bag2","offhand");Check(State(world)["bag"]==before["bag"]&&State(world)["equipment"]==before["equipment"],"game rejects compatible-transport but invalid equipment transfer without loss");
  drag("near0","bag2");state=State(world);Check(state["near"][0].is_null()&&state["bag"][2]["n"]==17,"nearby container merges real stored stack");Check(Totals(state)==original,"container transfer preserves resources");
  click("bag2");click("split");state=State(world);Check(state["bag"][0]["n"]==8&&state["bag"][2]["n"]==9&&Totals(state)==original,"split uses real empty slot without duplication");
  click("bag1");click("use");state=State(world);Check(state["health"]==80&&state["bag"][1]["n"]==5,"using food consumes one and changes health");
  drag("bag6","offhand");Check(State(world)["equipment"]["offhand"]["id"]=="shield","second equipment slot uses same transfer mechanism");
  drag("near2","bag3");const auto heavy=State(world);drag("near3","pack");state=State(world);Check(state["near"]==heavy["near"]&&state["bag"]==heavy["bag"]&&state["equipment"]==heavy["equipment"],"overweight pickup is rejected atomically without loss");
  drag("bag3","near2");drag("near3","pack");Check(State(world)["equipment"]["pack"]["id"]=="pack","freeing capacity permits the same equipment transfer");
  click("tab_settings");click("scale_large");Check(State(world)["scale"].get<double>()>1&&world.UI().Element(h,"settings_group")->visible,"settings tab actually changes readable text size");click("tab_inventory");
 }
 Check(world.Scripts()->Diagnostics().empty(),"real menu callbacks have no script faults");
 auto& renderer=host.GetRenderer();auto& window=host.GetWindow();window.Input().SetMap(project.Settings().input,error);
 InteractivePlay play;Check(play.Begin(world,WorldCoordinates{},error),"normal interactive Play begins");
 const auto body=world.RuntimeBody(witness);const auto initial=world.Physics().GetTransform(body);
 for(int i=0;i<12;++i){window.Input().BeginFrame();host.PumpResources();play.Frame(window,renderer,1.f/60,false,true);}
 Check(glm::length(world.Physics().GetTransform(body).position-initial.position)==0,"actual paused Play leaves physical endpoint unchanged");
 Check(world.CameraTexture(camera).IsValid()&&world.PresentationCameras()[0].updates>=12,"preview camera updates while physical world is paused");
 std::vector<unsigned char> pixels,before;renderer.CaptureFrame(1280,720,before);Check(WriteRgbPng((output/(read?"cold-menu.png":"menu.png")).string(),1280,720,before),"real menu GL frame captured");
 click("turn_right");window.Input().BeginFrame();play.Frame(window,renderer,1.f/60,false,true);renderer.CaptureFrame(1280,720,pixels);
 const auto rect=world.UI().LayoutOf(h,"preview")->rect;size_t changed=0;
 for(int y=int(rect.position.y);y<int(rect.position.y+rect.size.y);++y)for(int x=int(rect.position.x);x<int(rect.position.x+rect.size.x);++x){auto i=(size_t(y)*1280+x)*3;if(pixels[i]!=before[i]||pixels[i+1]!=before[i+1]||pixels[i+2]!=before[i+2])++changed;}
 Check(changed>1000,"rotating real geometry changes preview pixels, not a static portrait");
 Check(glm::length(world.Physics().GetTransform(body).position-initial.position)==0,"preview rotation does not advance body physics");
 Check(WriteRgbPng((output/"rotated-menu.png").string(),1280,720,pixels),"rotated preview captured");
 // An independent colour pattern detects vertical inversion and stale disabled targets.
 auto& source=world.PresentationCameras()[0];Check(renderer.BeginRenderTarget(source.target),"known-answer preview target opens");renderer.BeginUIFrame(source.settings.width,source.settings.height);renderer.DrawUIRect({0,0},{float(source.settings.width),source.settings.height*.5f},{1,0,0,1});renderer.DrawUIRect({0,source.settings.height*.5f},{float(source.settings.width),source.settings.height*.5f},{0,0,1,1});renderer.EndUIFrame();renderer.EndRenderTarget();
 RuntimeUI probe;UIDocument probeDoc;probeDoc.reference={200,100};UIElement root;root.id="canvas";root.kind=UIKind::Canvas;UIElement image;image.id="image";image.parent="canvas";image.kind=UIKind::Image;image.size={200,100};image.fit=false;image.background={0,1,0,1};probeDoc.elements={root,image};auto ph=probe.Add(probeDoc,"pixels",0,error);probe.SetPreviewCamera(ph,"image",camera);
 auto drawProbe=[&]{renderer.BeginFrame(200,100);renderer.EndFrame();renderer.BeginUIFrame(200,100);probe.Draw(renderer,200,100,&world);renderer.EndUIFrame();renderer.CaptureFrame(200,100,pixels);};drawProbe();
 auto channel=[&](int y,int c){return pixels[(size_t(y)*200+100)*3+c];};Check(channel(25,0)>200&&channel(25,2)<20&&channel(75,2)>200&&channel(75,0)<20,"camera-to-UI image has independently verified upright colour orientation");
 source.settings.enabled=false;drawProbe();Check(channel(25,1)>200&&channel(25,0)<20,"disabled camera clears to panel background instead of stale frame");source.settings.enabled=true;probe.Unload(ph);
 const auto targets=renderer.RenderTargetDiagnostics().liveTargets;
 for(int i=0;i<8;++i){
  click("resume");const auto updates=world.PresentationCameras()[0].updates;window.Input().BeginFrame();play.Frame(window,renderer,1.f/60,false,true);
  Check(!world.UI().Paused()&&!world.PresentationCameras()[0].settings.enabled&&world.PresentationCameras()[0].updates==updates,"actual Resume disables preview work while the world runs");
  input.Reset();input.SetPhysical("key:Escape",1);world.UpdateUIScripts(&input,.016f);world.UI().Input(input,{-1,-1},false,1280,720);world.DispatchUIEvents(&input,.016f);input.Reset();
  window.Input().BeginFrame();play.Frame(window,renderer,1.f/60,false,true);Check(world.UI().Paused(),"actual Escape reopens usable menu");
 }
 Check(renderer.RenderTargetDiagnostics().liveTargets==targets,"repeated UI presentation reuses target allocation");
 if(!read){GameSnapshot saved;saved.project="ui-menu-test";saved.scene="Scenes/main.judas";saved.content=std::string(64,'c');Check(WorldPersistence::Capture(world,saved.participants,error),"capture modern participant state: "+error);std::ofstream(output/"snapshot.save",std::ios::binary)<<EncodeGameSnapshot(saved);std::ofstream(output/"expected.json")<<State(world).dump(2);const auto pose=world.Physics().GetTransform(body);const auto velocity=world.Physics().GetLinearVelocity(body);std::ofstream(output/"expected-body.json")<<J{{"position",{pose.position.x,pose.position.y,pose.position.z}},{"velocity",{velocity.x,velocity.y,velocity.z}}}.dump();}
 play.End();world.Destroy();Check(renderer.RenderTargetDiagnostics().liveTargets==0,"Stop/world destruction releases preview GPU target");
 if(!read)ScriptPolicy(renderer,output);
 host.Shutdown();std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<"ERROR "<<e.what()<<'\n';return 2;}
