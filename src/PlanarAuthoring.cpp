#include "PlanarAuthoring.h"
#include "PhysicsWorld.h"
#include "Scene.h"
#include "Contacts.h"

bool HasSpatialMotionProducer(const SceneObject& o){
    return (o.body&&o.body->shape!=SceneShape::Planar)||o.characterMotor||o.ragdoll||o.deformable||o.wheel||o.powertrain||o.fluidVolume||o.liquidBasin||o.liquidContainer||o.liquidConnection;
}
bool ValidatePlanarDefinition(const SceneObject& o,std::string& error){
    if(!o.body||o.body->shape!=SceneShape::Planar)return true;
    const auto& b=*o.body;
    if(!b.planar||!ValidatePlanar(*b.planar,error)){if(error.empty())error="missing native 2D collider";return false;}
    if(b.planar->kind==PlanarKind::Chain&&b.motion!=SceneBodyMotion::Static){error="2D chains are static-only";return false;}
    if(o.characterMotor||o.ragdoll||o.deformable||o.wheel||o.powertrain||o.vehicle||o.combustible||o.socket||o.liquidInteraction||o.liquidContainer||!b.fluidCavities.empty()){
        error="2D bodies cannot use spatial motors, assemblies, sockets or fluid coupling; use the 2D player script and native planar joints";return false;
    }
    if(o.transform.scale!=glm::vec3(1)){error="2D collision uses explicit dimensions: keep transform scale 1 and edit collider/sprite sizes";return false;}
    const auto n=glm::vec3(ContactRotation(o.transform.rotation)[2]);
    if(std::abs(glm::dot(n,b.initialLinearVelocity))>1e-5f||glm::length(glm::cross(n,b.initialAngularVelocity))>1e-5f||
       (b.planar->lockRotation&&glm::length(b.initialAngularVelocity)>1e-5f)){
        error="2D initial velocity must lie in its fixed motion plane and respect rotation locking";return false;
    }
    return true;
}
bool ValidatePlanarScene(const Scene& scene,bool& hasPlanar,std::string& error){
    hasPlanar=false;bool spatial=false;
    for(const auto& o:scene.Objects()){
        if(!ValidatePlanarDefinition(o,error)){error="entity "+std::to_string(o.id)+": "+error;return false;}
        hasPlanar|=o.body&&o.body->shape==SceneShape::Planar;spatial|=HasSpatialMotionProducer(o);
    }
    if(hasPlanar&&spatial){error="This 2D physics world contains spatial motion producers. Mixed 2D/3D physical coupling is unsupported; use native 2D bodies/player presets. 3D visuals and cameras are allowed.";return false;}
    return true;
}

bool PlanarCommandAllowed(const PhysicsWorld& physics,BodyHandle body,const BodyTransform& next,
                          const glm::vec3* linear,const glm::vec3* angular) {
    Shape shape;BodyTransform current;
    if(!physics.GetBodyShape(body,shape,current)||shape.type!=ShapeType::Planar)return true;
    const auto n=physics.GetPlanarNormal(body);
    if(!std::isfinite(glm::dot(next.position,next.position))||!std::isfinite(glm::dot(next.rotation,next.rotation))||glm::dot(next.rotation,next.rotation)<1e-12f||
       std::abs(glm::dot(n,next.position-current.position))>1e-5f||glm::dot(glm::vec3(ContactRotation(next.rotation)[2]),n)<1-1e-6f)return false;
    if(linear){const auto v=*linear;if(!std::isfinite(glm::dot(v,v))||std::abs(glm::dot(n,v))>1e-5f)return false;}
    if(angular){const auto w=*angular;if(!std::isfinite(glm::dot(w,w))||glm::length(glm::cross(n,w))>1e-5f||(shape.planar->lockRotation&&glm::length(w)>1e-5f))return false;}
    return true;
}
