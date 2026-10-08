#pragma once
#include "PoseComposition.h"
#include "JointTypes.h"
#include "Classification.h"
#include "PhysicalAnimation.h"

// Authored geometry in joint-local metres. Skeleton joints and physical joints
// have separate identities: each mapping explicitly names both relationships.
enum class RagdollShape { Box, Sphere };
struct RagdollBone {
    std::string joint, parent;
    RagdollShape shape=RagdollShape::Box;
    glm::vec3 offset{0},halfExtents{.2f,.5f,.2f};
    glm::quat orientation{1,0,0,0};
    float radius=.2f,mass=1,friction=.6f,restitution=0;
    unsigned collisionLayer=0;
    CategoryMask collisionMask=kAllCategories;
    bool suppressParentCollision=true,autoAnchors=true;
    JointSettings constraint;
};
struct RagdollDefinition {
    bool enabled=true,playOnStart=false,selfCollision=true;
    // Ordinary M42 body events remain unchanged. An owner may explicitly
    // subscribe to its mapped bodies' external contacts through the same path.
    bool receiveContactEvents=false;
    std::vector<RagdollBone> bones;
    std::optional<PhysicalAnimationSettings> physicalAnimation;
};
bool ValidRagdollDefinition(const RagdollDefinition&,std::string& error);
bool RagdollDefinitionsEqual(const RagdollDefinition&,const RagdollDefinition&);
std::vector<glm::mat4> PoseGlobalMatrices(const Skeleton&,const SkeletalPose&);
// Reject shear/reflection rather than silently converting it into rigid motion.
bool DecomposeRigidPose(const glm::mat4&,JointTransform&,std::string& error);
