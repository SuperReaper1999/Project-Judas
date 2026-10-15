#!/usr/bin/env python3
from pathlib import Path
import difflib,hashlib,json
ROOT=Path('/home/conner/Documents/GitHub/Project-Judas');OUT=Path(__file__).resolve().parent
source=(ROOT/'src/PhysicsWorld.cpp').read_text();s=source
needle='    std::size_t impactActualBoundsRejected=0;'
assert s.count(needle)==1
s=s.replace(needle,needle+'''
    std::size_t impactFutureTreeBuilds=0,impactFutureTreeUpdates=0,impactFutureTreeBoundBuilds=0,
        impactFutureTreeBfsQueries=0,impactFutureTreeBfsCandidates=0,impactFutureTreeDirtyQueries=0,
        impactFutureTreeDirtyCandidates=0,impactFutureTreeComparedCandidates=0,
        impactFutureTreeAddedCandidates=0,impactFutureTreeRemovedCandidates=0,impactFutureTreeFallbacks=0;
''',1)
needle='''        double now=0;
        scheduledImpacts.clear();'''
assert s.count(needle)==1
s=s.replace(needle,'''        double now=0;
        // Isolated experiment: this tree owns no public query or presentation
        // state. Its leaves enclose remaining trajectories, not historical
        // previous-pose/segment sweeps retained by the public broadphase.
        DynamicAabbTree futureTree;
        std::vector<int> futureProxies;
        std::vector<unsigned> futureScratch;
        bool futureTreeBuilt=false,futureTreeUsable=true;
        const bool useFutureTree=std::getenv("JUDAS_CCD_LOCAL_ORIGINAL")==nullptr;
        const bool useFutureDirty=std::getenv("JUDAS_CCD_LOCAL_DIRTY_QUERY")!=nullptr;
        const bool compareFutureTree=std::getenv("JUDAS_CCD_LOCAL_COMPARE")!=nullptr;
        impactFutureTreeBuilds=impactFutureTreeUpdates=impactFutureTreeBoundBuilds=
            impactFutureTreeBfsQueries=impactFutureTreeBfsCandidates=impactFutureTreeDirtyQueries=
            impactFutureTreeDirtyCandidates=impactFutureTreeComparedCandidates=
            impactFutureTreeAddedCandidates=impactFutureTreeRemovedCandidates=impactFutureTreeFallbacks=0;
        const auto futureBound=[&](unsigned slot){
            ++impactFutureTreeBoundBuilds;
            auto& body=bodies[slot];
            if(!body.HasMotion())return CurrentBound(body);
            const bool simple=(body.shape.type==ShapeType::Box||body.shape.type==ShapeType::Sphere)&&
                body.shape.pivotOffset==glm::vec3(0)&&std::isfinite(body.boundingRadius)&&body.boundingRadius>0;
            if(simple){
                const auto bound=RemainingImpactBound(slot,now,dt);
                if(rigid_motion_bounds::FiniteBound(bound))return bound;
            }
            return tree.FatAabb(body.proxy);
        };
        const auto ensureFutureTree=[&](){
            if(futureTreeBuilt)return futureTreeUsable;
            futureTreeBuilt=true;++impactFutureTreeBuilds;
            futureProxies.assign(bodies.size(),DynamicAabbTree::kNull);
            for(unsigned slot:aliveSlots){
                const auto bound=futureBound(slot);
                if(!rigid_motion_bounds::FiniteBound(bound)){
                    futureTreeUsable=false;++impactFutureTreeFallbacks;return false;
                }
                futureProxies[slot]=futureTree.CreateProxy(bound,slot);
            }
            return true;
        };
        const auto updateFutureBody=[&](unsigned slot){
            if(!futureTreeBuilt||!futureTreeUsable||!useFutureTree)return;
            const auto bound=futureBound(slot);
            if(!rigid_motion_bounds::FiniteBound(bound)){
                futureTreeUsable=false;++impactFutureTreeFallbacks;return;
            }
            // Replacing, rather than only enlarging, removes old trajectory
            // history. No unchanged body's conservative enclosure is shrunk.
            futureTree.MoveProxy(futureProxies[slot],bound);++impactFutureTreeUpdates;
        };
        const auto futureCandidates=[&](const Aabb& query,bool dirty)->const std::vector<unsigned>&{
            if(dirty)++impactFutureTreeDirtyQueries;else ++impactFutureTreeBfsQueries;
            if(!useFutureTree||!ensureFutureTree()){
                const auto& result=QuerySlots(query);
                if(dirty)impactFutureTreeDirtyCandidates+=result.size();else impactFutureTreeBfsCandidates+=result.size();
                return result;
            }
            futureScratch.clear();
            futureTree.Query(query,[&](int proxy){futureScratch.push_back(futureTree.UserData(proxy));});
            std::sort(futureScratch.begin(),futureScratch.end());
            if(dirty)impactFutureTreeDirtyCandidates+=futureScratch.size();else impactFutureTreeBfsCandidates+=futureScratch.size();
            if(compareFutureTree){
                const auto& original=QuerySlots(query);impactFutureTreeComparedCandidates+=original.size();
                for(auto slot:futureScratch)if(!std::binary_search(original.begin(),original.end(),slot))++impactFutureTreeAddedCandidates;
                for(auto slot:original)if(!std::binary_search(futureScratch.begin(),futureScratch.end(),slot))++impactFutureTreeRemovedCandidates;
            }
            return futureScratch;
        };
        scheduledImpacts.clear();''',1)
