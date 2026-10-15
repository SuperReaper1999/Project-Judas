from pathlib import Path
import difflib,hashlib
source=Path('src/PhysicsWorld.cpp').read_text()
Path('/tmp/m87-manifold-cache-base.cpp').write_text(source)
s=source
anchor='''    std::size_t impactPoseEvaluations=0,impactPoseReuses=0,impactOrientationBuilds=0,impactTruthRequests=0,impactTruthReuses=0,impactBoundsRejected=0;
'''
replace=anchor+'''    // Full zero-margin primitive geometry is derived, never solver history.
    // Preserve directed argument/row order and every represented parent-pose bit.
    struct ImpactManifoldEntry {
        std::array<unsigned,4> pair{};
        std::array<uint32_t,14> pose{};
        uint64_t epoch=0;
        ContactManifold geometry;
    };
    std::vector<ImpactManifoldEntry> impactManifoldCache;
    uint64_t impactManifoldEpoch=0;
    std::size_t impactManifoldRequiredSize=64;
    std::size_t impactManifoldHits=0,impactManifoldMisses=0,impactManifoldReplacements=0;
    bool impactManifoldCacheActive=false;
    bool SimpleImpactPrimitive(unsigned slot,int part) const {
        const auto& shape=bodies[slot].shape;
        const auto type=shape.type==ShapeType::CompoundBoxes?shape.boxes[std::size_t(part)].type:shape.type;
        return type==ShapeType::Box||type==ShapeType::Sphere;
    }
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''    ContactManifold PairAt(unsigned a,unsigned b,int pa,int pb,double time,float margin) {
        PhysicsWorkTimer timer(work.geometry,work.active);
        const auto& aa=ImpactPoseAt(a,time);const auto& bb=ImpactPoseAt(b,time);
        return ComputeContacts(PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),
            PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation),margin,&*aa.orientation,&*bb.orientation);
    }
'''
replace='''    ContactManifold PairAt(unsigned a,unsigned b,int pa,int pb,double time,float margin) {
        PhysicsWorkTimer timer(work.geometry,work.active);
        const auto& aa=ImpactPoseAt(a,time);const auto& bb=ImpactPoseAt(b,time);
        if(impactManifoldCacheActive&&margin==0&&SimpleImpactPrimitive(a,pa)&&SimpleImpactPrimitive(b,pb)){
            // Lazily allocate only if an actual simple zero-margin query needs
            // this optional CCD cache. Capacity remains bounded across steps.
            if(impactManifoldCache.size()<impactManifoldRequiredSize){
                const auto previous=impactManifoldCache.capacity();
                impactManifoldCache.resize(impactManifoldRequiredSize);
                if(impactManifoldCache.capacity()!=previous){++cacheCounters.allocations;
                    cacheCounters.allocatedBytes+=impactManifoldCache.capacity()*sizeof(ImpactManifoldEntry);}
            }
            const std::array<unsigned,4> pair={a,b,unsigned(pa),unsigned(pb)};
            const std::array<float,14> represented={aa.pose.position.x,aa.pose.position.y,aa.pose.position.z,
                aa.pose.orientation.w,aa.pose.orientation.x,aa.pose.orientation.y,aa.pose.orientation.z,
                bb.pose.position.x,bb.pose.position.y,bb.pose.position.z,
                bb.pose.orientation.w,bb.pose.orientation.x,bb.pose.orientation.y,bb.pose.orientation.z};
            std::array<uint32_t,14> poseBits;
            static_assert(sizeof(poseBits)==sizeof(represented));
            std::memcpy(poseBits.data(),represented.data(),sizeof(represented));
            uint64_t hash=1469598103934665603ull;
            for(unsigned value:pair){hash^=value;hash*=1099511628211ull;}
            for(uint32_t value:poseBits){hash^=value;hash*=1099511628211ull;}
            hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;
            auto& cached=impactManifoldCache[std::size_t(hash)&(impactManifoldCache.size()-1)];
            if(cached.epoch==impactManifoldEpoch&&cached.pair==pair&&cached.pose==poseBits){
                ++impactManifoldHits;return cached.geometry;
            }
            ++impactManifoldMisses;impactManifoldReplacements+=cached.epoch==impactManifoldEpoch;
            auto geometry=ComputeContacts(PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),
                PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation),margin,&*aa.orientation,&*bb.orientation);
            cached={pair,poseBits,impactManifoldEpoch,geometry};
            return geometry;
        }
        return ComputeContacts(PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),
            PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation),margin,&*aa.orientation,&*bb.orientation);
    }
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''    bool PairTouches(unsigned a,unsigned b,int pa,int pb,double time) {
        PhysicsWorkTimer timer(work.geometry,work.active);
'''
replace='''    bool PairTouches(unsigned a,unsigned b,int pa,int pb,double time) {
        // The primitive existence path already computes this same full zero-
        // margin manifold. Share it with later discovery/depth/response queries;
        // cooked Boolean queries below retain their specialized implementation.
        if(SimpleImpactPrimitive(a,pa)&&SimpleImpactPrimitive(b,pb))return PairAt(a,b,pa,pb,time,0).count>0;
        PhysicsWorkTimer timer(work.geometry,work.active);
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''    void AdvanceImpacts(float dt,bool coupledVelocityChanged=false) {
        stepDuration=dt;
'''
replace='''    void AdvanceImpacts(float dt,bool coupledVelocityChanged=false) {
        // Shapes are immutable throughout this invocation. Guard the cache's
        // lifetime so later geometry publication cannot reuse old shape truth.
        struct CacheScope {bool& active;explicit CacheScope(bool& flag):active(flag){active=true;}
            ~CacheScope(){active=false;}} cacheScope(impactManifoldCacheActive);
        impactManifoldHits=impactManifoldMisses=impactManifoldReplacements=0;
        if(++impactManifoldEpoch==0){
            for(auto& entry:impactManifoldCache)entry.epoch=0;
            impactManifoldEpoch=1;
        }
        stepDuration=dt;
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''        // Support pairs whose rotation over the step could carry a feature
'''
replace='''        const auto desired=std::min<std::size_t>(8192,std::max<std::size_t>(64,candidatePairs.size()));
        impactManifoldRequiredSize=64;while(impactManifoldRequiredSize<desired)impactManifoldRequiredSize*=2;
'''+anchor
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''        return bytes+impactPoseSamples.capacity()*sizeof(ImpactPoseSample)+solver.FrameStorageBytes();
'''
replace='''        return bytes+impactPoseSamples.capacity()*sizeof(ImpactPoseSample)+
            impactManifoldCache.capacity()*sizeof(ImpactManifoldEntry)+solver.FrameStorageBytes();
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
anchor='''    JUDAS_PROFILE_COUNTER("Physics impact actual-contact reuses",double(w.impactTruthReuses),ProfileCounterMode::Latest);
'''
replace=anchor+'''    JUDAS_PROFILE_COUNTER("Physics impact manifold cache hits",double(w.impactManifoldHits),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact manifold cache misses",double(w.impactManifoldMisses),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact manifold cache replacements",double(w.impactManifoldReplacements),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact manifold cache bytes",double(w.impactManifoldCache.capacity()*sizeof(PhysicsWorld::Impl::ImpactManifoldEntry)),ProfileCounterMode::Latest);
'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
Path('/tmp/m87-manifold-cache.cpp').write_text(s)
patch=''.join(difflib.unified_diff(source.splitlines(keepends=True),s.splitlines(keepends=True),fromfile='a/src/PhysicsWorld.cpp',tofile='b/src/PhysicsWorld.cpp'))
Path('/tmp/m87-manifold-cache.patch').write_text(patch)
print('base sha256',hashlib.sha256(source.encode()).hexdigest(),'patch lines',len(patch.splitlines()))
