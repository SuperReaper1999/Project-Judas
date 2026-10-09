#include "ScriptSystem.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "InputSystem.h"
#include "WorldState.h"
#include "SceneSerialization.h"
#include "Prefab.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
using Json=nlohmann::json;
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
struct Fixture {
    inline static int number=0;fs::path root=fs::absolute(".cache/m73-fixture")/std::to_string(++number);AssetDatabase db;ResourceManager resources{nullptr,&db};Scene scene;RuntimeWorld world;InputSystem input;std::string error;
    Fixture(){fs::create_directories(root/"Assets");db.Scan(root.string(),(root/"Assets").string());}
    std::string Script(const std::string& name,const std::string& code){auto file=root/"Assets"/(name+".js");std::ofstream(file)<<code;AssetRecord record;Check(db.Track(file.string(),record,error),"register fixture script");return record.id;}
    EntityId Add(const std::string& script,uint64_t slot=1){auto& o=scene.CreateObject("Signal fixture");o.render=SceneRenderComponent{};o.scripts.push_back({slot,script,true,"{}"});return o.id;}
    void Build(){Check(world.Build(scene,&resources,error),"build ordinary scripted world");world.UpdateScripts(&input,.02f);}
    void Fixed(){world.FixedScripts(&input,1.f/60);if(!world.Scripts())return;world.Scripts()->DrainSignals(&input,1.f/60,false);}
    void Drain(bool ui=false){if(!world.Scripts())return;world.Scripts()->DrainSignals(&input,ui?.025f:1.f/60,ui);}
    Json State(EntityId id,uint64_t slot=1){for(auto& r:world.Scripts()->Capture())if(r.entity==id&&r.slot==slot)return Json::parse(r.json);return Json();}
    ~Fixture(){world.Destroy();resources.Shutdown();}
};
const char* receiver=R"JS(import {signals,time} from 'judas';let delivery=0;
export default class {
 constructor({entity}){this.entity=entity;this.state={received:[],restores:0};}
 register(){this.token=signals.subscribe('pulse');this.state.idempotent=this.token===signals.subscribe('pulse');}
 start(){this.register();} restore(){this.state.restores++;this.register();}
 onSignal(e){this.state.delivery=++delivery;this.state.received.push({n:e.payload.n,phase:e.phase,fixed:time.fixed,dt:time.delta,sender:e.senderId,slot:e.senderSlot,alive:!!e.sender});e.payload.n=-100;}
})JS";
int main(){
    fs::remove_all(".cache/m73-fixture");
    {
        Fixture f;auto r=f.Script("receiver",receiver);auto a=f.Add(r,7);f.scene.Find(a)->scripts.push_back({3,r,true,"{}"});auto b=f.Add(r);
        auto p=f.Script("publisher",R"JS(import {signals,world} from 'judas';export default class {constructor(){this.state={};}fixedUpdate(){if(this.state.done)return;this.state.done=true;let p={n:1};this.state.a=signals.emit('pulse',p);p.n=99;this.state.b=signals.send(world.entity('2'),'pulse',{n:2});this.state.none=signals.emit('absent',{n:3});this.state.stats=signals.stats;}})JS");
        f.Add(p);f.Build();f.Fixed();auto x=f.State(a,7),y=f.State(a,3),z=f.State(b);
        Check(x["delivery"]==1&&y["delivery"]==2&&z["delivery"]==4,"deterministic entity/authored-slot recipient ordering and FIFO messages");
        Check(x["idempotent"]&&y["idempotent"],"duplicate subscription idempotent per slot");
        Check(x["received"].size()==1&&y["received"].size()==1&&z["received"].size()==2,"broadcast versus entity-targeted slots");
        Check(x["received"][0]["n"]==1&&y["received"][0]["n"]==1&&z["received"][0]["n"]==1,"publisher/listener payload mutation isolated");
        Check(z["received"][0]["fixed"]&&z["received"][0]["phase"]=="fixed"&&std::abs(double(z["received"][0]["dt"])-1./60)<1e-6,"fixed envelope and ordinary fixed interval");
        Check(f.State(3)["none"]["recipients"]==0&&f.State(3)["none"]["sequence"].is_null(),"no listener accepts zero recipients without queued history");
        Check(z["received"][0]["sender"]=="3"&&z["received"][0]["slot"]=="1","full precision engine-derived sender identities");
        // Save capture does not run pending deliveries; restored counters register once.
        f.world.FixedScripts(&f.input,1.f/60);auto snapshot=CaptureWorldState(f.world);RuntimeWorld restored;Check(restored.Build(f.scene,&f.resources,f.error)&&ApplyWorldState(restored,snapshot,f.error),"legacy delta script state rehydrates normally");restored.UpdateScripts(&f.input,.02f);restored.Destroy();
        auto records=f.world.Scripts()->Capture();RuntimeWorld modern;Check(modern.Build(f.scene,&f.resources,f.error)&&modern.RestoreScriptState(records,f.error,true),"modern restore installs saved script state before restore");modern.UpdateScripts(&f.input,.02f);modern.Scripts()->DrainSignals(&f.input,.016f,false);
        auto states=modern.Scripts()->Capture();Check(Json::parse(states[0].json)["restores"]==1&&Json::parse(states[0].json)["received"].size()==1,"restore helper rebuilds subscriptions without replay/reset");modern.Destroy();
        // Slot retirement while messages wait: re-subscription must not resurrect old token.
        auto shared=f.Script("shared",R"JS(import {signals} from 'judas';let token;export default class {constructor(){this.state={};}start(){if(!token)token=signals.subscribe('new');else {try{signals.unsubscribe(token)}catch(e){this.state.rejected=true;}}}onSignal(){}})JS");
        Scene independent;auto& aa=independent.CreateObject("first");aa.scripts={{1,shared,true,"{}"},{2,shared,true,"{}"}};RuntimeWorld ownership;ownership.Build(independent,&f.resources,f.error);ownership.UpdateScripts(&f.input,.016f);
        auto own=ownership.Scripts()->Capture();Check(Json::parse(own[1].json)["rejected"],"one slot cannot cancel another slot token");ownership.Destroy();
    }
    {
        Fixture f;auto r=f.Script("chain",R"JS(import {signals,time} from 'judas';export default class {constructor(){this.state={n:0,ui:0};}start(){signals.subscribe('loop');signals.subscribe('menu','ui');signals.emit('loop');signals.emit('menu',null,'ui');}onSignal(e){if(e.phase==='ui'){this.state.ui++;this.state.uiFixed=time.fixed;return;}this.state.n++;signals.emit('loop');}})JS");auto id=f.Add(r);f.Build();f.Drain(true);Check(f.State(id)["ui"]==1&&!f.State(id)["uiFixed"]&&f.State(id)["n"]==0,"UI lane drains independently on a zero-fixed-step frame");f.Drain();Check(f.State(id)["n"]==1,"self-publishing handler cannot extend frozen drain");f.Drain();Check(f.State(id)["n"]==2,"later eligible fixed boundary delivers queued chain");
        for(int i=0;i<100;++i){f.Drain();}
        Check(f.State(id)["n"]==102,"bounded self-publishing loop has no recursive stack growth");
        f.world.EndScripts();Check(!f.world.Scripts(),"Stop discards transient queues and subscriptions");
    }
    {
        Fixture f;auto r=f.Script("retire",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={n:0};}start(){this.token=signals.subscribe('pulse');}update(){if(!this.state.did){this.state.did=true;signals.emit('pulse');signals.unsubscribe(this.token);this.token=signals.subscribe('pulse');}}onSignal(){this.state.n++;}})JS");auto id=f.Add(r);f.Build();f.Drain();Check(f.State(id)["n"]==0,"cancel and re-subscribe cannot receive old accepted reservation");
        auto before=f.world.Scripts()->Capture();f.world.EndRegionScripts({id});f.world.UpdateScripts(&f.input,.02f);f.Drain();Check(f.State(id)["n"]==0,"region retirement/revisit uses new generation without old queue");
        (void)before;
    }
    {
        Fixture f;auto r=f.Script("survivor",receiver);auto id=f.Add(r);auto p=f.Script("dying",R"JS(import {signals} from 'judas';export default class {constructor({entity}){this.entity=entity;this.state={};}fixedUpdate(){signals.emit('pulse',{n:4});this.entity.destroy();}})JS");f.Add(p);f.Build();f.Fixed();auto s=f.State(id);Check(s["received"].size()==1&&s["received"][0]["n"]==4&&!s["received"][0]["alive"],"destroyed publisher does not erase accepted event; sender wrapper null");
    }
    {
        Fixture f;auto bad=f.Script("faultReceiver",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={};}start(){signals.subscribe('pulse');}onSignal(){throw Error('intentional signal fault')}destroy(){throw Error('must not run')}})JS");f.Add(bad);auto good=f.Add(f.Script("afterFault",receiver));auto p=f.Script("faultPub",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={};}fixedUpdate(){signals.emit('pulse',{n:5});this.state.stats=signals.stats;}})JS");f.Add(p);f.Build();f.Fixed();f.Fixed();Check(f.world.Scripts()->Diagnostics().size()==1&&f.State(good)["received"].size()==2,"handler fault retires subscriptions while other listeners continue");Check(f.State(3)["stats"]["subscriptions"]==1,"fault cleanup does not depend on destroy callback");
    }
    {
        Fixture f;auto s=f.Script("payload",R"JS(import {signals,world} from 'judas';export default class {constructor(){this.state={rejects:0,getters:0};try{signals.emit('bad')}catch(e){this.state.constructorRejected=true;}}start(){signals.subscribe('x');const reject=v=>{try{signals.emit('x',v)}catch(e){this.state.rejects++}};const state=this.state;const cycle={};cycle.c=cycle;reject(cycle);reject({get n(){state.getters++;throw Error('getter invoked')}});reject(new Proxy({},{ownKeys(){state.getters++;throw Error('proxy invoked')}}));reject({n:NaN});reject({n:()=>1});reject(this);reject(world.entity('1'));reject({toJSON(){throw Error('toJSON invoked')}});reject(new Date());reject([,1]);reject('x'.repeat(17000));let deep={};let v=deep;for(let i=0;i<10;i++)v=v.n={};reject(deep);try{signals.send('1','x')}catch(e){this.state.rawRejected=true;}try{signals.subscribe('x'.repeat(129))}catch(e){this.state.nameRejected=true;}this.state.accept=signals.emit('x',{a:[null,true,2],b:'valid'});}onSignal(e){this.state.payload=e.payload;}})JS");auto id=f.Add(s);f.Build();f.Drain();auto state=f.State(id);Check(f.world.Scripts()->Diagnostics().empty()&&state["rejects"]==12&&state["getters"]==0,"reject cycles/accessors/proxies/nonfinite/functions/wrappers/prototypes/holes/oversize/deep data without hooks");Check(state["constructorRejected"]&&state["rawRejected"]&&state["nameRejected"],"constructor, raw target and overlong names rejected");Check(state["payload"]["a"][0].is_null()&&state["payload"]["a"][2]==2,"nested plain array/object payload retained");
        std::string schema;Check(ScriptSystem::Inspect(f.db,s,schema,f.error),"signals namespace can be imported by metadata VM");
    }
    {
        Fixture f;auto s=f.Script("limits",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={};}start(){for(let i=0;i<32;i++)signals.subscribe('s'+i);try{signals.subscribe('over')}catch(e){this.state.subscriptionRejected=true;}for(let i=0;i<256;i++)signals.emit('s0',{n:i});try{signals.emit('s0',{n:999})}catch(e){this.state.queueRejected=true;}this.state.before=signals.stats;}onSignal(e){this.state.n=(this.state.n??0)+1;this.state.last=e.payload.n;this.state.stats=signals.stats;}})JS");auto id=f.Add(s);f.Build();f.Drain();auto state=f.State(id);Check(state["subscriptionRejected"]&&state["queueRejected"]&&state["n"]==256&&state["last"]==255,"subscription/event capacities reject atomically and preserve FIFO");Check(state["before"]["queuedEvents"]==256&&state["before"]["queuedRecipients"]==256,"bounded retained queue counts exposed");
    }
    {
        Fixture f;auto s=f.Script("fanout",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={n:0};}start(){signals.subscribe('x');}fixedUpdate(){if(!this.state.checked){this.state.checked=true;try{signals.emit('x')}catch(e){this.state.rejected=true;}}}onSignal(){this.state.n++;}})JS");for(int i=0;i<257;++i)f.Add(s);f.Build();f.Fixed();Check(f.State(1)["rejected"]&&f.State(257)["n"]==0,"fan-out over capacity rejects whole emission rather than delivering prefix");
    }
    {
        Fixture f;auto r=f.Script("budget",R"JS(import {signals} from 'judas';export default class {constructor({entity}){this.entity=entity;this.state={n:0};}start(){signals.subscribe('x');}fixedUpdate(){if(this.entity.id==='1'&&!this.state.once){this.state.once=true;for(let i=0;i<3;i++)signals.emit('x',{n:i});}}onSignal(){this.state.n++;}})JS");
        for(int i=0;i<128;++i){f.Add(r);}
        f.Build();f.Fixed();Check(f.State(1)["n"]==2&&f.State(128)["n"]==2,"delivery cap defers accepted FIFO batch after 256 recipients");f.Drain();Check(f.State(1)["n"]==3&&f.State(128)["n"]==3,"remaining accepted work completes next eligible drain");
    }
    {
        Fixture f;auto s=f.Script("churn",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={};}start(){for(let i=0;i<500;i++){let t=signals.subscribe('x');signals.emit('x');signals.unsubscribe(t);}this.state.stats=signals.stats;}onSignal(){throw Error('cancelled')}})JS");auto id=f.Add(s);f.Build();auto state=f.State(id);Check(state["stats"]["subscriptions"]==0&&state["stats"]["queuedEvents"]==0&&state["stats"]["queuedBytes"]==0&&state["stats"]["queuedRecipients"]==0,"subscription and queued storage released over 500 publish/cancel cycles without drain");std::printf("SIGNAL_CHURN cycles=500 stats=%s\n",state["stats"].dump().c_str());
    }
    {
        Fixture f;auto s=f.Script("bytecap",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={accepted:0};}start(){signals.subscribe('x');try{for(let i=0;i<256;i++){signals.emit('x','p'.repeat(16000));this.state.accepted++;}}catch(e){this.state.rejected=true;}this.state.stats=signals.stats;}onSignal(){this.state.n=(this.state.n??0)+1;}})JS");auto id=f.Add(s);f.Build();auto state=f.State(id);Check(state["rejected"]&&state["accepted"]<256&&state["stats"]["queuedBytes"]<=1048576,"queue byte cap bounds large payload retention atomically");std::printf("SIGNAL_STORAGE stats=%s\n",state["stats"].dump().c_str());f.Drain();Check(f.State(id)["n"]==state["accepted"],"all accepted large payloads delivered once");
    }
    {
        Fixture f;auto s=f.Script("runawaySignal",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={};}start(){signals.subscribe('x');signals.emit('x');}onSignal(){while(true){}}})JS");f.Add(s);f.Build();f.Drain();Check(f.world.Scripts()->Diagnostics().size()==1,"signal handler uses existing runaway interrupt and fault path");
    }
    {
        Fixture f;std::ifstream file("docs/judasjs/examples/signals.js");std::string code{std::istreambuf_iterator<char>(file),{}};
        auto id=f.Add(f.Script("documentedSignals",code));f.Build();f.Fixed();f.world.Scripts()->UIFrame(&f.input,.02f);f.Drain(true);
        Check(f.State(id)["count"]==2&&f.State(id)["preview"]==1&&f.State(id)["fixed"]&&!f.State(id)["uiFixed"],"copyable broadcast/target/UI example executes through actual VM");
    }
    {
        Fixture f;auto first=f.Script("cancelPeer",R"JS(import {signals,world} from 'judas';export default class {constructor(){this.state={n:0};}start(){signals.subscribe('x');}onSignal(){this.state.n++;world.entity('2').destroy();}})JS");
        auto second=f.Script("peer",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={n:0};}start(){signals.subscribe('x');}onSignal(){this.state.n++;}})JS");
        f.Add(first);f.Add(second);auto p=f.Script("peerPub",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={};}fixedUpdate(){signals.emit('x');}})JS");f.Add(p);f.Build();f.Fixed();
        Check(f.State(1)["n"]==1&&!f.world.RuntimeDefinition(2),"listener destroys later frozen recipient safely");
        Check(f.world.Scripts()->Diagnostics().empty(),"destroyed frozen recipient is skipped without callback fault");
    }
    {
        Fixture f;auto r=f.Script("generation",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={n:0};}start(){this.token=signals.subscribe('x');signals.emit('x');}onSignal(){this.state.n++;}})JS");auto id=f.Add(r);f.Build();
        auto* d=const_cast<SceneObject*>(f.world.RuntimeDefinition(id));d->scripts[0].enabled=false;f.Drain();Check(f.State(id)["n"]==0,"disabled slot rejects accepted recipient before synchronization");
        f.world.UpdateScripts(&f.input,.02f);d=const_cast<SceneObject*>(f.world.RuntimeDefinition(id));d->scripts[0].enabled=true;f.world.UpdateScripts(&f.input,.02f);f.Drain();
        Check(f.State(id)["n"]==1,"re-enabled replacement instance receives only its new emission");
    }
    {
        Fixture f;auto s=f.Script("uiGuard",R"JS(import {signals,time} from 'judas';export default class {constructor({entity}){this.entity=entity;this.state={};}start(){signals.subscribe('x','ui');signals.emit('x',null,'ui');}onSignal(){try{this.entity.character.velocity={x:1,y:0,z:0}}catch(e){this.state.guarded=true;}this.state.fixed=time.fixed;}})JS");auto id=f.Add(s);f.scene.Find(id)->characterMotor=CharacterMotorSettings{};f.Build();f.Drain(true);
        Check(f.State(id)["guarded"]&&!f.State(id)["fixed"],"UI signal retains fixed-only CharacterMotor setter guard");
    }
    {
        Fixture first;auto r=first.Script("transient",R"JS(import {signals} from 'judas';export default class {constructor(){this.state={n:0};}register(){this.state.token=signals.subscribe('pending');}start(){this.register();signals.emit('pending');}restore(){this.register();}onSignal(){this.state.n++;}})JS");auto id=first.Add(r);first.Build();auto records=first.world.Scripts()->Capture();
        Check(first.State(id)["n"]==0,"save capture does not flush pending signal");
        RuntimeWorld restored;restored.Build(first.scene,&first.resources,first.error);restored.RestoreScriptState(records,first.error,true);restored.UpdateScripts(&first.input,.02f);restored.Scripts()->DrainSignals(&first.input,.016f,false);
        Check(Json::parse(restored.Scripts()->Capture()[0].json)["n"]==0,"new VM restore has no undelivered notification replay");
        Fixture other;auto old=first.State(id)["token"].dump();auto s=other.Script("foreign",std::string("import {signals} from 'judas';export default class {constructor(){this.state={n:0};}start(){signals.subscribe('pending');try{signals.unsubscribe(")+old+")}catch(e){this.state.foreignRejected=true}}onSignal(){this.state.n++}}" );auto oid=other.Add(s);other.Build();other.Drain();
        Check(other.State(oid)["n"]==0&&other.State(oid)["foreignRejected"],"world-local service neither delivers history nor accepts foreign-world token");restored.Destroy();
    }
    {
        Fixture f;auto r=f.Script("selfRetire",R"JS(import {signals} from 'judas';export default class {constructor({entity}){this.entity=entity;this.state={};}start(){this.token=signals.subscribe('self');signals.emit('self');}onSignal(){this.entity.destroy();}destroy(){if(signals.unsubscribe(this.token)!==false)throw Error('token was not retired');let denied=false;try{signals.emit('self')}catch(e){denied=true;}if(!denied)throw Error('teardown restarted messaging')}})JS");auto id=f.Add(r);f.Build();f.Drain();f.world.UpdateScripts(&f.input,.02f);
        Check(!f.world.RuntimeDefinition(id)&&f.world.Scripts()->Diagnostics().empty(),"self destruction permits safe retired-token unsubscribe; teardown emission still rejected");
    }
    for(int count:{0,32,128}){
        Fixture f;auto s=f.Script("perf",R"JS(import {signals} from 'judas';export default class {constructor({entity}){this.entity=entity;this.state={n:0};}start(){signals.subscribe('p');}fixedUpdate(){if(this.entity.id==='1')signals.emit('p');}onSignal(){this.state.n++;}})JS");for(int i=0;i<count;++i)f.Add(s);f.Build();auto start=std::chrono::steady_clock::now();for(int i=0;i<1000;++i)f.Drain();auto idle=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/1000;
        start=std::chrono::steady_clock::now();f.Fixed();auto active=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();std::printf("SIGNAL_PERF subscriptions=%d empty_drain_us=%.4f initial_fanout_plus_callbacks_us=%.3f\n",count,idle,active);
        auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.Clear();
        for(int sample=0;sample<50;++sample){ProfileFrame frame("M73 fanout");f.Fixed();}
        double routing=0,handler=0,acceptance=0;auto history=profiler.History();
        for(auto& frame:history)for(auto& scope:frame.scopes){auto us=double(scope.end-scope.start)/1000.;if(scope.name=="Signals routing and delivery")routing+=us;else if(scope.name=="Signals handler")handler+=us;else if(scope.name=="Signals API")acceptance+=us;}
        std::printf("SIGNAL_PROFILE subscriptions=%d samples=%zu acceptance_us=%.3f routing_excluding_handlers_us=%.3f handlers_us=%.3f\n",count,history.size(),acceptance/history.size(),(routing-handler)/history.size(),handler/history.size());profiler.Enable(false);

    }
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