needle='const auto candidates=QuerySlots(ActualImpactBound(a,ImpactPoseAt(a,now)));'
assert s.count(needle)==1
s=s.replace(needle,'const auto candidates=futureCandidates(ActualImpactBound(a,ImpactPoseAt(a,now)),false);',1)
needle='''                CoverStepReach(b,static_cast<float>(dt-now));'''
assert s.count(needle)==1
s=s.replace(needle,needle+'\n                updateFutureBody(slot);',1)
needle='''            for(unsigned slot:changed) {
                const auto candidates=QuerySlots(tree.FatAabb(bodies[slot].proxy));'''
assert s.count(needle)==1
s=s.replace(needle,'''            for(unsigned slot:changed) {
                const auto candidates=useFutureDirty&&useFutureTree?
                    futureCandidates(futureBound(slot),true):QuerySlots(tree.FatAabb(bodies[slot].proxy));''',1)
anchor='    JUDAS_PROFILE_COUNTER("Physics impact sampling fallbacks",double(w.stats.impactSamplingFallbacks),ProfileCounterMode::Latest);'
assert anchor in s
s=s.replace(anchor,anchor+'''
    JUDAS_PROFILE_COUNTER("Physics CCD future tree builds",double(w.impactFutureTreeBuilds),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future tree updates",double(w.impactFutureTreeUpdates),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future bound builds",double(w.impactFutureTreeBoundBuilds),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future BFS queries",double(w.impactFutureTreeBfsQueries),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future BFS candidates",double(w.impactFutureTreeBfsCandidates),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future dirty queries",double(w.impactFutureTreeDirtyQueries),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future dirty candidates",double(w.impactFutureTreeDirtyCandidates),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD original compared candidates",double(w.impactFutureTreeComparedCandidates),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future added candidates",double(w.impactFutureTreeAddedCandidates),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future removed candidates",double(w.impactFutureTreeRemovedCandidates),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD future tree fallbacks",double(w.impactFutureTreeFallbacks),ProfileCounterMode::Latest);''',1)
(OUT/'PhysicsWorld.base.cpp').write_text(source);(OUT/'PhysicsWorld.cpp').write_text(s)
(OUT/'future-tree.diff').write_text(''.join(difflib.unified_diff(source.splitlines(True),s.splitlines(True),fromfile='src/PhysicsWorld.cpp',tofile='src/PhysicsWorld.cpp')))
(OUT/'provenance.json').write_text(json.dumps({'base_sha256':hashlib.sha256(source.encode()).hexdigest(),'variant_sha256':hashlib.sha256(s.encode()).hexdigest(),'shared_source_modified':False},indent=2)+'\n')
print('Prepared future-tree experiment:',len(s)-len(source),'additional bytes')
