#include "PerformanceProfiler.h"
#include "PoseComposition.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
std::string SkeletonJointKey(const Skeleton& s,int node){
 if(node<0||size_t(node)>=s.names.size())return {};
 std::string name=s.names[node]; // escape delimiter so imported names remain unambiguous
 size_t p=0;while((p=name.find('%',p))!=std::string::npos){name.replace(p,1,"%25");p+=3;}
 p=0;while((p=name.find('/',p))!=std::string::npos){name.replace(p,1,"%2F");p+=3;}
 return s.parents[node]<0||s.parents[node]==s.motionRoot?name:SkeletonJointKey(s,s.parents[node])+"/"+name;
}
int FindSkeletonJoint(const Skeleton& s,const std::string& key){int found=-1;
 for(size_t i=0;i<s.names.size();++i)if(SkeletonJointKey(s,int(i))==key){if(found>=0)return -1;found=int(i);}
 if(found>=0)return found;
 for(size_t i=0;i<s.names.size();++i)if(s.names[i]==key){if(found>=0)return -1;found=int(i);}
 return found;
}
bool ResolveJointMask(const Skeleton& s,const std::vector<std::string>& keys,std::vector<int>& mask,std::string& error){
 std::vector<int> result;for(const auto& key:keys){int node=FindSkeletonJoint(s,key);if(node<0){error="missing or ambiguous skeleton joint: "+key;return false;}if(std::find(result.begin(),result.end(),node)!=result.end()){error="duplicate masked joint";return false;}result.push_back(node);}mask=std::move(result);return true;
}
bool ValidPose(const Skeleton& s,const SkeletalPose& p,std::string& error){
 if(p.local.size()!=s.rest.local.size()){error="pose does not match skeleton";return false;}
 for(const auto& v:p.local){for(int k=0;k<3;++k)if(!std::isfinite(v.translation[k])||!std::isfinite(v.scale[k])){error="nonfinite pose";return false;}
 if(!std::isfinite(glm::dot(v.rotation,v.rotation))||glm::dot(v.rotation,v.rotation)<1e-12f){error="invalid pose quaternion";return false;}}return true;
}
SkeletalPose BlendPoses(const SkeletalPose& a,const SkeletalPose& b,float weight,const std::vector<int>& mask){
 if(a.local.size()!=b.local.size())throw std::invalid_argument("incompatible poses");
 SkeletalPose result=a;
 auto blend=[&](int node){auto& v=result.local.at(node);const auto& source=b.local.at(node);v.translation=glm::mix(v.translation,source.translation,weight);v.scale=glm::mix(v.scale,source.scale,weight);v.rotation=glm::normalize(glm::slerp(glm::normalize(v.rotation),glm::normalize(source.rotation),weight));};
 if(mask.empty())for(size_t i=0;i<a.local.size();++i)blend(int(i));else for(int node:mask)blend(node);return result;
}
SkeletalPose ResolvePose(const Skeleton& s,const SkeletalPose& base,const std::vector<PoseContribution>& contributions){
 SkeletalPose result=base; // inputs remain independent, never mutate a producer's pose
 for(const auto& c:contributions){if(!c.enabled||c.weight<=0)continue;
  if(!c.additive){result=BlendPoses(result,c.pose,c.weight,c.mask);continue;}
  const auto& reference=c.reference.local.empty()?s.rest:c.reference;
  auto apply=[&](int node){auto& v=result.local.at(node);const auto& from=reference.local.at(node);const auto& to=c.pose.local.at(node);
   v.translation+=c.weight*(to.translation-from.translation);
   auto delta=glm::normalize(glm::inverse(glm::normalize(from.rotation))*glm::normalize(to.rotation));
   v.rotation=glm::normalize(v.rotation*glm::slerp(glm::quat(1,0,0,0),delta,c.weight));
   for(int k=0;k<3;++k){if(std::abs(from.scale[k])<1e-8f)throw std::invalid_argument("additive reference scale is zero");v.scale[k]*=glm::mix(1.f,to.scale[k]/from.scale[k],c.weight);}
  };
  if(c.mask.empty())for(size_t i=0;i<result.local.size();++i)apply(int(i));else for(int node:c.mask)apply(node);
 }return result;
}
bool AnimationLayerSettings::operator==(const AnimationLayerSettings& b)const{return id==b.id&&clip==b.clip&&referenceClip==b.referenceClip&&enabled==b.enabled&&additive==b.additive&&weight==b.weight&&speed==b.speed&&time==b.time&&referenceTime==b.referenceTime&&mask==b.mask;}
bool ValidAnimationLayers(const std::vector<AnimationLayerSettings>& layers,std::string& error){
 if(layers.size()>16){error="at most 16 animation layers";return false;}std::set<std::string> ids;
 for(const auto& l:layers)if(l.id.empty()||!ids.insert(l.id).second||!std::isfinite(l.weight)||l.weight<0||l.weight>1||!std::isfinite(l.speed)||!std::isfinite(l.time)||l.time<0||!std::isfinite(l.referenceTime)||l.referenceTime<0||l.mask.size()>kModelNodeLimit){error="invalid animation layer";return false;}
 return true;
}
float PoseMixer::Fraction()const{return duration>0?std::clamp(elapsed/duration,0.f,1.f):1;}
void PoseMixer::CrossFade(AnimationPlayback& target,const std::string& clip,float seconds){
 if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("invalid fade duration");
 if(seconds>0 && outgoing.size()>=16)throw std::invalid_argument("too many interrupted fade contributors");
 if(seconds==0){Clear();}else {float fraction=Fraction();std::vector<Source> sources;
  if(Transitioning()){for(auto source:outgoing){source.weight*=1-fraction;if(source.weight>0)sources.push_back(std::move(source));}if(fraction>0)sources.push_back({target,fraction});}
  else sources.push_back({target,1});
  outgoing=std::move(sources);duration=seconds;elapsed=0;
 }
 target.clip=clip;target.time=0;target.playing=true;target.stopped=false;paused=false;
}
SkeletalPose PoseMixer::Sample(const SkeletalAsset& a,AnimationPlayback& target,float dt){
 JUDAS_PROFILE_SCOPE("Pose clip mixing");
 float advance=paused?0:dt;auto pose=target.Evaluate(a,advance);
 if(!Transitioning())return pose;
 elapsed=std::min(duration,elapsed+advance);float fraction=Fraction();SkeletalPose combined=a.skeleton.rest;float total=0;
 for(auto& source:outgoing){auto sampled=source.playback.Evaluate(a,advance);float weight=source.weight*(1-fraction);if(weight>0){combined=BlendPoses(combined,sampled,weight/(total+weight));total+=weight;}}
 if(fraction>0)combined=BlendPoses(combined,pose,fraction/(total+fraction));
 if(fraction>=1)Clear();
 return combined;
}

