#include "Contacts.h"
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <cstring>
#include <cstdio>

// Independent retained pre-optimisation sequence, including all eight calls to
// the public unprepared closest-point query. Original generated geometry only.
int main(){
 std::mt19937 rng(0x5600970);std::uniform_real_distribution<float> value(-1,1);
 unsigned failures=0;
 for(unsigned i=0;i<100000;++i){
  const float scale=i%3==0?1e5f:i%3==1?.001f:10.f;
  glm::vec3 center{value(rng)*scale,value(rng)*scale,value(rng)*scale};
  glm::vec3 half{.001f+std::abs(value(rng))*scale,.001f+std::abs(value(rng))*scale,.001f+std::abs(value(rng))*scale};
  glm::vec3 a=center+glm::vec3(value(rng),value(rng),value(rng))*scale;
  glm::vec3 b=i%13==0?a:a+glm::vec3(value(rng),value(rng),value(rng))*scale;
  glm::quat q{i%11==0?1.f:value(rng),value(rng),value(rng),value(rng)};
  if(i%17==0)q={1,0,0,0};
  glm::vec3 expectedA=(a+b)*.5f,expectedB=center;
  for(int n=0;n<8;++n){expectedB=ClosestPointOnOBB(expectedA,center,q,half);expectedA=ClosestPointOnSegment(expectedB,a,b);}
  glm::vec3 actualA,actualB;ClosestPointsSegmentToOBB(a,b,center,q,half,actualA,actualB);
  failures+=std::memcmp(&expectedA,&actualA,3*sizeof(float))||std::memcmp(&expectedB,&actualB,3*sizeof(float));
 }
 std::printf("Prepared capsule-box projection: 100000 exact-state controls, %u failures\n",failures);return failures?1:0;
}
