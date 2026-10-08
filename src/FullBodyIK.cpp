#include "FullBodyIK.h"
#include "PerformanceProfiler.h"
#include "Ragdoll.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
namespace {
using Json=nlohmann::ordered_json;
bool Finite(glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool Finite(glm::dvec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool Quaternion(glm::quat q){float n=glm::dot(q,q);return std::isfinite(n)&&n>1e-12f;}
bool Key(const std::string& v){return !v.empty()&&v.size()<=256;}
glm::vec3 Log(glm::quat q){q=glm::normalize(q);if(q.w<0)q=-q;glm::vec3 v(q.x,q.y,q.z);float l=glm::length(v);return l<1e-8f?2.f*v:v*(2.f*std::atan2(l,std::max(0.f,q.w))/l);}
glm::quat Exp(glm::vec3 v){float l=glm::length(v);return l<1e-8f?glm::normalize(glm::quat(1,.5f*v)):glm::angleAxis(l,v/l);}
bool Ancestor(const Skeleton& s,int ancestor,int node){for(size_t n=0;n<=s.parents.size()&&node>=0;++n,node=s.parents[node])if(node==ancestor)return true;return false;}
bool Uniform(const glm::mat4& m,JointTransform& out,std::string& error){if(!DecomposeRigidPose(m,out,error))return false;if(glm::length(out.scale-glm::vec3(out.scale.x))>1e-4f*std::max(1.f,out.scale.x)){error="IK mapped path requires positive uniform scale";return false;}return true;}
void ClampPose(const Skeleton& s,const FullBodyIKMapping& map,SkeletalPose& pose){for(size_t i=0;i<map.settings.limits.size();++i){const auto& l=map.settings.limits[i];int node=map.limitNodes[i];auto frame=glm::normalize(l.frame);auto delta=Log(glm::inverse(frame)*glm::inverse(glm::normalize(s.rest.local[node].rotation))*pose.local[node].rotation*frame);delta=glm::clamp(delta,l.min,l.max);pose.local[node].rotation=glm::normalize(s.rest.local[node].rotation*frame*Exp(delta)*glm::inverse(frame));}}
struct Contact {const FullBodyIKTarget* target;int end;std::vector<uint8_t> affected;};
struct Evaluation {
 std::vector<glm::mat4> global;std::vector<glm::quat> rotations,parentFrames;
 std::vector<glm::vec3> points;std::vector<glm::quat> orientations;
 std::vector<float> errors;float objective=0;bool converged=true;
};
bool Evaluate(const Skeleton& s,const SkeletalPose& pose,const FullBodyIKMapping& map,const std::vector<Contact>& contacts,Evaluation& e,std::string& error){
 e.global=ResolveJointMatrices(s,pose);e.rotations.resize(map.joints.size());e.parentFrames.resize(map.joints.size());
 for(size_t j=0;j<map.joints.size();++j){int n=map.joints[j];JointTransform value,parent,fixed;if(!Uniform(e.global[n],value,error))return false;e.rotations[j]=value.rotation;
  if(s.parents[n]>=0&&!Uniform(e.global[s.parents[n]],parent,error))return false;
  if(!s.affine.empty()&&!Uniform(s.affine[n],fixed,error))return false;
  e.parentFrames[j]=glm::normalize(parent.rotation*fixed.rotation);}
 e.errors.clear();e.points.clear();e.orientations.clear();e.objective=0;e.converged=true;
 for(const auto& contact:contacts){const auto& t=*contact.target;JointTransform joint;if(!Uniform(e.global[contact.end],joint,error))return false;auto p=glm::vec3(e.global[contact.end]*glm::vec4(t.offset,1));auto q=glm::normalize(joint.rotation*t.frame);e.points.push_back(p);e.orientations.push_back(q);
  auto position=glm::vec3(t.position)-p;auto orientation=Log(glm::normalize(t.orientation)*glm::inverse(q));float wp=std::sqrt(t.positionWeight),wq=std::sqrt(t.orientationWeight)*map.settings.orientationScale;
  for(int k=0;k<3;++k)e.errors.push_back(wp*position[k]);
  for(int k=0;k<3;++k)e.errors.push_back(wq*orientation[k]);
  if(t.positionWeight>0&&glm::length(position)>map.settings.positionTolerance)e.converged=false;
  if(t.orientationWeight>0&&glm::length(orientation)>map.settings.orientationTolerance)e.converged=false;
 }
 for(float v:e.errors)e.objective+=v*v;
 return std::isfinite(e.objective);
}
// Cholesky on JJ^T + lambda^2 I. There are at most 72 target residual rows;
// the skeleton size does not determine the linear-system dimension.
bool SolveSPD(std::vector<double> a,const std::vector<float>& rhs,std::vector<double>& x){size_t n=rhs.size();std::vector<double> lower(n*n,0);for(size_t i=0;i<n;++i)for(size_t j=0;j<=i;++j){double v=a[i*n+j];for(size_t k=0;k<j;++k)v-=lower[i*n+k]*lower[j*n+k];if(i==j){if(v<=0||!std::isfinite(v))return false;lower[i*n+j]=std::sqrt(v);}else lower[i*n+j]=v/lower[j*n+j];}x.assign(n,0);for(size_t i=0;i<n;++i){double v=rhs[i];for(size_t k=0;k<i;++k)v-=lower[i*n+k]*x[k];x[i]=v/lower[i*n+i];}for(size_t z=n;z>0;--z){size_t i=z-1;double v=x[i];for(size_t k=i+1;k<n;++k)v-=lower[k*n+i]*x[k];x[i]=v/lower[i*n+i];}return true;}
Json V(glm::vec3 v){return Json::array({v.x,v.y,v.z});}Json V(glm::dvec3 v){return Json::array({v.x,v.y,v.z});}Json Q(glm::quat q){return Json::array({q.x,q.y,q.z,q.w});}
glm::vec3 ReadV(const Json& j){if(!j.is_array()||j.size()!=3)throw std::runtime_error("IK vector requires three numbers");return {j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};}
glm::dvec3 ReadD(const Json& j){if(!j.is_array()||j.size()!=3)throw std::runtime_error("IK position requires three numbers");return {j[0].get<double>(),j[1].get<double>(),j[2].get<double>()};}
glm::quat ReadQ(const Json& j){if(!j.is_array()||j.size()!=4)throw std::runtime_error("IK quaternion is [x,y,z,w]");return {j[3].get<float>(),j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};}
}
bool ValidFullBodyIKTargets(const std::vector<FullBodyIKTarget>& targets,std::string& error){
 if(targets.size()>kFullBodyIKTargetLimit){error="IK at most 12 targets";return false;}std::set<std::string> ids;
 for(size_t i=0;i<targets.size();++i){const auto& t=targets[i];if(!Key(t.id)||!ids.insert(t.id).second||!Key(t.chain)||int(t.space)<0||int(t.space)>1||!Finite(t.position)||!Finite(t.offset)||!Quaternion(t.orientation)||!Quaternion(t.frame)||!std::isfinite(t.positionWeight)||t.positionWeight<0||t.positionWeight>1||!std::isfinite(t.orientationWeight)||t.orientationWeight<0||t.orientationWeight>1){error="IK target["+std::to_string(i)+"] invalid identity/frame/weight";return false;}}
 return true;
}
bool ValidFullBodyIKSettings(const FullBodyIKSettings& k,std::string& error){
 if(!Key(k.bodyRoot)||k.chains.empty()||k.chains.size()>kFullBodyIKTargetLimit||k.spine.size()>kFullBodyIKJointLimit||k.limits.size()>kFullBodyIKJointLimit||!Finite(k.rootMin)||!Finite(k.rootMax)||glm::any(glm::greaterThan(k.rootMin,k.rootMax))||glm::any(glm::greaterThan(k.rootMin,glm::vec3(0)))||glm::any(glm::lessThan(k.rootMax,glm::vec3(0)))||glm::any(glm::greaterThan(glm::abs(k.rootMin),glm::vec3(10)))||glm::any(glm::greaterThan(glm::abs(k.rootMax),glm::vec3(10)))||k.iterations<1||k.iterations>kFullBodyIKIterationLimit){error="IK invalid body root, bounds, chain count or iteration cap";return false;}
 for(float v:{k.damping,k.positionTolerance,k.orientationTolerance,k.orientationScale,k.maxAngularStep,k.maxTranslationStep})if(!std::isfinite(v)||v<=0){error="IK numerical settings must be finite and positive";return false;}
 if(k.damping>1||k.orientationTolerance>glm::pi<float>()||k.orientationScale>10||k.maxAngularStep>1||k.maxTranslationStep>1||k.positionTolerance>1){error="IK numerical settings exceed supported bounds";return false;}
 std::set<std::string> ids;for(size_t i=0;i<k.chains.size();++i){const auto& c=k.chains[i];if(!Key(c.id)||!ids.insert(c.id).second||c.joints.empty()||c.joints.size()>kFullBodyIKJointLimit){error="IK chain["+std::to_string(i)+"] invalid identity/length";return false;}std::set<std::string> keys;for(const auto& key:c.joints)if(!Key(key)||!keys.insert(key).second){error="IK chain["+std::to_string(i)+"] duplicate/invalid joint";return false;}}
 ids.clear();for(size_t i=0;i<k.limits.size();++i){const auto& l=k.limits[i];if(!Key(l.joint)||!ids.insert(l.joint).second||!Quaternion(l.frame)||!Finite(l.min)||!Finite(l.max)||!Finite(l.preferred)||glm::any(glm::greaterThan(l.min,l.max))||glm::any(glm::greaterThan(glm::abs(l.min),glm::vec3(glm::pi<float>())))||glm::any(glm::greaterThan(glm::abs(l.max),glm::vec3(glm::pi<float>())))||glm::any(glm::lessThan(l.preferred,l.min))||glm::any(glm::greaterThan(l.preferred,l.max))||!std::isfinite(l.preferenceWeight)||l.preferenceWeight<0||l.preferenceWeight>1){error="IK limit["+std::to_string(i)+"] invalid authored rotation-vector range/frame";return false;}}
 if(!ValidFullBodyIKTargets(k.targets,error))return false;
 for(size_t i=0;i<k.targets.size();++i){bool found=false;for(const auto& c:k.chains)found|=c.id==k.targets[i].chain;if(!found){error="IK target["+std::to_string(i)+"] missing chain";return false;}}
 return true;
}
bool PrepareFullBodyIK(const Skeleton& s,const FullBodyIKSettings& k,FullBodyIKMapping& output,std::string& error){
 if(!ValidFullBodyIKSettings(k,error))return false;
 if(s.names.size()!=s.parents.size()||s.rest.local.size()!=s.names.size()||s.names.empty()||s.names.size()>kModelNodeLimit){error="IK invalid skeleton hierarchy";return false;}
 for(size_t i=0;i<s.parents.size();++i){int n=int(i);std::set<int> visited;while(n>=0){if(size_t(n)>=s.parents.size()||!visited.insert(n).second){error="IK cyclic or invalid skeleton parent";return false;}n=s.parents[n];}}
 FullBodyIKMapping map;map.settings=k;map.root=FindSkeletonJoint(s,k.bodyRoot);if(map.root<0){error="IK missing/ambiguous body root: "+k.bodyRoot;return false;}std::set<int> nodes;if(k.rootRotation)nodes.insert(map.root);
 for(size_t i=0;i<k.chains.size();++i){FullBodyIKMapping::Chain c;c.id=k.chains[i].id;for(const auto& key:k.chains[i].joints){int n=FindSkeletonJoint(s,key);if(n<0||!Ancestor(s,map.root,n)||(!c.joints.empty()&&!Ancestor(s,c.joints.back(),n))){error="IK chain["+std::to_string(i)+"] missing/non-descendant joint: "+key;return false;}c.joints.push_back(n);nodes.insert(n);}c.end=c.joints.back();map.chains.push_back(std::move(c));}
 std::set<int> spine;for(size_t i=0;i<k.spine.size();++i){int n=FindSkeletonJoint(s,k.spine[i]);bool useful=false;for(const auto& c:map.chains)useful|=n>=0&&Ancestor(s,n,c.end);if(n<0||!Ancestor(s,map.root,n)||!useful||!spine.insert(n).second){error="IK spine["+std::to_string(i)+"] missing/duplicate/nonparticipating ancestor";return false;}nodes.insert(n);}
 if(!k.rootRotation)nodes.erase(map.root);
 if(nodes.size()>kFullBodyIKJointLimit){error="IK at most 64 participating joints";return false;}for(int n:s.order)if(nodes.count(n))map.joints.push_back(n);if(map.joints.size()!=nodes.size()){error="IK invalid skeleton traversal";return false;}
 for(size_t i=0;i<k.limits.size();++i){int n=FindSkeletonJoint(s,k.limits[i].joint);if(n<0||!nodes.count(n)){error="IK limit["+std::to_string(i)+"] joint is not a participant";return false;}map.limitNodes.push_back(n);}
 // Inspect only participant ancestor paths. A head-hiding scale of zero on an
 // unrelated branch remains valid and does not require an inverse.
 auto globals=ResolveJointMatrices(s,s.rest);std::set<int> inspect{map.root};for(int n:map.joints)for(;n>=0;n=s.parents[n])inspect.insert(n);for(int n:inspect){JointTransform d;if(!Uniform(globals[n],d,error)){error="IK mapped joint "+SkeletonJointKey(s,n)+": "+error;return false;}}
 map.parents=s.parents;map.names=s.names;output=std::move(map);return true;
}
bool FullBodyIKTargetsToModel(const std::vector<FullBodyIKTarget>& targets,glm::dvec3 position,glm::quat orientation,glm::vec3 scale,std::vector<FullBodyIKTarget>& output,std::string& error){
 if(!ValidFullBodyIKTargets(targets,error)||!Finite(position)||!Quaternion(orientation)||!Finite(scale)||scale.x<=1e-6f||glm::length(scale-glm::vec3(scale.x))>1e-5f*std::max(1.f,scale.x)){error="IK world conversion requires a finite positive uniform entity frame";return false;}
 auto result=targets;auto inverse=glm::inverse(glm::normalize(orientation));for(auto& t:result){if(t.space==IKTargetSpace::World){auto relative=t.position-position;if(glm::length(relative)>1e6){error="IK world target lies outside supported local range";return false;}t.position=glm::dvec3(inverse*glm::vec3(relative))/double(scale.x);t.orientation=glm::normalize(inverse*t.orientation);t.space=IKTargetSpace::Model;}if(glm::length(t.position)>1e6){error="IK model target lies outside supported local range";return false;}t.orientation=glm::normalize(t.orientation);t.frame=glm::normalize(t.frame);}output=std::move(result);return true;
}
bool SolveFullBodyIK(const Skeleton& s,const SkeletalPose& reference,const FullBodyIKMapping& map,const std::vector<FullBodyIKTarget>& targets,FullBodyIKResult& output,std::string& error){
 JUDAS_PROFILE_SCOPE("Multi-target full-body IK");auto start=std::chrono::steady_clock::now();
 if(map.parents!=s.parents||map.names!=s.names||map.root<0||size_t(map.root)>=s.parents.size()){error="IK mapping is stale after skeleton replacement";return false;}if(!ValidPose(s,reference,error)||!ValidFullBodyIKTargets(targets,error))return false;
 std::vector<const FullBodyIKTarget*> sorted;for(size_t i=0;i<targets.size();++i){const auto& t=targets[i];if(t.space!=IKTargetSpace::Model||glm::length(t.position)>1e6){error="IK target["+std::to_string(i)+"] must be converted to bounded model space";return false;}if(std::none_of(map.chains.begin(),map.chains.end(),[&](const auto& c){return c.id==t.chain;})){error="IK target["+std::to_string(i)+"] missing mapped chain";return false;}sorted.push_back(&t);}std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a->id<b->id;});
 std::vector<Contact> contacts;for(const auto* t:sorted){auto c=std::find_if(map.chains.begin(),map.chains.end(),[&](const auto& x){return x.id==t->chain;});Contact v{t,c->end,{}};for(int n:map.joints)v.affected.push_back(Ancestor(s,n,v.end));contacts.push_back(std::move(v));}
 // Keep disabled and zero-weight targets in diagnostics while excluding their
 // rows from the active solve. Identity and field validation still apply.
 std::vector<Contact> active;for(const auto& c:contacts)if(map.settings.enabled&&c.target->enabled&&(c.target->positionWeight>0||c.target->orientationWeight>0))active.push_back(c);
 FullBodyIKResult result;result.pose=reference;Evaluation evaluated;if(!Evaluate(s,result.pose,map,active,evaluated,error))return false;
 glm::mat3 rootParent(1);if(!s.affine.empty())rootParent=glm::mat3(s.affine[map.root]);if(s.parents[map.root]>=0)rootParent=glm::mat3(evaluated.global[s.parents[map.root]])*rootParent;float det=glm::determinant(rootParent);if(!std::isfinite(det)||std::abs(det)<1e-12f){error="IK root translation frame is singular";return false;}auto inverseRoot=glm::inverse(rootParent);
 if(!active.empty()){
  bool needsCorrection=!evaluated.converged;ClampPose(s,map,result.pose);
  // An explicit small bend preference removes the straight-chain Jacobian
  // singularity. It is not inferred from names, gravity, or a universal axis.
  for(size_t i=0;i<map.limitNodes.size();++i){const auto& l=map.settings.limits[i];if(needsCorrection&&l.preferenceWeight>0){int n=map.limitNodes[i];auto f=glm::normalize(l.frame);auto preferred=glm::normalize(s.rest.local[n].rotation*f*Exp(l.preferred)*glm::inverse(f));result.pose.local[n].rotation=glm::normalize(glm::slerp(result.pose.local[n].rotation,preferred,.2f*l.preferenceWeight));}}
  ClampPose(s,map,result.pose);if(!Evaluate(s,result.pose,map,active,evaluated,error))return false;
 }
 const size_t columns=3+map.joints.size()*3;float lambda=map.settings.damping;
 for(unsigned iteration=0;iteration<map.settings.iterations&&!active.empty()&&!evaluated.converged;++iteration){result.iterations=iteration+1;const size_t rows=evaluated.errors.size();std::vector<double> jacobian(rows*columns,0);
  for(size_t c=0;c<active.size();++c){const auto& contact=active[c];float wp=std::sqrt(contact.target->positionWeight),wq=std::sqrt(contact.target->orientationWeight)*map.settings.orientationScale;
   for(int k=0;k<3;++k)jacobian[(c*6+k)*columns+k]=wp;
   for(size_t j=0;j<map.joints.size();++j)if(contact.affected[j]){auto origin=glm::vec3(evaluated.global[map.joints[j]][3]);for(int axis=0;axis<3;++axis){glm::vec3 unit(0);unit[axis]=1;auto direction=evaluated.parentFrames[j]*unit;auto delta=glm::cross(direction,evaluated.points[c]-origin);size_t column=3+j*3+axis;for(int k=0;k<3;++k){jacobian[(c*6+k)*columns+column]=wp*delta[k];jacobian[(c*6+3+k)*columns+column]=wq*direction[k];}}}
  }
  // Locked root axes have zero Jacobian; finite bounds are enforced by projection
  // and a monotone line search rather than stretching the effectors.
  for(int axis=0;axis<3;++axis)if(map.settings.rootMin[axis]==map.settings.rootMax[axis])for(size_t row=0;row<rows;++row)jacobian[row*columns+axis]=0;
  std::vector<double> gram(rows*rows,0);for(size_t a=0;a<rows;++a)for(size_t b=0;b<=a;++b){double value=0;for(size_t col=0;col<columns;++col)value+=jacobian[a*columns+col]*jacobian[b*columns+col];gram[a*rows+b]=gram[b*rows+a]=value;}for(size_t row=0;row<rows;++row)gram[row*rows+row]+=double(lambda)*lambda;
  std::vector<double> solved;if(!SolveSPD(std::move(gram),evaluated.errors,solved)){error="IK bounded damped system failed finite factorization";return false;}std::vector<double> step(columns,0);for(size_t col=0;col<columns;++col)for(size_t row=0;row<rows;++row)step[col]+=jacobian[row*columns+col]*solved[row];
  glm::vec3 translation{float(step[0]),float(step[1]),float(step[2])};float length=glm::length(translation);if(length>map.settings.maxTranslationStep)translation*=map.settings.maxTranslationStep/length;
  std::vector<glm::vec3> rotation(map.joints.size());for(size_t j=0;j<rotation.size();++j){rotation[j]={float(step[3+j*3]),float(step[4+j*3]),float(step[5+j*3])};float magnitude=glm::length(rotation[j]);if(magnitude>map.settings.maxAngularStep)rotation[j]*=map.settings.maxAngularStep/magnitude;}
  bool accepted=false;for(unsigned search=0;search<7;++search){float fraction=std::ldexp(1.f,-int(search));auto candidate=result.pose;auto correction=glm::clamp(result.rootCorrection+fraction*translation,map.settings.rootMin,map.settings.rootMax);candidate.local[map.root].translation=reference.local[map.root].translation+inverseRoot*correction;for(size_t j=0;j<map.joints.size();++j){int n=map.joints[j];candidate.local[n].rotation=glm::normalize(Exp(fraction*rotation[j])*candidate.local[n].rotation);}ClampPose(s,map,candidate);Evaluation next;if(!Evaluate(s,candidate,map,active,next,error))return false;if(next.objective+1e-12f<evaluated.objective){result.pose=std::move(candidate);result.rootCorrection=correction;evaluated=std::move(next);lambda=std::max(map.settings.damping,lambda*.5f);accepted=true;break;}}
  if(!accepted){lambda=std::min(1.f,lambda*2);if(lambda>=1.f)break;}
 }
 result.converged=evaluated.converged;Evaluation final;if(!Evaluate(s,result.pose,map,contacts,final,error))return false;for(size_t i=0;i<contacts.size();++i){const auto& t=*contacts[i].target;FullBodyIKResidual residual;residual.id=t.id;residual.actualPosition=final.points[i];residual.actualOrientation=final.orientations[i];residual.positionError=glm::length(glm::vec3(t.position)-final.points[i]);residual.orientationError=glm::length(Log(glm::normalize(t.orientation)*glm::inverse(final.orientations[i])));bool enabled=map.settings.enabled&&t.enabled&&(t.positionWeight>0||t.orientationWeight>0);bool reached=(t.positionWeight==0||residual.positionError<=map.settings.positionTolerance)&&(t.orientationWeight==0||residual.orientationError<=map.settings.orientationTolerance);residual.status=!enabled?"disabled":reached?"reached":"limited";result.targets.push_back(std::move(residual));}
 result.solveMicroseconds=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(!ValidPose(s,result.pose,error))return false;output=std::move(result);return true;
}
std::string SerializeFullBodyIKSettings(const FullBodyIKSettings& k){Json j={{"enabled",k.enabled},{"bodyRoot",k.bodyRoot},{"rootRotation",k.rootRotation},{"rootMin",V(k.rootMin)},{"rootMax",V(k.rootMax)},{"spine",k.spine},{"iterations",k.iterations},{"damping",k.damping},{"positionTolerance",k.positionTolerance},{"orientationTolerance",k.orientationTolerance},{"orientationScale",k.orientationScale},{"maxAngularStep",k.maxAngularStep},{"maxTranslationStep",k.maxTranslationStep},{"chains",Json::array()},{"limits",Json::array()},{"targets",Json::array()}};for(const auto& c:k.chains)j["chains"].push_back({{"id",c.id},{"joints",c.joints}});for(const auto& l:k.limits)j["limits"].push_back({{"joint",l.joint},{"frame",Q(l.frame)},{"min",V(l.min)},{"max",V(l.max)},{"preferred",V(l.preferred)},{"preferenceWeight",l.preferenceWeight}});for(const auto& t:k.targets)j["targets"].push_back({{"id",t.id},{"chain",t.chain},{"enabled",t.enabled},{"space",t.space==IKTargetSpace::World?"world":"model"},{"position",V(t.position)},{"orientation",Q(t.orientation)},{"positionWeight",t.positionWeight},{"orientationWeight",t.orientationWeight},{"offset",V(t.offset)},{"frame",Q(t.frame)}});return j.dump();}
bool ParseFullBodyIKSettings(const std::string& text,FullBodyIKSettings& output,std::string& error){try{if(text.size()>256*1024)throw std::runtime_error("IK configuration exceeds 256 KiB");auto j=Json::parse(text,[](int depth,Json::parse_event_t,const Json&){if(depth>16)throw std::runtime_error("IK configuration nesting bound");return true;});if(!j.is_object())throw std::runtime_error("IK configuration must be an object");FullBodyIKSettings k;k.enabled=j.value("enabled",true);k.bodyRoot=j.at("bodyRoot").get<std::string>();k.rootRotation=j.value("rootRotation",true);if(j.contains("rootMin"))k.rootMin=ReadV(j["rootMin"]);if(j.contains("rootMax"))k.rootMax=ReadV(j["rootMax"]);if(j.contains("spine"))k.spine=j["spine"].get<std::vector<std::string>>();k.iterations=j.value("iterations",64u);k.damping=j.value("damping",.02f);k.positionTolerance=j.value("positionTolerance",.005f);k.orientationTolerance=j.value("orientationTolerance",.017453293f);k.orientationScale=j.value("orientationScale",.25f);k.maxAngularStep=j.value("maxAngularStep",.22f);k.maxTranslationStep=j.value("maxTranslationStep",.06f);
 for(const auto& c:j.at("chains")){if(k.chains.size()>=kFullBodyIKTargetLimit)throw std::runtime_error("IK chain bound");k.chains.push_back({c.at("id").get<std::string>(),c.at("joints").get<std::vector<std::string>>()});}
 if(j.contains("limits"))for(const auto& v:j["limits"]){if(k.limits.size()>=kFullBodyIKJointLimit)throw std::runtime_error("IK limit bound");FullBodyIKJointLimit l;l.joint=v.at("joint").get<std::string>();if(v.contains("frame"))l.frame=ReadQ(v["frame"]);if(v.contains("min"))l.min=ReadV(v["min"]);if(v.contains("max"))l.max=ReadV(v["max"]);if(v.contains("preferred"))l.preferred=ReadV(v["preferred"]);l.preferenceWeight=v.value("preferenceWeight",0.f);k.limits.push_back(std::move(l));}
 if(j.contains("targets"))for(const auto& v:j["targets"]){if(k.targets.size()>=kFullBodyIKTargetLimit)throw std::runtime_error("IK target bound");FullBodyIKTarget t;t.id=v.at("id").get<std::string>();t.chain=v.at("chain").get<std::string>();t.enabled=v.value("enabled",true);auto space=v.value("space",std::string("model"));if(space!="model"&&space!="world")throw std::runtime_error("IK target space must be model or world");t.space=space=="model"?IKTargetSpace::Model:IKTargetSpace::World;if(v.contains("position"))t.position=ReadD(v["position"]);if(v.contains("orientation"))t.orientation=ReadQ(v["orientation"]);t.positionWeight=v.value("positionWeight",1.f);t.orientationWeight=v.value("orientationWeight",0.f);if(v.contains("offset"))t.offset=ReadV(v["offset"]);if(v.contains("frame"))t.frame=ReadQ(v["frame"]);k.targets.push_back(std::move(t));}
 if(!ValidFullBodyIKSettings(k,error))return false;
 output=std::move(k);return true;}catch(const std::exception& e){error=e.what();return false;}}
