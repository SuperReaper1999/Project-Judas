from pathlib import Path
import subprocess,difflib
source=subprocess.check_output(['git','show','HEAD:src/PhysicsWorld.cpp'],text=True)
s=source
anchor='''    std::size_t impactPoseEvaluations=0,impactPoseReuses=0,impactOrientationBuilds=0,impactTruthRequests=0,impactTruthReuses=0,impactBoundsRejected=0;
'''
replace=anchor+'''    // Derived actual-contact truth only, never trajectory/solver authority.
    // Full represented pose and absolute penetration target must match; a
    // velocity change may invalidate timing while leaving this geometry equal.
    struct ImpactTruthEntry {
        std::array<unsigned,4> pair{};
        std::array<uint32_t,14> pose{};
        uint64_t target=0,epoch=0;
        bool touching=false;
    };
    std::vector<ImpactTruthEntry> impactTruthCache;
    uint64_t impactTruthEpoch=0;
    std::size_t impactStepTruthHits=0,impactStepTruthMisses=0,impactStepTruthReplacements=0;
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''        std::array<ActualTruth,3> actualTruth{};std::size_t nextTruth=0;
        auto touchingAt=[&](double at){
'''
replace='''        std::array<ActualTruth,3> actualTruth{};std::size_t nextTruth=0;
        const std::array<unsigned,4> truthPair={a,b,unsigned(pa),unsigned(pb)};
        // `target` already includes this search's starting penetration. Caching
        // only its requested increment would conflate different predicates.
        uint64_t truthTarget=0;static_assert(sizeof(truthTarget)==sizeof(target));
        std::memcpy(&truthTarget,&target,sizeof(target));
        auto touchingAt=[&](double at){
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''            const bool touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);
            actualTruth[nextTruth]={represented,true,touching};nextTruth=(nextTruth+1)%actualTruth.size();return touching;
'''
replace='''            std::array<uint32_t,14> poseBits;
            static_assert(sizeof(poseBits)==sizeof(represented));
            std::memcpy(poseBits.data(),represented.data(),sizeof(represented));
            uint64_t hash=1469598103934665603ull;
            for(unsigned value:truthPair){hash^=value;hash*=1099511628211ull;}
            for(uint32_t value:poseBits){hash^=value;hash*=1099511628211ull;}
            hash^=truthTarget;hash*=1099511628211ull;
            hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;
            auto& cached=impactTruthCache[std::size_t(hash)&(impactTruthCache.size()-1)];
            bool touching;
            if(cached.epoch==impactTruthEpoch&&cached.target==truthTarget&&
               cached.pair==truthPair&&cached.pose==poseBits){
                touching=cached.touching;++impactStepTruthHits;++impactTruthReuses;
            }else{
                ++impactStepTruthMisses;
                impactStepTruthReplacements+=cached.epoch==impactTruthEpoch;
                touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);
                cached={truthPair,poseBits,truthTarget,impactTruthEpoch,touching};
            }
            actualTruth[nextTruth]={represented,true,touching};nextTruth=(nextTruth+1)%actualTruth.size();return touching;
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''        double now=0;
        scheduledImpacts.clear();
'''
replace='''        // Epoch invalidation is O(1) and cannot reuse a different step's body
        // generation, shape revision or terrain state. No table is allocated
        // for a step without possible continuous pairs; retained storage is
        // bounded to8192 entries and grows in powers of two for cheap lookup.
        impactStepTruthHits=impactStepTruthMisses=impactStepTruthReplacements=0;
        if(++impactTruthEpoch==0){
            for(auto& entry:impactTruthCache)entry.epoch=0;
            impactTruthEpoch=1;
        }
        if(!separatedPairs.empty()||!spinningPairs.empty()){
            const std::size_t desired=std::min<std::size_t>(8192,std::max<std::size_t>(64,candidatePairs.size()));
            std::size_t capacity=64;while(capacity<desired)capacity*=2;
            if(impactTruthCache.size()<capacity){
                const auto previous=impactTruthCache.capacity();impactTruthCache.resize(capacity);
                if(impactTruthCache.capacity()!=previous){++cacheCounters.allocations;
                    cacheCounters.allocatedBytes+=impactTruthCache.capacity()*sizeof(ImpactTruthEntry);}
            }
        }
        double now=0;
        scheduledImpacts.clear();
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''        return bytes+impactPoseSamples.capacity()*sizeof(ImpactPoseSample)+solver.FrameStorageBytes();
'''
replace='''        return bytes+impactPoseSamples.capacity()*sizeof(ImpactPoseSample)+
            impactTruthCache.capacity()*sizeof(ImpactTruthEntry)+solver.FrameStorageBytes();
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''    JUDAS_PROFILE_COUNTER("Physics impact actual-contact reuses",double(w.impactTruthReuses),ProfileCounterMode::Latest);
'''
replace=anchor+'''    JUDAS_PROFILE_COUNTER("Physics step actual-contact cache hits",double(w.impactStepTruthHits),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics step actual-contact cache misses",double(w.impactStepTruthMisses),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics step actual-contact cache replacements",double(w.impactStepTruthReplacements),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics step actual-contact cache bytes",double(w.impactTruthCache.capacity()*sizeof(PhysicsWorld::Impl::ImpactTruthEntry)),ProfileCounterMode::Latest);
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
s=s.replace('bounded to8192 entries','bounded to 8192 entries')
Path('/tmp/m87-step-truth-cache.cpp').write_text(s)
patch=''.join(difflib.unified_diff(source.splitlines(keepends=True),s.splitlines(keepends=True),fromfile='a/src/PhysicsWorld.cpp',tofile='b/src/PhysicsWorld.cpp'))
Path('/tmp/m87-step-truth-cache.patch').write_text(patch)
print('patch lines',len(patch.splitlines()))
