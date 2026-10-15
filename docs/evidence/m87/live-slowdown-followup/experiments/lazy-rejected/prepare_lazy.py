from pathlib import Path
import difflib
source=Path('src/PhysicsWorld.cpp').read_text();s=source
s=s.replace('struct ScheduledImpact { std::array<unsigned,4> key; double time; };','''struct ScheduledImpact {
        std::array<unsigned,4> key; double time;
        double queryStart=0,target=0; bool pending=false;
        std::array<float,2> angularSpeeds{};
    };''')
s=s.replace('double time=0,bool changedVelocity=false) {','double time=0,bool changedVelocity=false,const std::array<float,2>* frozenAngularSpeeds=nullptr) {',1)
a=s.index('    bool SpinsThroughSupport(');b=s.index('    // A quarter',a)
body=s[a:b].replace('double(glm::length(body.rigidBody.angularVelocity))','double(frozenAngularSpeeds&&!changedVelocity?(*frozenAngularSpeeds)[std::size_t(side)]:glm::length(body.rigidBody.angularVelocity))')
s=s[:a]+body+s[b:]
s=s.replace('double start,double finish,double target=0) {','double start,double finish,double target=0,const std::array<float,2>* frozenAngularSpeeds=nullptr) {',1)
s=s.replace('start,s.end,target);','start,s.end,target,frozenAngularSpeeds);',1).replace('s.end,finish,target);','s.end,finish,target,frozenAngularSpeeds);',1)
s=s.replace('finish-start,start) ?','finish-start,start,false,frozenAngularSpeeds) ?',1)
s=s.replace('SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)})) : finish+1;','SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)}),frozenAngularSpeeds) : finish+1;',1)
a=s.index('        auto schedule=[&](const std::array<unsigned,4>& key) {');b=s.index('        for(const auto& key:separatedPairs)',a)
s=s[:a]+'''        auto schedule=[&](const std::array<unsigned,4>& key) {
            if(!CanRespond(key[0],key[1]))return;
            ScheduledImpact entry;entry.key=key;entry.queryStart=now;
            entry.time=now;entry.pending=true;
            entry.angularSpeeds={glm::length(bodies[key[0]].rigidBody.angularVelocity),
                                 glm::length(bodies[key[1]].rigidBody.angularVelocity)};
            if(std::binary_search(supportPairs.begin(),supportPairs.end(),key)){
                entry.pending=false;entry.time=double(dt)+1;
            }else if(std::binary_search(spinningPairs.begin(),spinningPairs.end(),key))entry.target=SpinPenetrationTarget(key);
            const auto found=scheduledIndex.find(key);
            if(found!=scheduledIndex.end())scheduledImpacts[found->second]=entry;
            else {scheduledIndex.emplace(key,scheduledImpacts.size());scheduledImpacts.push_back(entry);}
        };
        // This certifies only geometric absence in an inclusive prefix. It
        // does not shorten ImpactTime's original sampling/search interval.
        auto prefixSeparated=[&](const ScheduledImpact& event,double through){
            const auto simple=[&](unsigned slot){const auto& body=bodies[slot];return
                (body.shape.type==ShapeType::Box||body.shape.type==ShapeType::Sphere)&&
                body.shape.pivotOffset==glm::vec3(0)&&std::isfinite(body.boundingRadius)&&body.boundingRadius>0;};
            if(!simple(event.key[0])||!simple(event.key[1]))return false;
            const auto& a=RemainingImpactBound(event.key[0],event.queryStart,through);
            const auto& b=RemainingImpactBound(event.key[1],event.queryStart,through);
            return rigid_motion_bounds::FiniteBound(a)&&rigid_motion_bounds::FiniteBound(b)&&!a.Overlaps(b);
        };
''' +s[b:]
a=s.index('            for (const auto& event:scheduledImpacts) {',s.index('        for(const auto& key:spinningPairs) schedule(key);'));b=s.index('            if (earliest>dt) break;',a)
s=s[:a]+'''            auto select=[&](const ScheduledImpact& event){
                if(event.time<earliest||(event.time==earliest&&event.key<selected)){
                    earliest=event.time;selected=event.key;seedA=event.key[0];seedB=event.key[1];
                }
            };
            for(const auto& event:scheduledImpacts)if(!event.pending)select(event);
            for(auto& event:scheduledImpacts){
                if(!event.pending||event.time>earliest||event.time>dt)continue;
                if(earliest<=dt&&prefixSeparated(event,earliest)){
                    // Actual contact is absent even at earliest. Consequently
                    // this query cannot return a TOI <= earliest. Retain its
                    // original start/mode and frozen spin-precheck inputs.
                    event.time=std::nextafter(earliest,std::numeric_limits<double>::infinity());
                    continue;
                }
                event.time=ImpactTime(event.key[0],event.key[1],int(event.key[2]),int(event.key[3]),
                    event.queryStart,dt,event.target,&event.angularSpeeds);
                event.pending=false;
                if(event.time<now)throw std::runtime_error("deferred impact query contradicted geometric prefix certificate");
                select(event);
            }
''' +s[b:]
Path('/tmp/m87-tight-remaining-bound/PhysicsWorld.lazy.cpp').write_text(s)
Path('/tmp/m87-tight-remaining-bound/pending-lazy.diff').write_text(''.join(difflib.unified_diff(source.splitlines(True),s.splitlines(True),fromfile='src/PhysicsWorld.cpp',tofile='src/PhysicsWorld.cpp')))
print('Wrote isolated pending-lazy.diff:',len(s)-len(source),'new chars; source untouched')
