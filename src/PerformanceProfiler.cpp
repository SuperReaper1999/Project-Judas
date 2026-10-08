#include "PerformanceProfiler.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

namespace {
struct Event {unsigned kind=0,node=0,parent=0,depth=0,mode=0;std::uint64_t thread=0,frame=0,start=0,end=0,exclusive=0;double value=0;bool wait=false;};
struct Open {unsigned node=0,parent=0;std::uint64_t sequence=0,epoch=0,start=0,children=0,frame=0;bool wait=false;};
struct Cache {std::uint64_t key=0;unsigned node=0;};
struct Lane {
    std::atomic<bool> claimed{false};std::uint64_t id=0;char name[64]{};
    std::array<Event,ProfileLimits::LaneEvents> events{};
    std::atomic<std::uint64_t> write{0},read{0};
    std::array<Open,ProfileLimits::Depth> stack{};unsigned depth=0;std::uint64_t sequence=0;
    std::array<Cache,8192> cache{};std::atomic<unsigned> open{0};
};
thread_local int localLane=-1;
std::string quote(std::string_view text){std::string out="\"";for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<32){char b[7];std::snprintf(b,sizeof b,"\\u%04x",c);out+=b;}else out+=char(c);}return out+'"';}
std::uint64_t coverage(std::vector<std::pair<std::uint64_t,std::uint64_t>> intervals){if(intervals.empty())return 0;std::sort(intervals.begin(),intervals.end());std::uint64_t total=0,a=intervals[0].first,b=intervals[0].second;for(auto p:intervals){if(p.first>b){total+=b-a;a=p.first;b=p.second;}else b=std::max(b,p.second);}return total+b-a;}
}
struct PerformanceProfiler::Impl {
    std::atomic<bool> enabled{false},frozen{false},scripts{true};std::atomic<std::uint64_t> epoch{1},frame{0},world{0};
    std::array<Lane,ProfileLimits::Threads> lanes{};
    std::mutex registration;
    std::array<std::array<char,128>,ProfileLimits::Labels> labels{};unsigned labelCount=1;
    struct Node {unsigned label=0,parent=0;};std::array<Node,ProfileLimits::Nodes> nodes{};unsigned nodeCount=1;
    std::atomic<std::uint64_t> eventsDropped{0},labelsDropped{0},nodesDropped{0},threadsDropped{0},incomplete{0},truncated{0},gpuDropped{0},nested{0};
    std::uint64_t nextThread=0,nextStep=0,nextGPU=0,previousStart=0,lastMemory=0,rss=0,virtualBytes=0;
    std::uint64_t frameEpoch=0;bool inFrame=false,gpuAvailable=false;std::string gpuStatus="No renderer/context",mode;
    ProfileFrameSnapshot current,startup;
    std::array<ProfileFrameSnapshot,ProfileLimits::Frames> history{};unsigned historySize=0,historyNext=0;
    std::array<ProfileFrameSnapshot,ProfileLimits::Spikes> spikes{};unsigned spikeSize=0,spikeNext=0;
    std::uint64_t spikeThreshold=33'333'333;
    void Push(Lane& lane,const Event& e){auto w=lane.write.load(std::memory_order_relaxed);if(w-lane.read.load(std::memory_order_acquire)>=lane.events.size()){++eventsDropped;return;}lane.events[w%lane.events.size()]=e;lane.write.store(w+1,std::memory_order_release);}
    unsigned NodeFor(Lane& lane,unsigned label,unsigned parent){std::uint64_t key=(std::uint64_t(parent)<<32)|label;auto slot=(key*11400714819323198485ull)>>51;
        for(unsigned n=0;n<lane.cache.size();++n){auto& c=lane.cache[(slot+n)%lane.cache.size()];if(c.key==key)return c.node;if(!c.key){std::lock_guard<std::mutex> lock(registration);unsigned id=0;for(unsigned i=1;i<nodeCount;++i)if(nodes[i].label==label&&nodes[i].parent==parent){id=i;break;}if(!id){if(nodeCount==nodes.size()){++nodesDropped;return 0;}id=nodeCount++;nodes[id]={label,parent};}c={key,id};return id;}}
        ++nodesDropped;return 0;
    }
};
PerformanceProfiler& PerformanceProfiler::Get(){static PerformanceProfiler profiler;return profiler;}
PerformanceProfiler::PerformanceProfiler():m(new Impl){}
PerformanceProfiler::~PerformanceProfiler()=default;
std::uint64_t PerformanceProfiler::Now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool PerformanceProfiler::Active()const{return m->enabled.load(std::memory_order_relaxed)&&!m->frozen.load(std::memory_order_relaxed);}
bool PerformanceProfiler::Enabled()const{return m->enabled.load();}
bool PerformanceProfiler::Frozen()const{return m->frozen.load();}
void PerformanceProfiler::Enable(bool v){if(m->enabled.exchange(v)!=v){++m->epoch;m->previousStart=0;}}
void PerformanceProfiler::Freeze(bool v){if(m->frozen.exchange(v)!=v){++m->epoch;m->previousStart=0;}}
void PerformanceProfiler::RegisterThread(std::string_view name){if(localLane>=0)return;std::lock_guard<std::mutex> lock(m->registration);for(unsigned i=0;i<m->lanes.size();++i){auto& l=m->lanes[i];if(!l.claimed.load()&&l.read.load()==l.write.load()){l.claimed=true;l.id=++m->nextThread;std::snprintf(l.name,sizeof l.name,"%.*s",int(std::min(name.size(),sizeof l.name-1)),name.data());l.depth=0;l.open=0;localLane=int(i);return;}}++m->threadsDropped;}
void PerformanceProfiler::ReleaseThread(){if(localLane<0)return;auto& l=m->lanes[unsigned(localLane)];m->incomplete+=l.depth;l.depth=0;l.open=0;l.claimed.store(false,std::memory_order_release);localLane=-1;}
std::uint32_t PerformanceProfiler::Intern(std::string_view name){if(!Active())return 0;std::lock_guard<std::mutex> lock(m->registration);if(name.empty()||name.size()>=128){++m->labelsDropped;return 0;}for(unsigned i=1;i<m->labelCount;++i)if(name==m->labels[i].data())return i;if(m->labelCount==m->labels.size()){++m->labelsDropped;return 0;}unsigned id=m->labelCount++;std::memcpy(m->labels[id].data(),name.data(),name.size());m->labels[id][name.size()]=0;return id;}
std::uint32_t PerformanceProfiler::Label(ProfileLabel& l){if(!Active())return 0;auto id=l.id.load(std::memory_order_relaxed);if(id==UINT32_MAX)return 0;if(!id){id=Intern(l.name);l.id.store(id?id:UINT32_MAX,std::memory_order_relaxed);}return id;}
PerformanceProfiler::Token PerformanceProfiler::Begin(std::uint32_t label,bool wait,std::uint64_t stamp){if(!label||!Active())return {};if(localLane<0)RegisterThread("CPU thread");if(localLane<0)return {};auto& l=m->lanes[unsigned(localLane)];if(l.depth==l.stack.size()){++m->eventsDropped;return {};}
    unsigned parent=l.depth?l.stack[l.depth-1].node:0,node=m->NodeFor(l,label,parent);if(!node)return {};unsigned depth=l.depth++;auto seq=++l.sequence;l.stack[depth]={node,parent,seq,m->epoch.load(),stamp?stamp:Now(),0,m->frame.load(),wait};l.open.store(l.depth,std::memory_order_relaxed);return {unsigned(localLane),depth,seq};}
