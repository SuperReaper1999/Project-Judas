#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "InputSystem.h"
#include "WorldPersistence.h"
#include "WorldState.h"
#include "Project.h"
#include "SceneSession.h"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cmath>
namespace fs=std::filesystem;
static int checks=0,failures=0;
static void Check(bool value,const char* label){++checks;if(!value)++failures;std::printf("%s %s\n",value?"PASS":"FAIL",label);}
static void Write(const fs::path& p,const std::string& bytes){std::ofstream(p)<<bytes;}
static std::string State(ScriptSystem& s){auto records=s.Capture();return records.empty()?"":records[0].json;}
static const char* helper=R"JS(export const closureNumber=17;
export function calculate(amount) {
 const doubled=amount*2;
 return doubled+closureNumber;
}
)JS";
static std::string Source(int amount){return R"JS(import {calculate} from './helper.js';
import {console,signals} from 'judas';
export default class {
 constructor({entity}) { this.entity=entity;this.state={count:0}; }
 start() { this.subscription=signals.subscribe('pulse');console.log('started'); }
 update(dt) {
  const amount=)JS"+std::to_string(amount)+R"JS(;
  const result=calculate(amount);
  this.state.count+=result;
  this.state.dt=dt;this.state.subscriptions=signals.stats.subscriptions;this.state.pending=signals.stats.queuedEvents;signals.emit('pulse');
 }
 onSignal() { this.state.received=(this.state.received||0)+1; }
 destroy() { this.entity.velocity={x:3,y:0,z:0}; }
}
)JS";}
int main(){
    auto root=fs::absolute(".cache/prealpha-pass3/maintainability/native-fixtures/debug");fs::remove_all(root);fs::create_directories(root/"Assets");
    Write(root/"Assets/helper.js",helper);Write(root/"Assets/driver.js",Source(4));
    AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());std::string error;AssetRecord main,dependency;
    Check(assets.Track((root/"Assets/driver.js").string(),main,error)&&assets.Track((root/"Assets/helper.js").string(),dependency,error),"normal tracked modules");
    ResourceManager resources(nullptr,&assets);Scene scene;
    auto& object=scene.CreateObject("independent consumer");auto id=object.id;object.body=SceneBodyComponent{};object.body->mass=2;object.body->motion=SceneBodyMotion::Dynamic;object.scripts.push_back({1,main.id,true,"{}"});
    RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&resources,error),"ordinary physical world builds");
    auto& scripts=world.PrepareScriptTools();InputSystem input;
    Check(scripts.CheckSources(error),"compile/link preflight does not require constructors or gameplay execution");
    Check(scripts.Capture().empty(),"preflight did not create instances");
    Write(root/"Assets/not-executed.js","throw new Error('project checker executed code');export const value=1;");AssetRecord inert;Check(assets.Track((root/"Assets/not-executed.js").string(),inert,error),"register adversarial unattached helper");
    Check(ScriptSystem::CheckProjectSources(assets).empty(),"project-wide compile/link never executes top-level code");
    std::vector<ScriptDebugStop> stops;int mode=0;
    scripts.SetDebugger([&](const ScriptDebugStop& stop){
        stops.push_back(stop);std::string e;Check(!scripts.Reload(true,e)&&e.find("boundary")!=std::string::npos,"reload rejected while VM stack suspended");
        if(mode==0){mode++;return ScriptDebugAction::Into;}
        if(mode==1){mode++;return ScriptDebugAction::Out;}
        return ScriptDebugAction::Continue;
    });
    scripts.SetBreakpoints({{"Assets/driver.js",8,true}});world.UpdateScripts(&input,.016f);std::printf("TRACE_DEBUG_INITIAL %s\n",State(scripts).c_str());
    Check(scripts.Diagnostics().empty(),"debugged script completed without fault");
    Check(stops.size()>=3&&stops[0].entity==id&&stops[0].slot==1&&stops[0].frames[0].file=="Assets/driver.js"&&stops[0].frames[0].line==8,"real source breakpoint before statement with owner and location");
    Check(!stops.empty()&&stops[0].frames[0].sourceKnown&&stops[0].frames[0].source==Source(4),"stopped source is the exact module snapshot compiled into VM");
    bool helperStopped=false,amountSeen=false,closureSeen=false;
    for(auto& stop:stops)for(auto& f:stop.frames){
        helperStopped|=f.file=="Assets/helper.js";
        for(auto& v:f.variables){if(v.name.find("amount")!=std::string::npos&&v.value=="4")amountSeen=true;if(v.name.find("closureNumber")!=std::string::npos&&v.value=="17")closureSeen=true;}
    }
    Check(helperStopped&&amountSeen&&closureSeen,"step into imported helper exposes actual arguments and closure values");
    Check(stops.size()>=3&&stops.back().frames[0].file=="Assets/driver.js","step out returns to caller");
    Check(State(scripts).find("\"count\":25")!=std::string::npos,"resumed instruction executes once with expected numerical result");
    Check(scripts.ConsoleMessages().size()==1,"opt-in console captures actual output");
    stops.clear();scripts.SetBreakpoints({{"Assets/driver.js",8,true}});
    scripts.SetDebugger([&](const auto& stop){stops.push_back(stop);return stops.size()==1?ScriptDebugAction::Over:ScriptDebugAction::Continue;});world.UpdateScripts(&input,.016f);
    Check(stops.size()==2&&stops[1].frames[0].file=="Assets/driver.js"&&stops[1].frames[0].line==9,"step over executes helper without stopping inside it");
    scripts.SetDebugger({});
    Project sessionProject;Check(sessionProject.Load("projects/developer_lab/developer_lab.judasproj",error),"normal project for scene-operation boundary control");
    auto session=std::make_shared<SceneSession>(sessionProject,sessionProject.StartupScenePath());world.SetSceneControl(session);
    auto pendingState=State(scripts);Check(session->Reload(error)&&!scripts.Reload(true,error)&&State(scripts)==pendingState,"queued scene replacement rejects script reload without teardown");world.SetSceneControl({});
    auto before=State(scripts);auto pose=world.Physics().GetTransform(world.RuntimeBody(id));
    Write(root/"Assets/helper.js","export function calculate( { broken !");
    Check(!ScriptSystem::CheckProjectSources(assets).empty(),"project diagnostics detect broken helper independently of active VM");
    Check(!scripts.Reload(true,error)&&error.find("helper.js")!=std::string::npos,"dependency syntax error rejects reload with source location");
    Check(State(scripts)==before&&world.Physics().GetTransform(world.RuntimeBody(id)).position==pose.position,"rejected preflight preserves VM state and physical endpoint");
    Write(root/"Assets/helper.js",helper);Write(root/"Assets/driver.js","import {absent} from './helper.js'; export default class {}");
    Check(!scripts.Reload(true,error)&&!error.empty(),"missing named import rejected before teardown");
    Check(State(scripts)==before,"named-export rejection retains old module/instance state");
    Write(root/"Assets/driver.js",Source(9));
    Check(scripts.Reload(true,error),"valid edited modules reload without rebuilding world");
    Check(State(scripts)==before,"only declared JSON instance state retained across reload");
    auto velocity=world.Physics().GetLinearVelocity(world.RuntimeBody(id));
    Check(std::abs(velocity.x-3)<1e-5f,"old destroy callback runs exactly through ordinary lifecycle path");
    world.UpdateScripts(&input,.02f);Check(State(scripts).find("\"count\":85")!=std::string::npos,"changed source and start run with retained state");
    Check(State(scripts).find("\"subscriptions\":1")!=std::string::npos&&State(scripts).find("\"pending\":0")!=std::string::npos,"reload rebinds one subscription and discards queued old-generation deliveries");
    scripts.DrainSignals(&input,.02f,false);std::printf("TRACE_DEBUG_RELOAD %s\n",State(scripts).c_str());Check(State(scripts).find("\"received\":1")!=std::string::npos,"only replacement-generation signal callback receives the new event");
    std::map<std::string,SaveChunk> chunks;Check(!WorldPersistence::Capture(world,chunks,error)&&error.find("Stop and Play")!=std::string::npos,"reloaded baseline cannot publish incompatible modern saves");
    Check(!CanCaptureLegacyWorldState(world,error),"legacy save also rejects reloaded baseline");
    bool captureRejected=false;try{(void)CaptureWorldState(world);}catch(const std::logic_error&){captureRejected=true;}Check(captureRejected,"direct native legacy capture cannot certify a stale source baseline");
    Check(scripts.Reload(false,error),"explicit fresh reload available");world.UpdateScripts(&input,.016f);Check(State(scripts).find("\"count\":35")!=std::string::npos,"fresh reload resets JS state but uses edited source");
    // Inspection must not invoke getters, proxies or toString implementations.
    Write(root/"Assets/driver.js",R"JS(export default class {
 constructor(){this.state={getterCalls:0};}
 update(){
  const unsafe={get value(){throw new Error('getter executed')},toString(){throw new Error('coercion executed')}};
  const proxy=new Proxy({}, {ownKeys(){throw new Error('proxy executed')}});
  this.state.ok=true;
 }
})JS");
    Check(scripts.Reload(false,error),"inspection adversary loads");bool accessor=false,proxy=false;
    scripts.SetDebugger([&](const auto& stop){
        for(auto& v:stop.frames[0].variables){for(auto& c:v.children)accessor|=c.value.find("accessor")!=std::string::npos;proxy|=v.value.find("proxy")!=std::string::npos;}
        return ScriptDebugAction::Continue;
    });scripts.SetBreakpoints({{"Assets/driver.js",6,true}});world.UpdateScripts(&input,.016f);
    Check(accessor&&proxy&&scripts.Diagnostics().empty(),"inspection skips accessors/proxy traps and object coercion");
    scripts.SetDebugger({});
    Write(root/"Assets/driver.js","export default class {update(){throw new Error('broken consumer')}}");Check(scripts.Reload(false,error),"runtime fault replacement compiles");world.UpdateScripts(&input,.01f);
    Check(scripts.Diagnostics().size()==1&&scripts.Diagnostics()[0].message.find("driver.js")!=std::string::npos,"runtime errors include useful source stack");
    Check(!scripts.Reload(true,error),"faulted state cannot silently be retained");
    Write(root/"Assets/driver.js",Source(4));Check(scripts.Reload(false,error),"fresh reload repairs faulted instance");world.UpdateScripts(&input,.01f);Check(scripts.Diagnostics().empty(),"repaired lifecycle runs without application restart");
    scripts.SetDebugger([](const auto&){return ScriptDebugAction::Cancel;});scripts.BreakOnNextInstruction();world.UpdateScripts(&input,.01f);
    Check(scripts.Diagnostics().size()==1&&scripts.Diagnostics()[0].message.find("cancelled by debugger")!=std::string::npos,"debug cancel unwinds VM safely through isolated fault boundary");
    scripts.SetDebugger({});Check(scripts.Reload(false,error),"cancelled callback can be deliberately replaced");
    for(int mode=0;mode<3;++mode){
        if(mode)scripts.SetDebugger([](const auto&){return ScriptDebugAction::Continue;});else scripts.SetDebugger({});
        scripts.SetBreakpoints({});
        std::vector<double> times;
        for(int r=0;r<7;++r){auto t=std::chrono::steady_clock::now();for(int i=0;i<1000;++i){world.UpdateScripts(&input,.016f);scripts.DrainSignals(&input,.016f,false);}times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count()/1000);}
        std::sort(times.begin(),times.end());std::printf("PERFORMANCE mode=%s median_update_us=%.3f worst_batch_us=%.3f\n",mode?"debug-attached":"off",times[3],times.back());
    }
    Check(scripts.Diagnostics().empty(),"benchmark remains live and healthy; no faulted-instance timings accepted");
    scripts.SetDebugger({});Write(root/"Assets/driver.js",R"JS(function one(n){return n+1;}
export default class {
 constructor(){this.state={total:0};}
 update(){for(let i=0;i<3;i++)this.state.total+=one(i);}
 destroy(){this.state.total=99;}
})JS");Check(scripts.Reload(false,error),"one-line repeated-call fixture reloads");unsigned calls=0;
    scripts.SetDebugger([&](const auto& stop){if(stop.frames.front().function=="one")++calls;return ScriptDebugAction::Continue;});scripts.SetBreakpoints({{"Assets/driver.js",1,true}});world.UpdateScripts(&input,.01f);
    Check(calls==3&&State(scripts).find("\"total\":6")!=std::string::npos,"same-line helper breakpoint fires for each distinct function entry without duplicate instructions");

    calls=0;scripts.SetDebugger([&](const auto&){++calls;scripts.SetBreakpoints({});return ScriptDebugAction::Continue;});scripts.SetBreakpoints({{"Assets/driver.js",1,true}});world.UpdateScripts(&input,.01f);
    Check(calls==1&&State(scripts).find("\"total\":12")!=std::string::npos,"breakpoint edits at a stop take effect before the next helper call");
    calls=0;scripts.SetDebugger([&](const auto&){++calls;return ScriptDebugAction::Continue;});scripts.SetBreakpoints({{"Assets/driver.js",5,true}});scripts.Stop();
    Check(calls==0&&scripts.Capture().empty(),"whole-session teardown disables debugger before destroy callbacks and retires instances");
    world.Destroy();resources.Shutdown();std::printf("M94 %d checks %d failures\n",checks,failures);return failures?1:0;
}
