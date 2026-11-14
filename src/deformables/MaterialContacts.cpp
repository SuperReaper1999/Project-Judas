#include "Deformable.h"
#include "deformables/RigidResponse.h"
#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "Narrowphase.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
#include <limits>
using V=glm::dvec3;
void DeformableInstance::RigidContacts(double h,double fraction,PhysicsWorld& physics,std::array<BodyHandle,64>& awakened,unsigned& awakenedCount){
    JUDAS_PROFILE_SCOPE("Deformable rigid contacts");
    double thickness=settings.material.thickness;
    V materialMinimum=Minimum(),materialMaximum=Maximum();
    // One tree query per patch, then actual authoritative primitive shapes. Fat
    // bounds are only candidates. Samples include vertices, edge midpoints and
    // face centroids, bounding the supported thin-surface sampling envelope.
    stats.candidates+=rigidCandidates.size();
    // Rigid poses are frozen for this projection call. Reactions alter
    // velocity, not this call's collision geometry. Prepare each candidate's
    // primitives once rather than rebuilding them for every surface sample.
    const std::vector<PreparedRigidPrimitive>* contactPrimitives=nullptr;
    glm::dquat bodyRotation,bodyInverse;
    // This projection only updates node positions and rigid velocities. No
    // topology, contacts, sleep assessment or time advance occurs between its
    // samples. Publish wake propagation once per candidate body in this call,
    // sharing the existing interval publication set with dynamic attachments.
    // Overflow retains ordinary wake propagation rather than dropping it.
    bool candidateWoken=false;
    bool pointBoundsEligible=false;
    std::size_t rejectedPoints=0;
    Aabb pointBounds;
    V pointAllowance(0);
    // The projection loop changes rigid velocity, not pose or mass properties.
    // Prepare the existing effective-mass arithmetic lazily once per candidate.
    bool responsePrepared=false;
    double rigidInverseMass=0;
    V rigidCenter(0);
    glm::dmat3 rigidInverseInertia(0);
    const auto effectiveMass=[&](BodyHandle body,V point,V normal){
        if(!responsePrepared){
            responsePrepared=true;
            if(physics.IsDynamicBody(body)){
                const double mass=physics.GetMass(body);
                if(mass>0){
                    rigidInverseMass=1/mass;
                    rigidCenter=V(physics.GetCenterOfMass(body));
                    rigidInverseInertia=glm::dmat3(glm::inverse(physics.GetInertiaWorld(body)));
                }else rigidInverseMass=0;
            }else rigidInverseMass=0;
        }
        if(!(rigidInverseMass>0))return 0.;
        const auto angular=glm::cross(point-rigidCenter,normal);
        return rigidInverseMass+glm::dot(angular,rigidInverseInertia*angular);
    };
    auto sample=[&](glm::uvec3 nodes,V weights,BodyHandle body,const Shape& shape,const BodyTransform& pose){
        if(asset->fracture&&fracture.removed[asset->fracture->nodePart[nodes.x]])return;
        V p(0),old(0);
        for(int k=0;k<3;++k){p+=positions[nodes[k]]*weights[k];
            old+=start[nodes[k]]*weights[k];
            }
        if(pointBoundsEligible&&(glm::any(glm::lessThan(p+pointAllowance,V(pointBounds.min)))||
                                 glm::any(glm::greaterThan(p-pointAllowance,V(pointBounds.max))))){++rejectedPoints;return;}
        auto contact=[&](V center,glm::dquat q,glm::dquat inverse,V half,double radius,const PrimitivePose* geometry=nullptr){V normal,point;
            double depth=0;
            V local(0);
            if(!geometry)local=inverse*(p-center);
            if(geometry){
                auto result=geometry->shape.type==ShapeType::Terrain?PointGeometry(p,*geometry):SegmentGeometry(p,p,thickness,*geometry,thickness);
                if(geometry->shape.type==ShapeType::Terrain&&result.valid)result.gap=(result.contains?-result.gap:result.gap)-thickness;
                if(!result.valid||result.gap>=0)return;
                normal=result.normal;point=result.point;depth=-result.gap;
            }
            else if(radius>0){double len=glm::length(local);
                if(len>=radius+thickness)return;
                normal=len>1e-10?q*(local/len):q*V(1,0,0);
                point=center+normal*radius;
                depth=radius+thickness-len;
                }
            else{V nearest=glm::clamp(local,-half,half),d=local-nearest;
                double len=glm::length(d);
                if(len>=thickness)return;
                if(len>1e-10){normal=q*(d/len);
                    point=center+q*nearest;
                    depth=thickness-len;
                    }else{V gap=half-glm::abs(local);
                    unsigned axis=gap.x<gap.y?(gap.x<gap.z?0:2):(gap.y<gap.z?1:2);
                    V n(0);
                    n[axis]=local[axis]>=0?1:-1;
                    normal=q*n;
                    nearest=local;
                    nearest[axis]=n[axis]*half[axis];
                    point=center+q*nearest;
                    depth=gap[axis]+thickness;
                    }}
            double w=0;
            for(int k=0;k<3;++k)w+=inverseMass[nodes[k]]*weights[k]*weights[k];
            double rigid=effectiveMass(body,point,normal),den=w+rigid;
            if(den<1e-20)return;
            double dl=depth/den;
            for(int k=0;k<3;++k)positions[nodes[k]]+=normal*dl*inverseMass[nodes[k]]*weights[k];
            // This projection's velocity change IS the particle impulse. The
            // equal opposite impulse is submitted once, including lever arm.
            if(rigid>0)JudasDeformables::ApplyReaction(physics,body,-normal*dl/h,point,&candidateWoken);
            V velocity=(p-old)/h-JudasDeformables::PointVelocity(physics,body,point);
            V tangent=velocity-normal*glm::dot(velocity,normal);
            double speed=glm::length(tangent);
            if(speed>1e-10){V dir=tangent/speed;
                double tangentialMass=w+effectiveMass(body,point,dir);
                double frictionImpulse=std::min(speed/tangentialMass,settings.material.friction*dl/h);
                for(int k=0;k<3;++k)positions[nodes[k]]-=dir*frictionImpulse*h*inverseMass[nodes[k]]*weights[k];
                if(rigid>0)JudasDeformables::ApplyReaction(physics,body,dir*frictionImpulse,point,&candidateWoken);
                }
            // Keep the rejection envelope conservative as preceding contacts
            // project nodes. A once-per-call frozen box could miss a later body.
            for(unsigned k=0;k<3;++k){materialMinimum=glm::min(materialMinimum,positions[nodes[k]]);materialMaximum=glm::max(materialMaximum,positions[nodes[k]]);}
            ++stats.contacts;
        };
        if(shape.type==ShapeType::Sphere)contact(V(pose.position),bodyRotation,bodyInverse,V(0),shape.radius);
        else if(shape.type==ShapeType::Box)contact(V(pose.position),bodyRotation,bodyInverse,V(shape.halfExtents),0);
        else {
            for(const auto& prepared:*contactPrimitives) {
                const auto& primitive=prepared.geometry;
                if(primitive.shape.asset || primitive.shape.type==ShapeType::Terrain ||
                   primitive.shape.type==ShapeType::Capsule)
                    contact({}, {}, {}, {},0,&primitive);
                else contact(prepared.center,prepared.rotation,prepared.inverse,
                    V(primitive.shape.halfExtents),primitive.shape.type==ShapeType::Sphere?primitive.shape.radius:0);
            }
        }
    };
    // Cache a bounded small working set. Dense or large-compound candidates
    // retain the original per-call preparation; no geometry/contact is omitted.
    PreparedRigidContact fallback;
    for(size_t candidateIndex=0;candidateIndex<rigidCandidates.size();++candidateIndex){
        const auto body=rigidCandidates[candidateIndex];
        candidateWoken=false;responsePrepared=false;
        auto* entry=candidateIndex<rigidPreparation.size()?&rigidPreparation[candidateIndex]:&fallback;
        if(entry==&fallback){fallback.ready=fallback.valid=fallback.expanded=false;fallback.primitives.clear();}
        if(!entry->ready){
            auto& prepared=*entry;
            prepared.ready=true;
            if(std::any_of(carrierCollisionExclusions.begin(),carrierCollisionExclusions.end(),[&](auto other){return other.id==body.id&&other.world==body.world;}))continue;
            unsigned layer;CategoryMask mask;
            if(!physics.GetCollisionFilter(body,layer,mask)||(mask&CategoryBit(settings.collisionLayer))==0||physics.IsBodySensor(body))continue;
            if(!physics.GetBodyShape(body,prepared.shape,prepared.pose,false))continue;
            if(!currentRigidContactPose){auto prev=physics.GetPreviousTransform(body);prev.position+=prev.rotation*prepared.shape.pivotOffset;prepared.pose.position=glm::mix(prev.position,prepared.pose.position,float(fraction));prepared.pose.rotation=glm::slerp(prev.rotation,prepared.pose.rotation,float(fraction));}
            prepared.bounds=ShapeAabb(prepared.shape,prepared.pose.position,prepared.pose.rotation);
            prepared.rotation=glm::dquat(prepared.pose.rotation);prepared.inverse=glm::inverse(prepared.rotation);
            prepared.valid=true;
            JUDAS_PROFILE_COUNTER("Deformable rigid shape preparations",1,ProfileCounterMode::Sum);
        }else JUDAS_PROFILE_COUNTER("Deformable rigid shape preparation reuses",1,ProfileCounterMode::Sum);
        if(entry!=&fallback&&entry->valid&&PrimitiveCount(entry->shape)>32){
            fallback=std::move(*entry);*entry=PreparedRigidContact{};entry=&fallback;
        }
        auto& prepared=*entry;
        if(!prepared.valid)continue;
        auto& shape=prepared.shape;const auto& pose=prepared.pose;
        // Broadphase fat/swept padding is not a reason to evaluate every
        // surface sample against a body whose actual shape cannot touch it.
        // Bounds are available without faulting paged terrain geometry in.
        const auto& bounds=prepared.bounds;
        double magnitude=1;for(unsigned k=0;k<3;++k)magnitude=std::max({magnitude,std::abs(double(bounds.min[k])),std::abs(double(bounds.max[k]))});
        const V allowance(thickness+16*std::numeric_limits<float>::epsilon()*magnitude);
        if(glm::any(glm::lessThan(materialMaximum+allowance,V(bounds.min)))||glm::any(glm::greaterThan(materialMinimum-allowance,V(bounds.max))))continue;
        // Reuse the actual primitive enclosure for each current sample, not
        // merely the whole patch. Terrain and cooked/recovery paths retain
        // their full queries. The existing thickness/roundoff allowance keeps
        // boundary samples; no vertex, edge or face coverage is reduced.
        pointBoundsEligible=!shape.asset&&
            (shape.type==ShapeType::Box||shape.type==ShapeType::Sphere||
             (shape.type==ShapeType::CompoundBoxes&&std::all_of(shape.boxes.begin(),shape.boxes.end(),[](const auto& child){return !child.asset&&(child.type==ShapeType::Box||child.type==ShapeType::Sphere);})));
        pointBounds=bounds;pointAllowance=allowance;
        if(!prepared.expanded){
            shape.asset=AcquireCollision(shape.asset);
            if(shape.type!=ShapeType::Sphere&&shape.type!=ShapeType::Box){
                RigidBody parent;parent.position=pose.position;parent.orientation=pose.rotation;
                const int count=PrimitiveCount(shape);prepared.primitives.reserve(count);
                for(int i=0;i<count;++i){auto primitive=PrimitiveAt(shape,parent,i);const auto center=PrimitiveCenter(primitive);const auto rotation=glm::dquat(primitive.body.orientation);prepared.primitives.push_back({std::move(primitive),center,rotation,glm::inverse(rotation)});}
            }
            prepared.expanded=true;
        }
        bodyRotation=prepared.rotation;bodyInverse=prepared.inverse;contactPrimitives=&prepared.primitives;
        // Reuse the interval-owned attachment wake publication. All these
        // projections share unchanged rigid connectivity and no sleep boundary.
        // Overflow retains ordinary wake propagation, never drops reactions.
        const bool alreadyTracked=std::any_of(awakened.begin(),awakened.begin()+awakenedCount,[&](auto target){return target.id==body.id&&target.world==body.world;});
        candidateWoken=alreadyTracked;
        for(unsigned n=0;n<positions.size();++n)sample(glm::uvec3(n),V(1,0,0),body,shape,pose);
        for(auto edge:asset->edges)sample({edge.x,edge.y,edge.y},V(.5,.5,0),body,shape,pose);
        for(unsigned f=0;f<asset->triangles.size();++f)if(FaceVisible(f))sample(asset->triangles[f],V(1./3),body,shape,pose);
        if(candidateWoken&&!alreadyTracked&&awakenedCount<awakened.size())awakened[awakenedCount++]=body;
    }
    JUDAS_PROFILE_COUNTER("Deformable rigid point bound rejections",double(rejectedPoints),ProfileCounterMode::Sum);
}
