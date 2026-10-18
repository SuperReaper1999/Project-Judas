#pragma once
#include "MeshContinuityProbe.h"
#include "NativeSATContinuity.h"

namespace episode_probe {
struct UnionMeasurement {
    Measurement measure;
    MeshContactEpisode selected;
    unsigned reacquisitionSearches=0, reacquisitionWitnesses=0;
};

inline bool AnyFiniteOverlap(const PrimitivePose &query,const PrimitivePose &mesh,
                            const std::vector<Plane> &) {return MeshFiniteHasOverlap(query,mesh);}

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
        if(!AnyFiniteOverlap(to,mesh,planes)){result.measure.separationTime=last;return Result::Separated;}
        bool certified=Constant(segment,first,last);
        if (certified) ++result.measure.constantCertificates;
        const double middle=(first+last)*.5;
        const auto midpoint=At(query,segment,middle);
        if (!certified) for (const auto feature:selected.fronts) {
            if(NativeOverlapThroughout(query,mesh,feature,segment,first,last)) {
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

// Native centre construction matches polyInto exactly, including represented
// vertex order and averaging. Do not replace it with a COM-side approximation.
inline double NativeMeshSide(const PrimitivePose&query,const PrimitivePose&mesh,unsigned feature){
    V center(0);const auto r=PrimitiveRotation(query);const auto c=PrimitiveCenter(query)-PrimitiveCenter(mesh);
    if(query.shape.type==ShapeType::Box){const V h(query.shape.halfExtents);for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1})center+=c+r*V(x*h.x,y*h.y,z*h.z);center/=8.;}
    else{for(V point:query.shape.asset->vertices)center+=c+r*point;center/=double(query.shape.asset->vertices.size());}
    const auto fixed=PrimitiveRotation(mesh);const auto&face=mesh.shape.asset->faces[feature];
    return glm::dot(fixed*face.normal,center-fixed*mesh.shape.asset->vertices[face.vertices[0]]);
}
inline std::vector<uint32_t> NativeFrontFeatures(const PrimitivePose&query,const PrimitivePose&mesh){
    auto result=MeshContactFeatures(query,mesh,nullptr);
    result.erase(std::remove_if(result.begin(),result.end(),[&](auto feature){return NativeMeshSide(query,mesh,feature)<0;}),result.end());
    return result;
}
// RESEARCH ONLY. A later actual native-front witness can independently acquire
// its own episode after an earlier gap. Certifying that witness's suffix avoids
// demanding historical continuity where ordinary sidedness was sufficient.
// Every currently influential inherited feature must be covered; disconnected
// unresolved components are never discarded merely because another acquires.
inline UnionMeasurement ReacquireUnion(const PrimitivePose&query,const PrimitivePose&mesh,
    const MeshContactEpisode&old,const RigidMotionSegment&segment,double start,double finish,
    unsigned maximumIntervals=1024){
    UnionMeasurement result;
    const auto endpoint=At(query,segment,finish);
    auto influential=MeshContactFeatures(endpoint,mesh,&old);
    influential.erase(std::remove_if(influential.begin(),influential.end(),[&](auto feature){return NativeMeshSide(endpoint,mesh,feature)>=0;}),influential.end());
    if(influential.empty())return ConnectedUnion(query,mesh,old,segment,start,finish,maximumIntervals);
    const auto begin=At(query,segment,start);const auto nativeBegin=NativeFrontFeatures(begin,mesh);
    result.selected.fronts=NativeFrontFeatures(endpoint,mesh);
    const auto merge=[&](const MeshContactEpisode&other){auto&f=result.selected.fronts;f.insert(f.end(),other.fronts.begin(),other.fronts.end());std::sort(f.begin(),f.end());f.erase(std::unique(f.begin(),f.end()),f.end());};
    const auto included=[&](unsigned feature){return std::binary_search(result.selected.fronts.begin(),result.selected.fronts.end(),feature);};
    for(const auto feature:influential){
        if(included(feature))continue;
        // No monotonicity inference: this is only a bounded search for an
        // actual witness. The suffix certificate independently proves there
        // was no hidden finite gap between that witness and the endpoint.
        if(!std::binary_search(nativeBegin.begin(),nativeBegin.end(),feature))continue;
        double lo=start,hi=finish,witness=start;
        auto acquisition=nativeBegin;
        for(unsigned iteration=0;iteration<32;++iteration){
            const double middle=(lo+hi)*.5;if(middle<=lo||middle>=hi)break;
            ++result.reacquisitionSearches;
            const auto pose=At(query,segment,middle);
            if(NativeMeshSide(pose,mesh,feature)>=0){
                lo=middle;auto native=NativeFrontFeatures(pose,mesh);
                if(std::binary_search(native.begin(),native.end(),feature)){witness=middle;acquisition=std::move(native);}
            }else hi=middle;
        }
        if(!(witness>start))continue;
        ++result.reacquisitionWitnesses;
        const unsigned remaining=result.measure.intervals<maximumIntervals?maximumIntervals-result.measure.intervals:0;
        auto suffix=ConnectedUnion(query,mesh,MeshContactEpisode{acquisition},segment,witness,finish,remaining);
        result.measure.intervals+=suffix.measure.intervals;
        result.measure.pointCertificates+=suffix.measure.pointCertificates;
        result.measure.constantCertificates+=suffix.measure.constantCertificates;
        if(suffix.measure.result==Result::Connected)merge(suffix.selected);
        if(result.measure.intervals>=maximumIntervals)break;
    }
    if(std::all_of(influential.begin(),influential.end(),included)){result.measure.result=Result::Connected;return result;}
    // Unresolved owners still require their original causal continuation. We
    // neither silently retain them through uncertainty nor invent a rear hit.
    const unsigned remaining=result.measure.intervals<maximumIntervals?maximumIntervals-result.measure.intervals:0;
    auto previous=ConnectedUnion(query,mesh,old,segment,start,finish,remaining);
    result.measure.intervals+=previous.measure.intervals;
    result.measure.pointCertificates+=previous.measure.pointCertificates;
    result.measure.constantCertificates+=previous.measure.constantCertificates;
    result.measure.result=previous.measure.result;result.measure.separationTime=previous.measure.separationTime;
    if(previous.measure.result==Result::Connected)result.selected=std::move(previous.selected);
    else result.selected.fronts.clear();
    return result;
}

} // namespace episode_probe
