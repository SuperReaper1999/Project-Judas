#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Process-owned diagnostics. No world pointers or simulation inputs. Recording
// uses fixed per-thread SPSC lanes; only completed snapshots reach consumers.
struct ProfileLabel {
    const char* name;
    std::atomic<std::uint32_t> id{0};
    explicit ProfileLabel(const char* value):name(value){}
};
enum class ProfileCounterMode : unsigned { Sum, Latest, Maximum };
struct ProfileName {
    char text[128]{};
    ProfileName()=default;
    ProfileName(std::string_view value){Assign(value);}
    ProfileName(const char* value){Assign(value);}
    void Assign(std::string_view value){auto n=std::min(value.size(),sizeof(text)-1);std::copy_n(value.data(),n,text);text[n]=0;}
    ProfileName& operator=(std::string_view value){Assign(value);return *this;}
    const char* c_str()const{return text;}
    operator std::string_view()const{return text;}
    bool operator==(std::string_view other)const{return std::string_view(text)==other;}
};
struct ProfileScopeRecord {
    std::uint32_t node=0,parent=0,depth=0;
    std::uint64_t thread=0,originFrame=0,start=0,end=0,exclusive=0;
    bool wait=false;
    ProfileName name;
};
struct ProfileCounterRecord {ProfileName name;double value=0;std::uint64_t observed=0;ProfileCounterMode mode=ProfileCounterMode::Latest;};
struct ProfileThreadRecord {std::uint64_t id=0;ProfileName name;};
struct ProfileFixedRecord {std::uint64_t id=0,start=0,end=0;double simulationSeconds=0;};
struct ProfileGPURecord {std::uint64_t token=0,frame=0,camera=0;ProfileName pass;bool pending=true;double milliseconds=0;};
struct ProfileLimits {
    static constexpr unsigned Threads=32,Labels=2048,Nodes=4096,Depth=64,LaneEvents=4096;
    static constexpr unsigned Frames=120,FrameEvents=2048,Counters=256,Steps=64,GPU=64,Spikes=8;
};
struct ProfileDiagnostics {
    std::uint64_t droppedEvents=0,exhaustedLabels=0,exhaustedNodes=0,exhaustedThreads=0;
    std::uint64_t incompleteScopes=0,truncatedFrames=0,gpuDropped=0,nestedFrames=0;
    unsigned openScopes=0;
};
struct ProfileFrameSnapshot {
    std::uint64_t id=0,world=0,start=0,end=0,interval=0,mainThread=0;
    bool startup=false,paused=false,capReached=false,gpuAvailable=false,incomplete=false;
    ProfileName mode,gpuStatus;
    double accumulatorSeconds=0,discardedSeconds=0;
    std::uint64_t coverage=0,waiting=0,unattributed=0,collectionNanoseconds=0,processResidentBytes=0,processVirtualBytes=0;
    std::size_t profilerReservedBytes=0;
    std::vector<ProfileScopeRecord> scopes;
    std::vector<ProfileCounterRecord> counters;
    std::vector<ProfileThreadRecord> threads;
    std::vector<ProfileFixedRecord> fixed;
    std::vector<ProfileGPURecord> gpu;
    std::vector<ProfileName> boundaries;
    ProfileDiagnostics diagnostics;
};
struct ProfileFrameIndex {std::uint64_t id=0;double milliseconds=0;unsigned fixedSteps=0;bool startup=false;};
struct ProfileNodeSummary {std::size_t frames=0,calls=0;double perFrameMilliseconds=0,perCallMilliseconds=0;};
struct ProfileSummary {std::size_t frames=0;double latest=0,mean=0,median=0,p95=0,maximum=0;};
class PerformanceProfiler {
public:
    static PerformanceProfiler& Get();
    static std::uint64_t Now(); // steady-clock nanoseconds, common to all lanes
    bool Active()const;
    bool Enabled()const;
    bool Frozen()const;
    void Enable(bool);
    void Freeze(bool);
    void Clear(); // invalidates open recording epochs; leaves bounded intern IDs stable
    void RegisterThread(std::string_view);
    void ReleaseThread(); // caller must own the lane; jobs call this before exiting
    std::uint32_t Intern(std::string_view);
    std::uint32_t Label(ProfileLabel&);
    struct Token {unsigned lane=0,depth=0;std::uint64_t sequence=0;};
    Token Begin(std::uint32_t label,bool wait=false,std::uint64_t stamp=0);
    void End(Token,std::uint64_t stamp=0);
    void Counter(std::uint32_t,double,ProfileCounterMode);
    void Counter(ProfileLabel&,double,ProfileCounterMode=ProfileCounterMode::Latest);
    bool BeginFrame(std::string_view mode,bool startup=false,std::uint64_t stamp=0);
    void EndFrame(std::uint64_t stamp=0);
    std::uint64_t FrameId()const;
    void FixedState(double accumulator,bool capped,double discarded,bool paused);
    void FixedStep(std::uint64_t start,std::uint64_t end,double dt);
    void Boundary(std::string_view);
    void GPUAvailability(bool,std::string_view reason);
    std::uint64_t GPUPending(std::string_view pass,std::uint64_t camera);
    void GPUComplete(std::uint64_t frame,std::uint64_t token,double milliseconds);
    void DropGPU();
    std::vector<ProfileFrameSnapshot> History()const;
    std::vector<ProfileFrameSnapshot> Spikes()const;
    ProfileFrameSnapshot Startup()const;
    ProfileSummary Summary()const;
    std::vector<ProfileFrameIndex> Timeline()const;
    ProfileFrameSnapshot Snapshot(std::uint64_t id)const;
    ProfileNodeSummary NodeSummary(std::uint32_t node,std::uint64_t thread)const;
    ProfileDiagnostics Diagnostics()const;
    bool Export(const std::string& path,std::string& error)const;
    void ConfigureEnvironment(std::string_view mode);
    bool ScriptAttribution()const;
    std::size_t ReservedBytes()const;
private:
    PerformanceProfiler();
    ~PerformanceProfiler();
    struct Impl;std::unique_ptr<Impl> m;
};
class ProfileScope {
public:
    explicit ProfileScope(ProfileLabel& label,bool wait=false,std::uint64_t stamp=0);
    explicit ProfileScope(std::uint32_t label,bool wait=false,std::uint64_t stamp=0);
    ~ProfileScope(){End();}
    void End(std::uint64_t stamp=0);
    ProfileScope(const ProfileScope&)=delete;ProfileScope& operator=(const ProfileScope&)=delete;
private:PerformanceProfiler::Token token;
};
class ProfileFrame {
public:
    ProfileFrame(const char* mode,bool startup=false):owned(PerformanceProfiler::Get().BeginFrame(mode,startup)){}
    ~ProfileFrame(){End();}void End(){if(owned){owned=false;PerformanceProfiler::Get().EndFrame();}}
private:bool owned;
};
class ProfileFixedStep {
public:
    explicit ProfileFixedStep(double seconds);
    ~ProfileFixedStep();
private:std::uint64_t start=0;double dt;ProfileScope scope;
};
class ProfileRun {
public:
    explicit ProfileRun(const char* mode){PerformanceProfiler::Get().ConfigureEnvironment(mode);}
    ~ProfileRun();
};
#define JUDAS_PROFILE_JOIN_I(a,b) a##b
#define JUDAS_PROFILE_JOIN(a,b) JUDAS_PROFILE_JOIN_I(a,b)
#define JUDAS_PROFILE_SCOPE_IMPL(name,line,wait) static ProfileLabel JUDAS_PROFILE_JOIN(pl_,line)(name); ProfileScope JUDAS_PROFILE_JOIN(ps_,line)(JUDAS_PROFILE_JOIN(pl_,line),wait)
#define JUDAS_PROFILE_SCOPE(name) JUDAS_PROFILE_SCOPE_IMPL(name,__LINE__,false)
#define JUDAS_PROFILE_WAIT(name) JUDAS_PROFILE_SCOPE_IMPL(name,__LINE__,true)
#define JUDAS_PROFILE_COUNTER_IMPL(name,value,mode,line) do { static ProfileLabel JUDAS_PROFILE_JOIN(pc_,line)(name); PerformanceProfiler::Get().Counter(JUDAS_PROFILE_JOIN(pc_,line),value,mode); } while(false)
#define JUDAS_PROFILE_COUNTER(name,value,mode) JUDAS_PROFILE_COUNTER_IMPL(name,value,mode,__LINE__)
