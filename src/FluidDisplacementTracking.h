#pragma once
#include "FluidBoundaryGeometry.h"
#include <optional>

struct FluidBoundarySolid {
    BodyHandle owner;
    BodyTransform pose;
    glm::vec3 half{0};
    float sphereRadius=0;
};

// A reproducible geometric cell frame from represented material layout. Cube
// axis permutations/signs are immaterial; the construction follows global
// rotations without assuming world-up. Degenerate clouds use the solid frame.
inline std::optional<glm::quat> FluidBoundaryFrame(const std::vector<FluidParticle>& particles) {
    if(particles.size()<3)return {};
    glm::dvec3 first(0);
    for(std::size_t i=1;i<particles.size();++i) {
        const auto edge=glm::dvec3(particles[i].position)-glm::dvec3(particles[0].position);
        if(glm::dot(edge,edge)==0)continue;
        if(glm::dot(first,first)==0){first=glm::normalize(edge);continue;}
        const auto transverse=edge-first*glm::dot(first,edge);
        if(glm::dot(transverse,transverse)<.25*glm::dot(edge,edge))continue;
        const auto second=glm::normalize(transverse),third=glm::cross(first,second);
        return glm::normalize(glm::quat_cast(glm::mat3(first,second,third)));
    }
    return {};
}

// Preserve the source cell cut by solid projection, anchored at its birth in
// the fluid chart. Only the current physical contact witness is updated. It is
// not advected with the wall's particle velocity: doing that invents a surface
// which follows a rising body. Departure or unsupported geometry expires it.
inline void UpdateFluidDisplacementMarker(
    FluidDisplacementMarker& marker, const glm::vec3& actual, const glm::vec3& push,
    BodyHandle contactOwner, const std::vector<FluidBoundarySolid>& solids,
    float clearance, float supportHalfWidth, const std::optional<glm::quat>& frame={}) {
    using namespace FluidBoundaryDetail;
    if(!contactOwner.IsValid())contactOwner=marker.owner;
    if(!contactOwner.IsValid()){marker={};return;}
    for(const auto& solid:solids)if(SameOwner(solid.owner,contactOwner)) {
        const double tolerance=8*std::numeric_limits<float>::epsilon()*
            (1+glm::length(glm::dvec3(actual))+glm::length(glm::dvec3(solid.pose.position)));
        const auto distance=[&](const glm::vec3& point) {
            return solid.sphereRadius>0 ?
                std::max(0.,glm::distance(glm::dvec3(point),glm::dvec3(solid.pose.position))-solid.sphereRadius) :
                Distance(point,solid.pose,solid.half);
        };
        if(distance(actual)>clearance+tolerance)continue;
        glm::vec3 witness;
        if(solid.sphereRadius>0) {
            const auto offset=actual-solid.pose.position;
            if(glm::dot(offset,offset)==0)continue;
            witness=solid.pose.position+glm::normalize(offset)*solid.sphereRadius;
        } else {
            const auto local=glm::inverse(solid.pose.rotation)*(actual-solid.pose.position);
            witness=solid.pose.position+solid.pose.rotation*glm::clamp(local,-solid.half,solid.half);
        }
        auto normal=glm::dvec3(actual)-glm::dvec3(witness);
        const double length=glm::length(normal);
        if(length>0)normal/=length;
        else {
            // A zero-clearance exact face needs no arbitrary world axis.
            const auto local=glm::inverse(solid.pose.rotation)*(actual-solid.pose.position);
            int face=0;
            for(int k=1;k<3;++k)if(solid.half[k]-std::abs(local[k])<solid.half[face]-std::abs(local[face]))face=k;
            glm::vec3 axis(0);axis[face]=local[face]<0?-1.f:1.f;
            normal=solid.pose.rotation*axis;
        }
        const double infinity=std::numeric_limits<double>::infinity();
        const bool retain=SameOwner(marker.owner,contactOwner) &&
            marker.halfWidth==supportHalfWidth && distance(marker.position)<=supportHalfWidth+tolerance &&
            bool(CellInterval(marker,witness,normal,-infinity,infinity));
        const auto candidate=retain ? marker.position : glm::vec3(glm::dvec3(actual)-glm::dvec3(push));
        if(distance(candidate)>supportHalfWidth+tolerance)continue;
        const auto rotation=retain ? marker.rotation : frame.value_or(solid.pose.rotation);
        marker={contactOwner,candidate,rotation,supportHalfWidth,actual,witness};
        return;
    }
    marker={};
}
