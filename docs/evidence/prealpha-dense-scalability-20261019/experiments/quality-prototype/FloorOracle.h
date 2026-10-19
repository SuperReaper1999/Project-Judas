#pragma once
#include "PhysicsWorld.h"
#include "Contacts.h"
#include <array>
#include <cmath>

namespace temporal_quality {
struct FloorFrame {
    bool valid=false;
    glm::dvec3 centre{0},half{0};
    glm::dmat3 rotation{1};
};
inline FloorFrame ReadFloor(const PhysicsWorld& physics, BodyHandle handle) {
    FloorFrame frame;
    Shape shape;BodyTransform pose;
    if(!physics.GetBodyShape(handle,shape,pose) || shape.type!=ShapeType::Box ||
       shape.pivotOffset!=glm::vec3(0) || shape.asset || physics.GetMass(handle)>0) return frame;
    frame.centre=glm::dvec3(pose.position);
    frame.half=glm::dvec3(shape.halfExtents);
    frame.rotation=ContactRotation(pose.rotation);
    frame.valid=true;
    for(unsigned axis=0;axis<3;++axis)
        frame.valid &= std::isfinite(frame.centre[axis]) && std::isfinite(frame.half[axis]) &&
                       frame.half[axis]>0 && std::isfinite(frame.rotation[axis][0]) &&
                       std::isfinite(frame.rotation[axis][1]) && std::isfinite(frame.rotation[axis][2]);
    return frame;
}
struct FloorObservation {
    bool valid=false,centreInside=false,fullyInside=false;
    double clearance=0;
};
inline FloorObservation ObserveFloor(const FloorFrame& floor,const Shape& shape,const BodyTransform& pose) {
    FloorObservation result;
    if(!floor.valid || shape.pivotOffset!=glm::vec3(0) || shape.asset ||
       (shape.type!=ShapeType::Box && shape.type!=ShapeType::Sphere)) return result;
    const glm::dvec3 centre=glm::transpose(floor.rotation)*(glm::dvec3(pose.position)-floor.centre);
    glm::dvec3 extents(0);
    if(shape.type==ShapeType::Sphere) extents=glm::dvec3(shape.radius);
    else {
        const auto rotation=ContactRotation(pose.rotation);
        for(unsigned axis=0;axis<3;++axis)
            for(unsigned bodyAxis=0;bodyAxis<3;++bodyAxis)
                extents[axis]+=std::abs(glm::dot(floor.rotation[axis],rotation[bodyAxis]))*shape.halfExtents[bodyAxis];
    }
    result.valid=true;
    for(unsigned axis=0;axis<3;++axis)
        result.valid &= std::isfinite(centre[axis]) && std::isfinite(extents[axis]) && extents[axis]>=0;
    if(!result.valid)return result;
    result.centreInside=std::abs(centre.x)<=floor.half.x && std::abs(centre.z)<=floor.half.z;
    result.fullyInside=std::abs(centre.x)+extents.x<=floor.half.x &&
                       std::abs(centre.z)+extents.z<=floor.half.z;
    result.clearance=centre.y-floor.half.y-extents.y;
    return result;
}
} // namespace temporal_quality
