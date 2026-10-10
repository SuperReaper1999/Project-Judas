#include "AnimationRetarget.h"
#include "SceneFingerprint.h"
#include "PlatformServices.h"
#include "../third_party/nlohmann/json.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace {
using Json=nlohmann::json;
void Require(bool condition,const std::string& text){if(!condition)throw std::runtime_error(text);}
bool Finite(glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
glm::quat Quaternion(glm::quat q,const std::string& label){
 Require(std::isfinite(glm::dot(q,q))&&glm::dot(q,q)>1e-12f,label+" quaternion must be finite and nonzero");
 Require(std::abs(glm::length(q)-1)<1e-4f,label+" quaternion must be normalized");return glm::normalize(q);
}
glm::quat Canonical(glm::quat q){q=Quaternion(q,"reference");if(q.w<0||(q.w==0&&(q.x<0||(q.x==0&&(q.y<0||(q.y==0&&q.z<0))))))q=-q;return q;}
std::string Escape(std::string name){size_t p=0;while((p=name.find('%',p))!=std::string::npos){name.replace(p,1,"%25");p+=3;}p=0;while((p=name.find('/',p))!=std::string::npos){name.replace(p,1,"%2F");p+=3;}return name;}
std::vector<std::string> Keys(const Skeleton& s){
 size_t n=s.names.size();Require(n>0&&n<=kModelNodeLimit,"retarget hierarchy bound is 1..4096 nodes");
 Require(s.parents.size()==n&&s.order.size()==n&&s.rest.local.size()==n&&(s.affine.empty()||s.affine.size()==n),"retarget skeleton hierarchy counts disagree");
 Require(s.motionRoot>=-1&&s.motionRoot<int(n)&&(s.motionRoot<0||s.parents[s.motionRoot]==-1),"retarget invalid motion root");std::vector<bool> visited(n);std::vector<std::string> keys(n);std::set<std::string> unique;size_t keyBytes=0;
 for(int node:s.order){Require(node>=0&&size_t(node)<n&&!visited[node],"retarget invalid hierarchy order");int parent=s.parents[node];Require(parent>=-1&&parent<int(n)&&(parent<0||visited[parent]),"retarget cyclic or invalid parent hierarchy");Require(!s.names[node].empty()&&s.names[node].size()<=4096,"retarget joint name bound");
  keys[node]=(parent<0||parent==s.motionRoot)?Escape(s.names[node]):keys[parent]+"/"+Escape(s.names[node]);
  keyBytes+=keys[node].size();Require(keys[node].size()<=65536&&keyBytes<=16*1024*1024,"retarget hierarchy keys exceed 64 KiB per key / 16 MiB total");Require(unique.insert(keys[node]).second,"retarget ambiguous full hierarchy key: "+keys[node]);visited[node]=true;
  const auto& t=s.rest.local[node];Require(Finite(t.translation)&&Finite(t.scale),"retarget nonfinite rest transform");Quaternion(t.rotation,"rest");
 }
 if(s.motionRoot>=0){const auto& root=s.rest.local[s.motionRoot];Require(glm::length(root.translation)<1e-7f&&glm::length(root.scale-glm::vec3(1))<1e-7f&&std::abs(std::abs(root.rotation.w)-1)<1e-7f,"retarget transparent motion root must retain identity rest transform");if(!s.affine.empty())for(int c=0;c<4;++c)for(int r=0;r<4;++r)Require(std::abs(s.affine[s.motionRoot][c][r]-(c==r?1.f:0.f))<1e-7f,"retarget transparent motion root fixed transform must be identity");}
 return keys;
}
using JointLookup=std::unordered_map<std::string,int>;
JointLookup Index(const std::vector<std::string>& keys){
 JointLookup result;result.reserve(keys.size());
 for(size_t i=0;i<keys.size();++i)result.emplace(keys[i],int(i));
 return result;
}
int Exact(const JointLookup& keys,const std::string& key){auto it=keys.find(key);Require(it!=keys.end(),"retarget missing full hierarchy key: "+key);return it->second;}
JointTransform Rigid(const glm::mat4& m,const std::string& label){
 for(int c=0;c<4;++c)for(int r=0;r<4;++r)Require(std::isfinite(m[c][r]),label+" nonfinite matrix");
 Require(std::abs(m[0][3])<1e-6f&&std::abs(m[1][3])<1e-6f&&std::abs(m[2][3])<1e-6f&&std::abs(m[3][3]-1)<1e-6f,label+" non-affine matrix");
 glm::vec3 x(m[0]),y(m[1]),z(m[2]);glm::vec3 scale(glm::length(x),glm::length(y),glm::length(z));
 Require(Finite(scale)&&std::min({scale.x,scale.y,scale.z})>1e-8f,label+" singular scale");
 float tolerance=std::max({scale.x,scale.y,scale.z})*1e-4f;
 Require(std::abs(scale.x-scale.y)<=tolerance&&std::abs(scale.x-scale.z)<=tolerance,label+" nonuniform scale on participating path");
 x/=scale.x;y/=scale.y;z/=scale.z;
 Require(std::abs(glm::dot(x,y))<1e-4f&&std::abs(glm::dot(y,z))<1e-4f&&std::abs(glm::dot(z,x))<1e-4f,label+" shear on participating path");
 Require(glm::dot(glm::cross(x,y),z)>.999f,label+" reflected orientation on participating path");
 JointTransform t;t.translation=glm::vec3(m[3]);t.scale=scale;t.rotation=glm::normalize(glm::quat_cast(glm::mat3(x,y,z)));return t;
}
glm::mat4 LocalMatrix(const Skeleton& s,const SkeletalPose& p,int i){const auto& t=p.local[i];auto m=glm::translate(glm::mat4(1),t.translation)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);return s.affine.empty()?m:s.affine[i]*m;}
Json Vec(glm::vec3 v){return Json::array({v.x,v.y,v.z});}
Json Quat(glm::quat q){q=Canonical(q);return Json::array({q.x,q.y,q.z,q.w});}
glm::quat ReadQuat(const Json& j){Require(j.is_array()&&j.size()==4,"retarget quaternion requires x/y/z/w");return Quaternion({j.at(3).get<float>(),j.at(0).get<float>(),j.at(1).get<float>(),j.at(2).get<float>()},"profile");}
void Identity(const std::string& s,const char* label){Require(!s.empty()&&s.size()<=4096,std::string("retarget ")+label+" identity must be a bounded stable identifier");}
void Corrections(const Skeleton& s,const JointLookup& keys,const std::vector<RetargetReferenceCorrection>& correction,SkeletalPose& pose){
 Require(correction.size()<=kModelNodeLimit,"retarget reference correction bound");std::set<int> used;
 for(const auto& c:correction){int node=Exact(keys,c.joint);Require(node!=s.motionRoot,"retarget reference cannot modify transparent motion root");Require(used.insert(node).second,"retarget duplicate reference correction: "+c.joint);pose.local[node].rotation=glm::normalize(pose.local[node].rotation*Quaternion(c.rotation,"reference correction"));}
}
struct Plan {
 SkeletalPose sourceRef,targetRef;std::vector<std::string> sourceKeys,targetKeys;JointLookup sourceIndex,targetIndex;
 std::vector<glm::mat4> sourceRefGlobal,targetRefGlobal;std::vector<JointTransform> sourceFrames,targetFrames;
 std::vector<int> sourceForTarget;std::vector<bool> sourcePath,targetPath,translate,sourceMapped;
 glm::quat alignment{1,0,0,0};float translationScale=1;
};
void MarkAncestors(const Skeleton& s,int node,std::vector<bool>& participating){while(node>=0&&!participating[node]){participating[node]=true;node=s.parents[node];}}
Plan Prepare(const Skeleton& source,const Skeleton& target,const RetargetProfile& p){
 Require(p.version==1,"retarget unsupported profile version");Identity(p.sourceIdentity,"source");Identity(p.targetIdentity,"target");
 Require(p.mapping.size()>0&&p.mapping.size()<=kModelNodeLimit,"retarget mapping requires 1..4096 joints");
 Require(p.translationJoints.size()<=kModelNodeLimit,"retarget translation joint bound");
 Require(std::isfinite(p.translationScale)&&p.translationScale>0&&p.translationScale<=1000000,"retarget translationScale must be positive, finite and <=1000000");
 Require(p.sourceSignature==SkeletonRetargetSignature(source),"retarget source skeleton signature changed; recalibrate profile");Require(p.targetSignature==SkeletonRetargetSignature(target),"retarget target skeleton signature changed; recalibrate profile");
 Plan plan;plan.sourceKeys=Keys(source);plan.targetKeys=Keys(target);plan.sourceIndex=Index(plan.sourceKeys);plan.targetIndex=Index(plan.targetKeys);plan.sourceRef=source.rest;plan.targetRef=target.rest;
 Corrections(source,plan.sourceIndex,p.sourceReference,plan.sourceRef);Corrections(target,plan.targetIndex,p.targetReference,plan.targetRef);
 plan.alignment=Quaternion(p.modelAlignment,"model alignment");plan.translationScale=p.translationScale;
 plan.sourceForTarget.assign(target.names.size(),-1);plan.sourcePath.assign(source.names.size(),false);plan.targetPath.assign(target.names.size(),false);plan.translate.assign(target.names.size(),false);plan.sourceMapped.assign(source.names.size(),false);std::set<int> usedSource;
 for(const auto& m:p.mapping){int a=Exact(plan.sourceIndex,m.source),b=Exact(plan.targetIndex,m.target);Require(a!=source.motionRoot&&b!=target.motionRoot,"retarget transparent motion root is not an authored joint");Require(usedSource.insert(a).second,"retarget source joint assigned twice: "+m.source);Require(plan.sourceForTarget[b]<0,"retarget target joint assigned twice: "+m.target);plan.sourceForTarget[b]=a;plan.sourceMapped[a]=true;MarkAncestors(source,a,plan.sourcePath);MarkAncestors(target,b,plan.targetPath);}
 for(const auto& correction:p.sourceReference)MarkAncestors(source,Exact(plan.sourceIndex,correction.joint),plan.sourcePath);
 for(const auto& correction:p.targetReference)MarkAncestors(target,Exact(plan.targetIndex,correction.joint),plan.targetPath);
 // Each nearest mapped ancestor must correspond on both sides. Extra helpers
 // are fine; a left hand mapped under an unrelated right branch is not.
 std::map<int,int> targetForSource;for(size_t b=0;b<plan.sourceForTarget.size();++b)if(plan.sourceForTarget[b]>=0)targetForSource[plan.sourceForTarget[b]]=int(b);
 for(size_t b=0;b<plan.sourceForTarget.size();++b)if(plan.sourceForTarget[b]>=0){int a=source.parents[plan.sourceForTarget[b]],t=target.parents[b];while(a>=0&&!targetForSource.count(a))a=source.parents[a];while(t>=0&&plan.sourceForTarget[t]<0)t=target.parents[t];Require((a<0&&t<0)||(a>=0&&t>=0&&targetForSource[a]==t),"retarget incompatible mapped ancestor paths at "+plan.targetKeys[b]);}
 for(const auto& key:p.translationJoints){int b=Exact(plan.targetIndex,key);Require(plan.sourceForTarget[b]>=0,"retarget translation joint must be mapped: "+key);Require(!plan.translate[b],"retarget duplicate translation joint: "+key);plan.translate[b]=true;}
 plan.sourceRefGlobal=ResolveJointMatrices(source,plan.sourceRef);plan.targetRefGlobal=ResolveJointMatrices(target,plan.targetRef);plan.sourceFrames.resize(source.names.size());plan.targetFrames.resize(target.names.size());
 for(size_t i=0;i<source.names.size();++i)if(plan.sourcePath[i]){Rigid(LocalMatrix(source,plan.sourceRef,int(i)),"source "+plan.sourceKeys[i]);plan.sourceFrames[i]=Rigid(plan.sourceRefGlobal[i],"source "+plan.sourceKeys[i]);}
 for(size_t i=0;i<target.names.size();++i)if(plan.targetPath[i]){Rigid(LocalMatrix(target,plan.targetRef,int(i)),"target "+plan.targetKeys[i]);plan.targetFrames[i]=Rigid(plan.targetRefGlobal[i],"target "+plan.targetKeys[i]);}
 return plan;
}
SkeletalPose Transfer(const Skeleton& source,const Skeleton& target,const Plan& plan,const SkeletalPose& sourcePose){
 Require(sourcePose.local.size()==source.names.size(),"retarget source pose count mismatch");
 for(size_t i=0;i<source.names.size();++i){const auto& t=sourcePose.local[i];Require(Finite(t.translation)&&Finite(t.scale),"retarget nonfinite source pose");Quaternion(t.rotation,"source pose");if(plan.sourcePath[i])Require(glm::length(t.scale-source.rest.local[i].scale)<=1e-5f*std::max(1.f,glm::length(source.rest.local[i].scale)),"retarget animated scale is unsupported on participating source paths");}
 auto sourceGlobal=ResolveJointMatrices(source,sourcePose);auto out=plan.targetRef;std::vector<glm::mat4> targetGlobal(target.names.size());
 for(int b:target.order){int a=plan.sourceForTarget[b];int parent=target.parents[b];
  if(a>=0){auto current=Rigid(sourceGlobal[a],"sampled source "+plan.sourceKeys[a]);auto delta=glm::normalize(current.rotation*glm::inverse(plan.sourceFrames[a].rotation));auto desired=glm::normalize(plan.alignment*delta*glm::inverse(plan.alignment)*plan.targetFrames[b].rotation);
   glm::quat parentRotation(1,0,0,0),fixedRotation(1,0,0,0);if(parent>=0)parentRotation=Rigid(targetGlobal[parent],"resolved target parent").rotation;if(!target.affine.empty())fixedRotation=Rigid(target.affine[b],"target fixed affine").rotation;
   out.local[b].rotation=glm::normalize(glm::inverse(parentRotation*fixedRotation)*desired);
   if(plan.translate[b]){glm::vec3 sourceModelDelta(0);int path=a;
    // Transfer only authored local translation deltas, including intervening
    // unmapped helpers up to the nearest mapped ancestor. Rotating a helper's
    // rest offset is already handled by angular transfer; copying that orbit
    // again as translation would destroy target-proportion preservation.
    do {glm::mat4 sourceParent(1);if(source.parents[path]>=0)sourceParent=plan.sourceRefGlobal[source.parents[path]];if(!source.affine.empty())sourceParent*=source.affine[path];sourceModelDelta+=glm::mat3(sourceParent)*(sourcePose.local[path].translation-plan.sourceRef.local[path].translation);path=source.parents[path];}while(path>=0&&!plan.sourceMapped[path]);
    glm::mat4 targetParent(1);if(parent>=0)targetParent=plan.targetRefGlobal[parent];if(!target.affine.empty())targetParent*=target.affine[b];
    auto modelDelta=plan.alignment*sourceModelDelta*plan.translationScale;out.local[b].translation+=glm::inverse(glm::mat3(targetParent))*modelDelta;
   }
  }
  targetGlobal[b]=LocalMatrix(target,out,b);if(parent>=0)targetGlobal[b]=targetGlobal[parent]*targetGlobal[b];
 }return out;
}
Json ProfileJson(const RetargetProfile& p){
 Identity(p.sourceIdentity,"source");Identity(p.targetIdentity,"target");Require(p.sourceSignature.size()==64&&p.targetSignature.size()==64,"retarget signatures require 64-character content hashes");
 Require(std::isfinite(p.translationScale)&&p.translationScale>0&&p.translationScale<=1000000,"retarget translationScale must be positive and finite");
 Require(p.mapping.size()<=kModelNodeLimit&&p.translationJoints.size()<=kModelNodeLimit&&p.sourceReference.size()<=kModelNodeLimit&&p.targetReference.size()<=kModelNodeLimit,"retarget serialized profile collection bound");
 size_t stringBytes=p.sourceIdentity.size()+p.targetIdentity.size()+p.sourceSignature.size()+p.targetSignature.size();
 auto boundedString=[&](const std::string& text){Require(text.size()<=65536,"retarget serialized hierarchy key exceeds 64 KiB");stringBytes+=text.size();Require(stringBytes<=kRetargetProfileBytes,"retarget serialized profile string budget exceeds 4 MiB");};
 for(const auto& mapping:p.mapping){boundedString(mapping.source);boundedString(mapping.target);}for(const auto& key:p.translationJoints)boundedString(key);for(const auto& correction:p.sourceReference)boundedString(correction.joint);for(const auto& correction:p.targetReference)boundedString(correction.joint);
 Json j={{"format","JudasRetarget"},{"version",p.version},{"sourceIdentity",p.sourceIdentity},{"targetIdentity",p.targetIdentity},{"sourceSignature",p.sourceSignature},{"targetSignature",p.targetSignature},{"modelAlignment",Quat(p.modelAlignment)},{"translationScale",p.translationScale},{"mapping",Json::array()},{"translationJoints",p.translationJoints},{"sourceReference",Json::array()},{"targetReference",Json::array()}};
 for(const auto& m:p.mapping)j["mapping"].push_back({{"source",m.source},{"target",m.target}});
 for(const auto& c:p.sourceReference)j["sourceReference"].push_back({{"joint",c.joint},{"rotation",Quat(c.rotation)}});
 for(const auto& c:p.targetReference)j["targetReference"].push_back({{"joint",c.joint},{"rotation",Quat(c.rotation)}});
 return j;
}
}

