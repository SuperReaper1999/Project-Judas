#pragma once
#include "MeshContinuityProbe.h"

namespace episode_probe {
struct UnionMeasurement {
    Measurement measure;
    MeshContactEpisode selected;
};

inline bool AnyFiniteOverlap(const PrimitivePose &query,const PrimitivePose &mesh,
                            const std::vector<Plane> &planes) {
    const auto qr=PrimitiveRotation(query),inverse=glm::transpose(PrimitiveRotation(mesh));
    const V center=PrimitiveCenter(query)-PrimitiveCenter(mesh);
    V lo(INFINITY),hi(-INFINITY);
    const auto point=[&](V p){const auto local=inverse*(center+qr*p);
        lo=glm::min(lo,local);hi=glm::max(hi,local);};
    if(query.shape.type==ShapeType::Box) for(unsigned i=0;i<8;++i)
        point(V(query.shape.halfExtents)*V(i&1?1:-1,i&2?1:-1,i&4?1:-1));
    else for(V p:query.shape.asset->vertices) point(p);
    std::vector<uint32_t> candidates;mesh.shape.asset->Candidates(lo,hi,candidates);
    for(auto feature:candidates) if(!Intersection(query,mesh,feature,planes).empty())return true;
    return false;
}

// Chronological finite-patch continuation. A certificate supplies a genuine
// fixed point on an owned finite triangle throughout its interval. Ownership
// can cross a welded edge only at a certified boundary where that actual edge
// lies inside the moving body. A separated interval never inherits its side.
inline UnionMeasurement ConnectedUnion(const PrimitivePose &query,
    const PrimitivePose &mesh, const MeshContactEpisode &old,
    const RigidMotionSegment &segment, double start, double finish,
    unsigned maximumIntervals = 1024) {
    UnionMeasurement result;
    const auto planes = Planes(query.shape);
    if (planes.empty()) return result;
    auto initial = MeshContactFeatures(At(query,segment,start),mesh,&old);
    if (initial.empty()) { result.measure.result=Result::Separated; return result; }
    MeshContactEpisode selected{std::move(initial)};
    const auto walk = [&](const auto &self,double first,double last,unsigned depth) -> Result {
        if (++result.measure.intervals>maximumIntervals) return Result::Uncertain;
        const auto from=At(query,segment,first),to=At(query,segment,last);
        bool certified=Constant(segment,first,last);
        if (certified) ++result.measure.constantCertificates;
        const double middle=(first+last)*.5;
        const auto midpoint=At(query,segment,middle);
        if (!certified) for (const auto feature:selected.fronts) {
            auto polygon=Intersection(midpoint,mesh,feature,planes);
            if (polygon.empty()) continue;
            V witness(0); for (V point:polygon) witness+=point;
            witness/=double(polygon.size());
            const auto fixed=TriangleWitness(witness,mesh,feature);
            if (fixed&&ContainsThroughout(*fixed,query,mesh,segment,planes,first,last)) {
                certified=true; ++result.measure.pointCertificates; break;
            }
        }
        if (certified) {
            selected.fronts=MeshContactFeatures(to,mesh,&selected);
            if (selected.fronts.empty()) {
                result.measure.separationTime=last; return Result::Separated;
            }
            return Result::Connected;
        }
        // Detect a genuine finite gap, including intervals whose endpoint
        // geometry both overlaps. This is a union test, not an infinite plane.
        if (!AnyFiniteOverlap(midpoint,mesh,planes)) {
            result.measure.separationTime=middle; return Result::Separated;
        }
        if (depth>=40||middle<=first||middle>=last) return Result::Uncertain;
        const auto left=self(self,first,middle,depth+1);
        return left==Result::Connected?self(self,middle,last,depth+1):left;
    };
    result.measure.result=walk(walk,start,finish,0);
    if (result.measure.result==Result::Connected) result.selected=std::move(selected);
    return result;
}
} // namespace episode_probe
