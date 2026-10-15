from pathlib import Path
import re,hashlib
p=Path('/tmp/m87-parallel-base.cpp');s=p.read_text();rootHasPinned='impactAnchorSamples' in s
s=s.replace('#include "PerformanceProfiler.h"','#include "PerformanceProfiler.h"\n#include "JobSystem.h"\n#include <exception>',1)
s=s.replace('namespace {\n\nusing Clock', '''// Isolated prototype injection only. Production must inject this optional
// execution service through PhysicsWorld ownership, not an environment policy.
static thread_local JobSystem* m87PrototypeJobs=nullptr;
static thread_local unsigned m87PrototypeWorkerLimit=4;
extern "C" void JudasM87PrototypePhysicsJobs(JobSystem* jobs,unsigned limit){m87PrototypeJobs=jobs;m87PrototypeWorkerLimit=limit;}
namespace {

using Clock''',1)
needle='''    void PrepareJoints(JointSolver& target,const std::vector<JointInput>& inputs,float dt,'''
insertion='''    // Only query scratch is selected by TLS; authoritative world state remains
    // immutable for the duration of each joined batch. Storage belongs to Impl.
    struct ImpactQueryScratch {
        std::vector<ImpactPoseSample> samples,startSamples;
        std::vector<std::uint64_t> sampleEpochs,startEpochs;
        std::uint64_t epoch=0;double start=0;
        PhysicsWorld::StepStats stats;WorkDiagnostics work;
        std::size_t impactPoseEvaluations=0,impactPoseReuses=0,impactOrientationBuilds=0,
            impactTruthRequests=0,impactTruthReuses=0,impactBoundsRejected=0,impactActualBoundsRejected=0;
        std::size_t allocations=0,allocatedBytes=0;
        ContactGeometryDiagnostics contacts;
        CollisionGeometryStats cooked;
        void Begin(bool profiling,double now){
            ++epoch;if(!epoch){std::fill(sampleEpochs.begin(),sampleEpochs.end(),0);std::fill(startEpochs.begin(),startEpochs.end(),0);++epoch;}
            start=now;stats={};work={};work.active=profiling;
            impactPoseEvaluations=impactPoseReuses=impactOrientationBuilds=impactTruthRequests=impactTruthReuses=impactBoundsRejected=impactActualBoundsRejected=0;
            allocations=allocatedBytes=0;contacts={};cooked={};
        }
    };
    inline static thread_local Impl* impactQueryOwner=nullptr;
    inline static thread_local ImpactQueryScratch* impactQueryScratch=nullptr;
    std::vector<std::unique_ptr<ImpactQueryScratch>> impactQueryWorkers;
    ContactGeometryDiagnostics parallelContactDiagnostics;
    CollisionGeometryStats parallelCookedDiagnostics;
    ImpactQueryScratch* ActiveImpactQuery(){return impactQueryOwner==this?impactQueryScratch:nullptr;}
    WorkDiagnostics& QueryWork(){auto* query=ActiveImpactQuery();return query?query->work:work;}
    PhysicsWorld::StepStats& QueryStats(){auto* query=ActiveImpactQuery();return query?query->stats:stats;}
    std::size_t& QueryCounter(std::size_t& original,std::size_t ImpactQueryScratch::*field){auto* query=ActiveImpactQuery();return query?query->*field:original;}
    ImpactPoseSample& QuerySample(unsigned slot,double time,bool anchorAllowed=true){
        if(auto* query=ActiveImpactQuery()){
            // A common-start sample is pinned separately: queries at future
            // bracket times cannot evict the event-start pose shared by pairs.
            auto& samples=anchorAllowed&&time==query->start?query->startSamples:query->samples;
            auto& epochs=anchorAllowed&&time==query->start?query->startEpochs:query->sampleEpochs;
            if(samples.size()<bodies.size()){
                const auto capacity=samples.capacity(),epochCapacity=epochs.capacity();samples.resize(bodies.size());epochs.resize(bodies.size());
                if(samples.capacity()!=capacity){++query->allocations;query->allocatedBytes+=(samples.capacity()-capacity)*sizeof(ImpactPoseSample);}
                if(epochs.capacity()!=epochCapacity){++query->allocations;query->allocatedBytes+=(epochs.capacity()-epochCapacity)*sizeof(std::uint64_t);}
            }
            auto& sample=samples[slot];
            if(epochs[slot]!=query->epoch){sample.time=std::numeric_limits<double>::quiet_NaN();sample.boundStart=sample.time;sample.tightValid=false;epochs[slot]=query->epoch;}
            return sample;
        }
        EnsureImpactPoseStorage();return OWNER_SAMPLE_FALLBACK;
    }
    static void AddContactDiagnostics(ContactGeometryDiagnostics& into,const ContactGeometryDiagnostics& after,const ContactGeometryDiagnostics& before={}){
#define ADD_DIAG(name) into.name+=after.name-before.name
        ADD_DIAG(predicates);ADD_DIAG(intervalResolved);ADD_DIAG(exactFallbacks);ADD_DIAG(unresolved);ADD_DIAG(invalidInputs);ADD_DIAG(numericGapFallbacks);ADD_DIAG(preparedOrientationMisses);
#undef ADD_DIAG
    }
    static void AddCookedDiagnostics(CollisionGeometryStats& into,const CollisionGeometryStats& after,const CollisionGeometryStats& before={}){
        into.nodes+=after.nodes-before.nodes;into.triangles+=after.triangles-before.triangles;into.convexTests+=after.convexTests-before.convexTests;
    }
    void MergeImpactQuery(ImpactQueryScratch& query){
#define ADD_QUERY(name) name+=query.name
        ADD_QUERY(impactPoseEvaluations);ADD_QUERY(impactPoseReuses);ADD_QUERY(impactOrientationBuilds);ADD_QUERY(impactTruthRequests);ADD_QUERY(impactTruthReuses);ADD_QUERY(impactBoundsRejected);ADD_QUERY(impactActualBoundsRejected);
#undef ADD_QUERY
#define ADD_STATS(name) stats.name+=query.stats.name
        ADD_STATS(impactQueries);ADD_STATS(impactSearchIterations);ADD_STATS(impactSearchLimit);ADD_STATS(impactUncertifiedAdvances);ADD_STATS(impactSearchStalls);ADD_STATS(impactSamplingFallbacks);ADD_STATS(impactSamplingTests);ADD_STATS(impactSamplingResolutionCaps);
#undef ADD_STATS
        stats.impactPeakIterations=std::max(stats.impactPeakIterations,query.stats.impactPeakIterations);
        stats.impactSamplingMaxInterval=std::max(stats.impactSamplingMaxInterval,query.stats.impactSamplingMaxInterval);
        for(auto pair:{std::make_pair(&work.toi,&query.work.toi),std::make_pair(&work.geometry,&query.work.geometry)}){pair.first->nanoseconds+=pair.second->nanoseconds;pair.first->calls+=pair.second->calls;}
        cacheCounters.allocations+=query.allocations;cacheCounters.allocatedBytes+=query.allocatedBytes;
        AddContactDiagnostics(parallelContactDiagnostics,query.contacts);AddCookedDiagnostics(parallelCookedDiagnostics,query.cooked);
    }
    template<class Compute>
    bool ParallelImpactQueries(std::size_t count,double now,Compute&& compute){
        JobSystem* jobs=m87PrototypeJobs;
        if(!jobs||jobs->IsShutDown()||count<256||m87PrototypeWorkerLimit==0)return false;
        const unsigned lanes=unsigned(std::min({std::size_t(m87PrototypeWorkerLimit),std::size_t(jobs->WorkerCount())+1,count/64}));
        if(lanes==0)return false;
        while(impactQueryWorkers.size()<lanes)impactQueryWorkers.push_back(std::make_unique<ImpactQueryScratch>());
        for(unsigned lane=0;lane<lanes;++lane)impactQueryWorkers[lane]->Begin(work.active,now);
        std::atomic<std::size_t> next{0};
        auto run=[&](unsigned lane){
            auto& query=*impactQueryWorkers[lane];
            struct RestoreTLS {Impl* owner;ImpactQueryScratch* scratch;~RestoreTLS(){impactQueryOwner=owner;impactQueryScratch=scratch;}} restore{impactQueryOwner,impactQueryScratch};
            impactQueryOwner=this;impactQueryScratch=&query;
            const auto contactsBefore=GetContactGeometryDiagnostics();const auto cookedBefore=GetCollisionGeometryStats();
            JUDAS_PROFILE_SCOPE("Physics CCD query worker batch");
            try {
                for(;;){const auto first=next.fetch_add(8,std::memory_order_relaxed);if(first>=count)break;for(auto i=first;i<std::min(first+8,count);++i)compute(i);}
            } catch(...) {
                AddContactDiagnostics(query.contacts,GetContactGeometryDiagnostics(),contactsBefore);AddCookedDiagnostics(query.cooked,GetCollisionGeometryStats(),cookedBefore);throw;
            }
            AddContactDiagnostics(query.contacts,GetContactGeometryDiagnostics(),contactsBefore);AddCookedDiagnostics(query.cooked,GetCollisionGeometryStats(),cookedBefore);
        };
        std::array<JobHandle,JobSystem::kMaxWorkers> handles{};unsigned handleCount=0;
        std::exception_ptr submitError;
        try{for(unsigned lane=1;lane<lanes;++lane){auto handle=jobs->Submit([&,lane](JobContext&){run(lane);},JobPriority::High,"Physics CCD query batch");if(handle.IsValid())handles[handleCount++]=handle;}}
        catch(...){submitError=std::current_exception();}
        std::exception_ptr mainError;try{run(0);}catch(...){mainError=std::current_exception();}
        JobHandle failed;
        for(unsigned i=0;i<handleCount;++i){const auto handle=handles[i];if(jobs->Wait(handle)!=JobState::Completed&&!failed.IsValid())failed=handle;}
        // All jobs have joined before allocating/copying error text or throwing.
        std::string workerError=failed.IsValid()?jobs->ErrorOf(failed):std::string{};
        if(failed.IsValid()&&workerError.empty())workerError="physics query job did not complete";
        for(unsigned i=0;i<handleCount;++i)jobs->Forget(handles[i]);
        // The owner-thread lane already contributed to its TLS diagnostics.
        // Merge worker TLS diagnostics only once into final StepStats below.
        impactQueryWorkers[0]->contacts={};impactQueryWorkers[0]->cooked={};
        for(unsigned lane=0;lane<lanes;++lane)MergeImpactQuery(*impactQueryWorkers[lane]);
        if(submitError)std::rethrow_exception(submitError);
        if(mainError)std::rethrow_exception(mainError);
        if(!workerError.empty())throw std::runtime_error(workerError);
        return true;
    }
'''
insertion=insertion.replace('OWNER_SAMPLE_FALLBACK', 'anchorAllowed&&time==impactQueryAnchorTime?impactAnchorSamples[slot]:impactPoseSamples[slot]' if rootHasPinned else 'impactPoseSamples[slot]')
assert s.count(needle)==1;s=s.replace(needle,insertion+needle)
# Replace only the geometry-query cluster, leaving authoritative solver and
# publication code untouched. Each field writes to selected query scratch.
start=s.index('    const ImpactPoseSample& ImpactPoseAt(');end=s.index('    void ResolveInitialImpacts(',start)
q=s[start:end]
q=re.sub(r'        EnsureImpactPoseStorage\(\);\n(?:        //[^\n]*\n)*        auto& sample=(?:time==impactQueryAnchorTime\?impactAnchorSamples\[slot\]:)?impactPoseSamples\[slot\];','        auto& sample=QuerySample(slot,time);',q,count=1)
q=q.replace('''        EnsureImpactPoseStorage();
        auto& sample=impactPoseSamples[slot];const auto& body=bodies[slot];''','''        auto& sample=QuerySample(slot,start,false);const auto& body=bodies[slot];''',1)
