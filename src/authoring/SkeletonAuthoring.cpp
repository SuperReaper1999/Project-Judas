#define GLM_ENABLE_EXPERIMENTAL
#include "SkeletonAuthoring.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <set>
#include <cmath>
bool FitSkeletonChain(const Skeleton& skeleton,const RagdollDefinition& current,const std::vector<std::string>& selected,float thickness,RagdollDefinition& out,std::string& error){
 if(selected.empty()||selected.size()>32||!std::isfinite(thickness)||thickness<=0){error="select 1–32 joints and positive thickness";return false;}
 auto result=current;auto global=PoseGlobalMatrices(skeleton,skeleton.rest);std::set<int> nodes;for(auto& name:selected){int n=FindSkeletonJoint(skeleton,name);if(n<0||!nodes.insert(n).second){error="missing/ambiguous/duplicate joint "+name;return false;}}
 for(auto n:skeleton.order)if(nodes.count(n)){auto key=skeleton.names[n];auto existing=std::find_if(result.bones.begin(),result.bones.end(),[&](const auto& b){return FindSkeletonJoint(skeleton,b.joint)==n;});bool added=existing==result.bones.end();RagdollBone bone=added?RagdollBone{}:*existing;bone.joint=key;int child=-1;for(auto c:skeleton.order)if(skeleton.parents[c]==n){if(child<0)child=c;if(nodes.count(c)){child=c;break;}}
 JointTransform transform;if(!DecomposeRigidPose(global[n],transform,error))return false;auto direction=child<0?glm::vec3(0,thickness*2,0):glm::vec3(glm::inverse(global[n])*glm::vec4(glm::vec3(global[child][3]),1));float length=glm::length(direction);if(!std::isfinite(length)||length<1e-6f){error="zero-length selected joint segment "+key;return false;}bone.offset=direction*.5f;bone.orientation=glm::normalize(glm::rotation(glm::vec3(0,1,0),direction/length));bone.halfExtents={thickness*.5f,std::max(length*.5f,thickness*.5f),thickness*.5f};bone.radius=thickness*.5f;
 if(added){bone.shape=RagdollShape::Box;bone.constraint.type=JointType::Ball;for(int p=skeleton.parents[n];p>=0;p=skeleton.parents[p]){auto parent=std::find_if(result.bones.begin(),result.bones.end(),[&](const auto& b){return FindSkeletonJoint(skeleton,b.joint)==p;});if(parent!=result.bones.end()){bone.parent=parent->joint;break;}}bone.autoAnchors=true;result.bones.push_back(bone);}else *existing=bone;
 }
 if(!ValidRagdollDefinition(result,error))return false;
 out=std::move(result);return true;
}
