// M83 representation feasibility only. Not a complete vehicle or crash-quality
// acceptance: collision shapes, mass and ordinary M45 joints are real, but
// there is no wheel/powertrain/render/JS cost and no plastic mechanism yet.
#include "PhysicsWorld.h"
#include "JointTypes.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
#include <map>
#include <string>
struct Member {BodyHandle body;glm::vec3 offset;};
void Run(unsigned cars,bool spring,bool impact,glm::quat rotation){
 PhysicsWorld w;w.Init();const float h=1.f/60;const auto world=[&](glm::vec3 p){return rotation*p;};
 const auto create=[&](glm::vec3 p,glm::vec3 half,float mass){BodyTransform t;t.position=world(p);t.rotation=rotation;return w.CreateShape(Shape::Box(half),t,mass>0,mass,.5f,0);};
 create({0,-.15f,0},{40,.15f,40},0);const auto wall=create({0,1,-.4f},{40,1,.4f},0);
 std::vector<Member> all;std::vector<double> times;unsigned countJoints=0,firstContact=0;bool finite=true;double drift=0;std::map<std::string,std::vector<double>> stages;
 for(unsigned car=0;car<cars;++car){const glm::vec3 origin((float(car)-float(cars-1)*.5f)*5,1.0f,impact?8:18);
  const auto root=create(origin,{.8f,.28f,.75f},500);all.push_back({root,{0,0,0}});
  for(unsigned side=0;side<2;++side){auto parent=root;glm::vec3 parentOffset(0);
   for(unsigned cell=0;cell<3;++cell){glm::vec3 offset(side?.6f:-.6f,-.05f,-(1.0f+.5f*cell));const auto child=create(origin+offset,{.11f,.10f,.22f},8);all.push_back({child,offset});
    JointSettings s;s.bodyA=parent;s.bodyB=child;s.spring=true;s.stiffness=1200000;s.damping=1500;const auto jointPoint=(parentOffset+offset)*.5f;s.anchorA=jointPoint-parentOffset;s.anchorB=jointPoint-offset;
    if(spring){s.type=JointType::Slider;s.frameA=s.frameB=glm::angleAxis(1.57079632679f,glm::vec3(0,1,0));s.spring=true;s.stiffness=120000;s.damping=1200;s.limits=true;s.lower=-.35f;s.upper=.03f;}
    const auto joint=w.CreateJoint(s);finite&=joint.IsValid();++countJoints;w.SetPairCollisionEnabled(parent,child,false);parent=child;parentOffset=offset;
   }
  }
  for(unsigned panel=0;panel<4;++panel){glm::vec3 offset(panel&1?.9f:-.9f,.10f,panel<2?-.45f:.45f);const auto child=create(origin+offset,{.025f,.18f,.25f},2);all.push_back({child,offset});JointSettings s;s.bodyA=root;s.bodyB=child;s.anchorA=offset;s.spring=true;s.stiffness=1200000;s.damping=1500;finite&=w.CreateJoint(s).IsValid();++countJoints;w.SetPairCollisionEnabled(root,child,false);}
  for(unsigned side=0;side<2;++side){glm::vec3 offset(side?.72f:-.72f,-.28f,-.55f);const auto child=create(origin+offset,{.08f,.08f,.08f},10);all.push_back({child,offset});JointSettings s;s.bodyA=root;s.bodyB=child;s.anchorA=offset;s.spring=true;s.stiffness=1200000;s.damping=1500;finite&=w.CreateJoint(s).IsValid();++countJoints;w.SetPairCollisionEnabled(root,child,false);}
 }
 for(auto& m:all)w.SetLinearVelocity(m.body,world({0,0,impact?-15.f:0}));
 for(unsigned step=0;step<600;++step){auto begin=std::chrono::steady_clock::now();for(auto& m:all)w.ApplyLinearAcceleration(m.body,world({0,-9.81f,0}),h);PhysicsWorld::VelocityCoupling quality;quality.iterations=12;w.StepCoupled(h,2,{}, {},&quality);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();times.push_back(ms);
  if(impact&&!firstContact)for(auto& m:all){std::vector<PhysicsWorld::BodyContactSample> contacts;if(w.ReadBodyContactSample(m.body,contacts))for(const auto& c:contacts)if(c.other.id==wall.id)firstContact=step+1;}
  stages[!impact?"rest":!firstContact?"approach":step+1<firstContact+60?"impact-window":"post-contact"].push_back(ms);
  for(auto& m:all){auto p=w.GetTransform(m.body);auto v=w.GetLinearVelocity(m.body);finite&=std::isfinite(glm::dot(p.position,p.position))&&std::isfinite(glm::dot(v,v));}
  for(unsigned c=0;c<cars;++c){const auto rootPose=w.GetTransform(all[c*13].body);for(unsigned i=1;i<13;++i){const auto& m=all[c*13+i];auto delta=glm::inverse(rootPose.rotation)*(w.GetTransform(m.body).position-rootPose.position);drift=std::max(drift,double(glm::length(delta-m.offset)));}}
 }
 std::sort(times.begin(),times.end());std::printf("NATIVE_ASSEMBLY cars=%u bodies=%zu joints=%u mode=%s state=%s rotated=%u median_ms=%.9g p95_ms=%.9g max_ms=%.9g finite=%u max_relative_motion_m=%.9g\n",cars,all.size(),countJoints,spring?"spring-slider":"six-axis-elastic",impact?"impact":"rest",rotation.w!=1,times[times.size()/2],times[size_t(.95*(times.size()-1))],times.back(),finite,drift);
 for(auto& item:stages){auto& samples=item.second;std::sort(samples.begin(),samples.end());std::printf("NATIVE_STAGE cars=%u mode=%s state=%s rotated=%u stage=%s samples=%zu median_ms=%.9g p95_ms=%.9g max_ms=%.9g first_wall_contact_step=%u\n",cars,spring?"spring-slider":"six-axis-elastic",impact?"impact":"rest",rotation.w!=1,item.first.c_str(),samples.size(),samples[samples.size()/2],samples[size_t(.95*(samples.size()-1))],samples.back(),firstContact);}
 w.Shutdown();
}
int main(){for(unsigned cars:{1u,4u})for(bool spring:{false,true})for(bool impact:{false,true})Run(cars,spring,impact,{1,0,0,0});Run(4,true,true,glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3))));}
