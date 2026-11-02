#include "FluidHydrostatics.h"
#include "FluidBoundaryGeometry.h"
#include "FluidSolidSupport.h"
#include <stdexcept>
#include <cmath>

namespace {
std::vector<std::pair<double,double>> OwnedSupport(
    const glm::vec3& point,const glm::vec3& up,float halfHeight,
    const BodyTransform& captured,const FluidSolidQuery& current) {
    const auto direction=glm::normalize(glm::dvec3(up));
    const auto& volume=current.volume;
    const auto old=FluidBoundaryDetail::SolidRayIntervals(point,direction,captured,
        volume.solidBoxes,volume.solidSphereRadius,-halfHeight,halfHeight);
    const auto now=FluidBoundaryDetail::SolidRayIntervals(point,direction,current.pose,
        volume.solidBoxes,volume.solidSphereRadius,-halfHeight,halfHeight);
    std::vector<std::pair<double,double>> result;
    for(const auto& a:old)for(const auto& b:now) {
        const double lo=std::max(a.first,b.first),hi=std::min(a.second,b.second);
        if(lo<=hi)result.emplace_back(lo,hi);
    }
    return result;
}

void ValidateBoundarySolid(BodyHandle owner, const FluidVolumeQuadrature& solid,
                           const BodyTransform& pose, bool hasMaterial) {
    const auto finite=[](auto v) {
        for(int i=0;i<static_cast<int>(v.length());++i)if(!std::isfinite(v[i]))return false;
        return true;
    };
    if (!owner.IsValid() || !hasMaterial || !finite(pose.position) ||
        !finite(pose.rotation) || (!std::isfinite(glm::dot(pose.rotation,pose.rotation)) || !(glm::dot(pose.rotation,pose.rotation)>0)) ||
        !std::isfinite(solid.solidSphereRadius) || solid.solidSphereRadius<0 ||
        (solid.solidSphereRadius==0 && solid.solidBoxes.empty()))
        throw std::invalid_argument("retained water boundary requires water and valid owned solid geometry");
    for(const auto& box : solid.solidBoxes) {
        if(box.type!=ShapeType::Box || !finite(box.localCenter) || !finite(box.halfExtents) ||
           !finite(box.rotation) || (!std::isfinite(glm::dot(box.rotation,box.rotation)) || !(glm::dot(box.rotation,box.rotation)>0)) ||
           glm::any(glm::lessThanEqual(box.halfExtents,glm::vec3(0))))
            throw std::invalid_argument("invalid retained water boundary solid");
    }
}
} // namespace

void FluidHydrostaticField::RetainDisplacedBoundary(
    BodyHandle owner, const FluidVolumeQuadrature& solid, const BodyTransform& pose,
    const std::vector<FluidParticle>& domainParticles) {
    ValidateBoundarySolid(owner,solid,pose,!m_particles.empty());
    // Construct and validate before changing the previous published snapshot.
    // This is geometry, not a second material particle field or pressure solve.
    auto domain=std::make_shared<FluidHydrostaticField>();
    domain->Build(domainParticles,m_density);
    OwnedBoundary replacement{owner,pose,solid.solidBoxes,solid.solidSphereRadius,std::move(domain),{}};
    for(auto& boundary : m_boundaries)if(FluidBoundaryDetail::SameOwner(owner,boundary.owner)) {
        boundary=std::move(replacement);
        return;
    }
    m_boundaries.push_back(std::move(replacement));
}

const FluidHydrostaticField::OwnedBoundary* FluidHydrostaticField::BoundaryAt(
    const glm::vec3&, const FluidSolidQuery* solid) const {
    // Match metadata here; finite captured/current support is intersected by
    // each caller. Centre membership cannot decide ownership of a whole slab.
    if(!solid || !solid->owner.IsValid())return nullptr;
    for(const auto& boundary : m_boundaries) {
        if(!FluidBoundaryDetail::SameOwner(boundary.owner,solid->owner))continue;
        if(boundary.sphereRadius!=solid->volume.solidSphereRadius ||
           boundary.boxes.size()!=solid->volume.solidBoxes.size())return nullptr;
        for(std::size_t i=0;i<boundary.boxes.size();++i) {
            const auto& a=boundary.boxes[i];const auto& b=solid->volume.solidBoxes[i];
            if(a.type!=b.type || a.localCenter!=b.localCenter || a.halfExtents!=b.halfExtents ||
               a.rotation!=b.rotation)return nullptr;
        }
        return &boundary;
    }
    return nullptr;
}

