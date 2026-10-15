from pathlib import Path
import re,hashlib
p=Path('/tmp/m87-narrow-base.cpp');s=p.read_text()
s=s.replace('#include "PerformanceProfiler.h"','#include "PerformanceProfiler.h"\n#include "JobSystem.h"\n#include <exception>',1)
s=s.replace('namespace {\n\nusing Clock','''// Temporary isolated prototype service injection. Not a production API.
static thread_local JobSystem* m87PrototypeJobs=nullptr;
static thread_local unsigned m87PrototypeWorkerLimit=4;
extern "C" void JudasM87PrototypePhysicsJobs(JobSystem* jobs,unsigned limit){m87PrototypeJobs=jobs;m87PrototypeWorkerLimit=limit;}
namespace {

using Clock''',1)
needle='''    void PrepareJoints(JointSolver& target,const std::vector<JointInput>& inputs,float dt,'''
insertion='''    // Indexed output only: workers never mutate physics bodies, geometry
    // caches, solver rows, touches, warm starts or event scheduling.
    struct InitialContactGeometry {
        unsigned a=0,b=0;float margin=0;
        const ContactPreparedOrientation* orientationA=nullptr;
        const ContactPreparedOrientation* orientationB=nullptr;
        ContactManifold manifold;bool touching=false;
    };
    struct InitialGeometryDiagnostics {ContactGeometryDiagnostics contacts;CollisionGeometryStats cooked;};
    std::vector<InitialContactGeometry> initialContactGeometry;
    ContactGeometryDiagnostics parallelContactDiagnostics;
    CollisionGeometryStats parallelCookedDiagnostics;
    static void AddContactDiagnostics(ContactGeometryDiagnostics& into,const ContactGeometryDiagnostics& after,const ContactGeometryDiagnostics& before={}){
#define ADD_DIAG(name) into.name+=after.name-before.name
        ADD_DIAG(predicates);ADD_DIAG(intervalResolved);ADD_DIAG(exactFallbacks);ADD_DIAG(unresolved);ADD_DIAG(invalidInputs);ADD_DIAG(numericGapFallbacks);ADD_DIAG(preparedOrientationMisses);
#undef ADD_DIAG
    }
    static void AddCookedDiagnostics(CollisionGeometryStats& into,const CollisionGeometryStats& after,const CollisionGeometryStats& before={}){
        into.nodes+=after.nodes-before.nodes;into.triangles+=after.triangles-before.triangles;into.convexTests+=after.convexTests-before.convexTests;
    }
    void PrepareInitialContactGeometry(float dt){
        initialContactGeometry.clear();JobSystem* jobs=m87PrototypeJobs;
        if(!jobs||jobs->IsShutDown()||m87PrototypeWorkerLimit==0||candidatePairs.size()<256)return;
        const auto eligible=[&](const std::pair<unsigned,unsigned>& pair){
            const auto& a=bodies[pair.first];const auto& b=bodies[pair.second];
            const auto simple=[](const Shape& shape){return !shape.asset&&(shape.type==ShapeType::Box||shape.type==ShapeType::Sphere);};
            return !a.sensor&&!b.sensor&&!a.queryOnly&&!b.queryOnly&&simple(a.shape)&&simple(b.shape)&&
                !((a.sleeping||a.rigidBody.IsStatic())&&(b.sleeping||b.rigidBody.IsStatic()));
        };
        const auto count=std::count_if(candidatePairs.begin(),candidatePairs.end(),eligible);
        if(count<256)return;
        const auto capacity=initialContactGeometry.capacity();initialContactGeometry.reserve(std::size_t(count));
        if(initialContactGeometry.capacity()!=capacity){++cacheCounters.allocations;cacheCounters.allocatedBytes+=initialContactGeometry.capacity()*sizeof(InitialContactGeometry);}
        for(const auto& pair:candidatePairs)if(eligible(pair)){
            auto& a=bodies[pair.first];auto& b=bodies[pair.second];
            const float margin=(glm::length(a.rigidBody.linearVelocity-b.rigidBody.linearVelocity)+
                glm::length(a.rigidBody.angularVelocity)*a.boundingRadius+
                glm::length(b.rigidBody.angularVelocity)*b.boundingRadius)*dt;
            InitialContactGeometry output;output.a=pair.first;output.b=pair.second;output.margin=margin;
            output.orientationA=&Orientation(a);output.orientationB=&Orientation(b);
            initialContactGeometry.push_back(std::move(output));
        }
        const unsigned lanes=unsigned(std::min({std::size_t(m87PrototypeWorkerLimit),std::size_t(jobs->WorkerCount())+1,initialContactGeometry.size()/64}));
        std::array<InitialGeometryDiagnostics,JobSystem::kMaxWorkers+1> diagnostics{};
        std::atomic<std::size_t> next{0};
        auto run=[&](unsigned lane){
            const auto contactsBefore=GetContactGeometryDiagnostics();const auto cookedBefore=GetCollisionGeometryStats();
            JUDAS_PROFILE_SCOPE("Physics initial geometry worker batch");
            try {
                for(;;){const auto first=next.fetch_add(8,std::memory_order_relaxed);if(first>=initialContactGeometry.size())break;
                    for(auto i=first;i<std::min(first+8,initialContactGeometry.size());++i){
                        auto& output=initialContactGeometry[i];const auto& a=bodies[output.a];const auto& b=bodies[output.b];
                        const auto childA=PrimitiveAt(a.shape,a.rigidBody,0,output.orientationA);
                        const auto childB=PrimitiveAt(b.shape,b.rigidBody,0,output.orientationB);
                        output.manifold=ComputeContacts(childA,childB,output.margin,output.orientationA,output.orientationB);
                        for(int p=0;p<output.manifold.count;++p){const auto& c=output.manifold.points[p];output.touching=output.touching||(c.hasLocalAnchors?c.signedSeparation<=0:c.penetration>=0);}
                        if(!output.touching&&output.manifold.count)output.touching=ComputeContacts(childA,childB,0,output.orientationA,output.orientationB).count>0;
                    }
                }
            }catch(...){AddContactDiagnostics(diagnostics[lane].contacts,GetContactGeometryDiagnostics(),contactsBefore);AddCookedDiagnostics(diagnostics[lane].cooked,GetCollisionGeometryStats(),cookedBefore);throw;}
            AddContactDiagnostics(diagnostics[lane].contacts,GetContactGeometryDiagnostics(),contactsBefore);AddCookedDiagnostics(diagnostics[lane].cooked,GetCollisionGeometryStats(),cookedBefore);
        };
        std::array<JobHandle,JobSystem::kMaxWorkers> handles{};unsigned handleCount=0;std::exception_ptr submitError;
        try{for(unsigned lane=1;lane<lanes;++lane){auto handle=jobs->Submit([&,lane](JobContext&){run(lane);},JobPriority::High,"Physics initial contact geometry");if(handle.IsValid())handles[handleCount++]=handle;}}
        catch(...){submitError=std::current_exception();}
        std::exception_ptr mainError;try{run(0);}catch(...){mainError=std::current_exception();}
        JobHandle failed;for(unsigned i=0;i<handleCount;++i)if(jobs->Wait(handles[i])!=JobState::Completed&&!failed.IsValid())failed=handles[i];
        std::string workerError=failed.IsValid()?jobs->ErrorOf(failed):std::string{};
        if(failed.IsValid()&&workerError.empty())workerError="initial geometry job did not complete";
        for(unsigned i=0;i<handleCount;++i)jobs->Forget(handles[i]);
        // Lane zero executes on the owner; its TLS counters are already included
        // in the native Step's before/after snapshots.
        for(unsigned lane=1;lane<lanes;++lane){AddContactDiagnostics(parallelContactDiagnostics,diagnostics[lane].contacts);AddCookedDiagnostics(parallelCookedDiagnostics,diagnostics[lane].cooked);}
        if(submitError)std::rethrow_exception(submitError);
        if(mainError)std::rethrow_exception(mainError);
        if(!workerError.empty())throw std::runtime_error(workerError);
        JUDAS_PROFILE_COUNTER("Physics initial geometry worker lanes",lanes,ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("Physics parallel initial manifolds",initialContactGeometry.size(),ProfileCounterMode::Latest);
    }
'''
assert s.count(needle)==1;s=s.replace(needle,insertion+needle,1)
# Initial geometry is queried only after integration and candidate filtering;
# row production and policy continue in the original sequential loop.
needle='''    w.cacheUsed.assign(w.contactCache.size(), 0);
    for (const auto& [slotA, slotB] : w.candidatePairs) {'''
