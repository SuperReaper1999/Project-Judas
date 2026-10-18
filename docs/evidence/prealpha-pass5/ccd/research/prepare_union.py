from pathlib import Path
import re,textwrap
R=Path.cwd(); D=R/'.cache/prealpha-pass5/ccd/union-v1/sources'; O=R/'.cache/prealpha-pass4/ccd/episode-v6-front-fast/sources/PhysicsWorld.cpp'
s=O.read_text()
# Preserve the accepted modular implementation, introducing cache-only prototypes.
# Every private-world translation unit is replaced coherently in a single object.
h=(D/'physics/WorldInternal.h').read_text().replace('#include "WorldProfiling.h"','#include "WorldProfiling.h"\n#include "MeshUnionContinuity.h"')
methods=s[s.index('    struct EpisodeRecord'):s.index('    ContactManifold PairAt')]
methods=methods.replace('const CollisionAsset* asset=nullptr;MeshContactEpisode episode;', 'const CollisionAsset* asset=nullptr;MeshContactEpisode episode; double anchorTime=0;')
# Cache the already-proved all-front certificate on an unchanged anchored event
# interval. This is a query optimization, never the owner of physical sidedness.
methods=methods.replace('bool MeshFrontSafe(', 'bool RawMeshFrontSafe(')
methods=methods.replace('const auto old=Episode(a,b,pa,pb);auto features=', 'const auto old=EpisodeForTime(a,b,pa,pb,time);auto features=')
methods=methods.replace('MeshContactEpisode{std::move(features)}};', 'MeshContactEpisode{std::move(features)},time};\n        episodeDerived.clear();')
methods=methods.replace('float margin,const ContactPreparedOrientation* aa,const ContactPreparedOrientation* bb)', 'float margin,const ContactPreparedOrientation* aa,const ContactPreparedOrientation* bb,double time=-1)')
methods=methods.replace('margin,Episode(a,b,pa,pb))', 'margin,time<0?Episode(a,b,pa,pb):EpisodeForTime(a,b,pa,pb,time))')
methods+='''
    std::map<std::array<unsigned,4>,std::pair<std::pair<double,double>,bool>> meshFrontCertificates;
    std::map<std::tuple<unsigned,unsigned,int,int,double>,MeshContactEpisode> episodeDerived;
    unsigned long long unionQueries=0,unionIntervals=0,unionCertificates=0,unionUncertain=0;
    bool MeshFrontSafe(unsigned a,unsigned b,int pa,int pb,double start,double finish) {
        const std::array<unsigned,4> key{a,b,unsigned(pa),unsigned(pb)};
        const auto found=meshFrontCertificates.find(key);
        if(found!=meshFrontCertificates.end()&&found->second.first==std::make_pair(start,finish))return found->second.second;
        const bool safe=RawMeshFrontSafe(a,b,pa,pb,start,finish);
        meshFrontCertificates[key]={{start,finish},safe};return safe;
    }
    const MeshContactEpisode* EpisodeForTime(unsigned a,unsigned b,int pa,int pb,double time) {
        const auto* previous=Episode(a,b,pa,pb);
        if(!previous||previous->fronts.empty())return nullptr;
        const auto identity=std::make_tuple(a,b,pa,pb,time);
        const auto cached=episodeDerived.find(identity);
        if(cached!=episodeDerived.end())return cached->second.fronts.empty()?nullptr:&cached->second;
        const auto& record=meshEpisodes.at({a,b,unsigned(pa),unsigned(pb)});
        MeshContactEpisode selected=*previous;
        const auto first=PrimitiveAt(bodies[a].shape,bodies[a].rigidBody,pa),second=PrimitiveAt(bodies[b].shape,bodies[b].rigidBody,pb);
        const bool fm=first.shape.type==ShapeType::TriangleMesh;
        const unsigned moving=fm?b:a;
        const auto& query=fm?second:first;const auto& mesh=fm?first:second;
        double covered=record.anchorTime;
        if(time<covered)throw std::runtime_error("finite-mesh episode query precedes causal anchor");
        for(const auto& segment:bodies[moving].motion.Segments()) {
            if(segment.end<=covered||segment.begin>time)continue;
            if(segment.begin>covered)throw std::runtime_error("finite-mesh episode trajectory gap");
            const double finish=std::min(time,segment.end);
            if(finish<=covered)continue;
            auto result=episode_probe::ConnectedUnion(query,mesh,selected,segment,covered,finish);
            ++unionQueries;unionIntervals+=result.measure.intervals;
            unionCertificates+=result.measure.pointCertificates+result.measure.constantCertificates;
            if(result.measure.result==episode_probe::Result::Uncertain){++unionUncertain;throw std::runtime_error("finite-mesh episode continuity uncertified within bounded policy");}
            if(result.measure.result==episode_probe::Result::Separated){selected.fronts.clear();covered=time;break;}
            selected=std::move(result.selected);covered=finish;if(covered==time)break;
        }
        if(covered<time&&bodies[moving].HasMotion())throw std::runtime_error("finite-mesh episode trajectory incomplete");
        auto entry=episodeDerived.emplace(identity,std::move(selected)).first;
        return entry->second.fronts.empty()?nullptr:&entry->second;
    }
    void BeginMeshEpisodes() {
        episodeDerived.clear();meshFrontCertificates.clear();
        for(const auto& pair:candidatePairs)for(int pa=0;pa<PrimitiveCount(bodies[pair.first].shape);++pa)for(int pb=0;pb<PrimitiveCount(bodies[pair.second].shape);++pb)
            if(CanRespond(pair.first,pair.second)&&MeshPair(pair.first,pair.second,pa,pb)) {
                const auto& a=bodies[pair.first];const auto& b=bodies[pair.second];
                auto first=PrimitiveAt(a.shape,a.rigidBody,pa),second=PrimitiveAt(b.shape,b.rigidBody,pb);
                const bool fm=first.shape.type==ShapeType::TriangleMesh;
                auto features=MeshContactFeatures(fm?second:first,fm?first:second,Episode(pair.first,pair.second,pa,pb));
                const std::array<unsigned,4> key{pair.first,pair.second,unsigned(pa),unsigned(pb)};
                if(features.empty())meshEpisodes.erase(key);
                else meshEpisodes[key]={ContactKey{pair.first,pair.second,a.generation,b.generation,pa,pb},a.contactObservationRevision,b.contactObservationRevision,(fm?first:second).shape.asset.get(),MeshContactEpisode{std::move(features)},0};
            }
    }
    void FinishMeshEpisodes(double dt) {
        std::vector<std::array<unsigned,4>> keys;for(const auto& e:meshEpisodes)keys.push_back(e.first);
        for(const auto& key:keys){
            if(key[0]>=bodies.size()||key[1]>=bodies.size()||!bodies[key[0]].alive||!bodies[key[1]].alive||!CanRespond(key[0],key[1])||!MeshPair(key[0],key[1],key[2],key[3])){meshEpisodes.erase(key);continue;}
            auto episode=EpisodeForTime(key[0],key[1],key[2],key[3],dt);
            const auto first=PrimitiveAt(bodies[key[0]].shape,bodies[key[0]].rigidBody,key[2]),second=PrimitiveAt(bodies[key[1]].shape,bodies[key[1]].rigidBody,key[3]);const bool fm=first.shape.type==ShapeType::TriangleMesh;
            auto features=MeshContactFeatures(fm?second:first,fm?first:second,episode);
            if(features.empty())meshEpisodes.erase(key);else {auto& e=meshEpisodes.at(key);e.episode.fronts=std::move(features);e.anchorTime=0;}
        }
        episodeDerived.clear();meshFrontCertificates.clear();
    }
'''
h=h.replace('    void Remove(BodyHandle handle);','    void Remove(BodyHandle handle);\n'+methods)
(D/'physics/WorldInternal.h').write_text(h)
# Impact query full/Boolean/deep paths consume the identical immutable selection.
q=(D/'physics/WorldImpactQueries.h').read_text()
q=q.replace('return ComputeContacts(PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation),','return EpisodeContacts(a,b,pa,pb,PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation),')
q=q.replace('&*aa.orientation, &*bb.orientation);','&*aa.orientation, &*bb.orientation,time);',1)
q=q.replace('    if (posa.shape.asset || posb.shape.asset)\n        return CookedHasPenetrationBeyond', '    if(MeshPair(a,b,pa,pb)){const bool fm=posa.shape.type==ShapeType::TriangleMesh;return MeshEpisodeHasPenetrationBeyond(fm?posb:posa,fm?posa:posb,target,EpisodeForTime(a,b,pa,pb,time));}\n    if (posa.shape.asset || posb.shape.asset)\n        return CookedHasPenetrationBeyond')
q=q.replace('    if (posa.shape.asset || posb.shape.asset)\n        return CookedHasContacts', '    if(MeshPair(a,b,pa,pb)){const bool fm=posa.shape.type==ShapeType::TriangleMesh;return MeshEpisodeHasContacts(fm?posb:posa,fm?posa:posb,EpisodeForTime(a,b,pa,pb,time));}\n    if (posa.shape.asset || posb.shape.asset)\n        return CookedHasContacts')
(D/'physics/WorldImpactQueries.h').write_text(q)
# Copy only the old validated per-feature continuation algorithm body; all other
# current modular world mechanisms and actual dense scheduling remain accepted4c.
start=s.index('    double ImpactTime('); brace=s.index('{',start); level=1;end=brace+1
while level:
    level+=(s[end]=='{')-(s[end]=='}');end+=1
