#include "RigidMotion.h"
#include <cstdio>
int main(){for(float spin:{1.f,5.f,20.f,100.f}) {RigidBody b; b.inverseMass=1;b.inverseInertiaLocal=SolidBoxInverseInertia(1,{.08f,.2f,.05f});b.angularVelocity={spin,spin*.7f,spin*.3f};auto energy=[](RigidBody b){glm::dvec3 w(b.angularVelocity);return .5*glm::dot(w,glm::inverse(glm::dmat3(b.InverseInertiaWorld()))*w);};for(bool exact:{false,true}){RigidMotion m;m.Begin(1,b,1./60,exact);auto end=m.Evaluate(1./60);printf("spin %g exact %d energy %.12g -> %.12g ratio %.12g\n",spin,exact,energy(b),energy(end),energy(end)/energy(b));}}}
