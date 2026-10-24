#include <stdexcept>
#include <initializer_list>
#include "RigidBody.h"
#include <cstdio>
#include <glm/gtc/quaternion.hpp>
glm::dvec3 Rotate(glm::dvec3 phi,glm::dvec3 x){double a=glm::length(phi);return a?glm::angleAxis(a,phi/a)*x:x;}
glm::vec3 Exact(const glm::mat3& inv,const glm::quat& q,const glm::vec3& v,float dt){
 glm::dmat3 B(inv),R=glm::mat3_cast(glm::normalize(glm::dquat(q)));auto m0=glm::inverse(B)*glm::transpose(R)*glm::dvec3(v);auto m=m0;
 auto residual=[&](glm::dvec3 x,double h){return x-.5*(m0+Rotate(-h*(B*x),m0));};
 auto solve=[&](double h,glm::dvec3& x){for(int it=0;it<24;++it){auto f=residual(x,h);if(glm::length(f)<1e-12*std::max(glm::length(m0),1e-12))return true;glm::dmat3 J;double eps=1e-7*std::max(glm::length(m0),1e-12);for(int j=0;j<3;++j){glm::dvec3 d(0);d[j]=eps;J[j]=(residual(x+d,h)-residual(x-d,h))/(2*eps);}x-=glm::inverse(J)*f;}return false;};
 bool ok=solve(dt,m);if(!ok)for(int n=8;n<=4096;n*=8){m=m0;ok=true;for(int k=1;k<=n&&ok;++k)ok=solve(double(dt)*k/n,m);if(ok)break;}
 if(!ok){printf("no convergence rate %.6g norm %.12g residual %.12g\n",v.x,glm::length(m0),glm::length(residual(m,dt)));fflush(stdout);throw std::runtime_error("no convergence");}return glm::vec3(R*(B*m));
}
int main(){setvbuf(stdout,nullptr,_IONBF,0);for(float rate:{1.f,5.f,20.f,100.f,1000.f}){RigidBody b;b.inverseMass=1;b.inverseInertiaLocal=SolidBoxInverseInertia(1,{.08f,.2f,.05f});b.angularVelocity={rate,.7f*rate,.3f*rate};glm::dvec3 L=glm::inverse(glm::dmat3(b.InverseInertiaWorld()))*glm::dvec3(b.angularVelocity);double E=.5*glm::dot(L,glm::dvec3(b.angularVelocity));auto g=Exact(b.inverseInertiaLocal,b.orientation,b.angularVelocity,1.f/60);auto q=glm::normalize(glm::angleAxis(glm::length(g)/60,g/glm::length(g))*b.orientation);auto w=TransportAngularVelocity(b.inverseInertiaLocal,b.orientation,q,b.angularVelocity);double out=.5*glm::dot(L,glm::dvec3(w));printf("rate %g initial %.12g final %.12g ratio %.12g\n",rate,E,out,out/E);}}
