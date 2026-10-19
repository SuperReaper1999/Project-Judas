#include "Contacts.h"
#include "ContactGeometryInternal.h"
#include "FilteredSat.h"
#include <cstdio>
#include <cmath>
#include <limits>
#include <cstdint>
#include <string>
using namespace contact_geometry;
static uint32_t state=0x7842bed1;
static uint32_t Random(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static float Between(float a,float b){return a+(b-a)*float(Random()>>8)*(1.f/16777216.f);}
static float Scaled(){return std::ldexp(Between(-1,1),int(Random()%125)-62);}
static glm::vec3 Vec(){return {Scaled(),Scaled(),Scaled()};}
static uint64_t checks=0,failures=0,certified=0,ambiguous=0,zeroaxes=0,oldambiguous=0,exactchecks=0,unsafe=0;
static void Check(bool value,const char*label,unsigned row,unsigned axis){++checks;if(!value){if(failures<20)std::printf("FAIL %s row=%u axis=%u\n",label,row,axis);++failures;}}
int main(int argc,char**argv){bool corrupt=argc>1&&std::string(argv[1])=="--reverse-filter";bool omit=argc>1&&std::string(argv[1])=="--omit-bound";
 for(unsigned i=0;i<12000;++i){
  ContactPose a,b;a.position=Vec();b.position=Vec();a.localCenter=Vec();b.localCenter=Vec();a.orientation={Scaled(),Scaled(),Scaled(),Scaled()};b.orientation={Scaled(),Scaled(),Scaled(),Scaled()};
  glm::vec3 ha=glm::abs(Vec()),hb=glm::abs(Vec());
  if(i%2==0){a.position={Between(-2,2),Between(-2,2),Between(-2,2)};b.position=a.position+glm::vec3(Between(-2,2),Between(-2,2),Between(-2,2));a.localCenter={0,0,0};b.localCenter={0,0,0};a.orientation={Between(-1,1),Between(-1,1),Between(-1,1),Between(-1,1)};b.orientation={Between(-1,1),Between(-1,1),Between(-1,1),Between(-1,1)};ha={.5f,.5f,.5f};hb=ha;}
  if(i%3==0){b.orientation=a.orientation;b.position=a.position;ha=hb;}
  if(i%5==0){a.position={65536,-32768,131072};b.position=a.position+glm::vec3(1,0,0);}
  if(i%7==0){b.localCenter=a.localCenter+glm::vec3(1,0,0);b.position=a.position-glm::vec3(1,0,0);}
  if(i%11==0){a.orientation={1,0,0,0};b.orientation={1,0,0,0};ha={.5f,.5f,.5f};hb=ha;b.position=a.position+glm::vec3(1,0,0);a.localCenter=b.localCenter={0,0,0};}
  if(i%13==0){a.orientation={1,0x1p-64f,0,0};b.orientation={1,0,0x1p-64f,0};}
  if(i%17==0){a.orientation={1,0x1p-65f,0,0};b.orientation={1,0,0x1p-65f,0};}
  Pair<double> n(a,b);Pair<Iv> v(a,b);SatWorkspace<Iv> workspace(v);FilteredSat filter(a,b,n,ha,hb);
  for(unsigned k=0;k<15;++k){
   auto numericAxis=Axis(n,int(k));auto intervalAxis=Axis(v,int(k));
   auto length=Dot(intervalAxis,intervalAxis);auto old=workspace.Evaluate(ha,hb,int(k),intervalAxis);
   auto scalar=Sat(n,ha,hb,int(k));int actual=filter.Sign(int(k),scalar);if(omit)actual=scalar>0?1:scalar<0?-1:FilteredSat::Unresolved;
   if(actual!=FilteredSat::Unresolved){
    ++certified;if(corrupt)actual=-actual;
    bool ok=actual>0?old.lo>0:old.hi<0;if(!ok)++oldambiguous;
    Check(ok,"filter does not preserve legacy interval sign",i,k);
   }else ++ambiguous;
   if(filter.NonzeroAxis(int(k),numericAxis))Check(length.lo>0,"nonzero filter does not preserve legacy interval resolution",i,k);
   else ++zeroaxes;
   // Exact expansion reference is not derived from the optimized workspace.
   // All ordinary pairs + a representative scaled subset exercise it.
   if(i<300||(i%101==0)){
    Pair<Exact> exact(a,b);auto expected=Sat(exact,ha,hb,int(k)).Sign();++exactchecks;
    if(actual!=FilteredSat::Unresolved)Check(actual==expected,"certified sign differs from exact polynomial",i,k);
    if(scalar!=0&&(scalar>0?1:-1)!=expected)++unsafe;
   }
  }
 }
 std::printf("checks=%llu failures=%llu certified=%llu ambiguous=%llu axis_fallback=%llu legacy_ambiguity=%llu exact_checks=%llu unsafe_scalar_signs=%llu\n",(unsigned long long)checks,(unsigned long long)failures,(unsigned long long)certified,(unsigned long long)ambiguous,(unsigned long long)zeroaxes,(unsigned long long)oldambiguous,(unsigned long long)exactchecks,(unsigned long long)unsafe);
 return failures?1:0;
}
