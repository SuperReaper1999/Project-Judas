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

// Target-directed finite ancestry. Backward traversal keeps only the actual
// welded component of a feature that is certified throughout the interval.
// A persistent disconnected component can never certify another component's
// missing history. No infinite-plane or whole-pair-touching inference is used.
inline UnionMeasurement TraceEndpoint(const PrimitivePose&query,const PrimitivePose&mesh,
    unsigned endpointFeature,const MeshContactEpisode&old,const RigidMotionSegment&segment,
    double start,double finish,unsigned maximumIntervals=1024){
    UnionMeasurement result;
    auto selected=MeshFiniteComponent(At(query,segment,finish),mesh,{endpointFeature});
    if(selected.empty()){result.measure.result=Result::Separated;return result;}
    const auto endpointComponent=selected;
    const auto walk=[&](const auto&self,double first,double last,unsigned depth)->Result{
        if(++result.measure.intervals>maximumIntervals)return Result::Uncertain;
        const auto from=At(query,segment,first);
        if(!MeshFiniteHasOverlap(from,mesh)){result.measure.separationTime=first;return Result::Separated;}
        if(Constant(segment,first,last)){
            ++result.measure.constantCertificates;return Result::Connected;
        }
        for(const auto feature:selected)if(NativeOverlapThroughout(query,mesh,feature,segment,first,last)){
            ++result.measure.pointCertificates;
            selected=MeshFiniteComponent(from,mesh,{feature});
            return selected.empty()?Result::Uncertain:Result::Connected;
        }
        const double middle=(first+last)*.5;
        if(depth>=40||middle<=first||middle>=last)return Result::Uncertain;
        if(!MeshFiniteHasOverlap(At(query,segment,middle),mesh)){result.measure.separationTime=middle;return Result::Separated;}
        const auto right=self(self,middle,last,depth+1);
        if(right!=Result::Connected)return right;
        // After proving the right interval, selected is the actual ancestry
        // component at its left boundary, not an unrelated native contact.
        if(selected.empty()){result.measure.separationTime=middle;return Result::Separated;}
        return self(self,first,middle,depth+1);
    };
    result.measure.result=walk(walk,start,finish,0);
    if(result.measure.result==Result::Connected){
        bool owned=false;for(auto f:selected)if(std::binary_search(old.fronts.begin(),old.fronts.end(),f)){owned=true;break;}
        if(owned)result.selected.fronts=endpointComponent;
        else result.measure.result=Result::Separated;
    }
    return result;
}
// RESEARCH ONLY. Independent ordinary-native acquisition supplies a legitimate
// later causal start; target-directed ancestry certifies every influenced face.
inline UnionMeasurement ReacquireUnion(const PrimitivePose&query,const PrimitivePose&mesh,
    const MeshContactEpisode&old,const RigidMotionSegment&segment,double start,double finish,
    unsigned maximumIntervals=1024){
    UnionMeasurement result;const auto endpoint=At(query,segment,finish);
    // All actual finite endpoint candidates may be reached by chronological
    // handoff even when their original seed no longer overlaps at the end.
    auto influential=MeshFiniteComponent(endpoint,mesh,{},true);
    influential.erase(std::remove_if(influential.begin(),influential.end(),[&](auto f){return NativeMeshSide(endpoint,mesh,f)>=0;}),influential.end());
    result.selected.fronts=NativeFrontFeatures(endpoint,mesh);
    if(influential.empty()){result.measure.result=Result::Connected;return result;}
    const auto begin=At(query,segment,start);const auto nativeBegin=NativeFrontFeatures(begin,mesh);
    const auto included=[&](unsigned feature){return std::binary_search(result.selected.fronts.begin(),result.selected.fronts.end(),feature);};
    const auto merge=[&](const MeshContactEpisode&other){auto&f=result.selected.fronts;f.insert(f.end(),other.fronts.begin(),other.fronts.end());std::sort(f.begin(),f.end());f.erase(std::unique(f.begin(),f.end()),f.end());};
    const auto add=[&](const UnionMeasurement&part){result.measure.intervals+=part.measure.intervals;result.measure.pointCertificates+=part.measure.pointCertificates;result.measure.constantCertificates+=part.measure.constantCertificates;};
    const auto remaining=[&](){return result.measure.intervals<maximumIntervals?maximumIntervals-result.measure.intervals:0;};
    bool unresolved=false;
    for(const auto feature:influential){
        if(included(feature))continue;
        bool acquired=false;
        if(std::binary_search(nativeBegin.begin(),nativeBegin.end(),feature)){
            double lo=start,hi=finish,witness=start;
            for(unsigned iteration=0;iteration<32;++iteration){
                const double middle=(lo+hi)*.5;if(middle<=lo||middle>=hi)break;
                ++result.reacquisitionSearches;const auto pose=At(query,segment,middle);
                if(NativeMeshSide(pose,mesh,feature)>=0){
                    lo=middle;const auto native=NativeFrontFeatures(pose,mesh);
                    if(std::binary_search(native.begin(),native.end(),feature))witness=middle;
                }else hi=middle;
            }
            if(witness>start){
                ++result.reacquisitionWitnesses;
                const auto suffix=TraceEndpoint(query,mesh,feature,MeshContactEpisode{{feature}},segment,witness,finish,remaining());add(suffix);
                if(suffix.measure.result==Result::Connected){merge(suffix.selected);acquired=true;}
            }
        }
        if(!acquired){
            const auto prior=TraceEndpoint(query,mesh,feature,old,segment,start,finish,remaining());add(prior);
            if(prior.measure.result==Result::Connected)merge(prior.selected);
            else if(prior.measure.result==Result::Uncertain)unresolved=true;
            // A proved missing ancestry expires only this feature's ownership;
            // independently acquired components remain intact.
        }
        if(result.measure.intervals>=maximumIntervals&&unresolved)break;
    }
    result.measure.result=unresolved?Result::Uncertain:Result::Connected;
    if(unresolved)result.selected.fronts.clear();
    return result;
}
} // namespace episode_probe