body=textwrap.dedent(s[brace+1:end-1])
body=body.replace('Episode(a,b,pa,pb)','EpisodeForTime(a,b,pa,pb,start)')
body=body.replace('meshBudget,EpisodeForTime(a,b,pa,pb,start)','meshBudget,EpisodeForTime(a,b,pa,pb,at)')
body=body.replace('finish - time,nullptr,EpisodeForTime(a,b,pa,pb,start)','finish - time,nullptr,EpisodeForTime(a,b,pa,pb,time)')
c=(D/'physics/WorldCCDSearch.cpp').read_text();start=c.index('auto PhysicsWorld::Impl::ImpactTime(');brace=c.index('{',start)
c=c[:brace+1]+'\n'+body+'\n}\n';(D/'physics/WorldCCDSearch.cpp').write_text(c)
e=(D/'physics/WorldImpactEvents.cpp').read_text()
e=e.replace('const double time = std::binary_search(supportPairs.begin()', 'const double time = MeshPair(key[0],key[1],key[2],key[3])&&!MeshFrontSafe(key[0],key[1],key[2],key[3],now,dt)?ImpactTime(key[0],key[1],key[2],key[3],now,dt):std::binary_search(supportPairs.begin()')
e=e.replace('    for (const auto &key : spinningPairs)\n        schedule(key);','    for (const auto &key : spinningPairs)\n        schedule(key);\n    for(const auto& key:supportPairs)if(MeshPair(key[0],key[1],key[2],key[3]))schedule(key);')
e=e.replace('        ++stats.impactEvents;','        episodeDerived.clear();meshFrontCertificates.clear();\n        for(const auto& key:eventPairs)PublishEpisode(key[0],key[1],key[2],key[3],now);\n        ++stats.impactEvents;')
e=e.replace('            b.motion.ChangeVelocity(b.rigidBody, now);','            b.motion.ChangeVelocity(b.rigidBody, now);\n            episodeDerived.clear();meshFrontCertificates.clear();')
(D/'physics/WorldImpactEvents.cpp').write_text(e)
p=(D/'PhysicsWorld.cpp').read_text().replace('    w.PrepareInitialContactGeometry(fixedDeltaTime);','    w.BeginMeshEpisodes();\n    w.PrepareInitialContactGeometry(fixedDeltaTime);')
p=p.replace('ComputeContacts(childA, childB, margin, &orientationA, &orientationB)', 'w.EpisodeContacts(slotA,slotB,partA,partB,childA,childB,margin,&orientationA,&orientationB)')
p=p.replace('ComputeContacts(childA, childB, 0, &orientationA, &orientationB)', 'w.EpisodeContacts(slotA,slotB,partA,partB,childA,childB,0,&orientationA,&orientationB)')
p=p.replace('    w.solver.SolvePositions();','    w.FinishMeshEpisodes(fixedDeltaTime);\n    w.solver.SolvePositions();')
(D/'PhysicsWorld.cpp').write_text(p)
# A single private-world object avoids mixing differing Impl layouts across TUs.
files=['PhysicsWorld.cpp']+[str(f.relative_to(D)) for f in sorted((D/'physics').glob('World*.cpp'))]
(D/'UnifiedWorld.cpp').write_text('\n'.join('#include "'+f+'"' for f in files)+'\n')
print('cache prototype prepared',len(files),'coherent private-world units; no production changes')
