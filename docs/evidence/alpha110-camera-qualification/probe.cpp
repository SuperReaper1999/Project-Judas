#include "CameraLens.h"
#include <cstdio>
int main(){CameraLens lens;auto a=BuildCameraFrame({0,0,10},{1,0,0,0},60,.1f,100,lens,800,400);
auto b=glm::perspective(glm::radians(60.f),2.f,.1f,100.f);
for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(a.projection[c][r]!=b[c][r])printf("DIFF %d %d %.12g %.12g delta=%.12g\n",c,r,a.projection[c][r],b[c][r],a.projection[c][r]-b[c][r]);
volatile float f=60,n=.1f,z=100,aspect=2;
a=BuildCameraFrame({0,0,10},{1,0,0,0},f,n,z,lens,800,400);
b=glm::perspective(glm::radians(float(f)),float(aspect),float(n),float(z));
printf("Runtime-identical=%d\n",a.projection==b);return a.projection==b?0:1;}
