#include "CanonicalSphereBox.h"
#include "Narrowphase.h"
#include "ContactPreparedGeometry.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
using JudasPhysicsDetail::CanReverseSphereBoxManifold;
using JudasPhysicsDetail::ReverseSphereBoxManifold;
namespace {
unsigned cases=0,failures=0,hits=0,misses=0,uncertain=0;
bool omitNormal=false,omitAnchors=false,allPrimitives=false;
std::uint32_t randomState=0x7293b105u;
std::uint32_t Random(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
float Between(float a,float b){return a+(b-a)*float(Random()>>8)*(1.f/16777216.f);}
template<class T>bool Same(T a,T b){return std::memcmp(&a,&b,sizeof a)==0;}
std::string Different(const ContactManifold& a,const ContactManifold& b){
 if(a.count!=b.count)return "count";if(a.uncertain!=b.uncertain)return "uncertain";
 for(int i=0;i<a.count;++i){const auto& x=a.points[i];const auto& y=b.points[i];
  if(x.hit!=y.hit)return "hit";
  for(int k=0;k<3;++k){
   if(!Same(x.point[k],y.point[k]))return "point";
   if(!Same(x.normal[k],y.normal[k]))return "normal";
   if(!Same(x.preciseNormal[k],y.preciseNormal[k]))return "preciseNormal";
   if(!Same(x.localAnchorA[k],y.localAnchorA[k]))return "localAnchorA";
   if(!Same(x.localAnchorB[k],y.localAnchorB[k]))return "localAnchorB";
   if(!Same(x.localWitnessA[k],y.localWitnessA[k]))return "localWitnessA";
   if(!Same(x.localWitnessB[k],y.localWitnessB[k]))return "localWitnessB";
  }
  if(!Same(x.penetration,y.penetration))return "penetration";
  if(!Same(x.signedSeparation,y.signedSeparation))return "signedSeparation";
  if(x.hasLocalAnchors!=y.hasLocalAnchors)return "hasLocalAnchors";
  if(x.anchorAInWorldFrame!=y.anchorAInWorldFrame)return "anchorAInWorldFrame";
  if(x.anchorBInWorldFrame!=y.anchorBInWorldFrame)return "anchorBInWorldFrame";
  if(x.separationState!=y.separationState)return "separationState";
 }
 return {};
}
void Convert(ContactManifold& m){
 if(!omitNormal&&!omitAnchors){ReverseSphereBoxManifold(m);return;}
 for(int i=0;i<m.count;++i){auto& c=m.points[i];if(!omitNormal){c.normal=-c.normal;c.preciseNormal=-c.preciseNormal;}if(!omitAnchors){std::swap(c.localAnchorA,c.localAnchorB);std::swap(c.localWitnessA,c.localWitnessB);}}
}
void Check(bool ok,const std::string& label){++cases;if(!ok){++failures;if(failures<20)std::printf("FAIL %s\n",label.c_str());}}
ContactManifold Compare(const std::string& label,const Shape& a,const RigidBody& ra,int pa,const Shape& b,const RigidBody& rb,int pb,bool prepared){
 const auto aa=PrimitiveAt(a,ra,pa),bb=PrimitiveAt(b,rb,pb);
 std::optional<ContactPreparedOrientation> oa,ob;
 if(prepared){oa.emplace(ra.orientation);ob.emplace(rb.orientation);}
 const auto direct=ComputeContacts(aa,bb,0,oa?&*oa:nullptr,ob?&*ob:nullptr);
 auto reverse=ComputeContacts(bb,aa,0,ob?&*ob:nullptr,oa?&*oa:nullptr);
 if(CanReverseSphereBoxManifold(aa.shape.type,bb.shape.type)||allPrimitives){
  Convert(reverse);const auto field=Different(direct,reverse);Check(field.empty(),label+" "+field);
  hits+=direct.count>0;misses+=direct.count==0&&!direct.uncertain;uncertain+=direct.uncertain;
 }
 return direct;
}
}
int main(int argc,char** argv){
 for(int i=1;i<argc;++i){const std::string arg=argv[i];omitNormal|=arg=="--omit-normal";omitAnchors|=arg=="--omit-anchors";allPrimitives|=arg=="--all-primitives";}
 const auto sphere=Shape::Sphere(.5f),box=Shape::Box({1,1,1});RigidBody a,b;a.position={0,1.4f,0};
 const auto touching=Compare("independent overlap",sphere,a,0,box,b,0,false);Check(touching.count==1&&touching.points[0].normal.y>0&&std::abs(touching.points[0].signedSeparation+.1)<2e-7,"independent known overlap/normal/depth");
 a.position={0,3,0};const auto separated=Compare("independent separation",sphere,a,0,box,b,0,false);Check(separated.count==0&&!separated.uncertain,"independent absent contact");
 for(float y:{std::nextafter(1.5f,0.f),1.5f,std::nextafter(1.5f,2.f)}){a.position={0,y,0};Compare("near tangent",sphere,a,0,box,b,0,true);Compare("near tangent reverse identity",box,b,0,sphere,a,0,true);}
 for(unsigned i=0;i<1200;++i){
  Shape s=Shape::Sphere(Between(.01f,2));Shape q=Shape::Box({Between(.03f,2),Between(.03f,2),Between(.03f,2)});
  RigidBody x,y;x.position={Between(-3,3),Between(-3,3),Between(-3,3)};y.position={Between(-1,1),Between(-1,1),Between(-1,1)};
  x.orientation=glm::quat(Between(.1f,2),Between(-1,1),Between(-1,1),Between(-1,1));y.orientation=glm::quat(Between(.1f,2),Between(-1,1),Between(-1,1),Between(-1,1));
  if(i%3==0){s.pivotOffset={Between(-1,1),Between(-1,1),Between(-1,1)};q.pivotOffset={Between(-1,1),Between(-1,1),Between(-1,1)};}
  Compare("random mixed "+std::to_string(i),s,x,0,q,y,0,i%2);
  Compare("random reverse "+std::to_string(i),q,y,0,s,x,0,i%2);
 }
 for(float scale:{1e-30f,1e-15f,1.f,1e15f,1e30f})for(float quaternionScale:{1e-30f,1.f,1e30f}){
  auto s=Shape::Sphere(scale*.5f),q=Shape::Box(glm::vec3(scale));RigidBody x,y;x.position={0,scale*1.4f,0};x.orientation=glm::quat(quaternionScale,.2f*quaternionScale,0,0);y.orientation=glm::quat(quaternionScale,0,.3f*quaternionScale,0);
  Compare("range mixed",s,x,0,q,y,0,false);Compare("range reverse",q,y,0,s,x,0,false);
 }
 for(float bad:{0.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  RigidBody x,y;x.orientation={bad,0,0,0};Compare("uncertain input",sphere,x,0,box,y,0,false);Compare("uncertain input reverse",box,y,0,sphere,x,0,false);
 }
 CompoundBox sChild({1.2f,-.4f,.7f},{0,0,0});sChild.type=ShapeType::Sphere;sChild.radius=.8f;sChild.rotation=glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3)));
 CompoundBox bChild({.2f,.5f,-.3f},{1,.4f,.9f});bChild.rotation=glm::angleAxis(-.8f,glm::normalize(glm::vec3(2,-3,1)));
 auto sa=Shape::Compound({sChild}),sb=Shape::Compound({bChild});sa.pivotOffset={.1f,.2f,-.1f};sb.pivotOffset={-.1f,.3f,.2f};
 for(unsigned i=0;i<100;++i){RigidBody x,y;x.position={Between(-1,1),Between(-1,1),Between(-1,1)};y.position={Between(-1,1),Between(-1,1),Between(-1,1)};x.orientation=glm::angleAxis(Between(-3,3),glm::normalize(glm::vec3(1,2,3)));y.orientation=glm::angleAxis(Between(-3,3),glm::normalize(glm::vec3(2,-3,1)));Compare("rotated offset children",sa,x,0,sb,y,0,true);Compare("rotated offset children reverse",sb,y,0,sa,x,0,true);}
 Check(hits>100&&misses>100&&uncertain>0,"actual hit/miss/uncertain branches exercised");
 Check(!CanReverseSphereBoxManifold(ShapeType::Box,ShapeType::Box)&&!CanReverseSphereBoxManifold(ShapeType::Sphere,ShapeType::Sphere)&&!CanReverseSphereBoxManifold(ShapeType::Capsule,ShapeType::Box),"unsupported shape pairs excluded");
 if(allPrimitives){for(unsigned i=0;i<50;++i){RigidBody x,y;x.position={.2f,.3f,.4f};y.orientation=glm::angleAxis(float(i)*.07f,glm::normalize(glm::vec3(1,2,3)));Compare("intentional unsupported box reversal",box,x,0,box,y,0,true);}}
 std::printf("SUMMARY %u cases %u failures; %u hits %u misses %u uncertain\n",cases,failures,hits,misses,uncertain);return failures?1:0;
}
