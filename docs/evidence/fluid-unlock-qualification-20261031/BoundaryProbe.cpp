#include "FluidHydrostatics.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
struct Uniform:GravityField {glm::vec3 g; explicit Uniform(glm::vec3 v):g(v){} glm::vec3 Sample(const glm::vec3&)const override{return g;}};
int main(){
 int failures=0;std::cout<<std::setprecision(17);
 for(int gap:{1,2,3,4,5})for(bool ring:{false,true})for(bool rotated:{false,true}){
  const glm::quat q=rotated?glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3))):glm::quat(1,0,0,0);
  const glm::vec3 shift=rotated?glm::vec3(3,-2,4):glm::vec3(0);
  std::vector<FluidParticle> particles;
  for(int x=-8;x<=8;++x)for(int z=-8;z<=8;++z)for(int y=-4;y<=-1;++y){
   if(ring&&std::abs(x)<gap&&std::abs(z)<gap)continue;
   const auto p=shift+q*glm::vec3(.25f*x,.25f*y,.25f*z);particles.push_back({p,p,glm::vec3(0),15.625f,glm::vec3(0)});
  }
  FluidHydrostaticField field;field.Build(particles,1000);
  BodyTransform pose;pose.position=shift+q*glm::vec3(0,-.5f,0);pose.rotation=q;
  const auto result=EvaluateFluidHydrostatics(field,MakeBoxFluidVolume(glm::vec3(.1f)),pose,Uniform(q*glm::vec3(0,-9.81f,0)));
  const double expected=ring?0:1;const bool pass=std::abs(result.fraction-expected)<1e-5;failures+=!pass;
  std::cout<<"CASE "<<(ring?"connected-ring-dry-hole":"solid-pool-wet-control")<<" gap_cells="<<gap<<" rotated="<<rotated<<" particles="<<particles.size()<<" expected_fraction="<<expected<<" actual_fraction="<<result.fraction<<" force_N="<<glm::length(result.buoyancyForce)<<" pass="<<pass<<'\n';
 }
 std::cout<<"failures="<<failures<<'\n';return failures?1:0;
}