std::string SkeletonRetargetSignature(const Skeleton& s){
 auto keys=Keys(s);Json record={{"revision","M74-rig-1"},{"nodes",Json::object()},{"skin",Json::object()}};
 for(size_t i=0;i<keys.size();++i){if(int(i)==s.motionRoot)continue;const auto& t=s.rest.local[i];Json fixed=Json::array();if(!s.affine.empty())for(int c=0;c<4;++c)for(int r=0;r<4;++r){Require(std::isfinite(s.affine[i][c][r]),"retarget nonfinite fixed affine");fixed.push_back(s.affine[i][c][r]);}
  record["nodes"][keys[i]]={{"translation",Vec(t.translation)},{"rotation",Quat(t.rotation)},{"scale",Vec(t.scale)},{"affine",fixed}};
 }
 Require(s.skinNodes.size()==s.inverseBind.size()&&s.skinNodes.size()<=kModelPaletteLimit,"retarget skin bind counts/bound disagree");std::map<std::string,std::vector<std::string>> bindings;
 // A hierarchy node may occur in multiple source mesh palettes with genuinely
 // different inverse binds. Sort the multiset, never reject legal duplication
 // or make profile compatibility depend on transient palette ordering.
 for(size_t k=0;k<s.skinNodes.size();++k){int i=s.skinNodes[k];Require(i>=0&&size_t(i)<keys.size(),"retarget invalid skin identity");Json matrix=Json::array();for(int c=0;c<4;++c)for(int r=0;r<4;++r){float v=s.inverseBind[k][c][r];Require(std::isfinite(v),"retarget nonfinite inverse bind matrix");matrix.push_back(v);}bindings[keys[i]].push_back(matrix.dump());}
 for(auto& entry:bindings){std::sort(entry.second.begin(),entry.second.end());record["skin"][entry.first]=entry.second;}
 return SceneFingerprintSha256(record.dump());
}
bool ParseRetargetProfile(const std::string& text,RetargetProfile& out,std::string& error){try{
 Require(text.size()<=kRetargetProfileBytes,"retarget profile exceeds 4 MiB");auto j=Json::parse(text,[](int depth,Json::parse_event_t,const Json&){Require(depth<40,"retarget profile nesting bound");return true;});
 Require(j.at("format")=="JudasRetarget"&&j.at("version")==1,"retarget profile header/version");RetargetProfile p;p.sourceIdentity=j.at("sourceIdentity");p.targetIdentity=j.at("targetIdentity");p.sourceSignature=j.at("sourceSignature");p.targetSignature=j.at("targetSignature");p.translationScale=j.value("translationScale",1.f);p.modelAlignment=j.contains("modelAlignment")?ReadQuat(j.at("modelAlignment")):glm::quat(1,0,0,0);
 Require(j.at("mapping").is_array()&&j.at("mapping").size()<=kModelNodeLimit,"retarget mapping bound");for(const auto& m:j.at("mapping"))p.mapping.push_back({m.at("source"),m.at("target")});if(j.contains("translationJoints"))p.translationJoints=j.at("translationJoints").get<std::vector<std::string>>();
 auto reference=[&](const char* key,std::vector<RetargetReferenceCorrection>& destination){if(!j.contains(key))return;Require(j.at(key).is_array()&&j.at(key).size()<=kModelNodeLimit,"retarget reference correction bound");for(const auto& c:j.at(key))destination.push_back({c.at("joint"),ReadQuat(c.at("rotation"))});};reference("sourceReference",p.sourceReference);reference("targetReference",p.targetReference);
 Identity(p.sourceIdentity,"source");Identity(p.targetIdentity,"target");Require(p.sourceSignature.size()==64&&p.targetSignature.size()==64,"retarget signatures require 64-character content hashes");Require(std::isfinite(p.translationScale)&&p.translationScale>0&&p.translationScale<=1000000,"retarget translationScale must be positive and finite");Require(p.translationJoints.size()<=kModelNodeLimit,"retarget translation joint bound");
 out=std::move(p);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
std::string SerializeRetargetProfile(const RetargetProfile& p){auto text=ProfileJson(p).dump(2)+'\n';Require(text.size()<=kRetargetProfileBytes,"retarget profile exceeds 4 MiB");return text;}
bool LoadRetargetProfile(const std::string& path,RetargetProfile& out,std::string& error){try{Require(std::filesystem::file_size(std::filesystem::u8path(path))<=kRetargetProfileBytes,"retarget profile exceeds 4 MiB");std::ifstream file(std::filesystem::u8path(path),std::ios::binary);Require(bool(file),"cannot read retarget profile: "+path);std::string text((std::istreambuf_iterator<char>(file)),{});return ParseRetargetProfile(text,out,error);}catch(const std::exception& e){error=e.what();return false;}}
bool SaveRetargetProfile(const std::string& path,const RetargetProfile& p,std::string& error){std::string temporary;try{auto destination=std::filesystem::u8path(path);Require(destination.extension()==".judasretarget","retarget profile must use .judasretarget");auto text=SerializeRetargetProfile(p);RetargetProfile checked;Require(ParseRetargetProfile(text,checked,error),error);std::filesystem::create_directories(destination.parent_path());temporary=CreateImportStagingFile(destination);{std::ofstream file(std::filesystem::u8path(temporary),std::ios::binary);file.write(text.data(),text.size());file.flush();Require(bool(file),"cannot write staged retarget profile");file.close();Require(bool(file),"cannot close staged retarget profile");}ReplaceStagedFile(std::filesystem::u8path(temporary),destination);error.clear();return true;}catch(const std::exception& e){if(!temporary.empty()){std::error_code ignored;std::filesystem::remove(std::filesystem::u8path(temporary),ignored);}error=e.what();return false;}}
bool ValidateRetargetProfile(const Skeleton& source,const Skeleton& target,const RetargetProfile& p,std::string& error){try{(void)Prepare(source,target,p);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool RetargetPose(const Skeleton& source,const Skeleton& target,const RetargetProfile& p,const SkeletalPose& input,SkeletalPose& out,std::string& error){try{auto plan=Prepare(source,target,p);auto pose=Transfer(source,target,plan,input);out=std::move(pose);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool BakeRetargetClip(const Skeleton& source,const AnimationClip& input,const Skeleton& target,const RetargetProfile& p,const RetargetBakeSettings& settings,AnimationClip& out,std::string& error,RetargetBakeReport* report){try{
 auto start=std::chrono::steady_clock::now();auto cancelled=[&]{Require(!(settings.cancelled&&settings.cancelled()),"retarget cancelled; last-good output retained");};cancelled();auto plan=Prepare(source,target,p);
 Require(input.motion.empty()&&input.motionTimes.empty(),"retarget source has extracted root-motion track; use original unextracted source performance");
 Require(std::isfinite(input.duration)&&input.duration>=0&&input.duration<=kRetargetMaxDuration,"retarget source duration bound is 0..3600 seconds");
 float begin=settings.begin,end=settings.end<0?input.duration:settings.end;Require(std::isfinite(begin)&&std::isfinite(end)&&begin>=0&&end>=begin&&end<=input.duration,"retarget trim must lie in source duration");
 Require(std::isfinite(settings.sampleRate)&&settings.sampleRate>=1&&settings.sampleRate<=kRetargetMaxRate,"retarget sampleRate must be 1..240 Hz");Require(!settings.name.empty()&&settings.name.size()<=1024,"retarget output clip needs bounded explicit name");
 // A set of OUTPUT float times removes near-equal float quantization collisions
 // before emitting keys; source trim endpoints remain explicit and exact.
 size_t nodeCount=source.names.size()+target.names.size();size_t maximumSamples=std::min(kRetargetMaxSamples,kRetargetMaxNodeEvaluations/nodeCount);
 std::set<float> times{0,end-begin};size_t grid=size_t(std::ceil(double(end-begin)*settings.sampleRate));Require(grid+1<=maximumSamples,"retarget sample/work bound exceeds one million samples or 64 million source+target node evaluations");
 for(size_t k=1;k<grid;++k){float time=float(double(k)/settings.sampleRate);if(time>0&&time<end-begin)times.insert(time);}
 size_t sourceKeys=0;std::set<std::pair<int,TrackPath>> uniqueTracks;
 for(const auto& track:input.tracks){Require(track.node>=0&&size_t(track.node)<source.names.size()&&uniqueTracks.insert({track.node,track.path}).second,"retarget source has invalid or duplicate track");Require(int(track.path)>=0&&int(track.path)<=2&&int(track.interpolation)>=0&&int(track.interpolation)<=2,"retarget source track path/interpolation");Require(!track.times.empty(),"retarget source has empty track");sourceKeys+=track.times.size();Require(sourceKeys<=kRetargetMaxKeys,"retarget source exceeds four million keys");Require(track.values.size()==track.times.size()*(track.interpolation==TrackInterpolation::CubicSpline?3:1),"retarget source track value count");float previous=-1;
  for(float time:track.times){Require(std::isfinite(time)&&time>=0&&time<=input.duration&&time>previous,"retarget source key times must be strictly increasing");previous=time;if(time>begin&&time<end){times.insert(time-begin);Require(times.size()<=maximumSamples,"retarget source plus bake sample/work bound");}}
  for(const auto& v:track.values)for(int i=0;i<4;++i)Require(std::isfinite(v[i]),"retarget source nonfinite key");
  if(track.path==TrackPath::Rotation)for(size_t k=0;k<track.values.size();++k)if(track.interpolation!=TrackInterpolation::CubicSpline||k%3==1)Require(glm::dot(track.values[k],track.values[k])>1e-12f,"retarget source has zero rotation key");
  if(track.interpolation==TrackInterpolation::Step&&plan.sourcePath[track.node])for(size_t k=1;k<track.values.size();++k){bool same=track.path==TrackPath::Rotation?std::abs(glm::dot(glm::normalize(track.values[k]),glm::normalize(track.values[0])))>1-1e-6f:glm::length(track.values[k]-track.values[0])<1e-6f;Require(same,"retarget animated STEP discontinuities on participating paths are unsupported; use a continuous source performance");}
  if(track.path==TrackPath::Scale&&plan.sourcePath[track.node]){for(size_t k=0;k<track.values.size();++k){bool tangent=track.interpolation==TrackInterpolation::CubicSpline&&k%3!=1;auto expected=tangent?glm::vec3(0):source.rest.local[track.node].scale;Require(glm::length(glm::vec3(track.values[k])-expected)<=1e-5f*std::max(1.f,glm::length(expected)),"retarget animated scale unsupported on participating paths");}}
 }
 Require(times.size()<=maximumSamples,"retarget source plus bake sample/work bound");
 std::vector<int> animated;for(int i:target.order)if(plan.sourceForTarget[i]>=0)animated.push_back(i);
 size_t tracks=animated.size()+p.translationJoints.size();Require(times.size()<=kRetargetMaxKeys/std::max(size_t(1),tracks),"retarget output exceeds four million keys; reduce duration/rate/mapped count");
 AnimationClip clip;clip.name=settings.name;clip.duration=end-begin;clip.loop=input.loop;
 // Only mapped rotations and explicitly permitted translations are baked.
 // Unmapped helper corrections, if any, need constant local rotation tracks.
 for(const auto& correction:p.targetReference){int i=Exact(plan.targetIndex,correction.joint);if(plan.sourceForTarget[i]<0)animated.push_back(i);}
 std::sort(animated.begin(),animated.end());animated.erase(std::unique(animated.begin(),animated.end()),animated.end());tracks=animated.size()+p.translationJoints.size();Require(times.size()<=kRetargetMaxKeys/std::max(size_t(1),tracks),"retarget correction/output key bound");
 for(int i:animated){AnimationTrack rotation;rotation.node=i;rotation.path=TrackPath::Rotation;rotation.times.reserve(times.size());rotation.values.reserve(times.size());clip.tracks.push_back(std::move(rotation));if(plan.translate[i]){AnimationTrack translation;translation.node=i;translation.path=TrackPath::Translation;translation.times.reserve(times.size());translation.values.reserve(times.size());clip.tracks.push_back(std::move(translation));}}
 for(float time:times){cancelled();float sourceTime=time==clip.duration?end:begin+time;auto pose=Transfer(source,target,plan,SampleClip(source,input,sourceTime));for(auto& track:clip.tracks){const auto& t=pose.local[track.node];glm::vec4 value;if(track.path==TrackPath::Translation)value={t.translation,0};else {auto q=t.rotation;if(!track.values.empty()&&glm::dot(glm::vec4(q.x,q.y,q.z,q.w),track.values.back())<0)q=-q;value={q.x,q.y,q.z,q.w};}track.times.push_back(time);track.values.push_back(value);}}
 RetargetBakeReport result;result.samples=times.size();result.mappedJoints=p.mapping.size();result.outputKeys=times.size()*tracks;result.estimatedWorkingBytes=result.outputKeys*(sizeof(float)+sizeof(glm::vec4))+(source.names.size()+target.names.size())*(sizeof(JointTransform)*3+sizeof(glm::mat4)*3)+times.size()*48;result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 cancelled();out=std::move(clip);if(report)*report=result;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
