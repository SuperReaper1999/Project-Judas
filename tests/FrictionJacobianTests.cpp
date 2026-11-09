#include "ContactVelocity.h"
#include "RigidBody.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <glm/gtc/quaternion.hpp>

namespace {
unsigned checks=0,failures=0,quadraticBadNulls=0;
double largestError=0,largestErrorBound=0,largestNativeError=0;
void Check(bool value,const char* label) {
    ++checks;if(!value){++failures;std::printf("FAIL %s\n",label);}
}
bool Bits(float a,float b) {return std::memcmp(&a,&b,sizeof(a))==0;}
float Native(float invMass,const glm::vec3& rA,const glm::vec3& rB,
             const glm::mat3& ia,const glm::mat3& ib,const glm::vec3& d) {
    return invMass+glm::dot(d,glm::cross(ia*glm::cross(rA,d),rA)+
                               glm::cross(ib*glm::cross(rB,d),rB));
}
double Oracle(float invMass,const glm::vec3& rA,const glm::vec3& rB,
              const glm::mat3& ia,const glm::mat3& ib,const glm::vec3& d) {
    const glm::dvec3 ja=glm::cross(glm::dvec3(rA),glm::dvec3(d));
    const glm::dvec3 jb=glm::cross(glm::dvec3(rB),glm::dvec3(d));
    return double(invMass)+glm::dot(ja,glm::dmat3(ia)*ja)+glm::dot(jb,glm::dmat3(ib)*jb);
}
// Rejected control, preserved exactly as a mathematical experiment: expand the
// three point-response columns, symmetrize, then evaluate d^T K d in binary32.
float ExpandedQuadratic(float invMass,const glm::vec3& rA,const glm::vec3& rB,
                        const glm::mat3& ia,const glm::mat3& ib,const glm::vec3& d) {
    glm::mat3 mobility(0);
    for(unsigned axis=0;axis<3;++axis) {
        glm::vec3 e(0);e[axis]=1;
        mobility[axis]=glm::cross(ia*glm::cross(rA,e),rA)+
                       glm::cross(ib*glm::cross(rB,e),rB);
    }
    mobility=(mobility+glm::transpose(mobility))*.5f;
    return invMass+glm::dot(d,mobility*d);
}
// Independent, condition-aware forward-error envelope. A float cross component
// has gamma3*(|r_j d_k|+|r_k d_j|) error. Propagate that through j^T I j in
// binary64, then bound the two float matvec/dot evaluations and final sums.
// This does not pretend an almost parallel float cross has small relative
// angular error. Exact r==d nulls have a separate exact-value requirement.
double AngularErrorBound(const glm::vec3& r,const glm::mat3& inertia,const glm::vec3& direction,
                         double& magnitude) {
    constexpr double u=std::numeric_limits<float>::epsilon()/2;
    constexpr double gamma3=3*u/(1-3*u);
    const glm::dvec3 rd(r),d(direction),j=glm::cross(rd,d);
    glm::dvec3 error;
    for(unsigned i=0;i<3;++i) {
        const auto a=(i+1)%3,b=(i+2)%3;
        error[i]=gamma3*(std::abs(rd[a]*d[b])+std::abs(rd[b]*d[a]));
    }
    double perturbation=0; magnitude=0;
    for(unsigned i=0;i<3;++i)for(unsigned k=0;k<3;++k) {
        const double coefficient=std::abs(double(inertia[k][i]));
        perturbation+=coefficient*(std::abs(j[i])*error[k]+error[i]*std::abs(j[k])+error[i]*error[k]);
        magnitude+=coefficient*(std::abs(j[i])+error[i])*(std::abs(j[k])+error[k]);
    }
    return perturbation;
}
double Bound(float invMass,const glm::vec3& rA,const glm::vec3& rB,
             const glm::mat3& ia,const glm::mat3& ib,const glm::vec3& d) {
    constexpr double u=std::numeric_limits<float>::epsilon()/2;
    constexpr double gamma16=16*u/(1-16*u);
    double magnitudeA=0,magnitudeB=0;
    const double eA=AngularErrorBound(rA,ia,d,magnitudeA),eB=AngularErrorBound(rB,ib,d,magnitudeB);
    return eA+eB+gamma16*(std::abs(double(invMass))+magnitudeA+magnitudeB);
}
void Compare(float invMass,const glm::vec3& rA,const glm::vec3& rB,
             const glm::mat3& ia,const glm::mat3& ib,const glm::vec3& direction) {
    const double oracle=Oracle(invMass,rA,rB,ia,ib,direction);
    const double answer=contact_velocity::FrictionInverseEffectiveMass(invMass,rA,rB,ia,ib,direction);
    const double bound=Bound(invMass,rA,rB,ia,ib,direction);
    largestError=std::max(largestError,std::abs(answer-oracle));
    largestErrorBound=std::max(largestErrorBound,bound);
    largestNativeError=std::max(largestNativeError,std::abs(double(Native(invMass,rA,rB,ia,ib,direction))-oracle));
    Check(std::isfinite(answer)&&answer>=invMass-bound,"stable scalar remains finite and PSD within explicit float error envelope");
    Check(std::abs(answer-oracle)<=bound+8*std::numeric_limits<double>::epsilon()*std::max(1.0,std::abs(oracle)),
          "stable scalar agrees with independent double spatial-Jacobian oracle within derived forward error");
}
void Mathematics() {
    for(unsigned rotation=0;rotation<12;++rotation) {
        const auto q=glm::angleAxis(.231f*float(rotation),glm::normalize(glm::vec3(1,2,3)));
        const glm::mat3 basis=glm::mat3_cast(q);
        const auto rA=glm::normalize(glm::vec3(1+.03f*rotation,2-.07f*rotation,3));
        const auto perpendicular=glm::normalize(glm::cross(rA,glm::vec3(0,0,1)));
        for(float scale:{1.f,1e4f,1e8f,1e12f}) {
            glm::mat3 diagonal(0);diagonal[0][0]=scale;diagonal[1][1]=.3f*scale;diagonal[2][2]=.07f*scale;
            const glm::mat3 ia=basis*diagonal*glm::transpose(basis);
            for(float tilt:{0.f,1e-6f,1e-4f,.01f}) {
                const auto d=tilt==0?rA:glm::normalize(rA+perpendicular*tilt);
                Compare(1,rA,glm::vec3(0),ia,glm::mat3(0),d);
                if(tilt==0) {
                    const float actual=contact_velocity::FrictionInverseEffectiveMass(1,rA,glm::vec3(0),ia,glm::mat3(0),d);
                    Check(Bits(actual,1.f),"exact represented angular null remains EXACT k=1 at every inertia scale and rotation");
                    const double expanded=ExpandedQuadratic(1,rA,glm::vec3(0),ia,glm::mat3(0),d);
                    quadraticBadNulls+=!std::isfinite(expanded)||std::abs(expanded-1)>1e-4;
                    if(rotation==0)std::printf("exact_null scale=%.9g oracle=%.12g stable=%.12g expanded=%.12g\n",
                        scale,Oracle(1,rA,glm::vec3(0),ia,glm::mat3(0),d),double(actual),expanded);
                }
            }
            for(unsigned sample=0;sample<16;++sample) {
                const auto d=glm::normalize(glm::vec3(std::sin(.31f*sample)+.1f,std::cos(.71f*sample),.13f*sample-.7f));
                const glm::vec3 rB{-.31f,.1f*float(sample),.83f};
                Compare(1.25f,rA,rB,ia,ia*.2f,d);
            }
        }
    }
    Check(quadraticBadNulls>0,"rejected expanded-quadratic control actually exposes its lost angular null");
}

}
int main() {
    Mathematics();
    std::printf("friction Jacobian: %u checks, %u failures, %u rejected-quadratic null violations; "
                "max_stable_error=%.12g max_derived_bound=%.12g max_native_error=%.12g\n",
                checks,failures,quadraticBadNulls,largestError,largestErrorBound,largestNativeError);
    return failures?1:0;
}
