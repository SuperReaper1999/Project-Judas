#pragma once
#include "SkeletalAnimation.h"
#include <functional>

// Authoring-only inputs. Runtime loads the baked ordinary AnimationClip; it has
// no profile, source skeleton, or retarget evaluation dependency.
struct RetargetMapping {std::string source,target;};
struct RetargetReferenceCorrection {
 std::string joint;
 glm::quat rotation{1,0,0,0}; // post-multiplied onto the rest LOCAL rotation
};
struct RetargetProfile {
 int version=1;
 std::string sourceIdentity,targetIdentity,sourceSignature,targetSignature;
 glm::quat modelAlignment{1,0,0,0}; // normalized source-model -> target-model frame
 float translationScale=1;
 std::vector<RetargetMapping> mapping;
 std::vector<std::string> translationJoints; // TARGET full keys, mapped only
 std::vector<RetargetReferenceCorrection> sourceReference,targetReference;
};
struct RetargetBakeSettings {
 std::string name;
 float begin=0,end=-1; // end=-1 uses source duration; output time starts at zero
 double sampleRate=60;
 std::function<bool()> cancelled;
};
struct RetargetBakeReport {
 size_t samples=0,mappedJoints=0,outputKeys=0,estimatedWorkingBytes=0;
 double milliseconds=0;
};
constexpr size_t kRetargetProfileBytes=4*1024*1024;
constexpr size_t kRetargetMaxSamples=1000000,kRetargetMaxKeys=4000000;
constexpr size_t kRetargetMaxNodeEvaluations=64000000;
constexpr double kRetargetMaxDuration=3600,kRetargetMaxRate=240;

// Full hierarchy, reference/bind content, sorted by stable escaped keys. Node
// array reordering does not invalidate correspondence; renamed paths do.
std::string SkeletonRetargetSignature(const Skeleton&);
bool ParseRetargetProfile(const std::string&,RetargetProfile&,std::string& error);
std::string SerializeRetargetProfile(const RetargetProfile&);
bool LoadRetargetProfile(const std::string&,RetargetProfile&,std::string& error);
bool SaveRetargetProfile(const std::string&,const RetargetProfile&,std::string& error);
bool ValidateRetargetProfile(const Skeleton& source,const Skeleton& target,const RetargetProfile&,std::string& error);

// Qs(t) * inverse(QsRef) is measured in normalized SOURCE model space.
// Qt(t) = C * delta * inverse(C) * QtRef. Target locals are recovered through
// the complete current target hierarchy, including retained unmapped helpers.
// Selected local translation DELTAS (including intervening unmapped helpers up
// to the nearest mapped ancestor) convert through reference parent frames and
// scale; rest offsets and all target local scales are retained.
bool RetargetPose(const Skeleton& source,const Skeleton& target,const RetargetProfile&,
                  const SkeletalPose& sourcePose,SkeletalPose& output,std::string& error);
bool BakeRetargetClip(const Skeleton& source,const AnimationClip&,const Skeleton& target,
                     const RetargetProfile&,const RetargetBakeSettings&,AnimationClip& output,
                     std::string& error,RetargetBakeReport* report=nullptr);
