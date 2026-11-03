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
 uiUpdate(){
  const d=ui.get('fixture');if(d&&input.pressed('pause'))d.modal=!d.modal;
  if(input.pressed('probe_paused_rumble'))
   this.state.pausedPulse=input.rumble({low:1,seconds:.1,device:'gamepad',gamepad:'test-pad'});
 }
 update(){
  const s=input.stickSamples('right',this.frameCursor);this.frameCursor=s.sequence;
  this.state.frames++;this.state.resets+=s.reset?1:0;this.state.overflows+=s.overflow?1:0;
  if(!s.reset&&!s.overflow)this.state.frameSamples+=s.samples.length;
  this.state.raw=input.stick('right');this.state.delta=input.stickDelta('right');this.state.mapped=input.vector('flick_stick');
  this.state.capacity=s.capacity;this.state.ordered=s.samples.every((v,i)=>i===0||(v.sequence>s.samples[i-1].sequence&&v.time>=s.samples[i-1].time));
  this.state.frameIsFixed=time.fixed;
  if(this.state.frames===1){
   this.state.tiltDefaultNull=input.tilt===null;
   input.setTiltEnabled(true);
   try{input.setTiltEnabled(1);this.state.tiltInvalidRejected=false;}
   catch(error){this.state.tiltInvalidRejected=error instanceof TypeError;}
  }
  if(input.pressed('probe_rumble')){
   this.state.haptics=input.canRumble;
   this.state.pulse=input.rumble({low:.6,high:.3,seconds:.15,device:'gamepad',gamepad:'test-pad'});
   this.state.unknownPad=input.rumble({low:1,seconds:.1,device:'gamepad',gamepad:'missing'});
   this.state.zeroPulse=input.rumble({seconds:.1});
   let rejected=0;
   for(const options of [{low:1.1,seconds:.1},{low:1,seconds:5.1},{device:'phone',gamepad:'test-pad',seconds:.1}]){
    try{input.rumble(options);}catch(error){if(error instanceof TypeError)rejected++;}
   }
   this.state.rumbleRejected=rejected;
  }
  if(input.pressed('stop_rumble'))input.stopRumble();
  this.state.tilt=input.tilt;
  if(this.state.tilt){
   const snapshot=input.tilt;snapshot.x=99;
   this.state.tiltCopied=input.tilt.x!==99;
  }
  if(input.pressed('disable_tilt')){
   input.setTiltEnabled(false);
   this.state.tiltDisabled=input.tilt===null;
  }
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
  this.state.fixedTilt=input.tilt;
 }
}
)JS";
    AssetRecord script;Check(host.Assets().Track((root/"Assets/probe.js").string(),script,error),"registered public-API probe asset");
    UIDocument doc;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;doc.elements.push_back(canvas);
    std::ofstream(root/"Assets/fixture.judasui")<<SerializeUIDocument(doc);
    AssetRecord uiAsset;Check(host.Assets().Track((root/"Assets/fixture.judasui").string(),uiAsset,error),"normal authored UI for pause routing");
    InputMap map=InputMap::Defaults();map.AddVector("flick_stick");map.AddBinding("flick_stick",{"stick:Right",1,.2f,1,true});
    map.Add("disable_tilt",false);map.AddBinding("disable_tilt",{"key:F10"});
    map.Add("probe_rumble",false);map.AddBinding("probe_rumble",{"key:F9"});
    map.Add("stop_rumble",false);map.AddBinding("stop_rumble",{"key:F11"});
    map.Add("probe_paused_rumble",false);map.AddBinding("probe_paused_rumble",{"key:F12"});
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
    Check(State(*world).value("tiltDefaultNull",false)&&State(*world)["tilt"].is_null()&&
          State(*world).value("tiltInvalidRejected",false),"tilt is opt-in and reports unavailable without a sample");
    window.BeginTestFrame();window.SetTestTilt(InputTilt{2,5,7},1);frame(1.f/60);state=State(*world);
    Check(state["tilt"]==J{{"x",-5},{"y",2},{"z",7}}&&state["fixedTilt"]==state["tilt"]&&
          state.value("tiltCopied",false),"rotated tilt marshals a detached sample to frame and fixed callbacks");
    window.BeginTestFrame();window.ApplyTestPhysical("key:F10",1);frame(0);
    window.SetTestTilt(InputTilt{2,5,7},0);
    Check(State(*world).value("tiltDisabled",false)&&!window.Input().Tilt(),"JS opt-out clears the current tilt sample and suppresses new samples");
    window.ApplyTestPhysical("key:Escape",0);
    auto press=[&](const char* key){window.BeginTestFrame();window.ApplyTestPhysical(key,1);frame(1.f/60);
        window.BeginTestFrame();window.ApplyTestPhysical(key,0);frame(1.f/60);};
    press("key:F9");state=State(*world);
    Check(state["haptics"]==J{{"gamepad",false},{"phone",false}}&&state["pulse"]==false,
          "hardware-free test window exposes no vibration");
    int pulses=0,stops=0;bool exactPulse=false;
    window.Input().rumbleBackend=[&](RumbleOperation op,RumbleDevice device,const std::string& token,
                                    float low,float high,unsigned ms){
        if(op==RumbleOperation::Stop){++stops;return true;}
        if(op==RumbleOperation::Query)return device==RumbleDevice::Gamepad;
        if(token!="test-pad")return false;
        ++pulses;exactPulse=device==RumbleDevice::Gamepad&&low==.6f&&high==.3f&&ms==150;
        return true;
    };
    press("key:F9");state=State(*world);
    Check(state["pulse"]==true&&state["unknownPad"]==false&&state["zeroPulse"]==false&&
          state["rumbleRejected"]==3&&pulses==1&&exactPulse,"JudasJS validates and routes a bounded pulse to the assigned device");
    press("key:F11");
    Check(stops==1,"JudasJS cancels outstanding feedback");
    press("key:F9");press("key:Escape");
    Check(stops==2&&play.IsPaused(),"project pause cancels outstanding feedback");
    press("key:Escape");press("key:F9");
    const auto oldCursor=window.Input().StickSamples("right").sequence;Flick(window.Input());
    Check(control->Reload(error)&&control->AdvanceOuter(world,play,host.Resources(),window.Input(),error),"registered scene reload at safe outer boundary");
    Check(stops==3,"scene replacement stops vibration");
    const auto fresh=window.Input().StickSamples("right",oldCursor);
    Check(fresh.reset&&fresh.samples.empty(),"scene replacement invalidates old paired observations before new scripts");
    frame(0);Check(world->Scripts()&&world->Scripts()->Diagnostics().empty(),"replacement VM has no stale input/script faults");
    play.Menu().HandleBackRequest();
    const int pulsesBeforeLegacyPause=pulses;
    press("key:F12");
    Check(play.IsPaused()&&State(*world).contains("pausedPulse")&&State(*world)["pausedPulse"]==false&&
          pulses==pulsesBeforeLegacyPause,"legacy pause blocks new hardware dispatch from UI callbacks");
    play.Menu().HandleBackRequest();
    press("key:F12");
    Check(!play.IsPaused()&&State(*world)["pausedPulse"]==true&&pulses==pulsesBeforeLegacyPause+1,
          "same UI vibration is dispatched after resuming");
    std::ofstream(out/"state.json")<<State(*world).dump(2)<<'\n';
    play.End();world->Destroy();host.Shutdown();
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
