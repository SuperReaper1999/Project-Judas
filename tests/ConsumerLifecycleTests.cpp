// Production VM, UI, harness and outer-boundary services; no substitute scheduler.
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "ScriptFaultNotice.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "TestHarness.h"
#include "ScreenshotWriter.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <thread>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool ok,const std::string& label){++checks;std::printf("%s %s\n",ok?"PASS":"FAIL",label.c_str());if(!ok)throw std::runtime_error(label);}
int main(int argc,char** argv){try{
 const auto root=fs::absolute(argc>1?argv[1]:"build/consumer-lifecycle");
 // Caller supplies a dedicated test directory. Never delete arbitrary projects.
 fs::create_directories(root);const auto projectRoot=root/"project";
 Project project;std::string error;
 if(fs::exists(projectRoot/"Lifecycle.judasproj"))Check(project.Load((projectRoot/"Lifecycle.judasproj").string(),error),error);
 else Check(Project::CreateNew(projectRoot.string(),"Lifecycle",project,error),error);
 project.Settings().startupScene="Scenes/main.judas";project.Settings().legacyGameplay=false;Check(project.Save(error),"save test project configuration");
 fs::create_directories(projectRoot/"Assets");
 std::ofstream(projectRoot/"Assets/quit.js")<<R"JS(import {ui} from 'judas';
 export default class {constructor(){this.state={frames:0};}uiUpdate(){if(++this.state.frames===3)ui.quit();}})JS";
 std::ofstream(projectRoot/"Assets/fault.js")<<R"JS(export default class {onUI(){throw Error('poisoned UI '+ 'x'.repeat(5000));}})JS";
 std::ofstream(projectRoot/"Assets/observe.js")<<R"JS(import {scripts,saves} from 'judas';
 export default class {constructor(){this.state={ticks:0};}uiUpdate(){this.state.ticks++;this.state.busy=saves.busy;
 const faults=scripts.faults();if(faults.length){if(faults.length!==1||faults[0].callback!=='onUI'||!faults[0].truncated||faults[0].message.length>4096)throw Error('fault snapshot contract');
 faults[0].message='changed';if(scripts.faults()[0].message==='changed')throw Error('live fault mutated');this.state.observed=true;}}})JS";
 std::ofstream(projectRoot/"Assets/save.js")<<R"JS(import {saves,scenes,session} from 'judas';
 export default class {constructor(){this.state={phase:0};}uiUpdate(){
 if(this.state.phase===0){if(saves.busy)throw Error('initial busy');this.token=saves.save('lifecycle-proof');if(!saves.busy)throw Error('save not busy');
 let rejected=false;try{scenes.reload();}catch(e){rejected=true;}if(!rejected)throw Error('busy policy weakened');this.state.phase=1;}
 else if(this.state.phase===1){const s=saves.status(this.token);if(s.state==='failed')throw Error(s.error);
 for(const key of ['queuedMs','identityMs','workerQueueMs','elapsedMs'])if(!Number.isFinite(s[key])||s[key]<0)throw Error('invalid timing '+key);
 if(s.state==='completed'){if(saves.busy||s.waitingFor!==''||s.elapsedMs<s.captureMs)throw Error('completion contract');this.state.done=true;this.state.phase=2;}}
 else if(this.state.phase===2)this.state.transitionBusy=saves.busy;
 }})JS";
 EngineHost host;Check(host.Init("Consumer lifecycle",640,360,false,error),error);host.OpenProjectAssets(project.RootDir(),project.AssetsDir());
 host.GetWindow().SetTestInputMode(true);host.Audio().Init(error,true);
 auto asset=[&](const char* name){if(const auto* existing=host.Assets().FindByRelativePath(std::string("Assets/")+name))return existing->id;AssetRecord r;Check(host.Assets().Track((projectRoot/"Assets"/name).string(),r,error),error);return r.id;};
 const auto quitter=asset("quit.js"),fault=asset("fault.js"),observer=asset("observe.js"),saver=asset("save.js");
 auto harness=[&](const char* name,const std::string& text,bool quitting,unsigned& frames){
  Scene scene;if(quitting)scene.CreateObject("Quit owner").scripts.push_back({1,quitter,true,"{}"});
  RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&host.Resources(),error),error);InteractivePlay play;Check(play.Begin(world,WorldCoordinates{},error),error);
  std::ofstream(root/name)<<text;frames=0;
  const int result=RunTestHarness(host.GetWindow(),host.GetRenderer(),play,(root/name).string(),{},[&](){++frames;},[](const std::string&){return false;});play.End();return result;
 };
 unsigned frames=0;
 Check(harness("quit.txt","FRAMES 12\nLOG_EVERY 0\nEXPECT_NO_QUIT 0\nEXPECT_QUIT 8\n",true,frames)==0&&frames==3,"JudasJS quit ends harness immediately and satisfies deadline");
 Check(harness("missing-quit.txt","FRAMES 4\nEXPECT_QUIT 3\n",false,frames)!=0,"negative control: missing quit fails");
 Check(harness("pending.txt","FRAMES 8\nEXPECT_NO_QUIT 7\n",true,frames)!=0,"early quit cannot skip future proof");
 Check(harness("same-frame.txt","FRAMES 5\nEXPECT_NO_QUIT 2\n",true,frames)!=0,"same-frame unexpected quit fails");
 Check(harness("pending-capture.txt","FRAMES 8\nSCREENSHOT 7 skipped.png\n",true,frames)!=0,"early quit cannot skip capture");
 Check(harness("pending-service.txt","FRAMES 5\nWAIT_SERVICES save 100 2\n",true,frames)!=0,"quit cannot bypass incomplete same-frame service wait");
 Scene scene;const auto owner=scene.CreateObject("Fault owner").id;scene.Find(owner)->scripts.push_back({1,fault,true,"{}"});
 const auto healthy=scene.CreateObject("Healthy observer").id;scene.Find(healthy)->scripts.push_back({1,observer,true,"{}"});
 RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&host.Resources(),error),error);InputSystem input;auto faultSession=std::make_shared<SceneSession>(project,project.StartupScenePath());world.SetSceneControl(faultSession);world.UpdateUIScripts(&input,.016f);
 Check(!faultSession->SavesIfLoaded(),"busy query alone does not instantiate or hash save service");
 UIDocument doc;doc.reference={640,360};UIElement canvas;canvas.id="root";canvas.kind=UIKind::Canvas;doc.elements.push_back(canvas);
 UIElement button;button.id="button";button.parent="root";button.kind=UIKind::Button;button.size={200,100};doc.elements.push_back(button);
 auto handle=world.UI().Add(doc,"fault-ui",owner,error,1);Check(handle!=0&&world.UI().Focus(handle,"button"),"script-owned UI has focused button");
 input.SetPhysical("key:Return",1);world.UI().Input(input,{},false,640,360);world.DispatchUIEvents(&input,.016f);world.UpdateUIScripts(&input,.016f);
 Check(world.Scripts()->InspectPhase(owner,1)==ScriptInstancePhase::Faulted,"actual UI activation faults only owner slot");
 bool observed=false;for(const auto& s:world.Scripts()->Capture())if(s.entity==healthy)observed=s.json.find("\"observed\":true")!=std::string::npos;
 Check(observed&&world.Scripts()->InspectPhase(healthy,1)==ScriptInstancePhase::Running,"healthy UI callback observes detached bounded fault snapshot");
 const auto notice=LatestScriptFaultNotice(world.Scripts(),&host.Assets());Check(notice.callback=="onUI"&&notice.source.find("fault.js")!=std::string::npos&&notice.message.find("poisoned UI")!=std::string::npos,"notice resolves actionable source and failing callback");
 InteractivePlay play;play.SetScriptFaultNoticeEnabled(true);Check(play.Begin(world,WorldCoordinates{},error),error);play.Frame(host.GetWindow(),host.GetRenderer(),.016f);
 std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(640,360,pixels);const auto i=(std::size_t(10)*640+10)*3;
 Check(pixels.size()==640*360*3&&pixels[i]>40&&pixels[i+1]<30&&pixels[i+2]<30,"fault notice is rendered over the real frame");
 WriteRgbPng((root/"fault-notice.png").string(),640,360,pixels);
 play.SetScriptFaultNoticeEnabled(false);play.End();Check(play.Begin(world,WorldCoordinates{},error),error);
 play.Frame(host.GetWindow(),host.GetRenderer(),.016f);host.GetRenderer().CaptureFrame(640,360,pixels);
 Check(!(pixels[i]>40&&pixels[i+1]<30&&pixels[i+2]<30),"explicit banner policy survives a new play session");play.End();
 // Actual save service, script request, busy rejection and completion while modal.
 Scene saveScene;saveScene.CreateObject("Save owner").scripts.push_back({1,saver,true,"{}"});Check(SaveSceneToFile(saveScene,project.StartupScenePath(),error),error);
 auto savedWorld=std::make_unique<RuntimeWorld>();savedWorld->legacyGameplay=false;Check(savedWorld->Build(saveScene,&host.Resources(),error),error);
 auto session=std::make_shared<SceneSession>(project,project.StartupScenePath());savedWorld->SetSceneControl(session);Check(play.Begin(*savedWorld,WorldCoordinates{},error),error);
 UIDocument modal;modal.modal=true;modal.elements.push_back(canvas);savedWorld->UI().Add(modal,"pause",0,error);
 bool done=false;const auto start=std::chrono::steady_clock::now();
 while(std::chrono::steady_clock::now()-start<std::chrono::seconds(10)){
  host.PumpResources();savedWorld->UpdateUIScripts(&input,.016f);Check(savedWorld->Scripts()->Diagnostics().empty(),"save callback healthy");
  Check(session->AdvanceOuter(savedWorld,play,host.Resources(),input,error),error);
  for(const auto& s:savedWorld->Scripts()->Capture())done|=s.json.find("\"done\":true")!=std::string::npos;
  if(done)break;
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 Check(done,"modal save completes with busy policy and real-time diagnostic fields");
 auto* service=session->SavesIfLoaded();Check(service!=nullptr,"script created ordinary save service");
 for(const char* operation:{"list","load","delete"}){
  const auto token=service->Request(operation,"lifecycle-proof","","{}",error);Check(token!=0,"lifecycle operation accepted");
  savedWorld->UpdateUIScripts(&input,.016f);bool busy=false;
  for(const auto& s:savedWorld->Scripts()->Capture())busy|=s.json.find("\"transitionBusy\":true")!=std::string::npos;
  Check(busy,"public busy also observes list/load/delete");
  Check(service->Cancel(token)&&!service->Busy()&&service->Status(token)->waitingFor.empty(),"queued cancellation releases gate and finishes diagnostics");
 }
 Check(session->Reload(error)&&session->OperationInProgress(),"scene request also publishes lifecycle busy");
 savedWorld->UpdateUIScripts(&input,.016f);Check(savedWorld->Scripts()->Diagnostics().empty(),"completed observer stays healthy during transition");
 bool transition=false;for(const auto& s:savedWorld->Scripts()->Capture())transition|=s.json.find("\"transitionBusy\":true")!=std::string::npos;
 Check(transition,"public busy query includes pending scene replacement");
 play.End();std::printf("CONSUMER_LIFECYCLE checks=%u failures=0\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
