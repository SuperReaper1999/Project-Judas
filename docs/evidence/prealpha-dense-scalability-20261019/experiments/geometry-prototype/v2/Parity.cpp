#include "Contacts.h"
#include "ContactPreparedGeometry.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

static uint32_t randomState=0x35a19b6c;
static uint32_t Random(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
static float Between(float a,float b){return a+(b-a)*float(Random()>>8)*(1.f/16777216.f);}
static glm::vec3 Vector(float a,float b){return {Between(a,b),Between(a,b),Between(a,b)};}
static glm::quat Rotation(){auto v=Vector(-1,1);if(glm::dot(v,v)<.001f)v={1,.3f,.2f};return glm::angleAxis(Between(-3.14f,3.14f),glm::normalize(v));}
static uint64_t digest=1469598103934665603ull;
template<class T> static void Add(T x){unsigned char raw[sizeof(x)];std::memcpy(raw,&x,sizeof(x));for(auto b:raw){digest^=b;digest*=1099511628211ull;}}
static void Add(glm::vec3 v){Add(v.x);Add(v.y);Add(v.z);}static void Add(glm::dvec3 v){Add(v.x);Add(v.y);Add(v.z);}
static void Add(const Contact &x){Add(x.hit);Add(x.point);Add(x.normal);Add(x.penetration);Add(x.signedSeparation);Add(x.preciseNormal);Add(x.localAnchorA);Add(x.localAnchorB);Add(x.localWitnessA);Add(x.localWitnessB);Add(x.hasLocalAnchors);Add(x.anchorAInWorldFrame);Add(x.anchorBInWorldFrame);Add(int(x.separationState));}
static uint64_t cases=0,hits=0,misses=0,uncertain=0,booleanMismatches=0;
static void Case(const Shape&a,const ContactPose&pa,const Shape&b,const ContactPose&pb,float margin,bool output){
 ResetContactGeometryDiagnostics();auto m=PrimitiveContacts(a,pa,b,pb,margin);auto d=GetContactGeometryDiagnostics();
 Add(m.count);Add(m.uncertain);for(int i=0;i<m.count;++i)Add(m.points[i]);
 // Predicate counters may change; actual failure/uncertainty semantics may not.
 Add(d.unresolved);Add(d.invalidInputs);Add(d.numericGapFallbacks);Add(d.preparedOrientationMisses);
 auto truth=PrimitiveHasContacts(a,pa,b,pb,margin);booleanMismatches+=(truth!=(m.count>0));Add(truth);
 ++cases;m.count?++hits:++misses;uncertain+=m.uncertain;
 if(output)std::printf("%llu %016llx %d %d\n",(unsigned long long)cases,(unsigned long long)digest,m.count,int(m.uncertain));
}
int main(int argc,char**argv){bool output=argc==1||std::string(argv[1])!="--time";
 unsigned count=argc>2?unsigned(std::stoul(argv[2])):120000;
 auto begin=std::chrono::steady_clock::now();
 for(unsigned i=0;i<count;++i){
  Shape a=Shape::Box(Vector(.0001f,2)),b=Shape::Box(Vector(.0001f,2));
  ContactPose pa{Vector(-2,2),Rotation(),Vector(-.5f,.5f)},pb{Vector(-2,2),Rotation(),Vector(-.5f,.5f)};
  if(i%3==0)pb.position=pa.position;
  if(i%17==0){pa.orientation*=1.002f;pb.orientation*=.998f;}
  if(i%11==0)pa.orientation=pb.orientation;
  if(i%13==0){pa.orientation={1,0,0,0};pb.orientation={1,0,0,0};pa.localCenter={0,0,0};pb.localCenter={0,0,0};pb.position=pa.position;pb.position.x=pa.position.x+a.halfExtents.x+b.halfExtents.x;}
  if(i%19==0){const glm::vec3 shift{65536,-32768,131072};pa.position+=shift;pb.position+=shift;}
  if(i%23==0){const float s=std::ldexp(1.f,int(Random()%129)-64);pa.orientation*=s;pb.orientation*=s;}
  if(i%29==0){const float s=std::ldexp(1.f,int(Random()%181)-90);a.halfExtents*=s;b.halfExtents*=s;pa.position*=s;pb.position*=s;pa.localCenter*=s;pb.localCenter*=s;}
  if(i%31==0){const float q=std::ldexp(1.f,int(Random()%121)-60);pa.orientation={1,q,0,0};pb.orientation={1,0,q,0};}
  if(i%37==0){a=Shape::Sphere(Between(.01f,2));}
  if(i%41==0){b=Shape::Sphere(Between(.01f,2));}
  if(i%47==0){pa.orientation.x=std::nextafter(pa.orientation.x,std::numeric_limits<float>::infinity());}
  float margin=i%5==0?.03125f:0;
  if(i%71==0)margin=-.015625f;
  Case(a,pa,b,pb,margin,output);
  Case(b,pb,a,pa,margin,output);
 }
 Shape box=Shape::Box({.5f,.5f,.5f});ContactPose pa,pb;
 for(float x:{1.f,std::nextafter(1.f,0.f),std::nextafter(1.f,2.f),0.f,2.f}){
  for(float s:{1.f,1e-30f,1e30f,std::numeric_limits<float>::denorm_min()}){pa={};pb={};pa.orientation*=s;pb.orientation*=s;pb.position={x,0,0};Case(box,pa,box,pb,0,output);}
 }
 for(int k=0;k<6;++k){pa={};pb={};if(k==0)pa.orientation={0,0,0,0};if(k==1)pa.position.x=NAN;if(k==2)pb.localCenter.y=INFINITY;if(k==3)pa.orientation.z=INFINITY;if(k==4)pa.orientation.w=NAN;if(k==5)pa.orientation.x=std::numeric_limits<float>::denorm_min();Case(box,pa,box,pb,0,output);}
 auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 std::fprintf(stderr,"cases=%llu hits=%llu misses=%llu uncertain=%llu boolean_mismatches=%llu digest=%016llx elapsed_ms=%.3f\n",(unsigned long long)cases,(unsigned long long)hits,(unsigned long long)misses,(unsigned long long)uncertain,(unsigned long long)booleanMismatches,(unsigned long long)digest,ms);
 return booleanMismatches?1:0;
}