q=q.replace('''        if(!body.HasMotion())sample.remainingBound=CurrentBound(bodies[slot]);''','''        if(!body.HasMotion())sample.remainingBound=ActiveImpactQuery()?ShapeAabb(body.shape,body.rigidBody.position,body.rigidBody.orientation):CurrentBound(bodies[slot]);''',1)
q=q.replace('''        auto& sample=impactPoseSamples[slot];''','''        auto& sample=QuerySample(slot,pose.time,false);''',1)
for name in ('impactPoseEvaluations','impactPoseReuses','impactOrientationBuilds','impactTruthRequests','impactTruthReuses','impactBoundsRejected','impactActualBoundsRejected'):
 q=re.sub(r'\b'+name+r'\b',f'QueryCounter({name},&ImpactQueryScratch::{name})',q)
q=q.replace('work.geometry','QueryWork().geometry').replace('work.toi','QueryWork().toi').replace('work.active','QueryWork().active')
q=re.sub(r'\bstats\.', 'QueryStats().',q)
s=s[:start]+q+s[end:]
# Replace schedule's pure query with parallel indexed output then stable owner
# commit. Serial branch retains precisely the original schedule call order.
a=s.index('        auto schedule=[&](const std::array<unsigned,4>& key) {',s.index('    void AdvanceImpacts('))
b=s.index('        for (;;) {',a)
old=s[a:b]
assert 'for(const auto& key:spinningPairs) schedule(key);' in old
new='''        auto queryTime=[&](const std::array<unsigned,4>& key) {
            return std::binary_search(supportPairs.begin(),supportPairs.end(),key) ? double(dt)+1 :
                std::binary_search(spinningPairs.begin(),spinningPairs.end(),key) ?
                ImpactTime(key[0],key[1],int(key[2]),int(key[3]),now,dt,SpinPenetrationTarget(key)) :
                ImpactTime(key[0],key[1],int(key[2]),int(key[3]),now,dt);
        };
        auto publishTime=[&](const std::array<unsigned,4>& key,double time) {
            const auto found=scheduledIndex.find(key);
            if(found!=scheduledIndex.end())scheduledImpacts[found->second].time=time;
            else {scheduledIndex.emplace(key,scheduledImpacts.size());scheduledImpacts.push_back({key,time});}
        };
        std::vector<std::array<unsigned,4>> requests;std::vector<double> times;
        auto scheduleBatch=[&](const std::vector<std::array<unsigned,4>>& keys){
            requests.clear();requests.reserve(keys.size());
            for(const auto& key:keys)if(CanRespond(key[0],key[1]))requests.push_back(key);
            times.resize(requests.size());
            const bool parallel=ParallelImpactQueries(requests.size(),now,[&](std::size_t i){times[i]=queryTime(requests[i]);});
            if(!parallel)for(std::size_t i=0;i<requests.size();++i)times[i]=queryTime(requests[i]);
            for(std::size_t i=0;i<requests.size();++i)publishTime(requests[i],times[i]);
        };
        std::vector<std::array<unsigned,4>> initialSchedule=separatedPairs;
        initialSchedule.insert(initialSchedule.end(),spinningPairs.begin(),spinningPairs.end());
        scheduleBatch(initialSchedule);
'''
s=s[:a]+new+s[b:]
assert s.count('            for(const auto& key:dirty) schedule(key);')==1
s=s.replace('            for(const auto& key:dirty) schedule(key);','            scheduleBatch(dirty);',1)
# Reset worker diagnostic totals for each native step before owner snapshots.
s=s.replace('    w.stats = StepStats{};','    w.parallelContactDiagnostics={};w.parallelCookedDiagnostics={};\n    w.stats = StepStats{};',1)
# Final counters explicitly include off-owner-thread work, not global TLS resets.
for name,diag in [('geometryPredicates','predicates'),('geometryExactFallbacks','exactFallbacks'),('geometryUnresolved','unresolved'),('geometryNumericGapFallbacks','numericGapFallbacks')]:
 old=f'w.stats.{name} = geometryAfter.{diag} - geometryBefore.{diag};';new=f'w.stats.{name} = geometryAfter.{diag} - geometryBefore.{diag} + w.parallelContactDiagnostics.{diag};'
 assert old in s;s=s.replace(old,new,1)