std::optional<float> FluidHydrostaticField::RetainedFraction(
    const glm::vec3& point, const glm::vec3& up, float halfHeight, float radius,
    const glm::vec3& tangent, const FluidSolidQuery* solid) const {
    const auto* boundary=BoundaryAt(point,solid);
    if(!boundary || !boundary->domain)return {};
    const auto owned=OwnedSupport(point,up,halfHeight,boundary->pose,*solid);
    if(owned.empty())return {};
    if(owned.size()==1 && owned.front().first<=-halfHeight && owned.front().second>=halfHeight)
        return boundary->domain->Query(point,up,halfHeight,radius,tangent,nullptr,true).fraction;
    // Owned geometry applies only to the part of this finite support lying in
    // BOTH captured and current solid. Remaining support uses current material;
    // stripping only owner identity prevents recursive retained-domain access.
    FluidSolidQuery exterior=*solid;exterior.owner={};
    const auto direction=glm::normalize(glm::dvec3(up));
    const auto sample=[&](const FluidHydrostaticField& source,double lo,double hi,bool retained) {
        if(!(hi>lo))return 0.;
        const auto center=glm::vec3(glm::dvec3(point)+direction*((lo+hi)*.5));
        const auto value=source.Query(center,up,static_cast<float>((hi-lo)*.5),radius,
                                      tangent,retained?nullptr:&exterior,true);
        return (hi-lo)*value.fraction;
    };
    double wet=0,cursor=-halfHeight;
    for(const auto& span:owned) {
        wet+=sample(*this,cursor,span.first,false);
        wet+=sample(*boundary->domain,span.first,span.second,true);
        cursor=span.second;
    }
    wet+=sample(*this,cursor,halfHeight,false);
    return static_cast<float>(wet/(2*halfHeight));
}

void FluidHydrostaticField::RetainDisplacementCells(
    BodyHandle owner, const FluidVolumeQuadrature& solid, const BodyTransform& pose,
    const std::vector<FluidDisplacementMarker>& cells) {
    ValidateBoundarySolid(owner,solid,pose,!m_particles.empty());
    const FluidSolidQuery query{solid,pose,owner};
    OwnedBoundary replacement{owner,pose,solid.solidBoxes,solid.solidSphereRadius,nullptr,{}};
    FluidHydrostaticDetail::PreparedSolidQuery pointGeometry({1,0,0},{0,1,0},{0,0,1},0,0,query);
    for(const auto& cell:cells) {
        const auto finite=[](auto v) {for(int k=0;k<static_cast<int>(v.length());++k)if(!std::isfinite(v[k]))return false;return true;};
        const float norm=glm::dot(cell.rotation,cell.rotation);
        if(!FluidBoundaryDetail::SameOwner(cell.owner,owner) || !(cell.halfWidth>0) ||
           !std::isfinite(cell.halfWidth) || !finite(cell.position) || !finite(cell.rotation) ||
           !finite(cell.materialPoint) || !finite(cell.contactPoint) || !std::isfinite(norm) ||
           std::abs(norm-1)>32*std::numeric_limits<float>::epsilon())
            throw std::invalid_argument("invalid displaced water cell provenance");
        // Verify an actual material sample, not a fabricated surface point.
        const auto found=m_cells.find(Cell(cell.materialPoint));
        bool material=false;
        if(found!=m_cells.end())for(const auto i:found->second) {
            const auto& particle=m_particles[i];
            const double rounding=8*std::numeric_limits<float>::epsilon()*(1+glm::length(glm::dvec3(particle.position)));
            if(particle.position==cell.materialPoint && particle.spacing*.5f==cell.halfWidth &&
               glm::distance(glm::dvec3(cell.contactPoint),glm::dvec3(particle.position))<=cell.halfWidth+rounding)material=true;
        }
        if(!material)continue;
        const auto normal=glm::dvec3(cell.materialPoint)-glm::dvec3(cell.contactPoint);
        const double infinity=std::numeric_limits<double>::infinity();
        if(glm::dot(normal,normal)>0 && !FluidBoundaryDetail::CellInterval(cell,cell.contactPoint,normal,-infinity,infinity))continue;
        pointGeometry.SelectSupport(cell.contactPoint);
        if(!pointGeometry.Touches(cell.contactPoint,0))continue;
        const auto rotation=glm::dquat(cell.rotation);
        FluidHydrostaticDetail::PreparedSolidQuery cellGeometry(rotation*glm::dvec3(1,0,0),
            rotation*glm::dvec3(0,1,0),rotation*glm::dvec3(0,0,1),2*cell.halfWidth,cell.halfWidth,query);
        if(cellGeometry.Occluded(cell.position))replacement.cells.push_back(cell);
    }
    for(auto& boundary:m_boundaries)if(FluidBoundaryDetail::SameOwner(boundary.owner,owner)) {
        boundary=std::move(replacement);return;
    }
    m_boundaries.push_back(std::move(replacement));
}

std::vector<std::pair<double,double>> FluidHydrostaticField::OwnedIntervals(
    const glm::vec3& point, const glm::vec3& up, float halfHeight,
    const FluidSolidQuery* solid) const {
    std::vector<std::pair<double,double>> intervals;
    const auto* boundary=BoundaryAt(point,solid);
    if(!boundary || boundary->cells.empty())return intervals;
    const auto owned=OwnedSupport(point,up,halfHeight,boundary->pose,*solid);
    if(owned.empty())return intervals;
    const FluidSolidQuery captured{solid->volume,boundary->pose,boundary->owner};
    FluidHydrostaticDetail::PreparedSolidQuery component({1,0,0},{0,1,0},{0,0,1},0,0,captured);
    const auto direction=glm::normalize(glm::dvec3(up));
    for(const auto& support:owned) {
        const auto selected=glm::vec3(glm::dvec3(point)+direction*((support.first+support.second)*.5));
        component.SelectSupport(selected);
        for(const auto& cell:boundary->cells) {
            if(!component.Touches(cell.contactPoint,0))continue;
            const auto span=FluidBoundaryDetail::CellInterval(cell,point,direction,support.first,support.second);
            if(span)intervals.push_back(*span);
        }
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
