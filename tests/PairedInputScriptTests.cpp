// M69: the real VM and play boundary, not a mocked input object.
#include "AuthoringJSON.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs=std::filesystem;
using J=AuthoringJSON::J;
namespace {
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
J State(RuntimeWorld& world){
    if(world.Scripts())for(const auto& s:world.Scripts()->Capture())if(s.entity==1)return J::parse(s.json);
    return J::object();
}
void Flick(InputSystem& input){
    input.SetStick("right",0,0,true);
    input.SetStick("right",.6f,.8f);
    input.SetStick("right",0,0);
}
}
int main(int argc,char** argv){
    const fs::path out=argc>1?argv[1]:".cache/m69/paired-js";
    fs::create_directories(out);const auto root=fs::absolute(out/"project");
    std::string error;Project project;
    Check(Project::CreateNew(root.string(),"M69 paired input",project,error),"ordinary private project created");
    if(failures){std::puts(error.c_str());return 1;}
    EngineHost host;
    Check(host.Init("M69 paired input",640,360,false,error),"real EngineHost and input service");
    if(failures){std::puts(error.c_str());return 1;}
    host.Audio().Init(error,true);host.OpenProjectAssets(root.string(),(root/"Assets").string());
    host.Resources().SetBlockingMode(true);
    std::ofstream(root/"Assets/probe.js")<<R"JS(
import {input,ui,time} from 'judas';
export default class {
 constructor(){this.state={frames:0,steps:0,frameSamples:0,fixedSamples:0,resets:0,overflows:0};this.frameCursor=0;this.fixedCursor=0;}
 uiUpdate(){const d=ui.get('fixture');if(d&&input.pressed('pause'))d.modal=!d.modal;}
 update(){
  const s=input.stickSamples('right',this.frameCursor);this.frameCursor=s.sequence;
  this.state.frames++;this.state.resets+=s.reset?1:0;this.state.overflows+=s.overflow?1:0;
  if(!s.reset&&!s.overflow)this.state.frameSamples+=s.samples.length;
  this.state.raw=input.stick('right');this.state.delta=input.stickDelta('right');this.state.mapped=input.vector('flick_stick');
  this.state.capacity=s.capacity;this.state.ordered=s.samples.every((v,i)=>i===0||(v.sequence>s.samples[i-1].sequence&&v.time>=s.samples[i-1].time));
  this.state.frameIsFixed=time.fixed;
  const a=input.stickSamples('right',0),b=input.stickSamples('right',0);
  this.state.repeated=JSON.stringify(a)===JSON.stringify(b);
  if(a.samples.length){a.samples[0].x=99;this.state.copied=input.stickSamples('right',0).samples[0].x!==99;}else this.state.copied=true;
  let rejected=0;for(const f of [()=>input.stick('middle'),()=>input.stickDelta('RIGHT'),()=>input.stickSamples('right',-.1),()=>input.stickSamples('left',Infinity),()=>input.stickSamples('right','0'),()=>input.stickSamples('right',s.sequence+1),()=>input.vector(3)]){try{f();}catch(e){if(e instanceof TypeError)rejected++;}}
  this.state.invalidRejected=rejected===7;
 }
 fixedUpdate(){
  const s=input.stickSamples('right',this.fixedCursor);this.fixedCursor=s.sequence;
  this.state.steps++;if(!s.reset&&!s.overflow)this.state.fixedSamples+=s.samples.length;
  this.state.fixedIsFixed=time.fixed;
 }
}
)JS";
    AssetRecord script;Check(host.Assets().Track((root/"Assets/probe.js").string(),script,error),"registered public-API probe asset");
    UIDocument doc;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;doc.elements.push_back(canvas);
    std::ofstream(root/"Assets/fixture.judasui")<<SerializeUIDocument(doc);
    AssetRecord uiAsset;Check(host.Assets().Track((root/"Assets/fixture.judasui").string(),uiAsset,error),"normal authored UI for pause routing");
    InputMap map=InputMap::Defaults();map.AddVector("flick_stick");map.AddBinding("flick_stick",{"stick:Right",1,.2f,1,true});
    project.Settings().input=map;project.Settings().startupScene="Scenes/probe.judas";
    project.Settings().legacyGameplay=false;Check(project.Save(error),"project saves paired binding normally");
    Scene scene;auto& owner=scene.CreateObject("Input reader");owner.scripts.push_back({1,script.id,true,"{}"});owner.ui=SceneUIComponent{uiAsset.id,"fixture",true};
    Check(SaveSceneToFile(scene,project.StartupScenePath(),error),"registered startup scene saved");
    auto world=std::make_unique<RuntimeWorld>();
    Check(world->Build(scene,&host.Resources(),error),"normal world with script and UI");world->legacyGameplay=false;
    if(failures){std::puts(error.c_str());host.Shutdown();return 1;}
    auto control=std::make_shared<SceneSession>(project,project.StartupScenePath());world->SetSceneControl(control);
    auto& window=host.GetWindow();window.SetTestInputMode(true);Check(window.Input().SetMap(map,error),"normal input map installed");
    InteractivePlay play;Check(play.Begin(*world,WorldCoordinates{},error),"ordinary play session begins");
    auto frame=[&](float dt){play.Frame(window,host.GetRenderer(),dt,false,false);};
    frame(0);window.BeginTestFrame();
    Flick(window.Input());const auto delivered=window.Input().StickSamples("right").samples.size();frame(1.f/240);
    auto state=State(*world);
    Check(delivered>=2&&state.value("frameSamples",0)==int(delivered)&&play.LastFixedStepsThisFrame()==0,"out-and-back delivered before a fixed step survives a zero-step render frame");
    Check(state["raw"]["x"]==0&&state["raw"]["y"]==0&&state["delta"]["x"]==0&&state["delta"]["y"]==0,"neutral endpoint and net delta do not erase the observation history");
    Check(state.value("repeated",false)&&state.value("copied",false)&&state.value("ordered",false)&&state.value("capacity",0)==128,"ordered timed copied snapshots are non-destructive across readers");
    Check(state.value("invalidRejected",false)&&!state.value("frameIsFixed",true),"public input arguments validated in real frame callbacks");
    // Initialize the fixed reader's cursor before the next observed flick.
    window.BeginTestFrame();frame(1.f/60);
    const int fixedBefore=State(*world).value("fixedSamples",0);
    window.BeginTestFrame();const auto before=window.Input().StickSamples("right").sequence;
    Flick(window.Input());const auto later=window.Input().StickSamples("right",before).samples.size();frame(.05f);
    state=State(*world);
    Check(play.LastFixedStepsThisFrame()>=3&&state.value("fixedSamples",0)==fixedBefore+int(later)&&state.value("fixedIsFixed",false),"catch-up fixed steps recognize new sample IDs once per script cursor");
    window.BeginTestFrame();window.Input().SetStick("right",.6f,.8f);frame(0);state=State(*world);
    Check(std::abs(state["raw"]["x"].get<double>()-.6)<1e-5&&std::abs(state["raw"]["y"].get<double>()-.8)<1e-5&&std::abs(state["mapped"]["x"].get<double>()-.6)<1e-5&&std::abs(state["mapped"]["y"].get<double>()-.8)<1e-5,"raw and circular values marshal independently through VM");
    window.BeginTestFrame();for(int i=0;i<140;++i)window.Input().SetStick("right",(i&1)?.3f:-.3f,.2f);frame(0);
    Check(State(*world).value("overflows",0)>0,"real JS cursor reader detects bounded history overflow");
    window.BeginTestFrame();window.Input().SetPhysical("key:Escape",1);frame(1.f/60);
    Check(play.IsPaused(),"authored modal pause opens through normal logical input");
    window.BeginTestFrame();window.Input().SetPhysical("key:Escape",0);Flick(window.Input());frame(1.f/60);
    const int acceptedBefore=State(*world).value("fixedSamples",0);
    window.BeginTestFrame();Flick(window.Input());window.Input().SetPhysical("key:Escape",1);frame(1.f/60);
    Check(!play.IsPaused()&&State(*world).value("fixedSamples",0)==acceptedBefore,"resume discards observations delivered while paused without losing its button edge");
    const auto oldCursor=window.Input().StickSamples("right").sequence;Flick(window.Input());
    Check(control->Reload(error)&&control->AdvanceOuter(world,play,host.Resources(),window.Input(),error),"registered scene reload at safe outer boundary");
    const auto fresh=window.Input().StickSamples("right",oldCursor);
    Check(fresh.reset&&fresh.samples.empty(),"scene replacement invalidates old paired observations before new scripts");
    frame(0);Check(world->Scripts()&&world->Scripts()->Diagnostics().empty(),"replacement VM has no stale input/script faults");
    std::ofstream(out/"state.json")<<State(*world).dump(2)<<'\n';
    play.End();world->Destroy();host.Shutdown();
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