for diag in ('nodes','triangles','convexTests'):
 old=f'collisionAfter.{diag}-collisionBefore.{diag}'
 if old not in s: print('missing cooked compact',diag)
 s=s.replace(old,f'collisionAfter.{diag}-collisionBefore.{diag}+w.parallelCookedDiagnostics.{diag}')
old='return bytes+(impactPoseSamples.capacity()+impactAnchorSamples.capacity())*sizeof(ImpactPoseSample)+solver.FrameStorageBytes();' if rootHasPinned else 'return bytes+impactPoseSamples.capacity()*sizeof(ImpactPoseSample)+solver.FrameStorageBytes();'
assert old in s
s=s.replace(old,"for(const auto& query:impactQueryWorkers)bytes+=sizeof(ImpactQueryScratch)+(query->samples.capacity()+query->startSamples.capacity())*sizeof(ImpactPoseSample)+(query->sampleEpochs.capacity()+query->startEpochs.capacity())*sizeof(std::uint64_t);\n        "+old,1)
Path('/tmp/m87-parallel-prototype.cpp').write_text(s)
Path('/tmp/m87-parallel-base.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  /tmp/m87-parallel-base.cpp\n')
print('prototype bytes',len(s));print('base',hashlib.sha256(p.read_bytes()).hexdigest());print('prototype',hashlib.sha256(s.encode()).hexdigest())
