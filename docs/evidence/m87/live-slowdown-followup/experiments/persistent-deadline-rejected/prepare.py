#!/usr/bin/env python3
from pathlib import Path
import difflib,hashlib,json
ROOT=Path('/home/conner/Documents/GitHub/Project-Judas')
OUT=Path(__file__).resolve().parent
source=(ROOT/'src/PhysicsWorld.cpp').read_text();s=source
s=s.replace('#include <optional>','#include <optional>\n#include <queue>',1)
s=s.replace('struct ScheduledImpact { std::array<unsigned,4> key; double time; };','''struct ScheduledImpact {
        std::array<unsigned,4> key; double time=0;
        double queryStart=0,target=0,certifiedThrough=0;
        bool pending=false;
        uint64_t revision=0;
        std::array<float,2> angularSpeeds{};
    };
    std::size_t impactDeadlineCalculations=0,impactDeadlineRefinements=0,impactDeadlineRetires=0,
        impactDeadlineFullQueries=0,impactDeadlinePending=0,impactDeadlineHeapRebuilds=0,
        impactDeadlineHeapPeakNodes=0,impactDeadlineHeapPeakBytes=0;''',1)
s=s.replace('double time=0,bool changedVelocity=false) {','double time=0,bool changedVelocity=false,const std::array<float,2>* frozenAngularSpeeds=nullptr) {',1)
a=s.index('    bool SpinsThroughSupport(');b=s.index('    // A quarter',a)
body=s[a:b].replace('double(glm::length(body.rigidBody.angularVelocity))','double(frozenAngularSpeeds&&!changedVelocity?(*frozenAngularSpeeds)[std::size_t(side)]:glm::length(body.rigidBody.angularVelocity))')
s=s[:a]+body+s[b:]
s=s.replace('double start,double finish,double target=0) {','double start,double finish,double target=0,const std::array<float,2>* frozenAngularSpeeds=nullptr) {',1)
s=s.replace('start,s.end,target);','start,s.end,target,frozenAngularSpeeds);',1).replace('s.end,finish,target);','s.end,finish,target,frozenAngularSpeeds);',1)
s=s.replace('finish-start,start) ?','finish-start,start,false,frozenAngularSpeeds) ?',1)
s=s.replace('SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)})) : finish+1;','SpinPenetrationTarget({a,b,unsigned(pa),unsigned(pb)}),frozenAngularSpeeds) : finish+1;',1)
a=s.index('        auto schedule=[&](const std::array<unsigned,4>& key) {');b=s.index('        for(const auto& key:separatedPairs)',a)
s=s[:a]+'''        impactDeadlineCalculations=impactDeadlineRefinements=impactDeadlineRetires=impactDeadlineFullQueries=
            impactDeadlinePending=impactDeadlineHeapRebuilds=impactDeadlineHeapPeakNodes=impactDeadlineHeapPeakBytes=0;
        struct DeadlineItem {double time;std::array<unsigned,4> key;std::size_t index;uint64_t revision;};
        struct DeadlineLater {
            bool operator()(const DeadlineItem& a,const DeadlineItem& b)const{
                return std::tie(a.time,a.key)>std::tie(b.time,b.key);
            }
        };
        struct DeadlineQueue:std::priority_queue<DeadlineItem,std::vector<DeadlineItem>,DeadlineLater>{
            using Base=std::priority_queue<DeadlineItem,std::vector<DeadlineItem>,DeadlineLater>;
            using Base::Base;
            std::size_t StorageBytes()const{return this->c.capacity()*sizeof(DeadlineItem);}
        } deadlines;
        const auto exactQuery=[&](ScheduledImpact& entry){
            const bool certified=entry.pending;
            const double previousCertificate=entry.certifiedThrough;
            ++impactDeadlineFullQueries;
            entry.time=ImpactTime(entry.key[0],entry.key[1],int(entry.key[2]),int(entry.key[3]),
                entry.queryStart,dt,entry.target,&entry.angularSpeeds);
            entry.pending=false;
            if(entry.time<now||(certified&&entry.time<=dt&&entry.time<=previousCertificate))
                throw std::runtime_error("deferred impact query contradicted geometric absence certificate");
        };
        const auto certify=[&](ScheduledImpact& entry,double through){
            ++impactDeadlineCalculations;
            const auto& a=RemainingImpactBound(entry.key[0],entry.queryStart,through);
            const auto& b=RemainingImpactBound(entry.key[1],entry.queryStart,through);
            if(!rigid_motion_bounds::FiniteBound(a)||!rigid_motion_bounds::FiniteBound(b)||a.Overlaps(b))return false;
            entry.certifiedThrough=through;
            if(through==dt){entry.pending=false;entry.time=double(dt)+1;++impactDeadlineRetires;}
            else {entry.pending=true;entry.time=std::nextafter(through,std::numeric_limits<double>::infinity());}
            return true;
        };
        const auto beginLazy=[&](ScheduledImpact& entry){
            const auto simple=[&](unsigned slot){const auto& body=bodies[slot];return
                (body.shape.type==ShapeType::Box||body.shape.type==ShapeType::Sphere)&&
                body.shape.pivotOffset==glm::vec3(0)&&std::isfinite(body.boundingRadius)&&body.boundingRadius>0;};
            const auto a=entry.key[0],b=entry.key[1];
            if(!simple(a)||!simple(b)||!(double(dt)>entry.queryStart))return false;
            const auto& aa=ImpactPoseAt(a,entry.queryStart);const auto& bb=ImpactPoseAt(b,entry.queryStart);
            const auto& boundA=ActualImpactBound(a,aa);const auto& boundB=ActualImpactBound(b,bb);
            if(!rigid_motion_bounds::FiniteBound(boundA)||!rigid_motion_bounds::FiniteBound(boundB))return false;
            double gap=0;
            for(int axis=0;axis<3;++axis)gap=std::max({gap,double(boundA.min[axis])-double(boundB.max[axis]),double(boundB.min[axis])-double(boundA.max[axis])});
            if(!(gap>0))return false; // Touching/overlapping AABBs retain the exact search immediately.
            const glm::dvec3 va=bodies[a].HasMotion()?glm::dvec3(aa.pose.linearVelocity):glm::dvec3(0),
                             vb=bodies[b].HasMotion()?glm::dvec3(bb.pose.linearVelocity):glm::dvec3(0);
            const double angular=(bodies[a].HasMotion()?glm::length(glm::dvec3(DriftGenerator(a,entry.queryStart)))*bodies[a].boundingRadius:0)+
                                 (bodies[b].HasMotion()?glm::length(glm::dvec3(DriftGenerator(b,entry.queryStart)))*bodies[b].boundingRadius:0);
            const double speed=glm::length(va-vb)+angular,remaining=double(dt)-entry.queryStart;
            if(!std::isfinite(speed)||speed<0)return false;
            // Only an efficiency hint. The actual represented trajectory must
            // pass the existing conservative sweep-bound certificate below.
            const double requested=speed>0?std::min(remaining,.5*gap/speed):remaining;
            if(!(requested>0)||!std::isfinite(requested))return false;
            double through=std::min(double(dt),entry.queryStart+requested);
            if(!(through>entry.queryStart))through=std::nextafter(entry.queryStart,double(dt));
            if(!certify(entry,through))return false;
            if(entry.pending)++impactDeadlinePending;
            return true;
        };
        const auto publish=[&](std::size_t index,ScheduledImpact entry){
            entry.revision=scheduledImpacts[index].revision+1;
            scheduledImpacts[index]=entry;
            if(entry.time<=dt)deadlines.push({entry.time,entry.key,index,entry.revision});
            impactDeadlineHeapPeakNodes=std::max(impactDeadlineHeapPeakNodes,deadlines.size());
            impactDeadlineHeapPeakBytes=std::max(impactDeadlineHeapPeakBytes,deadlines.StorageBytes());
            // Stale nodes are optional derived search state. Rebuild at a
            // bounded ratio without changing any live time/key/revision.
            if(deadlines.size()>3*scheduledImpacts.size()+256){
                std::vector<DeadlineItem> live;live.reserve(scheduledImpacts.size());
                for(std::size_t i=0;i<scheduledImpacts.size();++i){const auto& e=scheduledImpacts[i];if(e.time<=dt)live.push_back({e.time,e.key,i,e.revision});}
                DeadlineQueue replacement(DeadlineLater{},std::move(live));deadlines.swap(replacement);
                ++impactDeadlineHeapRebuilds;
            }
        };
        auto schedule=[&](const std::array<unsigned,4>& key) {
            if(!CanRespond(key[0],key[1]))return;
            ScheduledImpact entry;entry.key=key;entry.queryStart=now;entry.certifiedThrough=now;
            entry.angularSpeeds={glm::length(bodies[key[0]].rigidBody.angularVelocity),glm::length(bodies[key[1]].rigidBody.angularVelocity)};
            if(std::binary_search(supportPairs.begin(),supportPairs.end(),key))entry.time=double(dt)+1;
            else {
                if(std::binary_search(spinningPairs.begin(),spinningPairs.end(),key))entry.target=SpinPenetrationTarget(key);
                if(!beginLazy(entry))exactQuery(entry);
            }
            auto found=scheduledIndex.find(key);
            if(found==scheduledIndex.end()){
                const auto index=scheduledImpacts.size();scheduledIndex.emplace(key,index);
                scheduledImpacts.push_back({});publish(index,entry);
            }else publish(found->second,entry);
        };
''' +s[b:]
a=s.index('            double earliest=double(dt)+1; unsigned seedA=0,seedB=0;',s.index('        for(const auto& key:spinningPairs) schedule(key);'))
b=s.index('            now=earliest;',a)
s=s[:a]+'''            while(!deadlines.empty()){
                const auto& top=deadlines.top();
                if(top.index<scheduledImpacts.size()&&scheduledImpacts[top.index].revision==top.revision)break;
                deadlines.pop();
            }
            if(deadlines.empty())break;
            const auto next=deadlines.top();deadlines.pop();
            auto& chosen=scheduledImpacts[next.index];
            if(chosen.pending){
                ++impactDeadlineRefinements;
                const double span=chosen.certifiedThrough-chosen.queryStart;
                double through=std::min(double(dt),chosen.queryStart+2*span);
                if(!(through>chosen.certifiedThrough))through=std::nextafter(chosen.certifiedThrough,double(dt));
                if(!certify(chosen,through))exactQuery(chosen);
                publish(next.index,chosen);
                continue; // No physical time advances merely to refine a certificate.
            }
            const double earliest=next.time;
            const auto selected=next.key;
            const unsigned seedA=selected[0],seedB=selected[1];
            if(earliest>dt)break;
''' +s[b:]
# Private diagnostics only; no public binding or physics policy is introduced.
anchor='    JUDAS_PROFILE_COUNTER("Physics impact sampling fallbacks",double(w.stats.impactSamplingFallbacks),ProfileCounterMode::Latest);'
assert anchor in s
s=s.replace(anchor,anchor+'''
    JUDAS_PROFILE_COUNTER("Physics impact deadline calculations",double(w.impactDeadlineCalculations),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact deadline refinements",double(w.impactDeadlineRefinements),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact deadline retirements",double(w.impactDeadlineRetires),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact deadline full queries",double(w.impactDeadlineFullQueries),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact pending deadlines",double(w.impactDeadlinePending),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact deadline heap rebuilds",double(w.impactDeadlineHeapRebuilds),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact deadline heap peak nodes",double(w.impactDeadlineHeapPeakNodes),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact deadline heap peak bytes",double(w.impactDeadlineHeapPeakBytes),ProfileCounterMode::Latest);''',1)
(OUT/'PhysicsWorld.base.cpp').write_text(source)
(OUT/'PhysicsWorld.cpp').write_text(s)
(OUT/'persistent-lazy.diff').write_text(''.join(difflib.unified_diff(source.splitlines(True),s.splitlines(True),fromfile='src/PhysicsWorld.cpp',tofile='src/PhysicsWorld.cpp')))
(OUT/'provenance.json').write_text(json.dumps({'base_sha256':hashlib.sha256(source.encode()).hexdigest(),'variant_sha256':hashlib.sha256(s.encode()).hexdigest(),'base_source':str(ROOT/'src/PhysicsWorld.cpp'),'shared_source_modified':False},indent=2)+'\n')
print('Prepared isolated variant:',len(s)-len(source),'additional bytes')
