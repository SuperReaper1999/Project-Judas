// Ordinary two-body actuator persistence observer. No skeleton, animation,
// contacts, CharacterMotor, gravity or M77 controller participates.
#include "WorldPersistence.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include <cstdio>

int main(){
    Scene scene;std::string error;
    auto a=scene.CreateObject("A").id,b=scene.CreateObject("B").id;
    for(auto id:{a,b}){
        auto* object=scene.Find(id);object->body=SceneBodyComponent{};
        object->body->motion=SceneBodyMotion::Dynamic;object->body->managed=true;
        object->body->mass=1;object->body->halfExtents={.15f,.15f,.15f};
        object->transform.position={id==a?-.5f:.5f,0,0};
    }
    auto joint=scene.CreateObject("ordinary ball constraint").id;
    SceneJointComponent definition;definition.bodyA=a;definition.bodyB=b;
    definition.settings.type=JointType::Ball;
    definition.settings.anchorA={.5f,0,0};definition.settings.anchorB={-.5f,0,0};
    scene.Find(joint)->joint=definition;
    RuntimeWorld source,restored;
    if(!source.Build(scene,nullptr,error)){std::printf("BUILD %s\n",error.c_str());return 2;}
    JointAngularDrive command;command.enabled=true;
    command.target=glm::angleAxis(.4f,glm::vec3(1,0,0));
    command.stiffness=100;command.damping=10;command.maxTorque=20;
    command.relativeAngularVelocity={.3f,0,0};
    if(!source.Physics().SetJointAngularDrive(source.RuntimeJoint(joint),command))return 3;
    JointState before;source.Physics().GetJoint(source.RuntimeJoint(joint),before);
    std::map<std::string,SaveChunk> chunks;Scene captured;
    if(!WorldPersistence::Capture(source,chunks,error)||!WorldPersistence::SceneFrom(chunks,captured,error)||
       !restored.Build(captured,nullptr,error,nullptr,nullptr,true)||
       !WorldPersistence::Prepare(restored,error)||!WorldPersistence::Restore(restored,chunks,error)){
        std::printf("ROUNDTRIP %s\n",error.c_str());return 2;
    }
    JointState after;restored.Physics().GetJoint(restored.RuntimeJoint(joint),after);
    std::printf("COMMAND before enabled=%d target=(%.9g,%.9g,%.9g,%.9g) k=%.9g damping=%.9g cap=%.9g rate=(%.9g,%.9g,%.9g) warm=(%.9g,%.9g,%.9g)\n",
        before.angularDrive.enabled,before.angularDrive.target.w,before.angularDrive.target.x,before.angularDrive.target.y,before.angularDrive.target.z,
        before.angularDrive.stiffness,before.angularDrive.damping,before.angularDrive.maxTorque,
        before.angularDrive.relativeAngularVelocity.x,before.angularDrive.relativeAngularVelocity.y,before.angularDrive.relativeAngularVelocity.z,
        before.angularDrive.impulses.x,before.angularDrive.impulses.y,before.angularDrive.impulses.z);
    std::printf("COMMAND restored enabled=%d target=(%.9g,%.9g,%.9g,%.9g) k=%.9g damping=%.9g cap=%.9g rate=(%.9g,%.9g,%.9g)\n",
        after.angularDrive.enabled,after.angularDrive.target.w,after.angularDrive.target.x,after.angularDrive.target.y,after.angularDrive.target.z,
        after.angularDrive.stiffness,after.angularDrive.damping,after.angularDrive.maxTorque,
        after.angularDrive.relativeAngularVelocity.x,after.angularDrive.relativeAngularVelocity.y,after.angularDrive.relativeAngularVelocity.z);
    source.Physics().Step(1.f/60);restored.Physics().Step(1.f/60);
    auto sA=source.Physics().GetAngularVelocity(source.RuntimeBody(a));
    auto sB=source.Physics().GetAngularVelocity(source.RuntimeBody(b));
    auto rA=restored.Physics().GetAngularVelocity(restored.RuntimeBody(a));
    auto rB=restored.Physics().GetAngularVelocity(restored.RuntimeBody(b));
    std::printf("FIRST60HZ sourceA=(%.9g,%.9g,%.9g) sourceB=(%.9g,%.9g,%.9g) restoredA=(%.9g,%.9g,%.9g) restoredB=(%.9g,%.9g,%.9g)\n",
        sA.x,sA.y,sA.z,sB.x,sB.y,sB.z,rA.x,rA.y,rA.z,rB.x,rB.y,rB.z);
    const bool commandPreserved=before.angularDrive.enabled==after.angularDrive.enabled&&
        before.angularDrive.target==after.angularDrive.target&&
        before.angularDrive.stiffness==after.angularDrive.stiffness&&
        before.angularDrive.damping==after.angularDrive.damping&&
        before.angularDrive.maxTorque==after.angularDrive.maxTorque&&
        before.angularDrive.relativeAngularVelocity==after.angularDrive.relativeAngularVelocity;
    std::printf("AUTHORITATIVE_COMMAND %s; no warm-history or contact row existed at capture\n",commandPreserved?"PRESERVED":"LOST");
    return commandPreserved?0:4;
}
