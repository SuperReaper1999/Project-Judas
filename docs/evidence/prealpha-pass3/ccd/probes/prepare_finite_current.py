from pathlib import Path
root=Path.cwd(); out=root/'.cache/prealpha-pass3/ccd/finite-current-v2';out.mkdir();source=out/'sources';source.mkdir()
h=(root/'src/CollisionGeometry.h').read_text();h=h.replace('double rotationRadius,double duration);','double rotationRadius,double duration, const std::vector<uint32_t>* excluded=nullptr);\nstd::vector<uint32_t> MeshContactFeatures(const PrimitivePose&,const PrimitivePose&);\nbool MeshSelectedHasContacts(const PrimitivePose&,const PrimitivePose&,const std::vector<uint32_t>&);');(source/'CollisionGeometry.h').write_text(h)
c=(root/'src/CollisionGeometry.cpp').read_text();c=c.replace('Consumer consume,double penetrationThreshold=INFINITY) {','Consumer consume,double penetrationThreshold=INFINITY,const std::vector<uint32_t>* excluded=nullptr,std::vector<uint32_t>* acceptedFeatures=nullptr) {')
c=c.replace('polyInto(pa,a,origin);auto test=', 'polyInto(pa,a,origin);uint32_t triangleFeature=UINT32_MAX;auto test=')
c=c.replace('if(gap<=margin){\n    return consume(x+origin', 'if(gap<=margin){\n    if(acceptedFeatures)acceptedFeatures->push_back(triangleFeature);\n    return consume(x+origin')
c=c.replace('for(auto i:candidates(b,lo,hi,margin)){\n   ++stats.triangles;', 'for(auto i:candidates(b,lo,hi,margin)){\n   if(excluded&&std::binary_search(excluded->begin(),excluded->end(),i))continue;\n   triangleFeature=i;\n   ++stats.triangles;')
c=c.replace('double rotationRadius,double duration) {','double rotationRadius,double duration,const std::vector<uint32_t>* excluded) {')
c=c.replace('for(unsigned feature:selected){\n  ++stats.triangles;', 'for(unsigned feature:selected){\n  if(excluded&&std::binary_search(excluded->begin(),excluded->end(),feature))continue;\n  ++stats.triangles;')
a=c.index('ContactManifold CookedContacts(')
c=c[:a]+'''std::vector<uint32_t> MeshContactFeatures(const PrimitivePose& query,const PrimitivePose& mesh) {
 std::vector<uint32_t> result;
 visitCookedPolyContacts(query,mesh,0,[](V,V,V,double){return false;},INFINITY,nullptr,&result);
 std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
bool MeshSelectedHasContacts(const PrimitivePose& query,const PrimitivePose& mesh,const std::vector<uint32_t>& excluded) {
 return visitCookedPolyContacts(query,mesh,0,[](V,V,V,double){return true;},INFINITY,&excluded);
}
'''+c[a:];(source/'CollisionGeometry.cpp').write_text(c)
p=(root/'.cache/prealpha-pass3/ccd/BASELINE_PhysicsWorld.cpp').read_text();start=p.index('    double ImpactTime(');end=p.index('\n    void ResolveInitialImpacts(',start);m=p[start:end];m=m.replace('++stats.impactQueries;','++stats.impactQueries;\n        double fallbackTime=finish+1;',1)
marker='''        const glm::dvec3 relative=glm::dvec3(ba.HasMotion()?posa.linearVelocity:glm::vec3(0))-glm::dvec3(bb.HasMotion()?posb.linearVelocity:glm::vec3(0));'''
insert='''        std::vector<uint32_t> existingMeshFeatures;
        if(meshAdvance&&target==0){
            existingMeshFeatures=MeshContactFeatures(firstMesh?secondPrimitive:firstPrimitive,firstMesh?firstPrimitive:secondPrimitive);
            if(!existingMeshFeatures.empty()){
                fallbackTime=ImpactTime(a,b,pa,pb,start,finish,SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)}));
                if(fallbackTime<=finish)finish=fallbackTime;
            }
        }
'''
assert m.count(marker)==1;m=m.replace(marker,insert+marker)
old='''            const bool touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);'''
new='''            bool touching;
            if(meshAdvance&&target==0){
                PhysicsWorkTimer geometryTimer(work.geometry,work.active);
                const auto first=PrimitiveAt(bodies[a].shape,aa,pa),second=PrimitiveAt(bodies[b].shape,bb,pb);
                touching=MeshSelectedHasContacts(firstMesh?second:first,firstMesh?first:second,existingMeshFeatures);
            }else touching=target>0?PairPenetrationExceeds(a,b,pa,pb,at,target):PairTouches(a,b,pa,pb,at);'''
assert m.count(old)==1;m=m.replace(old,new)
m=m.replace('bodies[moving].boundingRadius,finish-time);','bodies[moving].boundingRadius,finish-time,&existingMeshFeatures);')
m=m.replace('return finish+1;','return fallbackTime;')
p=p[:start]+m+p[end:]
a='''        auto schedule=[&](const std::array<unsigned,4>& key) {'''
b='''        const auto meshPair=[&](const std::array<unsigned,4>& key){
            const auto type=[&](unsigned slot,unsigned part){const auto& shape=bodies[slot].shape;return shape.type==ShapeType::CompoundBoxes?shape.boxes[part].type:shape.type;};
            const auto a=type(key[0],key[2]),b=type(key[1],key[3]);const auto poly=[](ShapeType t){return t==ShapeType::Box||t==ShapeType::ConvexHull;};
            return (a==ShapeType::TriangleMesh&&!bodies[key[0]].HasMotion()&&poly(b))||(b==ShapeType::TriangleMesh&&!bodies[key[1]].HasMotion()&&poly(a));
        };
        auto schedule=[&](const std::array<unsigned,4>& key) {'''
assert p.count(a)==1;p=p.replace(a,b)
p=p.replace('const double time=std::binary_search(supportPairs.begin(),supportPairs.end(),key) ? double(dt)+1 :','const double time=meshPair(key)?ImpactTime(key[0],key[1],int(key[2]),int(key[3]),now,dt):\n                std::binary_search(supportPairs.begin(),supportPairs.end(),key) ? double(dt)+1 :')
p=p.replace('for(const auto& key:spinningPairs) schedule(key);','for(const auto& key:spinningPairs) schedule(key);\n        for(const auto& key:supportPairs)if(meshPair(key))schedule(key);')
(source/'PhysicsWorld.cpp').write_text(p)