replacement='''    w.cacheUsed.assign(w.contactCache.size(), 0);
    w.PrepareInitialContactGeometry(fixedDeltaTime);
    std::size_t initialGeometryIndex=0;
    for (const auto& [slotA, slotB] : w.candidatePairs) {'''
assert s.count(needle)==1;s=s.replace(needle,replacement,1)
needle='''        const float friction = std::sqrt(std::max(a.friction, 0.0f) * std::max(b.friction, 0.0f));'''
replacement='''        const Impl::InitialContactGeometry* prepared=nullptr;
        if(initialGeometryIndex<w.initialContactGeometry.size()){
            const auto& candidate=w.initialContactGeometry[initialGeometryIndex];
            if(candidate.a==slotA&&candidate.b==slotB){prepared=&candidate;++initialGeometryIndex;}
        }
'''+needle
assert s.count(needle)==1;s=s.replace(needle,replacement,1)
# Restrict changes to Step's initial narrowphase, not other callers.
a=s.index('    w.PrepareInitialContactGeometry(fixedDeltaTime);');b=s.index('    w.stats.contactPoints = w.lastStepContacts.size();',a)
q=s[a:b]
q=q.replace('''        const float margin =
            (''','''        const float margin = prepared?prepared->margin:
            (''',1)
q=q.replace('''const auto& orientationA = w.Orientation(a);''','''const auto& orientationA = prepared?*prepared->orientationA:w.Orientation(a);''',1).replace('''const auto& orientationB = w.Orientation(b);''','''const auto& orientationB = prepared?*prepared->orientationB:w.Orientation(b);''',1)
q=q.replace('''                    ComputeContacts(childA, childB, margin, &orientationA, &orientationB);''','''                    prepared?prepared->manifold:ComputeContacts(childA, childB, margin, &orientationA, &orientationB);''',1)
needle='''                bool touchingManifold=false;
                for (int p=0;p<manifold.count;++p) {
                    const auto& c=manifold.points[p];
                    touchingManifold=touchingManifold || (c.hasLocalAnchors ? c.signedSeparation<=0 : c.penetration>=0);
                }
                if(!touchingManifold && manifold.count)
                    touchingManifold=ComputeContacts(childA,childB,0,&orientationA,&orientationB).count>0;'''
