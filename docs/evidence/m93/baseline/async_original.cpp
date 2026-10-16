// FTFT2: drive Application::Run's normal loop with real IO, decoders and GL.
// Gates control timing only. They never supply data, states, handles or results.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <SDL2/SDL.h>
#include "Application.h"
#include "AssetDatabase.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"

namespace fs = std::filesystem;
using Point = ResourceTracePoint;
namespace {
int checks = 0, failures = 0;
void Check(bool condition, const std::string& text) {
    ++checks; if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", text.c_str());
}
std::string Id(int n) { char b[33]; std::snprintf(b, sizeof b, "%032x", 0xf7200000u + n); return b; }
const char* Name(Point p) {
    switch(p) {
#define P(x) case Point::x:return #x;
    P(FileReadBegin) P(FileReadChunk) P(FileReadEnd) P(DecodeBegin) P(DecodeEnd) P(LoadComplete)
    P(CancelRequested) P(StaleDiscarded) P(CpuReady) P(ManagerShutdownBegin) P(ManagerShutdownEnd)
    P(MeshCreated) P(MeshDestroyed) P(TextureCreated) P(TextureDestroyed)
    P(RendererShutdownBegin) P(RendererShutdownEnd) P(WorkersJoined) P(ContextDestroyed)
#undef P
    }
    return "unknown";
}
struct Gate {
    std::mutex mutex; std::condition_variable cv;
    bool reached=false, open=false;
    void Arrive() { std::unique_lock<std::mutex> lock(mutex); reached=true; cv.notify_all(); cv.wait(lock,[&]{return open;}); }
    void Wait() { std::unique_lock<std::mutex> lock(mutex); if(!cv.wait_for(lock,std::chrono::seconds(15),[&]{return reached;})) throw std::runtime_error("gate watchdog: operation never arrived"); }
    void Open() { std::lock_guard<std::mutex> lock(mutex); open=true; cv.notify_all(); }
};
struct Rule { std::string id; Point point; std::shared_ptr<Gate> gate; bool claimed=false, releaseOnCancel=false; };
struct Journal {
    std::mutex mutex; std::condition_variable cv;
    std::vector<ResourceTraceEvent> events; std::vector<Rule> rules;
    std::ofstream log; std::shared_ptr<Gate> poolGate;
    std::set<std::string> cancelled;
    std::vector<std::pair<std::shared_ptr<Gate>,std::vector<std::string>>> cancellationReleases;
    void ReleaseAfterCancellations(std::shared_ptr<Gate> gate,std::initializer_list<int> ids) {
        std::lock_guard<std::mutex> lock(mutex);std::vector<std::string> assets;
        for(int id:ids)assets.push_back(Id(id));
        cancellationReleases.push_back({std::move(gate),std::move(assets)});
    }
    explicit Journal(const fs::path& path):log(path) { log<<"sequence\toperation\tasset\tgeneration\tthread\tcontext_current\thandle\tbytes\tpath\n"; }
    std::shared_ptr<Gate> Hold(int id, Point point, bool releaseOnCancel=false) {
        auto gate=std::make_shared<Gate>(); std::lock_guard<std::mutex> lock(mutex);
        rules.push_back({Id(id),point,gate,false,releaseOnCancel}); return gate;
    }
    void Observe(const ResourceTraceEvent& event) {
        std::shared_ptr<Gate> hold; std::vector<std::shared_ptr<Gate>> release;
        {
            std::lock_guard<std::mutex> lock(mutex);
            events.push_back(event);
            log<<events.size()<<'\t'<<Name(event.point)<<'\t'<<event.asset<<'\t'<<event.generation<<'\t'<<event.thread<<'\t'<<event.contextCurrent<<'\t'<<event.handle<<'\t'<<event.bytes<<'\t'<<event.path<<'\n';log.flush();
            for(auto& r:rules) {
                if(event.asset==r.id && event.point==r.point && !r.claimed){r.claimed=true;hold=r.gate;}
                if(event.asset==r.id && event.point==Point::CancelRequested && r.releaseOnCancel)release.push_back(r.gate);
            }
            if(event.point==Point::CancelRequested)cancelled.insert(event.asset);
            for(auto& rule:cancellationReleases)
                if(std::all_of(rule.second.begin(),rule.second.end(),[&](const auto& id){return cancelled.count(id)!=0;}))release.push_back(rule.first);
            if(event.point==Point::ManagerShutdownEnd && poolGate)release.push_back(poolGate);
            cv.notify_all();
        }
        for(auto& gate:release)gate->Open();
        if(hold)hold->Arrive();
    }
    std::size_t Count(Point p,int id=0) { std::lock_guard<std::mutex> lock(mutex); return std::count_if(events.begin(),events.end(),[&](const auto& e){return e.point==p && (!id||e.asset==Id(id));}); }
    std::vector<ResourceTraceEvent> Snapshot(){std::lock_guard<std::mutex> lock(mutex);return events;}
    void OpenAll(){std::lock_guard<std::mutex> lock(mutex);for(auto& r:rules)r.gate->Open();if(poolGate)poolGate->Open();}
};
void Mesh(const fs::path& path,float size=1,bool large=false) {
    std::ofstream out(path);out<<"v 0 0 0\nv "<<size<<" 0 0\nv 0 1 0\nf 1 2 3\n";
    if(large){std::string padding(1022,'x');for(int i=0;i<2300;++i)out<<"#"<<padding<<'\n';}
}
void Texture(const fs::path& path) {
    std::vector<unsigned char> rgb(4*4*3);for(std::size_t i=0;i<rgb.size();i+=3){rgb[i]=17;rgb[i+1]=93;rgb[i+2]=201;}
    if(!WriteRgbPng(path.string(),4,4,rgb))throw std::runtime_error("cannot create real PNG fixture");
}
struct Fixture {
    fs::path root,a,b,closed;Scene scene;SceneObjectId moving=0;
    explicit Fixture(const std::string& name) {
        root=fs::absolute(fs::path("build/ftft2-fixtures")/name);fs::remove_all(root);
        a=root/"a";b=root/"b";closed=root/"closed";
        for(auto base:{a,b,closed}){fs::create_directories(base/"Assets");fs::create_directories(base/"Scenes");}
        for(int n=1;n<=19;++n){
            const bool texture=n==3||n==10;const auto path=a/"Assets"/(std::to_string(n)+(texture?".png":".obj"));
            if(texture)Texture(path);else Mesh(path,1,n==1||n==5||n==15);
            std::string error;if(!AssetDatabase::WriteMeta(path.string()+".judasmeta",Id(n),texture?AssetType::Texture:AssetType::Mesh,"FTFT2 generated fixture",error))throw std::runtime_error(error);
        }
        auto bp=b/"Assets/7.obj";Mesh(bp,9);std::string error;
        if(!AssetDatabase::WriteMeta(bp.string()+".judasmeta",Id(7),AssetType::Mesh,"FTFT2 other project",error))throw std::runtime_error(error);
        scene.Settings().name="FTFT2 async application";
        auto& start=scene.CreateObject("Start");start.transform.position=glm::vec3(0,2,6);start.playerStart=ScenePlayerStartComponent{};
        for(int i=0;i<2;++i){auto& object=scene.CreateObject("Shared asset consumer");object.transform.position=glm::vec3(float(i*2),0,-2);object.render=SceneRenderComponent{};object.render->shape=SceneShape::Mesh;object.render->meshAsset=Id(1);}
        auto& mover=scene.CreateObject("Moving primitive");moving=mover.id;mover.transform.position=glm::vec3(-2,1,0);mover.render=SceneRenderComponent{};mover.body=SceneBodyComponent{};mover.body->motion=SceneBodyMotion::Dynamic;mover.body->initialLinearVelocity=glm::vec3(0.25f,0,0);
        if(!SaveSceneToFile(scene,(a/"Scenes/main.judas").string(),error))throw std::runtime_error(error);
        std::ofstream project(a/"main.judasproj");project<<"JudasProject 1\nname \"FTFT2 async\"\nstartup-scene \"Scenes/main.judas\"\nassets-dir \"Assets\"\nscenes-dir \"Scenes\"\nsaves-dir \"Saves\"\n";
    }
    fs::path Asset(int n){return a/"Assets"/(std::to_string(n)+((n==3||n==10)?".png":".obj"));}
};
void Quit(){SDL_Event event{};event.type=SDL_QUIT;if(SDL_PushEvent(&event)!=1)throw std::runtime_error("SDL quit injection failed");}
void ToggleView(){SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_V;event.key.keysym.sym=SDLK_v;if(SDL_PushEvent(&event)!=1)throw std::runtime_error("SDL input injection failed");}
bool Ready(ResourceManager& rm,int id){return rm.StateOf(Id(id))==ResourceState::Ready;}
void Request(ResourceManager& rm,int n){rm.AddRef(Id(n));if(n==3||n==10)rm.RequestTexture(Id(n));else rm.RequestMesh(Id(n));}
void VerifyMesh(EngineHost& host,int id,float maxX) {
    auto handle=host.Resources().TryGetMesh(Id(id));MeshData data;
    const bool read=host.GetRenderer().ReadMeshForDiagnostics(handle,data);
    Check(handle.IsValid()&&read&&data.vertices.size()==3&&data.indices.size()==3,"actual GL mesh buffers are resident for asset "+std::to_string(id));
    float observed=0;for(const auto& v:data.vertices)observed=std::max(observed,v.position.x);
    Check(read&&observed==maxX,"GPU vertex contents match current asset, max x="+std::to_string(maxX));
}
void VerifyTexture(EngineHost& host,int id){TextureData data;const auto handle=host.Resources().TryGetTexture(Id(id));bool good=host.GetRenderer().ReadTextureForDiagnostics(handle,data)&&data.width==4&&data.height==4&&data.pixels.size()==64;for(std::size_t i=0;i+3<data.pixels.size();i+=4)good=good&&data.pixels[i]==17&&data.pixels[i+1]==93&&data.pixels[i+2]==201&&data.pixels[i+3]==255;Check(handle.IsValid()&&good,"actual GPU texture pixels match decoded PNG");}
std::vector<JobHandle> OccupyPool(EngineHost& host,const std::shared_ptr<Journal>& journal,int count=-1) {
    auto gate=std::make_shared<Gate>();journal->poolGate=gate;
    auto entered=std::make_shared<std::atomic<int>>(0);auto mutex=std::make_shared<std::mutex>();auto cv=std::make_shared<std::condition_variable>();
    const int workers=count<0?int(host.Jobs().WorkerCount()):count;std::vector<JobHandle> handles;
    for(int i=0;i<workers;++i)handles.push_back(host.Jobs().Submit([gate,entered,mutex,cv](JobContext&){ {std::lock_guard<std::mutex> lock(*mutex);++*entered;cv->notify_all();}gate->Arrive();},JobPriority::High,"FTFT2 worker scheduling gate"));
    std::unique_lock<std::mutex> lock(*mutex);if(!cv->wait_for(lock,std::chrono::seconds(15),[&]{return entered->load()==workers;}))throw std::runtime_error("pool gate watchdog");
    return handles;
}
void ReleasePool(EngineHost& host,const std::shared_ptr<Journal>& journal,const std::vector<JobHandle>& handles){journal->poolGate->Open();for(auto h:handles){host.Jobs().Wait(h);host.Jobs().Forget(h);}journal->poolGate.reset();}

void VerifyOwnership(const std::shared_ptr<Journal>& journal,std::thread::id owner) {
    const auto events=journal->Snapshot();std::size_t managerEnd=events.size(),joined=events.size(),rendererEnd=events.size(),contextEnd=events.size();
    std::set<unsigned int> meshes,textures;int decodes=0,creates=0,destroys=0;bool ownership=true,order=true;
    for(std::size_t i=0;i<events.size();++i){const auto& e=events[i];
        if(e.point==Point::DecodeBegin||e.point==Point::DecodeEnd){++decodes;ownership&=e.thread!=owner;}
        if(e.point==Point::FileReadChunk)ownership&=e.thread!=owner&&e.bytes>0;
        if(e.point==Point::MeshCreated){++creates;ownership&=e.thread==owner&&e.contextCurrent;order&=meshes.insert(e.handle).second&&i<rendererEnd&&i<contextEnd;}
        if(e.point==Point::TextureCreated){++creates;ownership&=e.thread==owner&&e.contextCurrent;order&=textures.insert(e.handle).second&&i<rendererEnd&&i<contextEnd;}
        if(e.point==Point::MeshDestroyed){++destroys;ownership&=e.thread==owner&&e.contextCurrent;order&=meshes.erase(e.handle)==1&&i<rendererEnd&&i<contextEnd;}
        if(e.point==Point::TextureDestroyed){++destroys;ownership&=e.thread==owner&&e.contextCurrent;order&=textures.erase(e.handle)==1&&i<rendererEnd&&i<contextEnd;}
        if(e.point==Point::ManagerShutdownEnd)managerEnd=i;
        if(e.point==Point::WorkersJoined)joined=i;
        if(e.point==Point::RendererShutdownEnd)rendererEnd=i;
        if(e.point==Point::ContextDestroyed){contextEnd=i;ownership&=e.thread==owner&&!e.contextCurrent;}
    }
    Check(decodes>0&&creates>0&&destroys>0&&ownership,"literal decode/read operations run on workers; GPU creation/destruction on owning thread with current context");
    Check(managerEnd<joined&&joined<rendererEnd&&rendererEnd<contextEnd&&contextEnd<events.size(),"shutdown order: resources drained, workers joined, renderer destroyed, then context destroyed");
    Check(order&&meshes.empty()&&textures.empty(),"all traced mesh/texture objects destroyed exactly once before renderer/context destruction");
    for(std::size_t i=joined+1;i<events.size();++i)Check(events[i].point!=Point::DecodeBegin&&events[i].point!=Point::DecodeEnd,"no decoder operation after worker join");
    std::ostringstream ownerText;ownerText<<owner;
    std::printf("OWNERSHIP main=%s decode_events=%d GPU_created=%d GPU_destroyed=%d trace_events=%zu\n",ownerText.str().c_str(),decodes,creates,destroys,events.size());
}

int RunCase(const std::string& name,const fs::path& output) {
    const int before=failures;const auto start=std::chrono::steady_clock::now();Fixture fixture(name);
    auto journal=std::make_shared<Journal>(output/(name+".events.tsv"));ApplicationControl control;
    control.hidden=true;control.resourceTrace=[journal](const auto& e){journal->Observe(e);};
    control.frameSeconds=[](float){return 1.0f/60.0f;};
    std::shared_ptr<Gate> gate;std::vector<JobHandle> blockers;int phase=0,frames=0;bool done=false,initialView=false;float initialX=0;unsigned int retainedHandle=0;std::uint64_t uploads=0;
    control.hostReady=[&](EngineHost& host){
        Check(!host.Resources().BlockingMode()&&host.Resources().Jobs()==&host.Jobs()&&host.Jobs().WorkerCount()>0&&host.Resources().GetRenderer()==&host.GetRenderer(),"normal application has async workers and real renderer (no headless residency)");
        std::printf("ASYNC enabled=%d workers=%u renderer_present=%d\n",!host.Resources().BlockingMode(),host.Jobs().WorkerCount(),host.Resources().GetRenderer()!=nullptr);
        if(name=="stale-generation"&&host.Jobs().WorkerCount()<2)
            throw std::runtime_error("late-after-new completion fixture requires at least two hardware-derived workers");
        if(name=="shared-progress")gate=journal->Hold(1,Point::FileReadChunk);
        if(name=="missing-recovery")fs::remove(fixture.Asset(8)); // DB was scanned; force actual fread path failure.
        if(name=="corrupt-recovery"){std::ofstream(fixture.Asset(9))<<"not an OBJ";std::ofstream(fixture.Asset(10))<<"not a PNG";}
    };
    control.worldReady=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
        if(name=="shared-progress"){
            gate->Wait();Check(host.Resources().RefCount(Id(1))==2,"two real authored consumers share demand");
            const auto count=host.Resources().Stats().misses;host.Resources().AddRef(Id(1));host.Resources().RequestMesh(Id(1));host.Resources().ReleaseRef(Id(1));
            Check(host.Resources().RefCount(Id(1))==2&&host.Resources().Stats().misses==count&&host.Resources().StateOf(Id(1))==ResourceState::Loading,"release one consumer keeps shared in-flight load alive");
            initialView=play.Session().ViewMode()==PlayerViewMode::FirstPerson;EntityPhysicalState s;world.GetEntityState(fixture.moving,s);initialX=s.position.x;
            host.GetWindow().SetEventHook([&](const SDL_Event& e){if(e.type==SDL_KEYDOWN&&e.key.keysym.scancode==SDL_SCANCODE_V)++phase;});ToggleView();
        }else{host.Jobs().WaitAll();host.Resources().Pump();VerifyMesh(host,1,1);}
    };
    control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
        if(done)return;
        if(++frames>10000)throw std::runtime_error("application-frame watchdog (not a performance acceptance threshold)");
        auto& rm=host.Resources();
        if(name=="shared-progress")return;
        if(phase==0){
            ++phase;
            if(name=="queued-cancel"){
                blockers=OccupyPool(host,journal);Request(rm,4);Check(rm.StateOf(Id(4))==ResourceState::Queued,"request genuinely queued behind bounded worker pool");rm.ReleaseRef(Id(4));ReleasePool(host,journal,blockers);host.Jobs().WaitAll();
            }else if(name=="read-cancel"){
                gate=journal->Hold(5,Point::FileReadChunk,true);Request(rm,5);gate->Wait();Check(rm.StateOf(Id(5))==ResourceState::Loading,"cancel after an actual file chunk, before next cooperative boundary");rm.ReleaseRef(Id(5));host.Jobs().WaitAll();
            }else if(name=="cpu-ready-cancel"){
                Request(rm,6);host.Jobs().WaitAll();rm.Pump(0);Check(rm.StateOf(Id(6))==ResourceState::CpuReady,"decoded CPU preparation awaits real upload budget");uploads=rm.Stats().uploads;rm.ReleaseRef(Id(6));
            }else if(name=="stale-generation"){
                gate=journal->Hold(7,Point::DecodeEnd);Request(rm,7);gate->Wait();Mesh(fixture.Asset(7),7);rm.Invalidate(Id(7));rm.RequestMesh(Id(7));
            }else if(name=="project-switch"||name=="project-close"){
                gate=journal->Hold(7,Point::DecodeEnd,true);Request(rm,7);gate->Wait();play.End();world.Destroy();
                Check(host.Jobs().Stats().running>0,"old project decode is held outstanding at project replacement entry");
                const auto target=name=="project-switch"?fixture.b:fixture.closed;
                host.OpenProjectAssets(target.string(),(target/"Assets").string());
                rm.ReleaseRef(Id(7)); // External consumer ends after the synchronous project drain.
                Check(host.Jobs().Stats().queued==0&&host.Jobs().Stats().running==0,"project replacement drains old work before database handoff");
                Check(!rm.TryGetMesh(Id(7)).IsValid(),"old project resource is unavailable after closure");
                Scene replacement;replacement.Settings().name="Other project";auto& s=replacement.CreateObject("Start");s.playerStart=ScenePlayerStartComponent{};
                std::string error;Check(world.Build(replacement,&rm,error)&&play.Begin(world,WorldCoordinates{},error),"application scene/session handoff uses real world lifecycle: "+error);
                if(name=="project-switch")Request(rm,7);
            }else if(name=="missing-recovery") {Request(rm,8);Request(rm,11);host.Jobs().WaitAll();}
            else if(name=="corrupt-recovery") {Request(rm,9);Request(rm,10);Request(rm,11);host.Jobs().WaitAll();}
            else if(name=="residency") {Request(rm,12);Request(rm,13);Request(rm,14);host.Jobs().WaitAll();}
            else if(name=="upload-budget") {for(int n:{2,3,11})Request(rm,n);host.Jobs().WaitAll();uploads=rm.Stats().uploads;}
            else if(name=="shutdown-outstanding"){
                Request(rm,17);host.Jobs().WaitAll();rm.Pump(0);Check(rm.StateOf(Id(17))==ResourceState::CpuReady,"shutdown fixture holds real CPU-ready data");
                gate=journal->Hold(15,Point::FileReadChunk);
                journal->ReleaseAfterCancellations(gate,{15,16});Request(rm,15);gate->Wait();
                blockers=OccupyPool(host,journal,int(host.Jobs().WorkerCount())-1);Request(rm,16);
                Check(rm.StateOf(Id(15))==ResourceState::Loading&&rm.StateOf(Id(16))==ResourceState::Queued,"shutdown has simultaneous running read and queued resource work");
                // Quit before another pump can consume CpuReady. Application
                // owns teardown; the normal loop still processes the SDL event.
                done=true;Quit();
            }
        }
    };
    control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
        auto& rm=host.Resources();
        if(done)return;
        if(name=="shared-progress"){
            if(frames==4){
                EntityPhysicalState s;world.GetEntityState(fixture.moving,s);
                Check(phase==1&&(play.Session().ViewMode()==PlayerViewMode::FirstPerson)!=initialView,"normal SDL events reached actual gameplay view toggle while IO outstanding");
                Check(!play.IsPaused()&&play.FixedStepsSinceReset()>=4&&s.position.x>initialX,"normal fixed-step scene advances physically while IO is held");
                std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);
                bool varied=false;
                for(std::size_t i=3;i+2<pixels.size();i+=3)
                    varied|=pixels[i]!=pixels[0]||pixels[i+1]!=pixels[1]||pixels[i+2]!=pixels[2];
                Check(host.GetRenderer().Stats().drawCalls>0&&host.GetRenderer().Stats().triangles>0&&varied&&rm.StateOf(Id(1))==ResourceState::Loading,"normal rendering produces framebuffer content while the real read remains pending");
                std::printf("PROGRESS frames=%d fixed_steps=%llu body_x_before=%.9g body_x_after=%.9g draw_calls=%u triangles=%u spatial_pixels=%d outstanding_state=%s\n",frames,
                    (unsigned long long)play.FixedStepsSinceReset(),initialX,s.position.x,host.GetRenderer().Stats().drawCalls,host.GetRenderer().Stats().triangles,varied,ResourceStateName(rm.StateOf(Id(1))));
                gate->Open();
            }
            if(frames>4&&Ready(rm,1)){VerifyMesh(host,1,1);Check(journal->Count(Point::DecodeBegin,1)==1,"shared authored demand decoded exactly once");done=true;}
        }else if(name=="queued-cancel"||name=="read-cancel"||name=="cpu-ready-cancel"){
            const int id=name=="queued-cancel"?4:name=="read-cancel"?5:6;
            Check(rm.StateOf(Id(id))==ResourceState::Cancelled&&!rm.TryGetMesh(Id(id)).IsValid(),"last demand cancellation cannot resurrect or upload resource");
            if(id==4)Check(journal->Count(Point::FileReadBegin,id)==0,"cancelled queue never executes actual IO");
            if(id==5)Check(journal->Count(Point::FileReadChunk,id)==1&&journal->Count(Point::DecodeBegin,id)==0,"running read stops at cooperative chunk boundary, without decoder entry");
            if(id==6)Check(rm.Stats().uploads==uploads&&journal->Count(Point::DecodeEnd,id)==1,"CPU-ready cancellation discards actual decoded data without GPU creation");
            done=true;
        }else if(name=="stale-generation"){
            if(phase==1&&Ready(rm,7)){VerifyMesh(host,7,7);retainedHandle=rm.TryGetMesh(Id(7)).id;gate->Open();host.Jobs().WaitAll();++phase;}
            else if(phase==2){Check(rm.TryGetMesh(Id(7)).id==retainedHandle&&journal->Count(Point::StaleDiscarded,7)>0,"late old completion discarded without replacing newer GPU handle");VerifyMesh(host,7,7);done=true;}
        }else if(name=="project-switch"&&Ready(rm,7)){VerifyMesh(host,7,9);Check(rm.RefCount(Id(7))==1,"new project's reference count reflects its actual consumer");done=true;}
        else if(name=="project-close"){Check(rm.StateOf(Id(7))==ResourceState::Unloaded&&!rm.TryGetMesh(Id(7)).IsValid(),"closed project data cannot return in subsequent normal pump");done=true;}
        else if(name=="missing-recovery"||name=="corrupt-recovery"){
            const int bad=name=="missing-recovery"?8:9;
            if(phase==1&&rm.StateOf(Id(bad))==ResourceState::Failed&&Ready(rm,11)&&(bad==8||rm.StateOf(Id(10))==ResourceState::Failed)){
                Check(!rm.ErrorOf(Id(bad)).empty(),"real missing/corrupt load fails with a visible error");VerifyMesh(host,11,1);
                if(bad==9)Check(!rm.ErrorOf(Id(10)).empty(),"actual image decoder reports corrupt PNG");
                Mesh(fixture.Asset(bad),4);rm.Invalidate(Id(bad));rm.RequestMesh(Id(bad));
                if(bad==9){Texture(fixture.Asset(10));rm.Invalidate(Id(10));rm.RequestTexture(Id(10));}++phase;
            }else if(phase==2&&Ready(rm,bad)&&(bad==8||Ready(rm,10))){VerifyMesh(host,bad,4);if(bad==9)VerifyTexture(host,10);VerifyMesh(host,11,1);done=true;}
        }else if(name=="residency"){
            if(phase==1&&Ready(rm,12)&&Ready(rm,13)&&Ready(rm,14)){
                VerifyMesh(host,12,1);retainedHandle=rm.TryGetMesh(Id(12)).id;rm.ReleaseRef(Id(12));rm.ReleaseRef(Id(13));rm.TryGetMesh(Id(13));
                rm.SetBudgetBytes(rm.Stats().bytesResident-rm.BytesOf(Id(12)));++phase;
            }else if(phase==2){MeshData data;Check(rm.StateOf(Id(12))==ResourceState::Unloaded&&Ready(rm,13)&&!host.GetRenderer().ReadMeshForDiagnostics(MeshHandle{retainedHandle},data),"LRU evicts and actually destroys the oldest unused GPU buffers");rm.SetBudgetBytes(0);++phase;}
            else if(phase==3){Check(Ready(rm,1)&&Ready(rm,14)&&rm.Stats().bytesResident>rm.Stats().budgetBytes&&rm.StateOf(Id(13))==ResourceState::Unloaded,"budget protects active consumers and reports unavoidable over-budget residency");std::printf("OVER_BUDGET resident=%llu budget=%llu\n",(unsigned long long)rm.Stats().bytesResident,(unsigned long long)rm.Stats().budgetBytes);rm.SetBudgetBytes(256ull*1024*1024);Request(rm,12);++phase;}
            else if(phase==4&&Ready(rm,12)){VerifyMesh(host,12,1);Check(rm.TryGetMesh(Id(12)).id!=retainedHandle,"evicted asset reloads into a newly created real GPU handle");done=true;}
        }else if(name=="upload-budget"){
            if(phase==1){Check(rm.Stats().uploads-uploads==2,"normal per-frame Pump uploads exactly its two-resource budget");int cpu=0;for(int n:{2,3,11})cpu+=rm.StateOf(Id(n))==ResourceState::CpuReady;Check(cpu==1,"third genuinely decoded resource remains CPU-ready");++phase;}
            else if(phase==2){VerifyTexture(host,3);VerifyMesh(host,2,1);VerifyMesh(host,11,1);done=true;}
        }
        if(done)Quit();
    };
    // Application finishes its close-event frame before leaving the loop.
    // Request the three real loads at that frame's ordinary after-frame boundary;
    // the next operation is the application's existing owner-driven teardown.
    if(name=="shutdown-outstanding"){
        auto setup=control.beforeFrame;
        control.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){++frames;};
        control.afterFrame=[&,setup](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
            if(host.GetWindow().ShouldClose())setup(host,world,play);
            else Quit();
        };
        control.beforeShutdown=[&](EngineHost& host,RuntimeWorld&,InteractivePlay&){
            Check(host.Resources().StateOf(Id(17))==ResourceState::CpuReady,"application exits with CPU-ready work unconsumed");
        };
    }
    // Release gates before Application's stack unwinds on fixture failure.
    // This is failure cleanup, never a success-path timing mechanism.
    const auto safeHook=[journal](auto hook){
        return [journal,hook](auto&... args){
            try { if(hook)hook(args...); }
            catch(...) { journal->OpenAll(); throw; }
        };
    };
    control.hostReady=safeHook(control.hostReady);
    control.worldReady=safeHook(control.worldReady);
    control.beforeFrame=safeHook(control.beforeFrame);
    control.afterFrame=safeHook(control.afterFrame);
    control.beforeShutdown=safeHook(control.beforeShutdown);
    std::string program="judas",project=(fixture.a/"main.judasproj").string();char* argv[]={program.data(),project.data()};
    try{Application app;Check(app.Run(2,argv,&control)==0,"normal application returned cleanly");}
    catch(const std::exception& e){journal->OpenAll();Check(false,std::string("integration exception: ")+e.what());}
    journal->OpenAll();VerifyOwnership(journal,std::this_thread::get_id());
    if(name=="shutdown-outstanding")Check(journal->Count(Point::DecodeBegin,16)==0&&journal->Count(Point::DecodeBegin,15)==0,"shutdown cancels queued work and running read before decoding");
    Check(done,"case completed every required assertion phase");
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("CASE %s %s frames=%d seconds=%.6f\n",name.c_str(),failures==before?"PASS":"FAIL",frames,seconds);
    return failures==before?0:1;
}
}
int main(int argc,char** argv) {
    std::string selected="all";fs::path output="build/ftft2-application";
    for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--case"&&i+1<argc)selected=argv[++i];else if(arg=="--output"&&i+1<argc)output=argv[++i];else return 2;}
    fs::create_directories(output);
    if(std::getenv("JUDAS_TEST_SCRIPT")||std::getenv("JUDAS_RESOURCE_MODE")){std::fprintf(stderr,"Unset scripted/blocking overrides for async integration\n");return 2;}
    const std::vector<std::string> cases={"shared-progress","queued-cancel","read-cancel","cpu-ready-cancel","stale-generation","project-switch","project-close","missing-recovery","corrupt-recovery","residency","upload-budget","shutdown-outstanding"};
    bool ran=false;for(const auto& name:cases)if(selected=="all"||selected==name){ran=true;RunCase(name,output);}
    if(!ran)return 2;
    std::printf("FTFT2 integration: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
