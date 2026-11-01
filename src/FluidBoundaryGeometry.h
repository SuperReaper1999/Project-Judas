#pragma once
#include "FluidWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace FluidBoundaryDetail {
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
