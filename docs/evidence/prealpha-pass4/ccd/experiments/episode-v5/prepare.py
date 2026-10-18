from pathlib import Path
s=Path(__file__).parent/'sources';h=(s/'CollisionGeometry.h').read_text();h+='''
struct MeshFeatureBudget { std::vector<std::pair<uint32_t,double>> limits; };
MeshFeatureBudget BuildMeshFeatureBudget(const PrimitivePose&,const PrimitivePose&,double,glm::dvec3,glm::dvec3,double,double,const MeshContactEpisode*);
bool MeshFeatureBudgetExceeded(const PrimitivePose&,const PrimitivePose&,const MeshFeatureBudget&,const MeshContactEpisode*);
''';(s/'CollisionGeometry.h').write_text(h)
c=(s/'CollisionGeometry.cpp').read_text();c=c.replace('consume(x+origin,y+origin,s.normal,gap);','consume(x+origin,y+origin,s.normal,gap,triangleFeature);');c=c.replace('[](V,V,V,double){','[](V,V,V,double,uint32_t){');c=c.replace('[&](V,V,V,double gap){','[&](V,V,V,double gap,uint32_t){');c=c.replace('[&](V x,V y,V normal,double gap){','[&](V x,V y,V normal,double gap,uint32_t){');# typedlambdaelse coversall visitusers
marker='ContactManifold CookedContacts(';at=c.index(marker);c=c[:at]+'''MeshFeatureBudget BuildMeshFeatureBudget(const PrimitivePose& query,const PrimitivePose& mesh,double amount,V velocity,V angular,double radius,double duration,const MeshContactEpisode* episode) {
 MeshFeatureBudget result;std::vector<std::pair<uint32_t,double>> actual;
 visitCookedPolyContacts(query,mesh,0,[&](V,V,V,double gap,uint32_t feature){actual.push_back({feature,-gap});return false;},INFINITY,episode);
 PolyScratchLease lease;auto& pa=lease.frame.first;const V origin=PrimitiveCenter(mesh);polyInto(pa,query,origin);V lo(INFINITY),hi(-INFINITY),center(0);
 for(V p:pa.points){lo=glm::min(lo,p+origin);hi=glm::max(hi,p+origin);center+=p;}center/=double(pa.points.size());const V delta=velocity*duration;lo=glm::min(lo,lo+delta);hi=glm::max(hi,hi+delta);
 const auto selected=candidates(mesh,lo,hi,glm::length(angular)*radius*duration);const auto& asset=*mesh.shape.asset;const M rotation=PrimitiveRotation(mesh);
 for(auto feature:selected){const auto& face=asset.faces[feature];const V n=rotation*face.normal;const double plane=glm::dot(n,rotation*asset.vertices[face.vertices[0]]);double bottom=INFINITY;
  for(V p:pa.points)bottom=std::min(bottom,glm::dot(n,p));const double depth=plane-bottom,half=glm::dot(n,center)-bottom;
  double limit=std::min(std::max(0.,depth)+amount,.5*half);bool found=false;
  for(const auto& old:actual)if(old.first==feature){if(!found){limit=old.second+amount;found=true;}else limit=std::max(limit,old.second+amount);}
  result.limits.push_back({feature,limit});
 }
 return result;
}
bool MeshFeatureBudgetExceeded(const PrimitivePose& query,const PrimitivePose& mesh,const MeshFeatureBudget& budget,const MeshContactEpisode* episode) {
 return visitCookedPolyContacts(query,mesh,0,[&](V,V,V,double gap,uint32_t feature){const auto found=std::lower_bound(budget.limits.begin(),budget.limits.end(),feature,[](const auto& entry,uint32_t id){return entry.first<id;});return found!=budget.limits.end()&&found->first==feature&&-gap>found->second;},INFINITY,episode);
}
'''+c[at:];(s/'CollisionGeometry.cpp').write_text(c)
p=(s/'PhysicsWorld.cpp').read_text();marker='        const glm::dvec3 relative =';start=p.index('    double ImpactTime(');at=p.index(marker,start);p=p[:at]+'''        const bool meshContinuation=meshAdvance&&!MeshContactFeatures(firstMesh?secondPrimitive:firstPrimitive,firstMesh?firstPrimitive:secondPrimitive,Episode(a,b,pa,pb)).empty();
        MeshFeatureBudget meshBudget;
        if(meshContinuation){const unsigned moving=firstMesh?b:a;meshBudget=BuildMeshFeatureBudget(firstMesh?secondPrimitive:firstPrimitive,firstMesh?firstPrimitive:secondPrimitive,SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)}),glm::dvec3(firstMesh?posb.linearVelocity:posa.linearVelocity),glm::dvec3(DriftGenerator(moving,start)),bodies[moving].boundingRadius,finish-start,Episode(a,b,pa,pb));}
'''+p[at:];old='''            const bool touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);''';assert old in p;p=p.replace(old,'''            bool touching;
            if(meshContinuation){const auto first=PrimitiveAt(bodies[a].shape,aa,pa),second=PrimitiveAt(bodies[b].shape,bb,pb);touching=MeshFeatureBudgetExceeded(firstMesh?second:first,firstMesh?first:second,meshBudget,Episode(a,b,pa,pb));}
            else touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);''');p=p.replace('const int advanceIterations = target > 0 ? 0 : 64;', 'const int advanceIterations = target > 0 || meshContinuation ? 0 : 64;');(s/'PhysicsWorld.cpp').write_text(p)
