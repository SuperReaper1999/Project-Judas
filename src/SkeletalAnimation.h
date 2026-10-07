#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <cstdint>
// CPU hierarchy and draw-palette bounds are independent of GPU uniform counts.
constexpr size_t kModelNodeLimit=4096, kModelPaletteLimit=16384;
struct JointTransform {glm::vec3 translation{0},scale{1};glm::quat rotation{1,0,0,0};};
struct SkeletalPose {std::vector<JointTransform> local;};
struct Skeleton {
 int motionRoot=-1; // transparent engine conversion root, never a source-joint identity
 std::vector<std::string> names;
 std::vector<int> parents,order; // includes non-skin ancestor nodes; parent-first traversal
 SkeletalPose rest;
 // Exact fixed affine nodes (glTF matrix nodes); empty for legacy TRS rigs.
 std::vector<glm::mat4> affine;
 std::vector<int> skinNodes;
 std::vector<glm::mat4> inverseBind;
};
enum class TrackPath {Translation,Rotation,Scale};
enum class TrackInterpolation {Step,Linear,CubicSpline};
struct AnimationTrack {int node=0;TrackPath path=TrackPath::Translation;TrackInterpolation interpolation=TrackInterpolation::Linear;std::vector<float> times;std::vector<glm::vec4> values;};
struct AnimationClip {std::string name;float duration=0;std::vector<AnimationTrack> tracks;bool loop=false;std::vector<float> motionTimes;std::vector<JointTransform> motion;};
struct SkeletalAsset {Skeleton skeleton;std::vector<AnimationClip> clips;};
SkeletalPose SampleClip(const Skeleton&,const AnimationClip&,float time);
std::vector<glm::mat4> ResolveJointMatrices(const Skeleton&,const SkeletalPose&);
std::vector<glm::mat4> ResolveSkinMatrices(const Skeleton&,const SkeletalPose&);
// Instance playback is one pose producer. RuntimeWorld composes copied pose
// contributions; Renderer depends only on the final resolved pose, not this player.
struct AnimationPlayback {
 std::string clip;bool playing=true,loop=true,stopped=false;float speed=1,time=0;
 SkeletalPose Evaluate(const SkeletalAsset&,float dt);
 SkeletalPose Seek(const SkeletalAsset&,float seconds);
 SkeletalPose Stop(const SkeletalAsset&);
};
// Time-addressable extracted model motion; never applied to physics implicitly.
JointTransform SampleRootMotion(const AnimationClip&,double time,bool loop);
JointTransform RootMotionInterval(const AnimationClip&,double from,double to,bool loop);
