#pragma once
#include "FluidWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
#include <vector>

namespace FluidBoundaryDetail {
// Intervals of a world-space ray within the exact primitive union. Unlike
// centre membership, this retains partial support of a finite-height query.
// Direction need not be unit: t always parameterizes point + direction*t.
inline std::vector<std::pair<double,double>> SolidRayIntervals(
    const glm::vec3& point, const glm::dvec3& direction, const BodyTransform& pose,
    const std::vector<CompoundBox>& boxes, float sphereRadius, double lo, double hi) {
    std::vector<std::pair<double,double>> intervals;
    const auto inverse=glm::inverse(glm::dquat(pose.rotation));
    const auto p=inverse*(glm::dvec3(point)-glm::dvec3(pose.position));
    const auto d=inverse*direction;
    if(sphereRadius>0) {
        const double a=glm::dot(d,d),b=2*glm::dot(p,d);
        const double c=glm::dot(p,p)-double(sphereRadius)*sphereRadius;
        if(a==0) {if(c<=0)intervals.emplace_back(lo,hi);return intervals;}
        const double discriminant=b*b-4*a*c;
        if(discriminant<0)return intervals;
        const double root=std::sqrt(discriminant);
        const double q=-.5*(b+std::copysign(root,b));
        double first=q/a,second=q!=0?c/q:-b/(2*a);
        if(first>second)std::swap(first,second);
        first=std::max(first,lo);second=std::min(second,hi);
        if(first<=second)intervals.emplace_back(first,second);
        return intervals;
    }
    for(const auto& box:boxes) {
        const auto childInverse=glm::inverse(glm::dquat(box.rotation));
        const auto local=childInverse*(p-glm::dvec3(box.localCenter));
        const auto ray=childInverse*d;
        double first=lo,second=hi;
        bool intersect=true;
        for(int k=0;k<3;++k) {
            if(ray[k]==0) {if(std::abs(local[k])>box.halfExtents[k])intersect=false;continue;}
            double a=(-double(box.halfExtents[k])-local[k])/ray[k];
            double b=(double(box.halfExtents[k])-local[k])/ray[k];
            if(a>b)std::swap(a,b);
            first=std::max(first,a);second=std::min(second,b);
            if(first>second)intersect=false;
        }
        if(intersect)intervals.emplace_back(first,second);
    }
    std::sort(intervals.begin(),intervals.end());
    std::size_t count=0;
    for(const auto& span:intervals) {
        if(count && span.first<=intervals[count-1].second)
            intervals[count-1].second=std::max(intervals[count-1].second,span.second);
        else intervals[count++]=span;
    }
    intervals.resize(count);
    return intervals;
}

inline bool SameOwner(BodyHandle a, BodyHandle b) {
    return a.id == b.id && a.world == b.world;
}
inline double Distance(const glm::vec3& point, const BodyTransform& pose,
                       const glm::vec3& half) {
    const auto p = glm::inverse(glm::dquat(pose.rotation)) *
        (glm::dvec3(point)-glm::dvec3(pose.position));
    return glm::length(glm::max(glm::abs(p)-glm::dvec3(half),glm::dvec3(0)));
}
inline bool Inside(const glm::vec3& point, const BodyTransform& pose,
                   const std::vector<CompoundBox>& boxes, float sphereRadius) {
    const auto p = glm::inverse(glm::dquat(pose.rotation)) *
        (glm::dvec3(point)-glm::dvec3(pose.position));
    if (sphereRadius > 0) return glm::dot(p,p) <= double(sphereRadius)*sphereRadius;
    for (const auto& box : boxes) {
        const auto local = glm::inverse(glm::dquat(box.rotation))*(p-glm::dvec3(box.localCenter));
        if (glm::all(glm::lessThanEqual(glm::abs(local),glm::dvec3(box.halfExtents)))) return true;
    }
    return false;
}
}

namespace FluidBoundaryDetail {
inline std::optional<std::pair<double,double>> CellInterval(
    const FluidDisplacementMarker& cell, const glm::vec3& point,
    const glm::dvec3& direction, double lo, double hi) {
    const auto inverse=glm::inverse(glm::dquat(cell.rotation));
    const auto p=inverse*(glm::dvec3(point)-glm::dvec3(cell.position));
    const auto d=inverse*direction;
    for(int k=0;k<3;++k) {
        if(d[k]==0) {if(std::abs(p[k])>cell.halfWidth)return {};continue;}
        double a=(-cell.halfWidth-p[k])/d[k],b=(cell.halfWidth-p[k])/d[k];
        if(a>b)std::swap(a,b);
        lo=std::max(lo,a);hi=std::min(hi,b);
        if(lo>hi)return {};
    }
    return std::make_pair(lo,hi);
}
}
