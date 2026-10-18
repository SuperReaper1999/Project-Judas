from pathlib import Path
import hashlib,json,datetime
r=Path.cwd();o=r/'.cache/prealpha-pass5/dense/cost-probe'
source=r/'src/Contacts.cpp';s=source.read_text();s='#include "DenseContactCost.h"\n'+s
for statement in ['Context reverse(pb,pa,orientationB,orientationA);','Context c(pa,pb,orientationA,orientationB);']:
 assert s.count(statement)==2
# Time only construction, retaining the original scoped Context object.
# A tiny start marker followed by an immediate post-construction accumulation
# avoids attributing SAT/manifold work to construction.
for statement in ['Context reverse(pb,pa,orientationB,orientationA);','Context c(pa,pb,orientationA,orientationB);']:
 s=s.replace(statement,'const auto denseContextStart = DenseContactCost::active ? DenseContactCost::Clock::now() : DenseContactCost::Clock::time_point{};\n            '+statement+'\n            if (DenseContactCost::active) { ++DenseContactCost::active->calls[DenseContactCost::Context]; DenseContactCost::active->nanoseconds[DenseContactCost::Context] += std::chrono::duration_cast<std::chrono::nanoseconds>(DenseContactCost::Clock::now()-denseContextStart).count(); }')
