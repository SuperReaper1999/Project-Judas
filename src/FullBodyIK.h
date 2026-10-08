#pragma once
#include "PoseComposition.h"
#include <cstdint>

// M70 works on an instance pose. These are skeleton keys, not gameplay anatomy.
// All positions are metres; all angular limits/errors are radians. Quaternion
// frames are local authored frames, independently of imported bone axes.
enum class IKTargetSpace { Model, World };
struct FullBodyIKChain { std::string id; std::vector<std::string> joints; };
struct FullBodyIKJointLimit {
    std::string joint;
    glm::quat frame{1,0,0,0};
    glm::vec3 min{-3.1415926f},max{3.1415926f},preferred{0};
    float preferenceWeight=0;
};
struct FullBodyIKTarget {
    std::string id,chain;
    bool enabled=true;
    IKTargetSpace space=IKTargetSpace::Model;
    glm::dvec3 position{0};
    glm::quat orientation{1,0,0,0};
    float positionWeight=1,orientationWeight=0;
    glm::vec3 offset{0};
    glm::quat frame{1,0,0,0};
};
struct FullBodyIKSettings {
    bool enabled=true,rootRotation=true;
    std::string bodyRoot;
    glm::vec3 rootMin{-.2f},rootMax{.2f};
    std::vector<std::string> spine;
    std::vector<FullBodyIKChain> chains;
    std::vector<FullBodyIKJointLimit> limits;
    std::vector<FullBodyIKTarget> targets;
    unsigned iterations=64;
    float damping=.02f,positionTolerance=.005f,orientationTolerance=.017453293f;
    float orientationScale=.25f,maxAngularStep=.22f,maxTranslationStep=.06f;
};
constexpr size_t kFullBodyIKTargetLimit=12,kFullBodyIKJointLimit=64;
constexpr unsigned kFullBodyIKIterationLimit=96;
struct FullBodyIKMapping {
    struct Chain {std::string id;std::vector<int> joints;int end=-1;};
    FullBodyIKSettings settings;
    int root=-1;
    std::vector<int> joints;
    std::vector<Chain> chains;
    std::vector<int> limitNodes;
    // Exact identity signature makes stale mappings fail after skeleton replacement.
    std::vector<int> parents;
    std::vector<std::string> names;
};
struct FullBodyIKResidual {
    std::string id,status;
    float positionError=0,orientationError=0;
    glm::vec3 actualPosition{0};
    glm::quat actualOrientation{1,0,0,0};
};
struct FullBodyIKResult {
    SkeletalPose pose;
    std::vector<FullBodyIKResidual> targets;
    unsigned iterations=0;
    bool converged=false;
    glm::vec3 rootCorrection{0};
    double solveMicroseconds=0;
};
struct FullBodyIKRuntime {
    FullBodyIKSettings settings;
    FullBodyIKMapping mapping;
    FullBodyIKResult result;
    bool prepared=false,hasSample=false;
    std::vector<FullBodyIKTarget> sampledTargets; // disposable fixed-boundary input
};
bool ValidFullBodyIKSettings(const FullBodyIKSettings&,std::string& error);
bool ValidFullBodyIKTargets(const std::vector<FullBodyIKTarget>&,std::string& error);
bool PrepareFullBodyIK(const Skeleton&,const FullBodyIKSettings&,FullBodyIKMapping&,std::string& error);
// World translation is double. Subtract it BEFORE float conversion. Positive
// uniform model scale is supported; shear/reflection and singular mapped paths
// are rejected. Unrelated hidden/singular subtrees need no inversions.
bool FullBodyIKTargetsToModel(const std::vector<FullBodyIKTarget>&,glm::dvec3 worldPosition,
    glm::quat worldOrientation,glm::vec3 scale,std::vector<FullBodyIKTarget>&,std::string& error);
// One simultaneous damped least-squares solve. The reference is never changed,
// poses do not warm-start from a previous final result, and only declared root
// translation and mapped rotations may change. False means invalid configuration;
// a finite unreachable compromise succeeds with per-target "limited" status.
bool SolveFullBodyIK(const Skeleton&,const SkeletalPose& reference,const FullBodyIKMapping&,
    const std::vector<FullBodyIKTarget>& modelTargets,FullBodyIKResult&,std::string& error);
std::string SerializeFullBodyIKSettings(const FullBodyIKSettings&);
bool ParseFullBodyIKSettings(const std::string&,FullBodyIKSettings&,std::string& error);
