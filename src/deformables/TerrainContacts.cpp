#include "Deformable.h"
#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>

// Cooked triangle surfaces deliberately do not invent inside/outside. Terrain
// owns that fact. Only nodes actually inside its published solid need this
// recovery, e.g. after a sculpt publishes above a previously resting sheet.
void DeformableInstance::RecoverTerrainContacts(double h,PhysicsWorld& physics){
    if(terrainRegions.empty())return;
    JUDAS_PROFILE_SCOPE("Embedded material terrain contacts");
    using V=glm::dvec3;
    for(unsigned node=0;node<positions.size();++node){
        if(inverseMass[node]==0)continue;
        for(const auto& region:terrainRegions){
            const V p=positions[node];if(!region.solid.Contains(glm::vec3(p)))continue;
            GeometryDistance nearest;
            for(auto body:region.bodies){
                unsigned layer;CategoryMask mask;
                if(!physics.GetCollisionFilter(body,layer,mask)||!(settings.collisionMask&CategoryBit(layer))||!(mask&CategoryBit(settings.collisionLayer))||physics.IsBodySensor(body))continue;
                Shape shape;BodyTransform pose;if(!physics.GetBodyShape(body,shape,pose,false))continue;
                auto bounds=ShapeAabb(shape,pose.position,pose.rotation);
                V delta=glm::max(V(bounds.min)-p,glm::max(p-V(bounds.max),V(0)));
                if(nearest.valid&&glm::dot(delta,delta)>nearest.gap*nearest.gap)continue;
                shape.asset=AcquireCollision(shape.asset);if(!shape.asset)continue;
                RigidBody parent;parent.position=pose.position;parent.orientation=pose.rotation;
                for(int part=0;part<PrimitiveCount(shape);++part){
                    auto result=PointGeometry(p,PrimitiveAt(shape,parent,part),nearest.valid?nearest.gap:INFINITY);
                    if(result.valid&&(!nearest.valid||result.gap<nearest.gap))nearest=result;
                }
            }
            if(!nearest.valid)continue; // No applicable collision layer/geometry.
            V outward=nearest.point-p;const double distance=glm::length(outward);
            if(distance<1e-12)outward=nearest.normal;else outward/=distance;
            const double depth=distance+settings.material.thickness;
            positions[node]+=outward*depth;
            // Match the ordinary static-contact Coulomb correction. The normal
            // projection contributes actual velocity through (position-start)/h.
            V velocity=(p-start[node])/h,tangent=velocity-outward*glm::dot(velocity,outward);
            const double speed=glm::length(tangent);
            if(speed>1e-10)positions[node]-=tangent/speed*std::min(speed*h,settings.material.friction*depth);
            ++stats.contacts;
        }
    }
}