#include "Ragdoll.h"

#include <glm/gtc/matrix_transform.hpp>
bool ValidLimbIK(const LimbIKSettings& s,std::string& error){
 if(s.id.empty()||s.id.size()>128||s.root.empty()||s.middle.empty()||s.end.empty()||s.root==s.middle||s.middle==s.end||s.root==s.end||!std::isfinite(s.weight)||s.weight<0||s.weight>1||s.order<1||s.order>=1000){error="IK requires distinct joint keys, weight 0..1 and order 1..999";return false;}
 for(auto v:{s.target,s.pole})for(int i=0;i<3;++i)if(!std::isfinite(v[i])){error="nonfinite IK target/pole";return false;}
 return true;
}
namespace {
glm::quat LimbAlignment(glm::vec3 from,glm::vec3 to,glm::vec3 bend){
 from=glm::normalize(from);to=glm::normalize(to);float d=glm::clamp(glm::dot(from,to),-1.f,1.f);
 if(d>1-1e-7f)return {1,0,0,0};
 if(d<-1+1e-7f){auto axis=bend-from*glm::dot(bend,from);if(glm::length(axis)<1e-6f)axis=glm::cross(from,std::abs(from.x)<.7f?glm::vec3(1,0,0):glm::vec3(0,1,0));return glm::angleAxis(glm::pi<float>(),glm::normalize(axis));}
 return glm::normalize(glm::quat(1+d,glm::cross(from,to)));
}
}
bool SolveLimbIK(const Skeleton& s,const SkeletalPose& input,const LimbIKSettings& k,SkeletalPose& output,float& targetError,std::string& diagnostic){
 if(!ValidLimbIK(k,diagnostic)||!ValidPose(s,input,diagnostic))return false;
 int root=FindSkeletonJoint(s,k.root),mid=FindSkeletonJoint(s,k.middle),end=FindSkeletonJoint(s,k.end);
 if(root<0||mid<0||end<0||s.parents[mid]!=root||s.parents[end]!=mid){diagnostic="IK joints must be an existing direct root/middle/end chain";return false;}
 auto globals=PoseGlobalMatrices(s,input);auto a=glm::vec3(globals[root][3]),b=glm::vec3(globals[mid][3]),c=glm::vec3(globals[end][3]);
 float l1=glm::length(b-a),l2=glm::length(c-b);if(l1<1e-5f||l2<1e-5f){diagnostic="IK chain has zero length";return false;}
 JointTransform ar,br,parent;std::string ignored;
 if(!DecomposeRigidPose(globals[root],ar,diagnostic)||!DecomposeRigidPose(globals[mid],br,diagnostic)){return false;}
 for(auto scale:{ar.scale,br.scale})if(glm::length(scale-glm::vec3(scale.x))>1e-4f||scale.x<=0){diagnostic="IK requires uniform positive chain scale";return false;}
 if(s.parents[root]>=0&&!DecomposeRigidPose(globals[s.parents[root]],parent,diagnostic))return false;
 auto delta=k.target-a;float distance=glm::length(delta);auto direction=distance>1e-6f?delta/distance:glm::normalize(c-a+glm::vec3(1e-8f));
 auto bend=k.pole-a; bend-=direction*glm::dot(bend,direction);
 if(glm::length(bend)<1e-5f){bend=b-a; bend-=direction*glm::dot(bend,direction);}
 if(glm::length(bend)<1e-5f){auto axis=std::abs(direction.x)<.7f?glm::vec3(1,0,0):glm::vec3(0,1,0);bend=axis-direction*glm::dot(axis,direction);}
 bend=glm::normalize(bend);float reach=glm::clamp(distance,std::abs(l1-l2)+1e-5f,l1+l2-1e-5f);
 float x=(l1*l1-l2*l2+reach*reach)/(2*reach),y=std::sqrt(std::max(0.f,l1*l1-x*x));
 auto desiredMiddle=a+direction*x+bend*y,desiredEnd=a+direction*reach;
 JointTransform rootFixed,midFixed;
 if(!s.affine.empty()&&(!DecomposeRigidPose(s.affine[root],rootFixed,diagnostic)||!DecomposeRigidPose(s.affine[mid],midFixed,diagnostic)))return false;
 output=input;auto rootRotation=glm::normalize(LimbAlignment(b-a,desiredMiddle-a,bend)*ar.rotation);
 output.local[root].rotation=glm::normalize(glm::inverse(parent.rotation*rootFixed.rotation)*rootRotation);
 globals=PoseGlobalMatrices(s,output);b=glm::vec3(globals[mid][3]);c=glm::vec3(globals[end][3]);
 if(!DecomposeRigidPose(globals[mid],br,diagnostic))return false;
 auto midRotation=glm::normalize(LimbAlignment(c-b,desiredEnd-b,bend)*br.rotation);
 output.local[mid].rotation=glm::normalize(glm::inverse(rootRotation*midFixed.rotation)*midRotation);
 output=BlendPoses(input,output,k.weight,{root,mid});globals=PoseGlobalMatrices(s,output);targetError=glm::length(glm::vec3(globals[end][3])-k.target);
 return ValidPose(s,output,diagnostic);
}
