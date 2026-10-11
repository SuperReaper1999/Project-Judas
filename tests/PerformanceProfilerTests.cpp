#include "PerformanceProfiler.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <new>
#include <thread>
static std::atomic<bool> countAllocations{false};static std::atomic<unsigned> allocations{0};
void* operator new(std::size_t n){if(countAllocations)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){unsigned checks=0,failed=0;auto check=[&](bool v,const char* msg){++checks;failed+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",msg);};auto& p=PerformanceProfiler::Get();
 auto initial=p.Diagnostics();check(!p.Enabled()&&!p.Frozen()&&!initial.droppedEvents&&!initial.exhaustedLabels&&!initial.exhaustedNodes&&!initial.exhaustedThreads&&!initial.incompleteScopes&&!initial.openScopes&&!initial.truncatedFrames&&!initial.gpuDropped&&!initial.nestedFrames,"fresh profiler lanes and counters retain zero initialization");
 p.Enable(true);p.Clear();p.RegisterThread("Test main");
 ProfileLabel a("Parent A"),b("Child"),c("Parent C"),sum("Sum"),latest("Latest"),maximum("Maximum");
 p.BeginFrame("headless",false,100);{ProfileScope parent(a,false,110);{ProfileScope child(b,false,130);child.End(170);}parent.End(190);}p.EndFrame(200);
 auto f=p.History().back();check(f.scopes.size()==2,"nested scopes recorded without GL/editor");check(f.scopes[0].end-f.scopes[0].start==40&&f.scopes[1].exclusive==40,"inclusive/exclusive use deterministic thread durations");check(f.coverage==80&&f.unattributed==20,"outer boundaries retain unattributed time");
 p.BeginFrame("headless",false,300);{ProfileScope parent(c,false,310);{ProfileScope child(b,true,320);child.End(360);}parent.End(390);}p.Counter(sum,2,ProfileCounterMode::Sum);p.Counter(sum,3,ProfileCounterMode::Sum);p.Counter(latest,4,ProfileCounterMode::Latest);p.Counter(latest,7,ProfileCounterMode::Latest);p.Counter(maximum,9,ProfileCounterMode::Maximum);p.Counter(maximum,8,ProfileCounterMode::Maximum);p.FixedState(.02,true,.12,false);p.FixedStep(330,340,1./60);p.FixedStep(350,365,1./60);p.EndFrame(400);
 auto g=p.History().back();check(f.scopes[0].node!=g.scopes[0].node,"same child label under different parents does not merge");check(g.waiting==40,"nested waits are separately accounted");check(g.fixed.size()==2&&g.capReached&&g.discardedSeconds==.12,"multiple fixed steps and actual backlog/cap are retained");check(g.counters.size()==3&&g.counters[0].value==5&&g.counters[1].value==7&&g.counters[2].value==9,"sum/latest/maximum counter semantics");
 p.BeginFrame("paused",false,500);p.FixedState(0,false,0,true);p.EndFrame(600);auto paused=p.History().back();check(paused.fixed.empty()&&paused.paused,"paused frame has zero fixed steps");
 auto previous=p.History().size();p.BeginFrame("boundary",false,700);{ProfileScope scope(a,false,710);p.Freeze(true);scope.End(750);}p.EndFrame(800);check(p.History().size()==previous&&p.Diagnostics().incompleteScopes>0,"freeze does not publish incomplete cross-boundary scopes");p.Freeze(false);
 p.BeginFrame("clear",false,900);{ProfileScope scope(a,false,910);p.Clear();scope.End(950);}p.EndFrame(1000);check(p.History().size()==1&&p.History().back().incomplete,"clear preserves open-stack safety and marks capture boundary");
 p.Clear();p.BeginFrame("startup",true,1000);{ProfileScope startup(a,false,1010);startup.End(40'001'000);}p.EndFrame(40'001'100);auto frozenSpike=p.Spikes().back();
 for(unsigned i=0;i<130;++i){p.BeginFrame("wrap",false,50'000'000+i*1000);{ProfileScope scope(a,false,50'000'010+i*1000);scope.End(50'000'500+i*1000);}p.EndFrame(50'000'800+i*1000);}
 check(p.History().size()==ProfileLimits::Frames,"frame history is bounded and wraps");check(p.Startup().startup&&p.Startup().scopes.size()==1,"startup is retained separately after ordinary history wraps");check(p.Spikes().back().id==frozenSpike.id&&frozenSpike.scopes.size()==1,"spike owns its data after ring wrapping");check(p.Summary().frames==120&&p.Summary().maximum>0,"bounded statistical window is explicit");
 p.Clear();p.BeginFrame("gpu",false,200'000'000);p.GPUAvailability(true,"synthetic test only");auto source=p.FrameId();auto token=p.GPUPending("camera",42);p.EndFrame(200'000'100);p.BeginFrame("later",false,200'001'000);p.EndFrame(200'001'100);p.GPUComplete(source,token,1.25);auto h=p.History();check(!h[0].gpu[0].pending&&h[0].gpu[0].frame==source&&h[1].gpu.empty(),"delayed GPU result updates its original frame only");p.Freeze(true);p.GPUComplete(source,token,2);check(p.History()[0].gpu[0].milliseconds==1.25,"GPU arrival cannot mutate frozen capture");p.Freeze(false);p.GPUAvailability(false,"headless");
 std::mutex mutex;std::condition_variable cv;bool started=false,finish=false;
 p.Clear();p.BeginFrame("worker start");std::thread worker([&](){p.RegisterThread("Worker test");{ProfileScope scope(a);{std::lock_guard<std::mutex> l(mutex);started=true;}cv.notify_one();{std::unique_lock<std::mutex> l(mutex);cv.wait(l,[&](){return finish;});}}p.ReleaseThread();});{std::unique_lock<std::mutex> l(mutex);cv.wait(l,[&](){return started;});}auto workerFrame=p.FrameId();p.EndFrame();p.BeginFrame("worker completion");{std::lock_guard<std::mutex> l(mutex);finish=true;}cv.notify_one();worker.join();p.EndFrame();auto work=p.History().back();check(work.scopes.size()==1&&work.scopes[0].originFrame==workerFrame&&work.scopes[0].thread!=work.mainThread,"cross-frame workers retain lane/time/source frame rather than main nesting");
 p.Clear();p.BeginFrame("hot allocation");{ProfileScope warm(a);{ProfileScope warmChild(b);}}allocations=0;countAllocations=true;for(unsigned i=0;i<1000;++i){ProfileScope warm(a);{ProfileScope child(b);}}countAllocations=false;p.EndFrame();check(allocations==0,"warmed hot native scopes allocate no heap memory");
 p.Clear();p.BeginFrame("overflow");for(unsigned i=0;i<10000;++i){ProfileScope scope(a);}p.EndFrame();auto d=p.Diagnostics();check(d.droppedEvents>0&&d.truncatedFrames>0&&p.History().back().scopes.size()==ProfileLimits::FrameEvents,"lane/history saturation is bounded and visible");
 p.Enable(false);auto old=p.Diagnostics();{ProfileScope noScope(a);}check(p.Diagnostics().droppedEvents==old.droppedEvents,"disabled recording performs no registration or event write");p.Enable(true);p.Clear();p.BeginFrame("world");p.Boundary("Test world boundary");p.EndFrame();check(!p.History().back().boundaries.empty(),"world lifecycle markers contain owned names only");std::string error;const auto report=(std::filesystem::temp_directory_path()/"judas-m56-headless.json").string();check(p.Export(report,error),"structured headless report exports");std::ifstream input(report);std::string json((std::istreambuf_iterator<char>(input)),{});check(json.find("steady monotonic nanoseconds")!=std::string::npos&&json.find("unattributed_ns")!=std::string::npos,"headless report states units and incomplete/unknown accounting");
 // Latest observations use timestamps, not the order independent lanes drain.
 p.Clear();p.BeginFrame("concurrent counters");ProfileLabel latestWorker("Concurrent latest");started=finish=false;
 std::thread first([&](){p.RegisterThread("Concurrent first");{ProfileScope scope(a);p.Counter(latestWorker,1,ProfileCounterMode::Latest);{std::lock_guard<std::mutex> lock(mutex);started=true;}cv.notify_one();{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&](){return finish;});}}p.ReleaseThread();});
 {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&](){return started;});}
 std::thread second([&](){p.RegisterThread("Concurrent second");{ProfileScope scope(a);}p.ReleaseThread();});
 p.Counter(latestWorker,7,ProfileCounterMode::Latest);{std::lock_guard<std::mutex> lock(mutex);finish=true;}cv.notify_one();first.join();second.join();p.EndFrame();
 auto concurrent=p.History().back();check(concurrent.scopes.size()==2&&concurrent.scopes[0].thread!=concurrent.scopes[1].thread,"simultaneous producers retain independent bounded lanes");check(concurrent.counters.size()==1&&concurrent.counters[0].value==7,"latest across workers follows observation time, not lane drain order");
 // Paired native instrumentation cost: bounded batches, no intentional overflow,
 // no sleeps, no claim that machine scheduling noise is a correctness failure.
 for(unsigned trial=0;trial<4;++trial){bool enabled=trial==1||trial==2;p.Enable(enabled);p.Clear();auto begin=PerformanceProfiler::Now();
  for(unsigned frame=0;frame<160;++frame){ProfileFrame measured("Native overhead");for(unsigned i=0;i<200;++i){ProfileScope outer(a);{ProfileScope child(b);}}}
  double elapsed=double(PerformanceProfiler::Now()-begin)/1e6;std::printf("M56 native trial %u enabled %u: %.6f ms/frame including collection; %.3f ns/scope including collection (400 scopes/frame)\n",trial,unsigned(enabled),elapsed/160,elapsed*1e6/(160*400));
 }
 p.Enable(true);
 unsigned exhausted=0;for(unsigned i=0;i<ProfileLimits::Labels+10;++i)if(!p.Intern("synthetic bounded "+std::to_string(i)))++exhausted;check(exhausted>0&&p.Diagnostics().exhaustedLabels>0,"unique script-like labels cannot grow storage indefinitely");
 p.Enable(false);std::printf("M56 profiler %u checks %u failures; reserved %zu bytes\n",checks,failed,p.ReservedBytes());return failed?1:0;
}