replacement='''                bool touchingManifold=prepared?prepared->touching:false;
                if(!prepared){
                    for (int p=0;p<manifold.count;++p) {
                        const auto& c=manifold.points[p];
                        touchingManifold=touchingManifold || (c.hasLocalAnchors ? c.signedSeparation<=0 : c.penetration>=0);
                    }
                    if(!touchingManifold && manifold.count)
                        touchingManifold=ComputeContacts(childA,childB,0,&orientationA,&orientationB).count>0;
                }'''
assert needle in q;q=q.replace(needle,replacement,1)
s=s[:a]+q+s[b:]
s=s.replace('    w.stats = StepStats{};','    w.parallelContactDiagnostics={};w.parallelCookedDiagnostics={};\n    w.stats = StepStats{};',1)
for name,diag in [('geometryPredicates','predicates'),('geometryExactFallbacks','exactFallbacks'),('geometryUnresolved','unresolved'),('geometryNumericGapFallbacks','numericGapFallbacks')]:
 old=f'w.stats.{name} = geometryAfter.{diag} - geometryBefore.{diag};';new=f'w.stats.{name} = geometryAfter.{diag} - geometryBefore.{diag} + w.parallelContactDiagnostics.{diag};'
 assert old in s;s=s.replace(old,new,1)
for diag in ('nodes','triangles','convexTests'):
 old=f'collisionAfter.{diag}-collisionBefore.{diag}';assert old in s;s=s.replace(old,f'collisionAfter.{diag}-collisionBefore.{diag}+w.parallelCookedDiagnostics.{diag}')
# Retained output storage is part of reported geometry scratch bytes.
needle='''        return bytes+''';i=s.index(needle,s.index('    std::size_t CacheBytes() const'))
s=s[:i]+'''        bytes+=initialContactGeometry.capacity()*sizeof(InitialContactGeometry);
'''+s[i:]
Path('/tmp/m87-narrow-prototype.cpp').write_text(s)
Path('/tmp/m87-narrow-base.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  /tmp/m87-narrow-base.cpp\n')
print('prototype bytes',len(s));print('base',hashlib.sha256(p.read_bytes()).hexdigest());print('prototype',hashlib.sha256(s.encode()).hexdigest())
