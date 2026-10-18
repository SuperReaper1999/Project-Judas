from pathlib import Path
out=Path(__file__).parent;s=out/'sources'
h=(s/'CollisionGeometry.h').read_text();h += '''
// Internal physical front-contact episode: stateless queries pass no episode.
struct MeshContactEpisode { std::vector<uint32_t> fronts; };
ContactManifold EpisodeCookedContacts(const PrimitivePose&,const PrimitivePose&,double,const MeshContactEpisode*);
std::vector<uint32_t> MeshContactFeatures(const PrimitivePose&,const PrimitivePose&,const MeshContactEpisode*);
bool MeshEpisodeHasContacts(const PrimitivePose&,const PrimitivePose&,const MeshContactEpisode*,const std::vector<uint32_t>* excluded=nullptr);
bool MeshEpisodeHasPenetrationBeyond(const PrimitivePose&,const PrimitivePose&,double,const MeshContactEpisode*);
''';h=h.replace('double rotationRadius,double duration);','double rotationRadius,double duration,const std::vector<uint32_t>* excluded=nullptr,const MeshContactEpisode* episode=nullptr);');h=h.replace('// Earliest possible overlap','struct MeshContactEpisode;\n// Earliest possible overlap');(s/'CollisionGeometry.h').write_text(h)
c=(s/'CollisionGeometry.cpp').read_text();c=c.replace('Consumer consume,double penetrationThreshold=INFINITY) {','Consumer consume,double penetrationThreshold=INFINITY,const MeshContactEpisode* episode=nullptr,const std::vector<uint32_t>* excluded=nullptr,std::vector<uint32_t>* features=nullptr) {');c=c.replace('polyInto(pa,a,origin);auto test=', 'polyInto(pa,a,origin);uint32_t triangleFeature=UINT32_MAX;auto test=');c=c.replace('if(gap<=margin){\n    return consume(x+origin', 'if(gap<=margin){\n    if(features)features->push_back(triangleFeature);\n    return consume(x+origin');c=c.replace('for(auto i:candidates(b,lo,hi,margin)){\n   ++stats.triangles;', 'for(auto i:candidates(b,lo,hi,margin)){\n   if(excluded&&std::binary_search(excluded->begin(),excluded->end(),i))continue;\n   triangleFeature=i;\n   ++stats.triangles;');c=c.replace('if(!asset.twoSided&&side<-eps)continue;\n   if(side<0)reverseTriangleSide(pb,asset,i);','const bool owned=episode&&std::binary_search(episode->fronts.begin(),episode->fronts.end(),i);\n   if(!asset.twoSided&&side<-eps&&!owned)continue;\n   if(asset.twoSided&&side<0)reverseTriangleSide(pb,asset,i);');c=c.replace('double rotationRadius,double duration) {','double rotationRadius,double duration,const std::vector<uint32_t>* excluded,const MeshContactEpisode* episode) {');c=c.replace('for(unsigned feature:selected){\n  ++stats.triangles;', 'for(unsigned feature:selected){\n  if(excluded&&std::binary_search(excluded->begin(),excluded->end(),feature))continue;\n  ++stats.triangles;');c=c.replace('if(!asset.twoSided&&glm::dot(b.normals[0],center-b.points[0])<-eps)continue;','if(!asset.twoSided&&glm::dot(b.normals[0],center-b.points[0])<-eps&&!(episode&&std::binary_search(episode->fronts.begin(),episode->fronts.end(),feature)))continue;')
a=c.index('ContactManifold CookedContacts(');b=c.index('bool CookedHasContacts(',a);block=c[a:b];block=block.replace('ContactManifold CookedContacts(const PrimitivePose& a,const PrimitivePose& b,double margin) {','ContactManifold EpisodeCookedContacts(const PrimitivePose& a,const PrimitivePose& b,double margin,const MeshContactEpisode* episode) {');block=block.replace('CookedContacts(b,a,margin)', 'EpisodeCookedContacts(b,a,margin,episode)');block=block.replace('contacts.push_back(make(a,b,x,y,normal,gap));return false;\n });','contacts.push_back(make(a,b,x,y,normal,gap));return false;\n },INFINITY,episode);');print(block[-700:]);c=c[:a]+block+'''ContactManifold CookedContacts(const PrimitivePose& a,const PrimitivePose& b,double margin) { return EpisodeCookedContacts(a,b,margin,nullptr); }
std::vector<uint32_t> MeshContactFeatures(const PrimitivePose& query,const PrimitivePose& mesh,const MeshContactEpisode* episode) {
 std::vector<uint32_t> result;visitCookedPolyContacts(query,mesh,0,[](V,V,V,double){return false;},INFINITY,episode,nullptr,&result);
 std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
bool MeshEpisodeHasContacts(const PrimitivePose& query,const PrimitivePose& mesh,const MeshContactEpisode* episode,const std::vector<uint32_t>* excluded) {
 return visitCookedPolyContacts(query,mesh,0,[](V,V,V,double){return true;},INFINITY,episode,excluded);
}
bool MeshEpisodeHasPenetrationBeyond(const PrimitivePose& query,const PrimitivePose& mesh,double threshold,const MeshContactEpisode* episode) {
 return visitCookedPolyContacts(query,mesh,0,[&](V,V,V,double gap){return -gap>threshold;},threshold,episode);
}
'''+c[b:];(s/'CollisionGeometry.cpp').write_text(c)
p=(s/'PhysicsWorld.cpp').read_text();marker='    ContactManifold PairAt(';at=p.index(marker);p=p[:at]+'''    struct EpisodeRecord { ContactKey key; uint64_t revisionA=0,revisionB=0; const CollisionAsset* asset=nullptr;MeshContactEpisode episode; };
    std::map<std::array<unsigned,4>,EpisodeRecord> meshEpisodes;
    bool MeshPair(unsigned a,unsigned b,int pa,int pb) const {
        const auto first=PrimitiveAt(bodies[a].shape,bodies[a].rigidBody,pa),second=PrimitiveAt(bodies[b].shape,bodies[b].rigidBody,pb);
        const auto poly=[](ShapeType t){return t==ShapeType::Box||t==ShapeType::ConvexHull;};
        return (first.shape.type==ShapeType::TriangleMesh&&!bodies[a].HasMotion()&&poly(second.shape.type))||
               (second.shape.type==ShapeType::TriangleMesh&&!bodies[b].HasMotion()&&poly(first.shape.type));
    }
    const MeshContactEpisode* Episode(unsigned a,unsigned b,int pa,int pb) const {
        const auto it=meshEpisodes.find({a,b,unsigned(pa),unsigned(pb)});if(it==meshEpisodes.end())return nullptr;
        const auto& e=it->second;const auto first=PrimitiveAt(bodies[a].shape,bodies[a].rigidBody,pa),second=PrimitiveAt(bodies[b].shape,bodies[b].rigidBody,pb);
        const auto asset=first.shape.type==ShapeType::TriangleMesh?first.shape.asset.get():second.shape.asset.get();
        return e.key.SameBodies(ContactKey{a,b,bodies[a].generation,bodies[b].generation,pa,pb})&&e.revisionA==bodies[a].contactObservationRevision&&e.revisionB==bodies[b].contactObservationRevision&&e.asset==asset?&e.episode:nullptr;
    }
    void PublishEpisode(unsigned a,unsigned b,int pa,int pb,double time) {
        if(!MeshPair(a,b,pa,pb))return;
        const auto& aa=ImpactPoseAt(a,time);const auto& bb=ImpactPoseAt(b,time);
        const auto first=PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),second=PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation);
        const auto& mesh=first.shape.type==ShapeType::TriangleMesh?first:second;const auto& query=first.shape.type==ShapeType::TriangleMesh?second:first;
        const auto old=Episode(a,b,pa,pb);auto features=MeshContactFeatures(query,mesh,old);
        const std::array<unsigned,4> key{a,b,unsigned(pa),unsigned(pb)};
        if(features.empty())meshEpisodes.erase(key);else meshEpisodes[key]={ContactKey{a,b,bodies[a].generation,bodies[b].generation,pa,pb},bodies[a].contactObservationRevision,bodies[b].contactObservationRevision,mesh.shape.asset.get(),MeshContactEpisode{std::move(features)}};
    }
    ContactManifold EpisodeContacts(unsigned a,unsigned b,int pa,int pb,const PrimitivePose& first,const PrimitivePose& second,float margin,const ContactPreparedOrientation* aa,const ContactPreparedOrientation* bb) {
        return MeshPair(a,b,pa,pb)?EpisodeCookedContacts(first,second,margin,Episode(a,b,pa,pb)):ComputeContacts(first,second,margin,aa,bb);
    }
'''+p[at:];p=p.replace('return ComputeContacts(PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),\n            PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation),margin,&*aa.orientation,&*bb.orientation);','return EpisodeContacts(a,b,pa,pb,PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),\n            PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation),margin,&*aa.orientation,&*bb.orientation);');p=p.replace('if(posa.shape.asset||posb.shape.asset)return CookedHasPenetrationBeyond(posa,posb,target);','if(MeshPair(a,b,pa,pb)){const bool first=posa.shape.type==ShapeType::TriangleMesh;return MeshEpisodeHasPenetrationBeyond(first?posb:posa,first?posa:posb,target,Episode(a,b,pa,pb));}\n        if(posa.shape.asset||posb.shape.asset)return CookedHasPenetrationBeyond(posa,posb,target);');p=p.replace('if(posa.shape.asset||posb.shape.asset)return CookedHasContacts(posa,posb,0);','if(MeshPair(a,b,pa,pb)){const bool first=posa.shape.type==ShapeType::TriangleMesh;return MeshEpisodeHasContacts(first?posb:posa,first?posa:posb,Episode(a,b,pa,pb));}\n        if(posa.shape.asset||posb.shape.asset)return CookedHasContacts(posa,posb,0);')
start=p.index('    double ImpactTime(');end=p.index('\n    void ResolveInitialImpacts(',start);m=p[start:end];m=m.replace('++stats.impactQueries;','++stats.impactQueries;\n        double fallbackTime=finish+1;',1);marker='        const glm::dvec3 relative =';a=m.index(marker);m=m[:a]+'''        const auto episode=Episode(a,b,pa,pb);
        std::vector<uint32_t> existingMeshFeatures;
        if(meshAdvance&&target==0){
            existingMeshFeatures=MeshContactFeatures(firstMesh?secondPrimitive:firstPrimitive,firstMesh?firstPrimitive:secondPrimitive,episode);
            if(!existingMeshFeatures.empty()){
                fallbackTime=ImpactTime(a,b,pa,pb,start,finish,SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)}));
                if(fallbackTime<=finish)finish=fallbackTime;
            }
        }
'''+m[a:];old='''            const bool touching = target > 0 ? PairPenetrationExceeds(a, b, pa, pb, at, target)
                                             : PairTouches(a, b, pa, pb, at);''';new='''            bool touching;
            if(meshAdvance&&target==0){
                const auto first=PrimitiveAt(bodies[a].shape,aa,pa),second=PrimitiveAt(bodies[b].shape,bb,pb);
                touching=MeshEpisodeHasContacts(firstMesh?second:first,firstMesh?first:second,episode,&existingMeshFeatures);
            }else touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);''';assert old in m;m=m.replace(old,new);m=m.replace('finish - time);\n                if (!std::isfinite(increment))','finish - time,&existingMeshFeatures,episode);\n                if (!std::isfinite(increment))');m=m.replace('return finish + 1;', 'return fallbackTime;');p=p[:start]+m+p[end:];p=p.replace('std::binary_search(supportPairs.begin(), supportPairs.end(), key) ? double(dt) + 1','MeshPair(key[0],key[1],key[2],key[3])?ImpactTime(key[0],key[1],key[2],key[3],now,dt):\n                std::binary_search(supportPairs.begin(), supportPairs.end(), key) ? double(dt) + 1');p=p.replace('for (const auto &key : spinningPairs)\n            schedule(key);','for (const auto &key : spinningPairs)\n            schedule(key);\n        for(const auto& key:supportPairs)if(MeshPair(key[0],key[1],key[2],key[3]))schedule(key);');p=p.replace('++stats.impactEvents;\n            stats.impactSafetyFallback', 'for(const auto& key:eventPairs)PublishEpisode(key[0],key[1],key[2],key[3],now);\n            ++stats.impactEvents;\n            stats.impactSafetyFallback');p=p.replace('prepared?prepared->manifold:ComputeContacts(childA, childB, margin, &orientationA, &orientationB);','prepared?prepared->manifold:w.EpisodeContacts(slotA,slotB,partA,partB,childA,childB,margin,&orientationA,&orientationB);');p=p.replace('touchingManifold=ComputeContacts(childA,childB,0,&orientationA,&orientationB).count>0;','touchingManifold=w.EpisodeContacts(slotA,slotB,partA,partB,childA,childB,0,&orientationA,&orientationB).count>0;');# preserve current/previous across boundaries
