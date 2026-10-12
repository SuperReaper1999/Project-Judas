// Audit-only observer. Calls the unchanged shipping integrator, not a reference solver.
// Analytic oracle: a torque-free isolated body conserves world angular momentum.
// No contacts, gravity, drives, limits, scripted resets or external work.
#include "RigidBody.h"
#include <iostream>
#include <iomanip>
#include <cmath>
static glm::dvec3 L(const RigidBody& b) {
 const glm::dmat3 R=glm::mat3_cast(glm::dquat(b.orientation));
 return R*glm::inverse(glm::dmat3(b.inverseInertiaLocal))*glm::transpose(R)*glm::dvec3(b.angularVelocity);
}
int main(){
 std::cout<<std::setprecision(12);
 bool controls=true;
 for(bool sphere:{true,false})for(int hz:{60,120,240,480}) {
  RigidBody b;b.inverseMass=1.f/3.f;
  b.inverseInertiaLocal=sphere?SolidSphereInverseInertia(3,.5f):SolidBoxInverseInertia(3,{.2f,.5f,.9f});
  b.angularVelocity={1,2,3};auto initial=L(b);double maxError=0,maxQError=0;
  for(int k=0;k<2*hz;++k){IntegrateRigidBody(b,1.f/hz);maxError=std::max(maxError,glm::length(L(b)-initial));maxQError=std::max(maxQError,std::abs(double(glm::length(b.orientation))-1));}
  const double relative=maxError/glm::length(initial);
  if(relative>1e-4)controls=false;
  if(maxQError>1e-5 || glm::length(b.position)>1e-6f)controls=false;
  std::cout<<"{\"shape\":\""<<(sphere?"sphere":"triaxial-box")<<"\",\"hz\":"<<hz<<",\"seconds\":2,\"initialAngularMomentum\":"<<glm::length(initial)<<",\"maximumAngularMomentumError\":"<<maxError<<",\"relativeError\":"<<relative<<",\"quaternionNormError\":"<<maxQError<<"}\n";
 }
 std::cout<<"{\"observerControlsPass\":"<<(controls?"true":"false")<<"}\n";
 // This is a measurement, not a product acceptance gate. Findings retain actual errors.
 return controls?0:1;
}
