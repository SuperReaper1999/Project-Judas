#pragma once
// Proposal only: isolated M87 investigation; no engine source is changed.
#include "Narrowphase.h"
#include "RigidMotion.h"
#include <optional>
#include <cmath>
#include <cstring>

namespace m87_proposal {
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
inline std::optional<Aabb> TightRemainingBound(const Shape& shape,
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
inline Aabb IntersectProvenBounds(const Aabb& sphere,const Aabb& tight) {
    const Aabb both{glm::max(sphere.min,tight.min),glm::min(sphere.max,tight.max)};
    // If a numeric/range inconsistency ever produces an empty intersection,
    // retain the established enclosure; do not use it to reject a collision.
    return FiniteBound(both)?both:sphere;
}
} // namespace m87_proposal
