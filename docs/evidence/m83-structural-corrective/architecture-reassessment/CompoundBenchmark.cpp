// Feasibility measurement for the undamaged aggregate only. Not a deformable
// car test. Actual13-piece compound collision,576 kg, ordinary CCD/contacts.
#include "PhysicsWorld.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
void Run(unsigned count,bool impact,glm::quat rotation){PhysicsWorld w;w.Init();const auto world=[&](glm::vec3 p){return rotation*p;};
 BodyTransform floor;floor.position=world({0,-.15f,0});floor.rotation=rotation;w.CreateShape(Shape::Box({40,.15f,40}),floor,false,0,.5f,0);
 BodyTransform wall;wall.position=world({0,1,-.4f});wall.rotation=rotation;auto barrier=w.CreateShape(Shape::Box({40,1,.4f}),wall,false,0,.5f,0);
 std::vector<CompoundBox> boxes={{{0,0,0},{.8f,.28f,.75f}}};
 for(unsigned side=0;side<2;++side)for(unsigned cell=0;cell<3;++cell)boxes.push_back({{side?.6f:-.6f,-.05f,-(1.0f+.5f*cell)},{.11f,.10f,.22f}});
 for(unsigned panel=0;panel<4;++panel)boxes.push_back({{panel&1?.9f:-.9f,.10f,panel<2?-.45f:.45f},{.025f,.18f,.25f}});
 for(unsigned side=0;side<2;++side)boxes.push_back({{side?.72f:-.72f,-.28f,-.55f},{.08f,.08f,.08f}});
 std::vector<BodyHandle> bodies;for(unsigned i=0;i<count;++i){BodyTransform t;t.position=world({(float(i)-float(count-1)*.5f)*5,1,impact?8.f:18.f});t.rotation=rotation;auto b=w.CreateShape(Shape::Compound(boxes),t,true,576,.5f,0);bodies.push_back(b);w.SetLinearVelocity(b,world({0,0,impact?-15.f:0}));}
 std::vector<double> approach,hit,post;unsigned first=0;bool finite=true;
 for(unsigned frame=0;frame<600;++frame){auto before=std::chrono::steady_clock::now();for(auto b:bodies)w.ApplyLinearAcceleration(b,world({0,-9.81f,0}),1.f/60);w.Step(1.f/60);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
  if(impact&&!first)for(auto b:bodies){std::vector<PhysicsWorld::BodyContactSample> samples;if(w.ReadBodyContactSample(b,samples))for(const auto& s:samples)if(s.other.id==barrier.id)first=frame+1;}
  (!impact?post:!first?approach:frame+1<first+60?hit:post).push_back(ms);
  for(auto b:bodies){auto t=w.GetTransform(b);finite&=std::isfinite(glm::dot(t.position,t.position));}
 }
 auto report=[&](const char* stage,std::vector<double>& samples){if(samples.empty())return;std::sort(samples.begin(),samples.end());std::printf("COMPOUND_STAGE cars=%u children_each=%zu dynamic_bodies=%zu mass_each_kg=576 state=%s rotated=%u stage=%s samples=%zu median_ms=%.9g p95_ms=%.9g max_ms=%.9g finite=%u first_wall_contact_step=%u\n",count,boxes.size(),bodies.size(),impact?"impact":"rest",rotation.w!=1,stage,samples.size(),samples[samples.size()/2],samples[size_t(.95*(samples.size()-1))],samples.back(),finite,first);};report("approach",approach);report("impact-window",hit);report(impact?"post-contact":"rest",post);w.Shutdown();}
int main(){std::setvbuf(stdout,nullptr,_IOLBF,0);for(unsigned n:{1u,4u})for(bool impact:{false,true})Run(n,impact,{1,0,0,0});Run(4,true,glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3))));}
