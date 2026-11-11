#pragma once
#include "PhysicsWorld.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <vector>

// Diagnostic-only, bounded aggregation. No particle/model data or physical
// writes. Observer overhead is part of profiled step time, never headline A/B.
struct CrashCollisionSearchTrace {
    using Sample=PhysicsWorld::CollisionSearchSample;
    using Json=nlohmann::json;
    struct Pair {
        Sample last{};
        std::size_t calls=0,hits=0,updates=0,unchanged=0,unchangedUncached=0,containedMiss=0,changedStart=0,changedFinish=0;
        double milliseconds=0,advance=0,sampling=0,preparation=0,maximum=0;
        std::size_t queries=0,iterations=0,samples=0,fallbacks=0,geometry=0,
            visits=0,knots=0,allocations=0,geometryHits=0,geometryRequests=0,
            geometryEvictions=0,searchEvictions=0;
        std::array<std::size_t,3> contact{};
    };
    std::map<std::array<unsigned,4>,Pair> pairs;
    std::vector<Sample> worst;
    std::array<std::size_t,24> costHistogram{},durationHistogram{};
    std::size_t calls=0,droppedPairs=0;
    void Clear(){pairs.clear();worst.clear();costHistogram.fill(0);durationHistogram.fill(0);calls=droppedPairs=0;}
    static unsigned Bucket(double milliseconds) {
        unsigned bucket=0;double edge=.001;
        while(bucket<23 && milliseconds>edge){++bucket;edge*=2;}
        return bucket;
    }
    void Observe(const Sample& s) {
        ++calls;++costHistogram[Bucket(s.milliseconds)];++durationHistogram[Bucket((s.finish-s.start)*1000)];
        const std::array<unsigned,4> key{s.a,s.b,s.partA,s.partB};
        auto it=pairs.find(key);
        if(it==pairs.end()) {
            if(pairs.size()>=4096){++droppedPairs;return;}
            it=pairs.emplace(key,Pair{}).first;
        }
        auto& p=it->second;
        if(p.calls) {
            const bool same=p.last.revisionA==s.revisionA && p.last.revisionB==s.revisionB;
            if(!same)++p.updates;
            else {
                ++p.unchanged;p.unchangedUncached+=!s.cacheHit;
                p.changedStart+=p.last.start!=s.start;p.changedFinish+=p.last.finish!=s.finish;
                // Diagnostic opportunity count only: sampled negatives are NOT
                // promoted to certificates and this never skips a query.
                p.containedMiss+=p.last.target==s.target && p.last.hit>p.last.finish &&
                    s.start>=p.last.start && s.finish<=p.last.finish;
            }
        }
        ++p.calls;p.hits+=s.cacheHit;p.milliseconds+=s.milliseconds;
        p.advance+=s.advanceMs;p.sampling+=s.samplingMs;p.preparation+=s.preparationMs;
        p.maximum=std::max(p.maximum,s.milliseconds);++p.contact[unsigned(s.startContact+1)];
        p.queries+=s.queries;p.iterations+=s.iterations;p.samples+=s.samplingTests;
        p.fallbacks+=s.fallbacks;p.geometry+=s.geometryCalls;
        p.visits+=s.segmentVisits;p.knots+=s.segmentKnots;p.allocations+=s.segmentAllocations;
        p.geometryHits+=s.geometryHits;p.geometryRequests+=s.geometryRequests;
        p.geometryEvictions+=s.geometryEvictions;p.searchEvictions+=s.searchEvictions;p.last=s;
        if(worst.size()<32)worst.push_back(s);
        else {auto low=std::min_element(worst.begin(),worst.end(),[](const auto& a,const auto& b){return a.milliseconds<b.milliseconds;});if(s.milliseconds>low->milliseconds)*low=s;}
    }
    static Json Describe(const Sample& s) {
        return {{"a",s.a},{"b",s.b},{"pa",s.partA},{"pb",s.partB},
            {"typeA",int(s.typeA)},{"typeB",int(s.typeB)},
            {"pass",s.pass},{"sweep",s.sweep},{"event",s.event},{"mover",s.mover},{"initialSweep",s.initialSweep},
            {"compoundA",s.compoundA},{"compoundB",s.compoundB},
            {"start",s.start},{"finish",s.finish},{"target",s.target},
            {"hit",s.hit},{"startContact",s.startContact},{"cacheHit",s.cacheHit},
            {"revisionA",s.revisionA},{"revisionB",s.revisionB},
            {"ms",s.milliseconds},{"advanceMs",s.advanceMs},{"samplingMs",s.samplingMs},
            {"preparationMs",s.preparationMs},{"queries",s.queries},
            {"iterations",s.iterations},{"samples",s.samplingTests},{"fallbacks",s.fallbacks},
            {"geometryCalls",s.geometryCalls},{"poseEvaluations",s.poseEvaluations},
            {"orientationPreparations",s.orientationPreparations}};
    }
    Json Encode()const {
        Json rows=Json::array(),slow=Json::array();
        for(const auto& [_,p]:pairs){auto j=Describe(p.last);
            j["calls"]=p.calls;j["cacheHits"]=p.hits;j["trajectoryUpdates"]=p.updates;
            j["unchangedTrajectoryCalls"]=p.unchanged;j["unchangedUncachedCalls"]=p.unchangedUncached;
            j["containedPreviousMiss"]=p.containedMiss;j["unchangedChangedStart"]=p.changedStart;j["unchangedChangedFinish"]=p.changedFinish;
            j["totalMs"]=p.milliseconds;j["maximumMs"]=p.maximum;
            j["advanceMs"]=p.advance;j["samplingMs"]=p.sampling;j["preparationMs"]=p.preparation;
            j["queries"]=p.queries;j["iterations"]=p.iterations;j["samples"]=p.samples;
            j["fallbacks"]=p.fallbacks;j["geometryCalls"]=p.geometry;
            j["startContacts"]=p.contact;j["segmentVisits"]=p.visits;j["segmentKnots"]=p.knots;
            j["segmentAllocations"]=p.allocations;j["geometryHits"]=p.geometryHits;
            j["geometryRequests"]=p.geometryRequests;j["geometryEvictions"]=p.geometryEvictions;
            j["searchEvictions"]=p.searchEvictions;rows.push_back(std::move(j));}
        for(const auto& s:worst)slow.push_back(Describe(s));
        return {{"calls",calls},{"droppedPairs",droppedPairs},{"pairs",rows},{"worst",slow},
            {"costHistogramMs",costHistogram},{"intervalHistogramMs",durationHistogram},
            {"histogramFirstUpperMs",.001},{"histogramRatio",2}};
    }
};