marker='    w.cacheUsed.assign(w.contactCache.size(), 0);';p=p.replace(marker,marker+'''\n    for(const auto& pair:w.candidatePairs)for(int pa=0;pa<PrimitiveCount(w.bodies[pair.first].shape);++pa)for(int pb=0;pb<PrimitiveCount(w.bodies[pair.second].shape);++pb)if(w.MeshPair(pair.first,pair.second,pa,pb)){
      const auto& a=w.bodies[pair.first];const auto& b=w.bodies[pair.second];auto first=PrimitiveAt(a.shape,a.rigidBody,pa),second=PrimitiveAt(b.shape,b.rigidBody,pb);const bool fm=first.shape.type==ShapeType::TriangleMesh;
      auto features=MeshContactFeatures(fm?second:first,fm?first:second,w.Episode(pair.first,pair.second,pa,pb));const std::array<unsigned,4> key{pair.first,pair.second,unsigned(pa),unsigned(pb)};
      if(features.empty())w.meshEpisodes.erase(key);else w.meshEpisodes[key]={ContactKey{pair.first,pair.second,a.generation,b.generation,pa,pb},a.contactObservationRevision,b.contactObservationRevision,(fm?first:second).shape.asset.get(),MeshContactEpisode{std::move(features)}};
    }
''');(s/'PhysicsWorld.cpp').write_text(p)