void PerformanceProfiler::End(Token t,std::uint64_t stamp){if(!t.sequence)return;auto& l=m->lanes[t.lane];if(l.depth!=t.depth+1||l.stack[t.depth].sequence!=t.sequence){++m->incomplete;return;}auto s=l.stack[t.depth];--l.depth;l.open.store(l.depth,std::memory_order_relaxed);auto end=stamp?stamp:Now();auto duration=end>=s.start?end-s.start:0;
    if(l.depth)l.stack[l.depth-1].children+=duration;
    if(s.epoch!=m->epoch.load()||!Active()){++m->incomplete;return;}
    m->Push(l,{0,s.node,s.parent,t.depth,0,l.id,s.frame,s.start,end,duration>=s.children?duration-s.children:0,0,s.wait});}
void PerformanceProfiler::Counter(ProfileLabel& label,double value,ProfileCounterMode mode){if(!Active())return;Counter(Label(label),value,mode);}
void PerformanceProfiler::Counter(std::uint32_t label,double value,ProfileCounterMode mode){if(!label||!Active()||!std::isfinite(value))return;if(localLane<0)RegisterThread("CPU thread");if(localLane<0)return;auto& l=m->lanes[unsigned(localLane)];m->Push(l,{1,label,0,0,unsigned(mode),l.id,m->frame.load(),Now(),0,0,value,false});}
bool PerformanceProfiler::BeginFrame(std::string_view mode,bool first,std::uint64_t stamp){if(!Active())return false;if(m->inFrame){++m->nested;return false;}RegisterThread("Main");m->inFrame=true;m->frameEpoch=m->epoch.load();
    // EndFrame swaps the evicted slot into current; history stays intact
    // while a new frame is open, including a mid-frame freeze.
    auto& f=m->current;f.scopes.clear();f.counters.clear();f.threads.clear();f.fixed.clear();f.gpu.clear();f.boundaries.clear();
    f.scopes.reserve(ProfileLimits::FrameEvents);f.counters.reserve(ProfileLimits::Counters);f.threads.reserve(ProfileLimits::Threads);f.fixed.reserve(ProfileLimits::Steps);f.gpu.reserve(ProfileLimits::GPU);f.boundaries.reserve(64);
    f.paused=f.capReached=f.incomplete=false;f.accumulatorSeconds=f.discardedSeconds=0;f.id=++m->frame;f.world=m->world.load();f.start=stamp?stamp:Now();f.interval=m->previousStart?f.start-m->previousStart:0;m->previousStart=f.start;f.mode=mode;f.startup=first;f.mainThread=localLane>=0?m->lanes[unsigned(localLane)].id:0;return true;}
