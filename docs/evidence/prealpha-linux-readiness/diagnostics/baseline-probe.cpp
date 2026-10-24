#include "PhysicsWorld.h"
#include <cstdio>
#include <vector>
int main(){
 for(auto [count,speed]:std::vector<std::pair<int,float>>{{1,1},{1,100},{1,5000},{300,100}}){
  PhysicsWorld w;w.Init();std::vector<BodyHandle> balls;
  for(int i=0;i<count;++i){float z=3.f*i;w.CreateStaticBox({.6f,0,z},{.05f,1,1},0,1);w.CreateStaticBox({-.6f,0,z},{.05f,1,1},0,1);auto h=w.CreateDynamicSphere({0,0,z},.05f,1,0,1);w.SetLinearVelocity(h,{speed,0,0});balls.push_back(h);}
  w.Step(1.f/60);auto s=w.LastStepStats();size_t held=0;
  for(auto h:balls){bool stopped=false;for(auto m:w.GetBodyMotionSegments(h))stopped|=m.begin<m.end&&m.linearVelocity==glm::vec3(0);held+=stopped;auto p=w.GetTransform(h);auto v=w.GetLinearVelocity(h);std::printf("POSE %d %g %u %a %a %a %a\n",count,speed,h.id,p.position.x,p.position.y,p.position.z,v.x);}
  std::printf("CASE count=%d speed=%g events=%zu holdsObserved=%zu safety=%zu caps=%zu\n",count,speed,s.impactEvents,held,s.impactSafetyFallback,s.impactEventCapFallback);
 }
}
