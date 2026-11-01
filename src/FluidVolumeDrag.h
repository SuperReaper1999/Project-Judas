#pragma once
#include "FluidHydrostatics.h"
#include <array>
#include <cmath>
#include <stdexcept>

struct FluidVolumeDragImpulse {
    glm::dvec3 linear{0}, angular{0};
};

// Integrate df = -k*(bodyMass/bodyVolume)*(v + omega cross r - u) dV.
// The six-dimensional resistance is an integral of positive semidefinite local
// resistances. Implicit midpoint dissipates relative motion without a clamp,
// an angular tuning coefficient, or a new timestep/iteration budget.
inline FluidVolumeDragImpulse EvaluateFluidVolumeDrag(
    const FluidHydrostaticResult& wet, double mass, const glm::dmat3& inertia,
    glm::dvec3 velocity, glm::dvec3 omega, double rate, double dt, double lead) {
    if(!std::isfinite(mass) || mass<=0 || !std::isfinite(rate) || rate<0 ||
       !std::isfinite(dt) || dt<=0 || !std::isfinite(lead) || lead<0)
        throw std::invalid_argument("invalid distributed fluid drag parameters");
    if(rate==0 || wet.submergedVolume==0)return {};
    if(!std::isfinite(wet.totalVolume) || wet.totalVolume<=0 ||
       !std::isfinite(wet.submergedVolume) || wet.submergedVolume<0)
        throw std::invalid_argument("invalid distributed fluid drag volume");
    using Vector=std::array<double,6>;
    using Matrix=std::array<Vector,6>;
    Matrix m{},c{},a{},l{};
    Vector u{},b{},rhs{},y{},du{};
    const double scale=rate*mass/wet.totalVolume;
    const auto first=wet.wetFirstMoment;
    const glm::dmat3 cross(0,first.z,-first.y,-first.z,0,first.x,first.y,-first.x,0);
    const auto flow=double(wet.submergedVolume)*(glm::dvec3(wet.fluidVelocity)+lead*glm::dvec3(wet.fluidAcceleration));
    const auto spin=wet.fluidVelocityMoment+lead*wet.fluidAccelerationMoment;
    for(int i=0;i<3;++i) {
        u[i]=velocity[i];u[i+3]=omega[i];b[i]=scale*flow[i];b[i+3]=scale*spin[i];
        m[i][i]=mass;c[i][i]=scale*wet.submergedVolume;
        for(int j=0;j<3;++j) {
            m[i+3][j+3]=.5*(inertia[j][i]+inertia[i][j]);
            c[i][j+3]=-scale*cross[j][i];c[i+3][j]=scale*cross[j][i];
            c[i+3][j+3]=scale*wet.wetAngularMoment[j][i];
        }
    }
    for(int i=0;i<6;++i) {
        rhs[i]=dt*b[i];
        for(int j=0;j<6;++j) {
            a[i][j]=m[i][j]+.5*dt*c[i][j];rhs[i]-=dt*c[i][j]*u[j];
            if(!std::isfinite(a[i][j]) || !std::isfinite(u[j]))
                throw std::invalid_argument("nonfinite distributed fluid drag state");
        }
        if(!std::isfinite(rhs[i]))throw std::invalid_argument("nonfinite distributed fluid drag load");
    }
    // Fixed-size Cholesky, not an iterative solver or convergence budget.
    for(int i=0;i<6;++i)for(int j=0;j<=i;++j) {
        double value=a[i][j];for(int k=0;k<j;++k)value-=l[i][k]*l[j][k];
        if(i==j) {
            if(!(value>0) || !std::isfinite(value))throw std::invalid_argument("nonpositive fluid drag mass matrix");
            l[i][j]=std::sqrt(value);
        } else l[i][j]=value/l[j][j];
    }
    for(int i=0;i<6;++i){double value=rhs[i];for(int j=0;j<i;++j)value-=l[i][j]*y[j];y[i]=value/l[i][i];}
    for(int i=5;i>=0;--i){double value=y[i];for(int j=i+1;j<6;++j)value-=l[j][i]*du[j];du[i]=value/l[i][i];}
    FluidVolumeDragImpulse result;
    for(int i=0;i<3;++i) {
        result.linear[i]=mass*du[i];
        for(int j=0;j<3;++j)result.angular[i]+=m[i+3][j+3]*du[j+3];
    }
    return result;
}
