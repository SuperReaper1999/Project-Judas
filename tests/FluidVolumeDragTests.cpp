#include "FluidVolumeDrag.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <limits>
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cout<<"FAIL "<<label<<'\n';}}
void Near(glm::dvec3 a,glm::dvec3 b,double tol,const char* label){Check(glm::length(a-b)<=tol,label);}
int main(){
    FluidHydrostaticResult full;full.totalVolume=full.submergedVolume=8;
    full.wetAngularMoment=glm::dmat3(16./3);
    const double mass=12,rate=2,dt=1./60;const glm::dmat3 inertia(8);
    const glm::dvec3 v(1,2,-3),w(2,-1,4);
    const auto impulse=EvaluateFluidVolumeDrag(full,mass,inertia,v,w,rate,dt,0);
    const double factor=(1-rate*dt/2)/(1+rate*dt/2);
    Near(v+impulse.linear/mass,v*factor,1e-12,"full wet translation follows authored viscous law");
    Near(w+glm::inverse(inertia)*impulse.angular,w*factor,1e-12,"full wet spin follows same authored viscous law");
    // Legacy centroid-only drag at r=0 cannot observe any of this spin.
    Check(glm::length(impulse.angular)>1,"distributed loading detects centroid drag's angular nullspace");
    auto missing=full;missing.wetAngularMoment=glm::dmat3(0);
    Near(EvaluateFluidVolumeDrag(missing,mass,inertia,{},w,rate,dt,0).angular,{},0,"missing angular mechanism is an effective negative control");
    FluidHydrostaticResult half=full;half.submergedVolume=4;half.wetFirstMoment={0,-2,0};half.wetAngularMoment=glm::dmat3(8./3);
    for(double timestep:{.0001,.016666666666666666,.1,1.,10.})for(int i=0;i<16;++i){
        glm::dvec3 u(std::sin(i),std::cos(i*.7),.1*i),spin(.2*i,std::sin(i*.8),std::cos(i));
        auto j=EvaluateFluidVolumeDrag(half,mass,inertia,u,spin,rate,timestep,0);
        const auto next=u+j.linear/mass,nextSpin=spin+glm::inverse(inertia)*j.angular;
        const double oldE=.5*mass*glm::dot(u,u)+.5*glm::dot(spin,inertia*spin);
        const double newE=.5*mass*glm::dot(next,next)+.5*glm::dot(nextSpin,inertia*nextSpin);
        const auto mid=.5*(u+next),midSpin=.5*(spin+nextSpin);
        const double work=glm::dot(j.linear,mid)+glm::dot(j.angular,midSpin);
        Check(newE<=oldE+1e-10,"stationary fluid does not add rigid energy");
        Check(std::abs(newE-oldE-work)<1e-9,"impulse work equals actual rigid energy change");
    }
    auto moving=half;moving.fluidVelocity={3,-1,2};moving.fluidVelocityMoment=glm::cross(moving.wetFirstMoment,glm::dvec3(moving.fluidVelocity));
    auto rest=EvaluateFluidVolumeDrag(moving,mass,inertia,moving.fluidVelocity,{},rate,dt,0);
    Near(rest.linear,{},1e-12,"co-moving fluid creates no linear drag");Near(rest.angular,{},1e-12,"co-moving fluid creates no angular drag");
    auto a=EvaluateFluidVolumeDrag(half,mass,inertia,v,w,rate,dt,0);
    auto b=EvaluateFluidVolumeDrag(moving,mass,inertia,v+glm::dvec3(moving.fluidVelocity),w,rate,dt,0);
    Near(a.linear,b.linear,1e-12,"Galilean covariance of drag force");Near(a.angular,b.angular,1e-12,"Galilean covariance of drag torque");
    auto turn=glm::mat3_cast(glm::angleAxis(.83,glm::normalize(glm::dvec3(1,-2,3))));
    auto rotated=half;rotated.wetFirstMoment=turn*half.wetFirstMoment;rotated.wetAngularMoment=turn*half.wetAngularMoment*glm::transpose(turn);
    auto c=EvaluateFluidVolumeDrag(rotated,mass,turn*inertia*glm::transpose(turn),turn*v,turn*w,rate,dt,0);
    Near(c.linear,turn*a.linear,1e-11,"arbitrary gravity-frame rotation preserves linear load");Near(c.angular,turn*a.angular,1e-11,"arbitrary gravity-frame rotation preserves angular load");
    double previous=1;
    for(int n:{30,60,120,240}){
        glm::dvec3 velocity(1,0,0),omega(0,2,0);
        for(int i=0;i<n;++i){auto j=EvaluateFluidVolumeDrag(full,mass,inertia,velocity,omega,rate,1./n,0);velocity+=j.linear/mass;omega+=glm::inverse(inertia)*j.angular;}
        double error=std::abs(omega.y-2*std::exp(-rate));Check(error<previous*.26,"physical angular decay converges at second order");previous=error;
    }
    Near(EvaluateFluidVolumeDrag(full,mass,inertia,v,w,0,dt,0).angular,{},0,"zero authored drag remains zero");
    auto dry=full;dry.submergedVolume=0;Near(EvaluateFluidVolumeDrag(dry,mass,inertia,v,w,rate,dt,0).linear,{},0,"dry body remains unaffected");
    for(int i=0;i<3;++i){bool rejected=false;try{EvaluateFluidVolumeDrag(full,i==0?0:mass,inertia,v,w,i==1?-1:rate,i==2?0:dt,0);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"invalid configuration is rejected");}
    struct ZeroGravity final:GravityField {glm::vec3 Sample(const glm::vec3&)const override{return {};}} zero;
    std::vector<FluidParticle> cloud;
    const glm::vec3 h(.5f,.3f,.2f);
    for(int z=-8;z<8;++z)for(int y=-8;y<8;++y)for(int x=-8;x<8;++x){
        glm::vec3 position=glm::vec3(x+.5f,y+.5f,z+.5f)*.25f;
        if(glm::all(glm::lessThan(glm::abs(position),h)))continue;
        FluidParticle p;p.position=position;p.velocity={1,0,0};p.mass=15.625f;cloud.push_back(p);
    }
    FluidHydrostaticField field;field.Build(cloud,1000);
    auto measured=EvaluateFluidHydrostatics(field,MakeBoxFluidVolume(h),BodyTransform{},zero);
    Check(std::abs(measured.fraction-1)<1e-6,"actual sampler reports full wet body");
    const double volume=8.*h.x*h.y*h.z;
    glm::dmat3 expected(0);expected[0][0]=volume*(h.y*h.y+h.z*h.z)/3;
    expected[1][1]=volume*(h.x*h.x+h.z*h.z)/3;expected[2][2]=volume*(h.x*h.x+h.y*h.y)/3;
    for(int i=0;i<3;++i)Near(measured.wetAngularMoment[i],expected[i],1e-8,"volume moments include finite subcell inertia");
    auto spinImpulse=EvaluateFluidVolumeDrag(measured,10,expected*(10/volume),{1,0,0},{0,0,5},rate,dt,0);
    Near(spinImpulse.linear,{},1e-7,"actual co-moving field adds no translational force");
    Check(spinImpulse.angular.z<0,"actual wet geometry produces physical spin resistance");
    std::cout<<"Fluid volume drag: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
