#include "PerformanceProfiler.h"
#include "SkeletalAnimation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
SkeletalPose SampleClip(const Skeleton& skeleton,const AnimationClip& clip,float time){
 JUDAS_PROFILE_SCOPE("Animation clip sampling");
 SkeletalPose pose=skeleton.rest;
 for(const auto& track:clip.tracks){
  const auto& times=track.times;if(times.empty())continue;
  auto upper=std::upper_bound(times.begin(),times.end(),time);size_t a=upper==times.begin()?0:size_t(upper-times.begin()-1),b=std::min(a+1,times.size()-1);
  float h=times[b]-times[a],t=h>0?std::clamp((time-times[a])/h,0.f,1.f):0;
  auto value=[&](size_t i){return track.values[i*(track.interpolation==TrackInterpolation::CubicSpline?3:1)+(track.interpolation==TrackInterpolation::CubicSpline?1:0)];};
  glm::vec4 v=value(a);
  if(track.interpolation==TrackInterpolation::Linear){if(track.path==TrackPath::Rotation){auto x=value(a),y=value(b);auto q=glm::normalize(glm::slerp(glm::quat(x.w,x.x,x.y,x.z),glm::quat(y.w,y.x,y.y,y.z),t));v={q.x,q.y,q.z,q.w};}else v=glm::mix(v,value(b),t);}
  else if(track.interpolation==TrackInterpolation::CubicSpline){float t2=t*t,t3=t2*t;v=(2*t3-3*t2+1)*value(a)+(t3-2*t2+t)*h*track.values[a*3+2]+(-2*t3+3*t2)*value(b)+(t3-t2)*h*track.values[b*3];}
  auto& local=pose.local.at(track.node);
  if(track.path==TrackPath::Translation)local.translation=glm::vec3(v);
  else if(track.path==TrackPath::Scale)local.scale=glm::vec3(v);
  else {auto q=glm::quat(v.w,v.x,v.y,v.z);local.rotation=glm::dot(q,q)>1e-12f?glm::normalize(q):skeleton.rest.local[track.node].rotation;}
 }
 return pose;
}
std::vector<glm::mat4> ResolveJointMatrices(const Skeleton& s,const SkeletalPose& pose){
 JUDAS_PROFILE_SCOPE("Pose skin matrices");
 if(pose.local.size()!=s.parents.size())throw std::invalid_argument("pose does not match skeleton");
 std::vector<glm::mat4> global(pose.local.size());
 for(int i:s.order){const auto& p=pose.local[i];auto local=glm::translate(glm::mat4(1),p.translation)*glm::mat4_cast(p.rotation)*glm::scale(glm::mat4(1),p.scale);if(!s.affine.empty())local=s.affine.at(i)*local;global[i]=s.parents[i]<0?local:global[s.parents[i]]*local;}
 return global;
}
std::vector<glm::mat4> ResolveSkinMatrices(const Skeleton& s,const SkeletalPose& pose){
 auto global=ResolveJointMatrices(s,pose);
 std::vector<glm::mat4> skin;skin.reserve(s.skinNodes.size());for(size_t i=0;i<s.skinNodes.size();++i)skin.push_back(global[s.skinNodes[i]]*s.inverseBind[i]);return skin;
}
SkeletalPose AnimationPlayback::Evaluate(const SkeletalAsset& asset,float dt){
 if(stopped)return asset.skeleton.rest;
 auto it=std::find_if(asset.clips.begin(),asset.clips.end(),[&](const auto& c){return c.name==clip;});
 if(it==asset.clips.end()){if(clip.empty()&&!asset.clips.empty()){clip=asset.clips.front().name;it=asset.clips.begin();}else {return asset.skeleton.rest;}}
 if(playing)time+=dt*speed;
 if(loop&&it->duration>0){time=std::fmod(time,it->duration);if(time<0)time+=it->duration;}
 else {if((speed>=0&&time>=it->duration)||(speed<0&&time<=0))playing=false;time=std::clamp(time,0.f,it->duration);}
 return SampleClip(asset.skeleton,*it,time);
}
SkeletalPose AnimationPlayback::Seek(const SkeletalAsset& a,float seconds){stopped=false;time=seconds;return Evaluate(a,0);}
SkeletalPose AnimationPlayback::Stop(const SkeletalAsset& a){playing=false;stopped=true;time=0;return a.skeleton.rest;}

namespace {
JointTransform ComposeMotion(const JointTransform& a,const JointTransform& b){JointTransform c;c.translation=a.translation+a.rotation*b.translation;c.rotation=glm::normalize(a.rotation*b.rotation);return c;}
JointTransform InvertMotion(const JointTransform& a){JointTransform b;b.rotation=glm::inverse(a.rotation);b.translation=-(b.rotation*a.translation);return b;}
JointTransform PowerMotion(JointTransform base,long long n){if(n<0){base=InvertMotion(base);n=-n;}JointTransform result;while(n){if(n&1)result=ComposeMotion(result,base);base=ComposeMotion(base,base);n>>=1;}return result;}
}
JointTransform SampleRootMotion(const AnimationClip& c,double time,bool loop){
 if(!std::isfinite(time)||std::abs(time)>1e9)throw std::invalid_argument("root motion time must be finite and bounded");
 if(c.motion.empty())return {};
 double cycle=loop&&c.duration>0?std::floor(time/c.duration):0;
 if(std::abs(cycle)>1000000)throw std::invalid_argument("root motion cycle bound");
 double phase=loop?time-cycle*c.duration:std::clamp(time,0.0,double(c.duration));
 auto upper=std::upper_bound(c.motionTimes.begin(),c.motionTimes.end(),float(phase));size_t a=upper==c.motionTimes.begin()?0:size_t(upper-c.motionTimes.begin()-1),b=std::min(a+1,c.motion.size()-1);
 float span=c.motionTimes[b]-c.motionTimes[a];float t=span>0?std::clamp(float((phase-c.motionTimes[a])/span),0.f,1.f):0;
 JointTransform sampled;sampled.translation=glm::mix(c.motion[a].translation,c.motion[b].translation,t);sampled.rotation=glm::normalize(glm::slerp(c.motion[a].rotation,c.motion[b].rotation,t));return ComposeMotion(PowerMotion(c.motion.back(),static_cast<long long>(cycle)),sampled);
}
JointTransform RootMotionInterval(const AnimationClip& c,double from,double to,bool loop){return ComposeMotion(InvertMotion(SampleRootMotion(c,from,loop)),SampleRootMotion(c,to,loop));}