void PerformanceProfiler::FixedState(double accumulator,bool cap,double discarded,bool paused){if(!m->inFrame||!Active())return;m->current.accumulatorSeconds=accumulator;m->current.capReached=cap;m->current.discardedSeconds=discarded;m->current.paused=paused;}
void PerformanceProfiler::FixedStep(std::uint64_t a,std::uint64_t b,double dt){if(!m->inFrame||!Active())return;if(m->current.fixed.size()==ProfileLimits::Steps){++m->truncated;return;}m->current.fixed.push_back({++m->nextStep,a,b,dt});}
void PerformanceProfiler::Boundary(std::string_view name){++m->world;if(!Active())return;unsigned label=Intern(name);if(!label)return;if(localLane<0)RegisterThread("Main");if(localLane>=0){auto& l=m->lanes[unsigned(localLane)];m->Push(l,{2,label,0,0,0,l.id,m->frame.load(),Now()});}}
void PerformanceProfiler::GPUAvailability(bool v,std::string_view reason){m->gpuAvailable=v;m->gpuStatus=reason;}
std::uint64_t PerformanceProfiler::GPUPending(std::string_view pass,std::uint64_t camera){if(!Active()||!m->inFrame)return 0;if(m->current.gpu.size()==ProfileLimits::GPU){++m->gpuDropped;return 0;}auto token=++m->nextGPU;m->current.gpu.push_back({token,FrameId(),camera,ProfileName(pass),true,0});return token;}
void PerformanceProfiler::GPUComplete(std::uint64_t frame,std::uint64_t token,double ms){if(!Active()||!std::isfinite(ms)||ms<0){++m->gpuDropped;return;}bool found=false;auto update=[&](ProfileFrameSnapshot& f){if(f.id!=frame)return;for(auto& g:f.gpu)if(g.token==token){g.pending=false;g.milliseconds=ms;found=true;}};update(m->current);update(m->startup);for(auto& f:m->history)update(f);for(auto& f:m->spikes)update(f);if(!found)++m->gpuDropped;}
void PerformanceProfiler::DropGPU(){++m->gpuDropped;}
std::uint64_t PerformanceProfiler::FrameId()const{return m->frame.load();}
ProfileDiagnostics PerformanceProfiler::Diagnostics()const{ProfileDiagnostics d;d.droppedEvents=m->eventsDropped.load();d.exhaustedLabels=m->labelsDropped.load();d.exhaustedNodes=m->nodesDropped.load();d.exhaustedThreads=m->threadsDropped.load();d.incompleteScopes=m->incomplete.load();d.truncatedFrames=m->truncated.load();d.gpuDropped=m->gpuDropped.load();d.nestedFrames=m->nested.load();for(auto& l:m->lanes)d.openScopes+=l.open.load();return d;}
std::size_t PerformanceProfiler::ReservedBytes()const{
    std::size_t bytes=sizeof(Impl);auto add=[&](const ProfileFrameSnapshot& f){bytes+=f.scopes.capacity()*sizeof(ProfileScopeRecord)+f.counters.capacity()*sizeof(ProfileCounterRecord)+f.fixed.capacity()*sizeof(ProfileFixedRecord)+f.gpu.capacity()*sizeof(ProfileGPURecord)+f.threads.capacity()*sizeof(ProfileThreadRecord)+f.boundaries.capacity()*sizeof(ProfileName);};add(m->current);add(m->startup);for(auto& f:m->history)add(f);for(auto& f:m->spikes)add(f);return bytes;
}
void PerformanceProfiler::EndFrame(std::uint64_t stamp){if(!m->inFrame)return;auto collectionStart=Now();m->inFrame=false;auto& f=m->current;f.end=stamp?stamp:Now();f.incomplete=m->frameEpoch!=m->epoch.load();
    std::lock_guard<std::mutex> lock(m->registration); // consumer only; producers never take this to record
    for(auto& l:m->lanes){auto read=l.read.load(std::memory_order_relaxed),write=l.write.load(std::memory_order_acquire);bool seen=false;
        while(read!=write){auto e=l.events[read%l.events.size()];++read;seen=true;if(!Active())continue;
            if(e.kind==0){if(f.scopes.size()==ProfileLimits::FrameEvents){++m->truncated;continue;}f.scopes.push_back({e.node,e.parent,e.depth,e.thread,e.frame,e.start,e.end,e.exclusive,e.wait,m->labels[m->nodes[e.node].label].data()});}
            else if(e.kind==1){auto name=m->labels[e.node].data();auto i=std::find_if(f.counters.begin(),f.counters.end(),[&](auto& c){return c.name==name;});if(i==f.counters.end()){if(f.counters.size()==ProfileLimits::Counters){++m->truncated;continue;}f.counters.push_back({name,e.value,e.start,ProfileCounterMode(e.mode)});}else if(i->mode==ProfileCounterMode::Sum)i->value+=e.value;else if(i->mode==ProfileCounterMode::Maximum)i->value=std::max(i->value,e.value);else if(e.start>=i->observed){i->value=e.value;i->observed=e.start;}}
            else if(f.boundaries.size()<64)f.boundaries.push_back(m->labels[e.node].data());else ++m->truncated;
        }l.read.store(read,std::memory_order_release);if(seen)f.threads.push_back({l.id,l.name});
    }
    if(!Active())return;
    std::vector<std::pair<std::uint64_t,std::uint64_t>> covered,waits;
    for(auto& s:f.scopes)if(s.thread==f.mainThread){auto a=std::max(f.start,s.start),b=std::min(f.end,s.end);if(b>a){if(!s.parent)covered.push_back({a,b});if(s.wait)waits.push_back({a,b});}}
    f.coverage=coverage(std::move(covered));f.waiting=coverage(std::move(waits));f.unattributed=(f.end-f.start)>f.coverage?f.end-f.start-f.coverage:0;f.gpuAvailable=m->gpuAvailable;f.gpuStatus=m->gpuStatus;f.diagnostics=Diagnostics();
    if(!m->lastMemory||f.end-m->lastMemory>=1'000'000'000ull){
#ifdef _WIN32
PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory);
if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))){m->virtualBytes=memory.PrivateUsage;m->rss=memory.WorkingSetSize;}
#else
std::ifstream in("/proc/self/statm");std::uint64_t pages=0,resident=0;if(in>>pages>>resident){auto size=std::uint64_t(sysconf(_SC_PAGESIZE));m->virtualBytes=pages*size;m->rss=resident*size;}
#endif
m->lastMemory=f.end;}
    f.processResidentBytes=m->rss;f.processVirtualBytes=m->virtualBytes;f.profilerReservedBytes=ReservedBytes();
    if(f.startup)m->startup=f;
    if(f.end-f.start>=m->spikeThreshold){m->spikes[m->spikeNext]=f;m->spikeNext=(m->spikeNext+1)%ProfileLimits::Spikes;m->spikeSize=std::min(m->spikeSize+1,ProfileLimits::Spikes);}
    std::swap(m->history[m->historyNext],m->current);m->historyNext=(m->historyNext+1)%ProfileLimits::Frames;m->historySize=std::min(m->historySize+1,ProfileLimits::Frames);
    auto& saved=m->history[(m->historyNext+ProfileLimits::Frames-1)%ProfileLimits::Frames];saved.collectionNanoseconds=Now()-collectionStart;
    if(m->startup.id==saved.id)m->startup.collectionNanoseconds=saved.collectionNanoseconds;
    for(auto& spike:m->spikes)if(spike.id==saved.id)spike.collectionNanoseconds=saved.collectionNanoseconds;
}
void PerformanceProfiler::Clear(){++m->epoch;m->historySize=m->historyNext=m->spikeSize=m->spikeNext=0;m->startup={};m->previousStart=0;m->current.fixed.clear();m->current.gpu.clear();
    for(auto& l:m->lanes)l.read.store(l.write.load(std::memory_order_acquire),std::memory_order_release);
}
std::vector<ProfileFrameSnapshot> PerformanceProfiler::History()const{std::vector<ProfileFrameSnapshot> out;for(unsigned i=0;i<m->historySize;++i)out.push_back(m->history[(m->historyNext+ProfileLimits::Frames-m->historySize+i)%ProfileLimits::Frames]);return out;}
std::vector<ProfileFrameSnapshot> PerformanceProfiler::Spikes()const{std::vector<ProfileFrameSnapshot> out;for(unsigned i=0;i<m->spikeSize;++i)out.push_back(m->spikes[(m->spikeNext+ProfileLimits::Spikes-m->spikeSize+i)%ProfileLimits::Spikes]);return out;}
ProfileFrameSnapshot PerformanceProfiler::Startup()const{return m->startup;}
std::vector<ProfileFrameIndex> PerformanceProfiler::Timeline()const{
    std::vector<ProfileFrameIndex> out;out.reserve(m->historySize);
    for(unsigned i=0;i<m->historySize;++i){const auto& f=m->history[(m->historyNext+ProfileLimits::Frames-m->historySize+i)%ProfileLimits::Frames];out.push_back({f.id,double(f.end-f.start)/1e6,unsigned(f.fixed.size()),f.startup});}return out;
}
ProfileFrameSnapshot PerformanceProfiler::Snapshot(std::uint64_t id)const{
    if(m->startup.id==id)return m->startup;
    for(unsigned i=0;i<m->historySize;++i){const auto& f=m->history[(m->historyNext+ProfileLimits::Frames-m->historySize+i)%ProfileLimits::Frames];if(f.id==id)return f;}
    for(unsigned i=0;i<m->spikeSize;++i){const auto& f=m->spikes[(m->spikeNext+ProfileLimits::Spikes-m->spikeSize+i)%ProfileLimits::Spikes];if(f.id==id)return f;}return {};
}
ProfileNodeSummary PerformanceProfiler::NodeSummary(std::uint32_t node,std::uint64_t thread)const{
    ProfileNodeSummary result;result.frames=m->historySize;double ns=0;
    for(unsigned i=0;i<m->historySize;++i){const auto& f=m->history[(m->historyNext+ProfileLimits::Frames-m->historySize+i)%ProfileLimits::Frames];for(const auto& s:f.scopes)if(s.node==node&&s.thread==thread){ns+=double(s.end-s.start);++result.calls;}}
    if(result.frames)result.perFrameMilliseconds=ns/1e6/result.frames;
    if(result.calls)result.perCallMilliseconds=ns/1e6/result.calls;
    return result;
}
ProfileSummary PerformanceProfiler::Summary()const{
    ProfileSummary result;result.frames=m->historySize;if(!result.frames)return result;
    std::array<double,ProfileLimits::Frames> v{};
    for(unsigned i=0;i<m->historySize;++i){const auto& f=m->history[(m->historyNext+ProfileLimits::Frames-m->historySize+i)%ProfileLimits::Frames];v[i]=double(f.end-f.start)/1e6;result.mean+=v[i];}
    result.latest=v[m->historySize-1];result.mean/=result.frames;std::sort(v.begin(),v.begin()+m->historySize);auto n=m->historySize;
    result.median=n%2?v[n/2]:(v[n/2-1]+v[n/2])*.5;result.p95=v[size_t((n-1)*.95)];result.maximum=v[n-1];return result;
}
bool PerformanceProfiler::ScriptAttribution()const{return m->scripts.load();}
void PerformanceProfiler::ConfigureEnvironment(std::string_view mode){m->mode=mode;const char* enabled=std::getenv("JUDAS_PROFILE");Enable(enabled&&std::strcmp(enabled,"0"));Freeze(false);const char* scripts=std::getenv("JUDAS_PROFILE_SCRIPTS");m->scripts=!(scripts&&!std::strcmp(scripts,"0"));if(auto spike=std::getenv("JUDAS_PROFILE_SPIKE_MS")){double ms=std::strtod(spike,nullptr);if(std::isfinite(ms)&&ms>0)m->spikeThreshold=std::uint64_t(ms*1e6);}}
ProfileScope::ProfileScope(ProfileLabel& l,bool wait,std::uint64_t stamp){auto& p=PerformanceProfiler::Get();if(p.Active())token=p.Begin(p.Label(l),wait,stamp);}
ProfileScope::ProfileScope(std::uint32_t l,bool wait,std::uint64_t stamp){token=PerformanceProfiler::Get().Begin(l,wait,stamp);}
void ProfileScope::End(std::uint64_t stamp){if(token.sequence){PerformanceProfiler::Get().End(token,stamp);token={};}}
namespace {ProfileLabel fixedLabel("Fixed simulation step");}
ProfileFixedStep::ProfileFixedStep(double seconds):dt(seconds),scope(fixedLabel){if(PerformanceProfiler::Get().Active())start=PerformanceProfiler::Now();}
ProfileFixedStep::~ProfileFixedStep(){if(start)PerformanceProfiler::Get().FixedStep(start,PerformanceProfiler::Now(),dt);}
ProfileRun::~ProfileRun(){if(const char* path=std::getenv("JUDAS_PROFILE_OUTPUT")){std::string error;if(!PerformanceProfiler::Get().Export(path,error))std::fprintf(stderr,"Profiler report: %s\n",error.c_str());}}
bool PerformanceProfiler::Export(const std::string& path,std::string& error)const{
    std::ofstream out(path);if(!out){error="cannot write profiler snapshot: "+path;return false;}
    out<<std::setprecision(17);
    auto diagnostics=[&](const ProfileDiagnostics& d){out<<"{\"dropped_events\":"<<d.droppedEvents<<",\"exhausted_labels\":"<<d.exhaustedLabels<<",\"exhausted_nodes\":"<<d.exhaustedNodes<<",\"exhausted_threads\":"<<d.exhaustedThreads<<",\"incomplete_scopes\":"<<d.incompleteScopes<<",\"open_scopes\":"<<d.openScopes<<",\"truncated_records\":"<<d.truncatedFrames<<",\"gpu_dropped\":"<<d.gpuDropped<<",\"nested_frames\":"<<d.nestedFrames<<'}';};
    auto frame=[&](const ProfileFrameSnapshot& f){out<<"{\"id\":"<<f.id<<",\"world_boundary_id\":"<<f.world<<",\"mode\":"<<quote(f.mode)<<",\"startup\":"<<(f.startup?"true":"false")<<",\"capture_boundary\":"<<(f.incomplete?"true":"false")<<",\"start_ns\":"<<f.start<<",\"end_ns\":"<<f.end<<",\"interval_ns\":"<<f.interval<<",\"collection_ns\":"<<f.collectionNanoseconds<<",\"main_thread\":"<<f.mainThread<<",\"covered_ns\":"<<f.coverage<<",\"waiting_ns\":"<<f.waiting<<",\"unattributed_ns\":"<<f.unattributed<<",\"fixed\":{\"accumulator_seconds\":"<<f.accumulatorSeconds<<",\"discarded_seconds\":"<<f.discardedSeconds<<",\"cap_reached\":"<<(f.capReached?"true":"false")<<",\"paused\":"<<(f.paused?"true":"false")<<",\"steps\":[";
        bool comma=false;for(auto& s:f.fixed){if(comma)out<<',';comma=true;out<<"{\"id\":"<<s.id<<",\"start_ns\":"<<s.start<<",\"end_ns\":"<<s.end<<",\"simulation_seconds\":"<<s.simulationSeconds<<'}';}out<<"]},\"threads\":[";comma=false;for(auto& t:f.threads){if(comma)out<<',';comma=true;out<<"{\"id\":"<<t.id<<",\"name\":"<<quote(t.name)<<'}';}out<<"],\"scopes\":[";comma=false;
        for(auto& s:f.scopes){if(comma)out<<',';comma=true;out<<"{\"node\":"<<s.node<<",\"parent\":"<<s.parent<<",\"depth\":"<<s.depth<<",\"thread\":"<<s.thread<<",\"origin_frame\":"<<s.originFrame<<",\"name\":"<<quote(s.name)<<",\"start_ns\":"<<s.start<<",\"end_ns\":"<<s.end<<",\"inclusive_ns\":"<<(s.end-s.start)<<",\"exclusive_ns\":"<<s.exclusive<<",\"wait\":"<<(s.wait?"true":"false")<<'}';}
        out<<"],\"counters\":[";comma=false;for(auto& c:f.counters){if(comma)out<<',';comma=true;out<<"{\"name\":"<<quote(c.name)<<",\"value\":"<<c.value<<",\"mode\":"<<quote(c.mode==ProfileCounterMode::Sum?"frame_sum":c.mode==ProfileCounterMode::Maximum?"frame_max":"frame_latest_observation")<<'}';}
        out<<"],\"gpu\":{\"available\":"<<(f.gpuAvailable?"true":"false")<<",\"status\":"<<quote(f.gpuStatus)<<",\"passes\":[";comma=false;for(auto& g:f.gpu){if(comma)out<<',';comma=true;out<<"{\"token\":"<<g.token<<",\"source_frame\":"<<g.frame<<",\"camera\":"<<g.camera<<",\"pass\":"<<quote(g.pass)<<",\"pending\":"<<(g.pending?"true":"false")<<",\"milliseconds\":";if(g.pending)out<<"null";else out<<g.milliseconds;out<<'}';}
        out<<"]},\"memory\":{\"profiler_reserved_estimate_bytes\":"<<f.profilerReservedBytes<<",\"linux_resident_bytes\":"<<f.processResidentBytes<<",\"linux_virtual_bytes\":"<<f.processVirtualBytes<<"},\"boundaries\":[";comma=false;for(auto& b:f.boundaries){if(comma)out<<',';comma=true;out<<quote(b);}out<<"],\"diagnostics\":";diagnostics(f.diagnostics);out<<'}';};
    out<<"{\"schema\":1,\"clock\":\"steady monotonic nanoseconds\",\"scope_units\":\"elapsed ns on named thread; workers and nested scopes overlap\",\"gpu_units\":\"ms from delayed GL timestamps, inclusive pass intervals\",\"capture_enabled\":"<<(Enabled()?"true":"false")<<",\"frozen\":"<<(Frozen()?"true":"false")<<",\"waits_included_in_outer_work\":true,\"limits\":{\"threads\":"<<ProfileLimits::Threads<<",\"labels\":"<<ProfileLimits::Labels<<",\"nodes\":"<<ProfileLimits::Nodes<<",\"events_per_lane\":"<<ProfileLimits::LaneEvents<<",\"events_per_frame\":"<<ProfileLimits::FrameEvents<<",\"history_frames\":"<<ProfileLimits::Frames<<",\"spikes\":"<<ProfileLimits::Spikes<<"},\"diagnostics\":";diagnostics(Diagnostics());out<<",\"startup\":";if(m->startup.id)frame(m->startup);else out<<"null";
    out<<",\"frames\":[";bool comma=false;for(auto& f:History()){if(comma)out<<',';comma=true;frame(f);}out<<"],\"spikes\":[";comma=false;for(auto& f:Spikes()){if(comma)out<<',';comma=true;frame(f);}out<<"]}\n";
    if(!out){error="failed writing profiler snapshot: "+path;return false;}error.clear();return true;
}