a='SatResult BoxSat(Context& c,const glm::vec3& ha,const glm::vec3& hb,float margin) {';assert s.count(a)==1;s=s.replace(a,a+'\n        DenseContactCost::Timer denseSatTimer(DenseContactCost::Sat);')
a='for (int axis=0;axis<15;++axis){';assert s.count(a)==1;s=s.replace(a,a+'\n            DenseContactCost::Count(&DenseContactCost::Bucket::axesAttempted);')
a='const Iv s=workspace.Evaluate(ha,hb,axis,ai);';assert s.count(a)==1;s=s.replace(a,'const auto denseRowsBefore = workspace.preparedRows;\n            '+a+'\n            DenseContactCost::Count(&DenseContactCost::Bucket::axesEvaluated);\n            DenseContactCost::Count(&DenseContactCost::Bucket::matrixRows, unsigned(__builtin_popcount(workspace.preparedRows ^ denseRowsBefore)));')
a='if (!sat.hit)return manifold;';assert s.count(a)==1;s=s.replace(a,a+'\n        DenseContactCost::Timer denseWitnessTimer(DenseContactCost::Witness);')
a='if (sat.axis>=6){';assert s.count(a)==1;s=s.replace(a,a+'\n            DenseContactCost::Count(&DenseContactCost::Bucket::edgeWitness);')
a='const bool referenceA=sat.axis<3;';assert s.count(a)==1;s=s.replace(a,'DenseContactCost::Count(&DenseContactCost::Bucket::faceWitness);\n        '+a)
a='auto clip=[&](int k,double sign,double limit){';assert s.count(a)==1;s=s.replace(a,a+'\n            DenseContactCost::Count(&DenseContactCost::Bucket::clipCalls);')
a='for (int signs=0;signs<8;++signs){';assert s.count(a)==1;s=s.replace(a,a+'\n                DenseContactCost::Count(&DenseContactCost::Bucket::fallbackCorners);')
(o/'Contacts.cpp').write_text(s)
s=(r/'src/physics/WorldImpactEvents.cpp').read_text();s='#include "DenseContactCost.h"\n#include <cstdio>\n#include <cstdlib>\n#include <map>\n'+s
marker='auto PhysicsWorld::Impl::AdvanceImpacts(float dt, bool coupledVelocityChanged) -> void {'
assert s.count(marker)==1
insert=r'''
    struct DenseObservation {
        struct Sample { std::array<float,14> pose; int mode; };
        std::array<DenseContactCost::Bucket,2> bucket;
        std::map<std::array<unsigned,4>,Sample> previous;
        std::uint64_t events=0;
        void Observe(int mode, const std::array<unsigned,4>& key,
                     const RigidBody& a,const RigidBody& b, const ContactManifold& result) {
            auto& into=bucket[mode];
            const std::array<float,14> pose{a.position.x,a.position.y,a.position.z,a.orientation.w,a.orientation.x,a.orientation.y,a.orientation.z,b.position.x,b.position.y,b.position.z,b.orientation.w,b.orientation.x,b.orientation.y,b.orientation.z};
            const auto found=previous.find(key);
            if(found!=previous.end()&&std::memcmp(found->second.pose.data(),pose.data(),sizeof(pose))==0) {
                ++into.equalPose; into.equalPoseOtherMode+=found->second.mode!=mode;
            }
            previous[key]={pose,mode};
            ++into.queries;into.hits+=result.count!=0;into.misses+=result.count==0;into.uncertain+=result.uncertain;
            if(result.count) { into.canonicalHits+=key[0]<key[1];into.reversedHits+=key[0]>key[1]; }
        }
        ~DenseObservation() {
            const char* path=std::getenv("JUDAS_PASS5_CONTACT_COST_OBSERVER");if(!path)return;
            FILE* f=std::fopen(path,"a");if(!f)return;
            std::fprintf(f,"{\"events\":%llu,\"categories\":[",(unsigned long long)events);
            for(unsigned i=0;i<bucket.size();++i) {
                const auto& b=bucket[i];
                std::fprintf(f,"%s{\"mode\":\"%s\",\"queries\":%llu,\"aabbRejected\":%llu,\"hits\":%llu,\"misses\":%llu,\"uncertain\":%llu,\"axesAttempted\":%llu,\"axesEvaluated\":%llu,\"matrixRows\":%llu,\"faceWitness\":%llu,\"edgeWitness\":%llu,\"clipCalls\":%llu,\"fallbackCorners\":%llu,\"predicates\":%llu,\"exactFallbacks\":%llu,\"numericFallbacks\":%llu,\"canonicalHits\":%llu,\"reversedHits\":%llu,\"equalPose\":%llu,\"equalPoseOtherMode\":%llu,\"timesNs\":[",
                    i?",":"",i?"response":"discovery",(unsigned long long)b.queries,(unsigned long long)b.aabbRejected,(unsigned long long)b.hits,(unsigned long long)b.misses,(unsigned long long)b.uncertain,(unsigned long long)b.axesAttempted,(unsigned long long)b.axesEvaluated,(unsigned long long)b.matrixRows,(unsigned long long)b.faceWitness,(unsigned long long)b.edgeWitness,(unsigned long long)b.clipCalls,(unsigned long long)b.fallbackCorners,(unsigned long long)b.predicates,(unsigned long long)b.exactFallbacks,(unsigned long long)b.numericFallbacks,(unsigned long long)b.canonicalHits,(unsigned long long)b.reversedHits,(unsigned long long)b.equalPose,(unsigned long long)b.equalPoseOtherMode);
                for(unsigned k=0;k<b.nanoseconds.size();++k)std::fprintf(f,"%s%llu",k?",":"",(unsigned long long)b.nanoseconds[k]);
                std::fprintf(f,"],\"timerCalls\":[");for(unsigned k=0;k<b.calls.size();++k)std::fprintf(f,"%s%llu",k?",":"",(unsigned long long)b.calls[k]);std::fprintf(f,"]}");
            }
            std::fprintf(f,"]}\n");std::fclose(f);
        }
    } denseObserver;
    const auto observedPairAt=[&](int mode,unsigned a,unsigned b,int pa,int pb,double at,float margin) {
        auto& into=denseObserver.bucket[mode];
        const auto beforeBound=impactActualBoundsRejected;
        const auto before=GetContactGeometryDiagnostics();
        ContactManifold result;
        { DenseContactCost::UseBucket active(into); DenseContactCost::Timer full(DenseContactCost::FullPair); result=PairAt(a,b,pa,pb,at,margin); }
        const auto after=GetContactGeometryDiagnostics();
        into.aabbRejected+=impactActualBoundsRejected-beforeBound;
        into.predicates+=after.predicates-before.predicates;
        into.exactFallbacks+=after.exactFallbacks-before.exactFallbacks;
        into.numericFallbacks+=after.numericGapFallbacks-before.numericGapFallbacks;
        denseObserver.Observe(mode,{a,b,unsigned(pa),unsigned(pb)},GeometryPoseAt(a,at),GeometryPoseAt(b,at),result);
        return result;
    };
'''
s=s.replace(marker,marker+insert)
a='auto geometry = PairAt(a, b, pa, pb, now, 0);';assert s.count(a)==1;s=s.replace(a,'auto geometry = observedPairAt(0, a, b, pa, pb, now, 0);')
a=': PairAt(pair.first, pair.second, pa, pb, now, 0);';assert s.count(a)==1;s=s.replace(a,': observedPairAt(1, pair.first, pair.second, pa, pb, now, 0);')
a='now = earliest;';assert s.count(a)==1;s=s.replace(a,a+'\n        ++denseObserver.events;')
(o/'WorldImpactEvents.cpp').write_text(s)
inputs=['src/Contacts.cpp','src/physics/WorldImpactEvents.cpp','src/physics/ImpactCandidateCache.h']
(o/'SOURCES.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'purpose':'Cache-only detailed attribution; every original collision call/arithmetic retained. Observer timings never used as accepted performance.','inputs':{f:hashlib.sha256((r/f).read_bytes()).hexdigest()for f in inputs},'generated':{str(p.relative_to(o)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(o.iterdir())if p.suffix in ['.h','.cpp']}},indent=2)+'\n')
print('Prepared cache-only two-TU cost probe; no compile/run')
