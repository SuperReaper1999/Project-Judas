#include "FluidHydrostatics.h"
#include "GravityField.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
struct Uniform final:GravityField{glm::vec3 Sample(const glm::vec3&)const override{return {0,-9.81f,0};}};
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1]);glm::vec3 half;BodyTransform pose;in>>half.x>>half.y>>half.z>>pose.position.x>>pose.position.y>>pose.position.z>>pose.rotation.w>>pose.rotation.x>>pose.rotation.y>>pose.rotation.z;std::vector<FluidParticle> p;FluidParticle v;while(in>>v.position.x>>v.position.y>>v.position.z>>v.velocity.x>>v.velocity.y>>v.velocity.z>>v.acceleration.x>>v.acceleration.y>>v.acceleration.z>>v.mass){v.previousPosition=v.position;p.push_back(v);}if(p.empty())return 3;FluidHydrostaticField f;f.Build(p,1000);auto volume=MakeBoxFluidVolume(half);Uniform gravity;std::cout<<std::setprecision(17);for(int i=-5;i<100;++i){auto start=std::chrono::steady_clock::now();auto r=EvaluateFluidHydrostatics(f,volume,pose,gravity);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(i>=0)std::cout<<i<<','<<ms<<','<<r.fraction<<','<<r.buoyancyForce.y<<'\n';}}
