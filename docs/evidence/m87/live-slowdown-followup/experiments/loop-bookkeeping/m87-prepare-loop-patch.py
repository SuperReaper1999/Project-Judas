from pathlib import Path
import subprocess,difflib
source=subprocess.check_output(['git','show','HEAD:src/PhysicsWorld.cpp'],text=True)
Path('/tmp/m87-loop-base.cpp').write_text(source)
s=source
s=s.replace('#include <utility>\n','#include <utility>\n#include <unordered_set>\n',1)
a='''            std::vector<std::pair<unsigned,unsigned>> touching;
            {
'''
b='''            std::vector<std::pair<unsigned,unsigned>> touching;
            // Keep discovery/row order in the vector. Hash membership only
            // removes repeated queries at this unchanged event-time geometry.
            std::unordered_set<uint64_t> visitedPairs;
            struct DiscoveredManifold {
                std::size_t touchingIndex; int partA,partB; ContactManifold geometry;
            };
            std::vector<DiscoveredManifold> discoveredManifolds;
            const auto simplePart=[&](unsigned slot,int part){
                const auto& shape=bodies[slot].shape;
                const auto type=shape.type==ShapeType::CompoundBoxes?shape.boxes[std::size_t(part)].type:shape.type;
                return type==ShapeType::Box||type==ShapeType::Sphere;
            };
            {
'''
assert s.count(a)==1;s=s.replace(a,b)
a='''                    const auto pair=std::minmax(a,b);
                    if (std::find(touching.begin(),touching.end(),std::pair<unsigned,unsigned>(pair))!=touching.end()) continue;
                    bool hit=false;
                    for(int pa=0;pa<PrimitiveCount(bodies[a].shape);++pa)
                        for(int pb=0;pb<PrimitiveCount(bodies[b].shape);++pb)
                            hit=hit||PairTouches(a,b,pa,pb,now);
                    if(hit) {touching.emplace_back(pair);include(b);}
'''
b='''                    const auto pair=std::minmax(a,b);
                    const uint64_t pairKey=(uint64_t(pair.first)<<32)|pair.second;
                    if(visitedPairs.count(pairKey))continue;
                    bool hit=false,reliableMiss=true;
                    std::optional<DiscoveredManifold> discovered;
                    for(int pa=0;pa<PrimitiveCount(bodies[a].shape);++pa)
                        for(int pb=0;pb<PrimitiveCount(bodies[b].shape);++pb){
                            if(hit)continue; // Preserve the existing first-hit short circuit.
                            if(simplePart(a,pa)&&simplePart(b,pb)){
                                auto geometry=PairAt(a,b,pa,pb,now,0);
                                hit=geometry.count>0;
                                reliableMiss&=!geometry.uncertain;
                                // Response is always in ascending body order.
                                // A reversed discovery must retain the original
                                // canonical response query, not flip its rows.
                                if(hit&&a==pair.first)
                                    discovered=DiscoveredManifold{touching.size(),pa,pb,std::move(geometry)};
                            }else{
                                hit=PairTouches(a,b,pa,pb,now);
                                // Specialized Boolean paths stay unchanged;
                                // a miss carries no public uncertainty proof.
                                reliableMiss=false;
                            }
                        }
                    if(hit){
                        visitedPairs.insert(pairKey);
                        if(discovered)discoveredManifolds.push_back(std::move(*discovered));
                        touching.emplace_back(pair);include(b);
                    }else if(reliableMiss){
                        // Only a resolved primitive separation may suppress
                        // the reverse traversal's identical existence query.
                        visitedPairs.insert(pairKey);
                    }
'''
assert s.count(a)==1;s=s.replace(a,b)
a='''            for (const auto& pair:touching) {
                auto& a=bodies[pair.first];auto& b=bodies[pair.second];
                for(int pa=0;pa<PrimitiveCount(a.shape);++pa)
                    for(int pb=0;pb<PrimitiveCount(b.shape);++pb) {
                        const auto m=PairAt(pair.first,pair.second,pa,pb,now,0);
'''
b='''            std::size_t discoveredIndex=0;
            for(std::size_t touchingIndex=0;touchingIndex<touching.size();++touchingIndex){
                const auto& pair=touching[touchingIndex];
                const DiscoveredManifold* discovered=discoveredIndex<discoveredManifolds.size()&&
                    discoveredManifolds[discoveredIndex].touchingIndex==touchingIndex?
                    &discoveredManifolds[discoveredIndex++]:nullptr;
                auto& a=bodies[pair.first];auto& b=bodies[pair.second];
                for(int pa=0;pa<PrimitiveCount(a.shape);++pa)
                    for(int pb=0;pb<PrimitiveCount(b.shape);++pb) {
                        const auto m=discovered&&discovered->partA==pa&&discovered->partB==pb?
                            discovered->geometry:PairAt(pair.first,pair.second,pa,pb,now,0);
'''
assert s.count(a)==1;s=s.replace(a,b)
a='''                changed.push_back(slot);
                b.motion.ChangeVelocity(b.rigidBody,now);
'''
b='''                changed.push_back(slot);
                // Membership remains nonzero; value two also marks the
                // changed subset without scanning it for every scheduled pair.
                included[slot]=2;
                b.motion.ChangeVelocity(b.rigidBody,now);
'''
assert s.count(a)==1;s=s.replace(a,b)
a='''            for(const auto& event:scheduledImpacts)
                if(std::find(changed.begin(),changed.end(),event.key[0])!=changed.end() ||
                   std::find(changed.begin(),changed.end(),event.key[1])!=changed.end()) dirty.push_back(event.key);
'''
b='''            for(const auto& event:scheduledImpacts)
                if(included[event.key[0]]==2||included[event.key[1]]==2)dirty.push_back(event.key);
'''
assert s.count(a)==1;s=s.replace(a,b)
Path('/tmp/m87-loop-scalability.cpp').write_text(s)
patch=''.join(difflib.unified_diff(source.splitlines(keepends=True),s.splitlines(keepends=True),fromfile='a/src/PhysicsWorld.cpp',tofile='b/src/PhysicsWorld.cpp'))
Path('/tmp/m87-loop-scalability.patch').write_text(patch)
print('patch lines',len(patch.splitlines()))
