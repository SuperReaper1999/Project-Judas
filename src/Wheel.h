#pragma once
#include "PhysicsWorld.h"
#include <string>

// A suspension mount belongs to an ordinary dynamic body. The component's
// owner is a separate visual entity; no vehicle count, drivetrain or input
// policy is implied. All axes below are mount-local, never world-up.
struct WheelSettings {
    bool enabled=true;
    uint64_t chassis=0;
    std::string part; // Optional stable rigid-fracture part key; chassis then identifies its owner.
    glm::vec3 mount{0};
    glm::quat orientation{1,0,0,0}; // down -Y, axle +X, forward -Z
    float radius=.35f, minLength=.1f, restLength=.35f, maxLength=.6f;
    float springStiffness=20000, damping=2200, spinInertia=1;
    float friction=1, rollingResistance=.02f; // resistance coefficient: torque = coefficient * load * radius
    unsigned collisionLayer=0;
    CategoryMask collisionMask=kAllCategories, requiredTags=0, excludedTags=0;
};
struct WheelCommand {
    float driveTorque=0, brakeTorque=0, steeringAngle=0; // N m, N m, radians
};
struct WheelState {
    bool available=false, contact=false;
    BodyHandle body;
    glm::vec3 point{0}, normal{0}, groundVelocity{0}, center{0};
    glm::quat rotation{1,0,0,0};
    float length=0, normalForce=0, angularVelocity=0, angle=0;
    float longitudinalSlip=0, lateralSlip=0, longitudinalImpulse=0, lateralImpulse=0;
    bool initialOverlap=false;
};
bool ValidateWheelSettings(const WheelSettings&,std::string& error);
bool ValidateWheelCommand(const WheelCommand&,std::string& error);

// Independently evaluated spinning DOF + compliant unilateral contact. Tyre
// impulses are limited by the suspension load and react on dynamic ground.
// The visual pose is a result, not a source of collision or chassis motion.
class WheelInstance {
public:
    WheelSettings settings;
    WheelCommand command;
    WheelState state;
    WheelState previous;
    void Step(PhysicsWorld&,BodyHandle chassis,float dt);
    void Publish(PhysicsWorld&,BodyHandle chassis);
    // Only durable command/spin/travel state is archived. Contact handles and
    // broadphase scratch are reconstructed from authoritative geometry.
    void Persist(class SaveArchive&);
private:
    std::vector<BodyHandle> candidates;
};
