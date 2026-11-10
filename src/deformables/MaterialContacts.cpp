#include "Deformable.h"
#include "deformables/RigidResponse.h"
#include "CollisionGeometry.h"
#include "Narrowphase.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
using V=glm::dvec3;
void DeformableInstance::RigidContacts(double h,double fraction,PhysicsWorld& physics){
    JUDAS_PROFILE_SCOPE("Deformable rigid contacts");
    double thickness=settings.material.thickness;
    // One tree query per patch, then actual authoritative primitive shapes. Fat
    // bounds are only candidates. Samples include vertices, edge midpoints and
    // face centroids, bounding the supported thin-surface sampling envelope.
    stats.candidates+=rigidCandidates.size();
    auto sample=[&](glm::uvec3 nodes,V weights,BodyHandle body,const Shape& shape,const BodyTransform& pose){
        if(asset->fracture&&fracture.removed[asset->fracture->nodePart[nodes.x]])return;
        V p(0),old(0);
        for(int k=0;k<3;++k){p+=positions[nodes[k]]*weights[k];
            old+=start[nodes[k]]*weights[k];
            }
        auto contact=[&](V center,glm::dquat q,V half,double radius,const PrimitivePose* geometry=nullptr){V normal,point;
            double depth=0;
            V local(0);
            if(!geometry)local=glm::inverse(q)*(p-center);
            if(geometry){
                auto result=geometry->shape.type==ShapeType::Terrain?PointGeometry(p,*geometry):SegmentGeometry(p,p,thickness,*geometry,thickness);
                if(geometry->shape.type==ShapeType::Terrain&&result.valid)result.gap=(result.contains?-result.gap:result.gap)-thickness;
                if(!result.valid||result.gap>=0)return;normal=result.normal;point=result.point;depth=-result.gap;
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
            double rigid=JudasDeformables::EffectiveMass(physics,body,point,normal),den=w+rigid;
            if(den<1e-20)return;
            double dl=depth/den;
            for(int k=0;k<3;++k)positions[nodes[k]]+=normal*dl*inverseMass[nodes[k]]*weights[k];
            // This projection's velocity change IS the particle impulse. The
            // equal opposite impulse is submitted once, including lever arm.
            if(rigid>0)JudasDeformables::ApplyReaction(physics,body,-normal*dl/h,point);
            V velocity=(p-old)/h-JudasDeformables::PointVelocity(physics,body,point);
            V tangent=velocity-normal*glm::dot(velocity,normal);
            double speed=glm::length(tangent);
            if(speed>1e-10){V dir=tangent/speed;
                double tangentialMass=w+JudasDeformables::EffectiveMass(physics,body,point,dir);
                double frictionImpulse=std::min(speed/tangentialMass,settings.material.friction*dl/h);
                for(int k=0;k<3;++k)positions[nodes[k]]-=dir*frictionImpulse*h*inverseMass[nodes[k]]*weights[k];
                if(rigid>0)JudasDeformables::ApplyReaction(physics,body,dir*frictionImpulse,point);
                }++stats.contacts;
        };
        if(shape.type==ShapeType::Sphere)contact(V(pose.position),glm::dquat(pose.rotation),V(0),shape.radius);
        else if(shape.type==ShapeType::Box)contact(V(pose.position),glm::dquat(pose.rotation),V(shape.halfExtents),0);
        else if(shape.type==ShapeType::Capsule||shape.type==ShapeType::Terrain){
            RigidBody parent;parent.position=pose.position;parent.orientation=pose.rotation;
            auto primitive=PrimitiveAt(shape,parent,0);contact({}, {}, {},0,&primitive);
        }
        else if(shape.type==ShapeType::CompoundBoxes||shape.asset){RigidBody parent;parent.position=pose.position;parent.orientation=pose.rotation;
            for(int i=0;i<PrimitiveCount(shape);++i){auto primitive=PrimitiveAt(shape,parent,i);if(primitive.shape.asset)contact({}, {}, {},0,&primitive);else contact(PrimitiveCenter(primitive),glm::dquat(primitive.body.orientation),V(primitive.shape.halfExtents),primitive.shape.type==ShapeType::Sphere?primitive.shape.radius:0);}
        }
    };
    for(auto body:rigidCandidates){
        if(std::any_of(carrierCollisionExclusions.begin(),carrierCollisionExclusions.end(),[&](auto other){return other.id==body.id&&other.world==body.world;}))continue;
        unsigned layer;
        CategoryMask mask;
        if(!physics.GetCollisionFilter(body,layer,mask)||(mask&CategoryBit(settings.collisionLayer))==0||physics.IsBodySensor(body))continue;
        Shape shape;
        BodyTransform pose;
        if(!physics.GetBodyShape(body,shape,pose))continue;
        auto prev=physics.GetPreviousTransform(body);
        prev.position+=prev.rotation*shape.pivotOffset;
        pose.position=glm::mix(prev.position,pose.position,float(fraction));
        pose.rotation=glm::slerp(prev.rotation,pose.rotation,float(fraction));
        for(unsigned n=0;n<positions.size();++n)sample(glm::uvec3(n),V(1,0,0),body,shape,pose);
        for(auto edge:asset->edges)sample({edge.x,edge.y,edge.y},V(.5,.5,0),body,shape,pose);
        for(unsigned f=0;f<asset->triangles.size();++f)if(FaceVisible(f))sample(asset->triangles[f],V(1./3),body,shape,pose);
    }
}
