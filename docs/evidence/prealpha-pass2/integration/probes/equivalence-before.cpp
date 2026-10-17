#include "GravityField.h"
#include <cstdio>
int main(){
 GravityEquilibrium a,b;a.magnitude=b.magnitude=9.81;a.up=b.up=glm::normalize(glm::dvec3(1,2,3));b.center={512,-512,1024};
 const bool plane=a.Equivalent(b);std::printf("plane equivalent=%d coordinate_difference=%.17g\n",plane,a.Coordinate({2,3,4})-b.Coordinate({2,3,4}));
 a.kind=b.kind=GravityEquilibrium::Kind::Radius;a.center=b.center={1,2,3};b.up={1,0,0};const bool radius=a.Equivalent(b);std::printf("radial equivalent=%d coordinate_difference=%.17g\n",radius,a.Coordinate({2,3,4})-b.Coordinate({2,3,4}));
 return plane&&radius?0:1;
}
