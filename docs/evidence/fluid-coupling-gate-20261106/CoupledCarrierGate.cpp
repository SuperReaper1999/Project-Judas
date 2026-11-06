#include "CutMassMomentum.h"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
using Real=CutCompensated;
int main(){
 std::cout<<std::setprecision(17);int checks=0,failures=0;
 auto check=[&](bool value){++checks;failures+=!value;};
 // One transported velocity component, with transverse inflow carrying zero
 // momentum in that component and an equal axial outflow. The mass carrier
 // is rho*dt*A*u. It is a nonlinear carrier gate, not a full basin geometry.
 const double dt=double(1.f/60.f),area=2.2351741790771504e-9;
 const double mass=1.3411045074462915e-10;
 const double frozenVelocity=3299.2337958479102;
 const Real impulse=Real(mass)*Real(frozenVelocity);
 for(auto backend:{CutMomentumBackend::Fast,CutMomentumBackend::Reference}){
  for(double density:{1.2,1000.}){
   const double k=density*dt*area;
   auto evaluate=[&](double advector,CutMassMomentumResult& result){
    Real flux=Real(k)*Real(advector);std::string error;
    bool ok=TransportCutMassMomentum({1.},{mass},{mass},{},
      {{0,Real()-flux,{}},{0,flux,{}}},{{Real(),Real(),Real()}},
      {{impulse,Real(),Real()}},0.,1e-24,result,error,backend);
    if(!ok)std::cerr<<error<<'\n';
    return ok;
   };
   CutMassMomentumResult post;
   check(evaluate(frozenVelocity,post));
   const double postVelocity=post.velocity[0][0].Value();
   // A postprocessed velocity with the old advecting flux is NOT a solution
   // of this declared backward-Euler coupled mass/momentum discretization.
   Real oldResidual=Real(mass)*Real(frozenVelocity)+Real(k)*Real(frozenVelocity)*Real(frozenVelocity)-impulse;
   check(std::abs(oldResidual.Value())>1e-4*std::abs(impulse.Value()));
   check(std::abs(postVelocity-frozenVelocity)>100.);
   const double root=2.*impulse.Value()/(mass+std::sqrt(mass*mass+4.*k*impulse.Value()));
   CutMassMomentumResult coupled;
   check(evaluate(root,coupled));
   check(std::abs(coupled.velocity[0][0].Value()-root)<=1e-12*root);
   check(std::abs(coupled.energyResidual)<=1e-18);
   std::cout<<"COUPLING backend="<<(backend==CutMomentumBackend::Fast?"fast":"reference")
    <<" density="<<density<<" frozenVelocity="<<frozenVelocity
    <<" postprocessedVelocity="<<postVelocity<<" coupledVelocity="<<root
    <<" frozenMomentumResidual="<<oldResidual.Value()
    <<" impulse="<<impulse.Value()<<" coupledEnergyResidual="<<coupled.energyResidual<<'\n';
  }
 }
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
