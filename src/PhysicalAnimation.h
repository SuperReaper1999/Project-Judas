#pragma once
#include "PoseComposition.h"
#include <memory>
#include <optional>

// These are pose/physics authority modes, not game states. A fully physical
// root is free: drives restore only relative orientations across mapped joints.
enum class PhysicalAnimationMode { Animation, Partial, Active, Passive };
const char* PhysicalAnimationModeName(PhysicalAnimationMode);
bool ParsePhysicalAnimationMode(const std::string&,PhysicalAnimationMode&);

struct PhysicalAnimationRegion {
    std::string id;
    std::vector<std::string> joints;
    bool enabled=true;
    float stiffness=30;       // N m / rad
    float damping=4;          // N m s / rad
    float maxTorque=20;       // N m, magnitude cap
    float effortWeight=1;     // scales stiffness, damping and effort cap
    float poseWeight=1;       // visual/local-pose blend; does not alter physics
};
struct PhysicalAnimationSettings {
    bool enabled=true;
    std::vector<PhysicalAnimationRegion> regions; // later enabled region wins overlaps
};
struct PhysicalAnimationPlacement {
    glm::vec3 position{0};
    glm::quat rotation{1,0,0,0};
};
struct PhysicalAnimationRequest {
    PhysicalAnimationMode mode=PhysicalAnimationMode::Animation;
    float fade=.2f;
    bool motorHandoff=false,resumeMotor=false;
    std::optional<PhysicalAnimationPlacement> placement;
};
struct PhysicalDriveObservation {
    std::string joint,region;
    float angleError=0,torque=0;
    bool saturated=false,sleeping=false;
};
// Disposable history is fixed-step only. It is intentionally not durable state:
// fresh load/reimport starts with no invented preceding motion sample.
struct PhysicalAnimationState {
    PhysicalAnimationMode mode=PhysicalAnimationMode::Animation;
    std::optional<PhysicalAnimationRequest> pending;
    std::vector<PhysicalDriveObservation> observations;
    std::string diagnostic;
    std::vector<glm::mat4> previousReferenceWorld,recentReferenceWorld;
    std::vector<glm::quat> previousDriveTargets;
    float motionDt=0;
    bool motorWasEnabled=false,rebuild=false;
    std::shared_ptr<const SkeletalAsset> observedAsset;
    // Validated setup cache. Never serialized or shared between instances.
    std::shared_ptr<const SkeletalAsset> mappedAsset;
    std::optional<PhysicalAnimationSettings> mappedSettings;
    std::vector<std::string> mappedBones,mappedJointKeys;
    std::vector<int> mappedRegions;
};
bool ValidPhysicalAnimationSettings(const PhysicalAnimationSettings&,std::string&);
bool PhysicalAnimationSettingsEqual(const PhysicalAnimationSettings&,const PhysicalAnimationSettings&);
bool ValidPhysicalAnimationRequest(const PhysicalAnimationRequest&,std::string&);
std::string SerializePhysicalAnimationSettings(const PhysicalAnimationSettings&);
bool ParsePhysicalAnimationSettings(const std::string&,PhysicalAnimationSettings&,std::string&);

struct PhysicalTorqueResult {
    glm::vec3 torque{0};
    float angleError=0;
    bool saturated=false;
};
// Implicit quaternion-PD response retaining the complete joint inertia tensor.
// Inputs/torque are world-space; the target itself is a relative orientation.
// Opposite torque is applied to the ordinary parent body by the caller.
PhysicalTorqueResult CalculatePhysicalTorque(glm::quat targetRelative,
    glm::quat child,glm::quat parent,glm::vec3 childVelocity,
    glm::vec3 parentVelocity,const glm::mat3& childInertia,
    const glm::mat3& parentInverseInertia,const PhysicalAnimationRegion&,float dt);
