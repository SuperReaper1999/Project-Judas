#pragma once
// Conservative, step-local shape bounds. They reject candidates only; actual
// contact geometry and the anchored RigidMotion trajectory remain authoritative.
#include "Narrowphase.h"
#include "RigidMotion.h"
#include "ContactGeometryInternal.h"
#include <optional>
#include <limits>
#include <cmath>
#include <cstring>

namespace rigid_motion_bounds {
inline bool Finite3(const glm::vec3& x) {
    return std::isfinite(x.x)&&std::isfinite(x.y)&&std::isfinite(x.z);
}
inline bool Finite4(const glm::quat& x) {
    return std::isfinite(x.w)&&std::isfinite(x.x)&&std::isfinite(x.y)&&std::isfinite(x.z);
}
inline bool SameQuat(const glm::quat& a,const glm::quat& b) {
    return std::memcmp(&a.w,&b.w,sizeof(float))==0 &&
           std::memcmp(&a.x,&b.x,sizeof(float))==0 &&
           std::memcmp(&a.y,&b.y,sizeof(float))==0 &&
           std::memcmp(&a.z,&b.z,sizeof(float))==0;
}
inline bool FiniteBound(const Aabb& b) {
    return Finite3(b.min)&&Finite3(b.max)&&b.min.x<=b.max.x&&b.min.y<=b.max.y&&b.min.z<=b.max.z;
}
// This deliberately covers only one segment. A future prescribed stop or
// an impulse is a real trajectory discontinuity, not an interpolated endpoint.
// `from` and `to` must be the segment's existing Evaluate(start/finish) poses.
inline std::optional<Aabb> TryTranslationSweepBound(const Shape& shape,
    const RigidMotionSegment& segment,const RigidBody& from,const RigidBody& to,
    double start,double finish) {
    if(!std::isfinite(start)||!std::isfinite(finish)||start>finish ||
       start<segment.begin || finish>segment.end ||
       !std::isfinite(float(finish-segment.begin)) ||
       !Finite3(segment.position)||!Finite3(segment.linearVelocity) ||
       !Finite3(from.position)||!Finite3(to.position) ||
       shape.pivotOffset!=glm::vec3(0))return std::nullopt;
    if(shape.type==ShapeType::Sphere) {
        if(!(shape.radius>0)||!std::isfinite(shape.radius))return std::nullopt;
        // The centred sphere's orientation cannot change its geometry.
    } else if(shape.type==ShapeType::Box) {
        if(segment.driftAngularVelocity!=glm::vec3(0)||
           !Finite4(segment.orientation)||!Finite3(shape.halfExtents)||
           glm::any(glm::lessThanEqual(shape.halfExtents,glm::vec3(0))))return std::nullopt;
        // For dynamic zero drift, RigidMotion skips normalization only when
        // the advanced quaternion has exactly the anchor's bits. In particular,
        // a signed-zero component can otherwise trigger normalization of a
        // slightly non-unit quaternion. Check the same zero arithmetic rather
        // than treating that path as proven constant orientation.
        const glm::quat omega(0.f,segment.driftAngularVelocity.x,
            segment.driftAngularVelocity.y,segment.driftAngularVelocity.z);
        const auto advanced=segment.orientation+(omega*segment.orientation)*.5f;
        if(!segment.prescribed&&!SameQuat(advanced,segment.orientation))return std::nullopt;
        if(!SameQuat(from.orientation,segment.orientation)||
           !SameQuat(to.orientation,segment.orientation))return std::nullopt;
    } else return std::nullopt;
    // Each binary32 centre component is p0 + v*float(t-begin), evaluated
    // monotonically on one segment. Multiplication by a fixed v reverses order
    // only when v<0; round-to-nearest multiplication/addition remain monotone.
    // Therefore all represented centres lie between the endpoint centres.
    // With invariant shape orientation, union of the already outward-rounded
    // endpoint ShapeAabbs encloses every interior shape without new tolerances.
    const auto bound=ShapeAabb(shape,from.position,from.orientation).Union(
        ShapeAabb(shape,to.position,to.orientation));
    return FiniteBound(bound)?std::optional<Aabb>(bound):std::nullopt;
}
inline Aabb IntersectConservativeBounds(const Aabb& sphere,const Aabb& tight) {
    const Aabb both{glm::max(sphere.min,tight.min),glm::min(sphere.max,tight.max)};
    // If a numeric/range inconsistency ever produces an empty intersection,
    // retain the established enclosure; do not use it to reject a collision.
    return FiniteBound(both)?both:sphere;
}
// A dynamic Cayley segment can also be enclosed without assuming a constant
// orientation. All quaternion intervals below follow the actual represented
// GLM operations; no speed threshold or geometric epsilon changes the CCD.
using contact_geometry::Iv;
inline float LoFloat(double value){float f=float(value);return double(f)>value?std::nextafter(f,-std::numeric_limits<float>::infinity()):f;}
inline float HiFloat(double value){float f=float(value);return double(f)<value?std::nextafter(f,std::numeric_limits<float>::infinity()):f;}
inline Iv Square(Iv v){const auto a=contact_geometry::Abs(v);return a*a;}
inline std::optional<Aabb> TryDynamicBoxSweepBound(const Shape& shape,const RigidMotionSegment& segment,
        const RigidBody& from,const RigidBody& to,double start,double finish){
    if(shape.type!=ShapeType::Box||shape.pivotOffset!=glm::vec3(0)||segment.prescribed||
       !std::isfinite(start)||!std::isfinite(finish)||start>finish||start<segment.begin||finish>segment.end||
       !std::isfinite(float(finish-segment.begin))||!Finite3(segment.position)||!Finite3(segment.linearVelocity)||
       !Finite3(segment.driftAngularVelocity)||!Finite4(segment.orientation)||!Finite3(from.position)||!Finite3(to.position)||
       !Finite3(shape.halfExtents)||glm::any(glm::lessThanEqual(shape.halfExtents,glm::vec3(0))))return std::nullopt;
    try{
        // omega*q0 is fixed on an anchored segment. Each component of the
        // UNnormalised binary32 quaternion is monotone in represented elapsed
        // time; its endpoint values enclose every intermediate raw component.
        const glm::quat omega(0.f,segment.driftAngularVelocity.x,segment.driftAngularVelocity.y,segment.driftAngularVelocity.z);
        auto raw=[&](double time){return segment.orientation+(omega*segment.orientation)*(.5f*float(time-segment.begin));};
        const auto first=raw(start),last=raw(finish);
        if(!Finite4(first)||!Finite4(last))return std::nullopt;
        const std::array<float,4> firstV{first.w,first.x,first.y,first.z},lastV{last.w,last.x,last.y,last.z},anchor{segment.orientation.w,segment.orientation.x,segment.orientation.y,segment.orientation.z};
        std::array<Iv,4> q;
        std::array<float,4> minimumMagnitude{},maximumMagnitude{},low{},high{};
        for(unsigned i=0;i<4;++i){
            low[i]=std::min(firstV[i],lastV[i]);high[i]=std::max(firstV[i],lastV[i]);
            minimumMagnitude[i]=low[i]<=0&&high[i]>=0?0.f:std::min(std::abs(low[i]),std::abs(high[i]));
            maximumMagnitude[i]=std::max(std::abs(low[i]),std::abs(high[i]));
        }
        // GLM length uses only positive squares/additions/sqrt. Evaluating the
        // SAME float expression at componentwise minimum and maximum absolute
        // values encloses every actual represented length by monotonicity.
        const float minimumLength=glm::length(glm::quat(minimumMagnitude[0],minimumMagnitude[1],minimumMagnitude[2],minimumMagnitude[3]));
        const float maximumLength=glm::length(glm::quat(maximumMagnitude[0],maximumMagnitude[1],maximumMagnitude[2],maximumMagnitude[3]));
        if(!(minimumLength>0)||!std::isfinite(maximumLength))return std::nullopt;
        const float inverseMinimum=1.f/maximumLength,inverseMaximum=1.f/minimumLength;
        if(!std::isfinite(inverseMaximum))return std::nullopt;
        bool unchangedPossible=true;
        for(unsigned i=0;i<4;++i)unchangedPossible=unchangedPossible&&low[i]<=anchor[i]&&high[i]>=anchor[i];
        for(unsigned i=0;i<4;++i){
            const std::array<float,4> product{low[i]*inverseMinimum,low[i]*inverseMaximum,high[i]*inverseMinimum,high[i]*inverseMaximum};
            q[i]={double(*std::min_element(product.begin(),product.end())),double(*std::max_element(product.begin(),product.end()))};
            if(unchangedPossible){q[i].lo=std::min(q[i].lo,double(anchor[i]));q[i].hi=std::max(q[i].hi,double(anchor[i]));}
        }
        // The anchor union also covers RigidMotion's exact-bit branch that
        // skips normalization for an unchanged represented quaternion. If any
        // raw component interval excludes its anchor, that branch is impossible.
        const auto w=q[0],x=q[1],y=q[2],z=q[3],two=Iv(2);
        const auto ww=Square(w),xx=Square(x),yy=Square(y),zz=Square(z);
        const auto d=ww+xx+yy+zz;if(!(d.lo>0))return std::nullopt;
        const contact_geometry::M<Iv> n={
            contact_geometry::V<Iv>{ww+xx-yy-zz,two*(x*y+w*z),two*(x*z-w*y)},
            contact_geometry::V<Iv>{two*(x*y-w*z),ww-xx+yy-zz,two*(y*z+w*x)},
            contact_geometry::V<Iv>{two*(x*z+w*y),two*(y*z-w*x),ww-xx-yy+zz}};
        Aabb bound;
        for(unsigned k=0;k<3;++k){
            Iv extent(0);for(unsigned j=0;j<3;++j)extent=extent+contact_geometry::Abs(n[j][k]/d)*Iv(double(shape.halfExtents[j]));
            const Iv center(double(std::min(from.position[k],to.position[k])),double(std::max(from.position[k],to.position[k])));
            bound.min[k]=LoFloat((center-extent).lo);bound.max[k]=HiFloat((center+extent).hi);
        }
        return FiniteBound(bound)?std::optional<Aabb>(bound):std::nullopt;
    }catch(const contact_geometry::ArithmeticFailure&){return std::nullopt;}
}
} // namespace rigid_motion_bounds
