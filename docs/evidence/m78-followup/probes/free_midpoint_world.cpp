// Independent analytic angular-momentum observer through unmodified PhysicsWorld.
#include "PhysicsWorld.h"
#include <iostream>
#include <iomanip>
#include <cmath>
int main(){std::cout<<std::setprecision(12);bool controls=true;
 for(bool sphere:{true,false})for(int hz:{60,120,240,480}){
  PhysicsWorld world;if(!world.Init())return 2;
  auto body=sphere?world.CreateDynamicSphere({0,0,0},.5f,3,0,0):world.CreateDynamicBox({0,0,0},{.2f,.5f,.9f},3,0,0);
  world.SetAngularVelocity(body,{1,2,3});
  auto momentum=[&]{return glm::dmat3(world.GetInertiaWorld(body))*glm::dvec3(world.GetAngularVelocity(body));};
  const auto initial=momentum();double peak=0;size_t contacts=0;const double initialEnergy=.5*glm::dot(initial,glm::dvec3(world.GetAngularVelocity(body)));double peakEnergy=0;
  for(int k=0;k<2*hz;++k){world.Step(1.f/hz);peak=std::max(peak,glm::length(momentum()-initial));contacts+=world.LastStepContactCount();peakEnergy=std::max(peakEnergy,std::abs(.5*glm::dot(momentum(),glm::dvec3(world.GetAngularVelocity(body)))-initialEnergy));}
  const double relative=peak/glm::length(initial);
  if(relative>1e-4||contacts||glm::length(world.GetTransform(body).position)>1e-6f)controls=false;
  std::cout<<"{\"path\":\"PhysicsWorld::Step\",\"shape\":\""<<(sphere?"sphere":"triaxial-box")<<"\",\"hz\":"<<hz<<",\"seconds\":2,\"contactRows\":"<<contacts<<",\"maximumAngularMomentumError\":"<<peak<<",\"relativeError\":"<<relative<<",\"peakEnergyRelativeError\":"<<peakEnergy/initialEnergy<<"}\n";
 }
 std::cout<<"{\"observerControlsPass\":"<<(controls?"true":"false")<<"}\n";return controls?0:1;
}
