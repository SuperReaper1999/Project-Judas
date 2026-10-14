// Coarse representation feasibility, NOT final vehicle acceptance. Same five
// collider boxes and volume-weighted576kg distribution in both representations.
// An isolated JointSolver prototype adds finite six-axis elasticity. No plastic
// state, wheels, powertrain, JS, renderer or activation mechanism is tested here.
#include "PhysicsWorld.h"
#include "JointTypes.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
void Run(unsigned count,bool articulated,bool impact,glm::quat rotation){PhysicsWorld w;w.Init();w.SetSleepingEnabled(false);
 const auto world=[&](glm::vec3 p){return rotation*p;};const auto make=[&](glm::vec3 p,Shape s,float m){BodyTransform t;t.position=world(p);t.rotation=rotation;return w.CreateShape(s,t,m>0,m,0,0);};
 make({0,-.15f,0},Shape::Box({40,.15f,40}),0);auto wall=make({0,1,-.4f},Shape::Box({40,1,.4f}),0);
 std::vector<CompoundBox> boxes={{{0,0,0},{.8f,.28f,.75f}}};for(unsigned side=0;side<2;++side)for(unsigned cell=0;cell<2;++cell)boxes.push_back({{side?.5f:-.5f,-.03f,-(1.05f+.65f*cell)},{.42f,.24f,.3f}});
 float totalVolume=0;for(auto& b:boxes)totalVolume+=8*b.halfExtents.x*b.halfExtents.y*b.halfExtents.z;
 std::vector<BodyHandle> bodies;std::vector<std::vector<BodyHandle>> groups;double normalDrift=0,allDrift=0;bool finite=true;
 for(unsigned c=0;c<count;++c){glm::vec3 origin((float(c)-float(count-1)*.5f)*5,1,impact?8.f:18.f);std::vector<BodyHandle> group;
  if(!articulated){auto b=make(origin,Shape::Compound(boxes),576);bodies.push_back(b);group.push_back(b);}
  else{for(auto& box:boxes){float m=576*(8*box.halfExtents.x*box.halfExtents.y*box.halfExtents.z)/totalVolume;auto b=make(origin+box.localCenter,Shape::Box(box.halfExtents),m);group.push_back(b);bodies.push_back(b);}
   for(unsigned i=1;i<5;++i){unsigned parent=(i==1||i==3)?0:i-1;auto offset=boxes[i].localCenter,other=boxes[parent].localCenter;auto point=(offset+other)*.5f;JointSettings s;s.bodyA=group[parent];s.bodyB=group[i];s.anchorA=point-other;s.anchorB=point-offset;s.spring=true;s.stiffness=1200000;s.damping=1500;auto j=w.CreateJoint(s);finite&=j.IsValid();w.SetPairCollisionEnabled(s.bodyA,s.bodyB,false);}
  }groups.push_back(group);
 }
 for(auto b:bodies)w.SetLinearVelocity(b,world({1,0,impact?-15.f:0}));
 std::map<std::string,std::vector<double>> times;unsigned first=0;
 for(unsigned frame=0;frame<600;++frame){auto begin=std::chrono::steady_clock::now();PhysicsWorld::VelocityCoupling quality;quality.iterations=12;
  try{w.StepCoupled(1.f/60,2,[&](float h,unsigned){for(auto b:bodies)w.ApplyLinearAcceleration(b,world({0,-9.81f,0}),h);},{},&quality);}catch(const std::exception& e){std::printf("COARSE_FAILURE cars=%u articulated=%u impact=%u rotated=%u frame=%u error=%s\n",count,articulated,impact,rotation.w!=1,frame,e.what());return;}
  double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
  if(impact&&!first)for(auto b:bodies){std::vector<PhysicsWorld::BodyContactSample> contacts;if(w.ReadBodyContactSample(b,contacts))for(auto& s:contacts)if(s.other.id==wall.id)first=frame+1;}
  times[!impact?"coasting":!first?"approach":frame+1<first+60?"impact-window":"post-contact"].push_back(ms);
  for(auto b:bodies){auto t=w.GetTransform(b);auto v=w.GetLinearVelocity(b);finite&=std::isfinite(glm::dot(t.position,t.position))&&std::isfinite(glm::dot(v,v));}
  if(articulated)for(auto& g:groups){auto root=w.GetTransform(g[0]);for(unsigned i=1;i<g.size();++i){double error=glm::length(glm::inverse(root.rotation)*(w.GetTransform(g[i]).position-root.position)-boxes[i].localCenter);allDrift=std::max(allDrift,error);if(!impact&&frame>300)normalDrift=std::max(normalDrift,error);}}
 }
 for(auto& item:times){auto& t=item.second;std::sort(t.begin(),t.end());std::printf("COARSE_STAGE cars=%u bodies=%zu joints=%u mode=%s state=%s rotated=%u stage=%s samples=%zu median_ms=%.9g p95_ms=%.9g max_ms=%.9g finite=%u normal_deflection_m=%.9g largest_relative_motion_m=%.9g sleeping_disabled=1 physics_hz=60 substeps=2 joint_rounds=12\n",count,bodies.size(),articulated?count*4:0,articulated?"six-axis-elastic":"compound",impact?"impact":"coasting",rotation.w!=1,item.first.c_str(),t.size(),t[t.size()/2],t[size_t(.95*(t.size()-1))],t.back(),finite,normalDrift,allDrift);}
 w.Shutdown();}
int main(){std::setvbuf(stdout,nullptr,_IOLBF,0);for(unsigned count:{1u,4u})for(bool articulated:{false,true})for(bool impact:{false,true})Run(count,articulated,impact,{1,0,0,0});Run(4,true,true,glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3))));}
