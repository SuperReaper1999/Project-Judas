#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "RigidBody.h"
#include "ImpactPairVisitSet.h"
#include "ReentrantQueryScratch.h"
#include <functional>
#include <set>
#include <glm/gtc/quaternion.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks=0,positive=0,negative=0;
std::uint64_t digest=1469598103934665603ull;
void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
void Mix(const void* data,std::size_t size){auto p=static_cast<const unsigned char*>(data);while(size--){digest^=*p++;digest*=1099511628211ull;}}
void Hash(const ContactManifold& contacts){
 Mix(&contacts.count,sizeof(contacts.count));
 for(int i=0;i<contacts.count;++i){const auto& c=contacts.points[i];
  Mix(&c.point,sizeof(c.point));Mix(&c.normal,sizeof(c.normal));Mix(&c.penetration,sizeof(c.penetration));
  Mix(&c.signedSeparation,sizeof(c.signedSeparation));Mix(&c.preciseNormal,sizeof(c.preciseNormal));
  for(auto v:{c.localAnchorA,c.localAnchorB,c.localWitnessA,c.localWitnessB})Mix(&v,sizeof(v));
 }
}
std::uint32_t seed=0x73bde81;
float Unit(){seed=1664525u*seed+1013904223u;return float(seed>>8)*(2.f/16777216.f)-1.f;}
glm::vec3 Vector(){const auto x=Unit(),y=Unit(),z=Unit();return {x,y,z};}
glm::quat Rotation(){const auto axis=glm::normalize(Vector());return glm::angleAxis(Unit()*3.f,axis);}
std::shared_ptr<CollisionAsset> Cook(const MeshData& mesh,bool convex=false,bool twoSided=false){
 CollisionCookSettings settings;settings.convex=convex;settings.twoSided=twoSided;
 auto asset=std::make_shared<CollisionAsset>();std::string error;
 if(!CookCollision(mesh,settings,*asset,error))throw std::runtime_error(error);
 return asset;
}
MeshData Cube(){MeshData mesh;for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1}){MeshVertex v;v.position={float(x),float(y),float(z)};mesh.vertices.push_back(v);}
 mesh.indices={0,4,6,0,6,2,1,3,7,1,7,5,0,1,5,0,5,4,2,6,7,2,7,3,0,2,3,0,3,1,4,5,7,4,7,6};return mesh;
}
}
void CheckNestedScratch(){
 struct Frame {unsigned value=0;};
 Frame* first=nullptr;
 std::function<void(unsigned,unsigned)> visit=[&](unsigned depth,unsigned throwAt){
  ReentrantQueryScratch<Frame> scratch;if(!depth)first=&scratch.frame;
  scratch.frame.value=depth;
  if(depth==throwAt)throw std::runtime_error("injected nested visitor failure");
  if(depth<65)visit(depth+1,throwAt);
  Check(scratch.frame.value==depth,"nested query overwrites its caller's scratch");
 };
 visit(0,100);
 for(unsigned depth:{0u,31u,32u,33u,65u}){
  bool threw=false;try{visit(0,depth);}catch(const std::runtime_error& e){threw=std::string(e.what())=="injected nested visitor failure";}
  Check(threw,"nested visitor failure is not propagated");
  ReentrantQueryScratch<Frame> restored;
  Check(&restored.frame==first,"exception unwind does not restore outer scratch ownership");
 }
}
void CheckPairMembership(){
 ImpactPairVisitSet visited;
 for(unsigned slots:{2u,17u,128u,4096u,4097u}){
  visited.Begin(slots);std::set<std::uint64_t> independent;
  auto key=[](unsigned a,unsigned b){if(a>b)std::swap(a,b);return(std::uint64_t(a)<<32)|b;};
  for(unsigned i=0;i<1000;++i){
   seed=1664525u*seed+1013904223u;unsigned a=seed%slots;
   seed=1664525u*seed+1013904223u;unsigned b=seed%slots;if(a==b)b=(b+1)%slots;
   const auto pair=key(a,b);
   Check(visited.Contains(pair)==(independent.count(pair)!=0),"pair membership differs from independent ordered set");
   visited.Insert(pair);independent.insert(pair);
   Check(visited.Contains(pair),"inserted pair is not present");
  }
  Check(visited.CompactBytes()<=1024*1024,"compact pair scratch exceeds bounded budget");
  visited.Begin(slots);
  for(auto pair:independent)Check(!visited.Contains(pair),"new event retains stale pair membership");
 }
}
int main(){try{
 MeshData triangle;for(auto p:{glm::vec3(-1,0,-1),glm::vec3(1,0,-1),glm::vec3(0,0,1)}){MeshVertex v;v.position=p;triangle.vertices.push_back(v);}triangle.indices={0,2,1};
 auto hull=Cook(Cube(),true);
 const std::vector<Shape> targets={Shape::Cooked(Cook(triangle),"front triangle"),Shape::Cooked(Cook(triangle,false,true),"two-sided triangle"),Shape::Cooked(hull,"hull fixture")};
 const auto begin=std::chrono::steady_clock::now();
 for(unsigned i=0;i<12000;++i){
  PrimitivePose target;target.shape=targets[i%targets.size()];target.parentOrientation=Rotation();target.parentPosition=Vector()*20.f;
  PrimitivePose query;query.shape=i%5==0?Shape::Cooked(hull,"hull fixture"):Shape::Box({.23f,.4f,.1f});
  query.parentPosition=target.parentPosition+target.parentOrientation*(Vector()*1.8f);
  query.parentOrientation=Rotation();query.parentLocalCenter=Vector()*.03f;
  for(double margin:{-.05,0.,.1}){
   const auto contacts=CookedContacts(query,target,margin);Hash(contacts);
   Check(CookedHasContacts(query,target,margin)==bool(contacts.count),"Boolean truth differs from complete accepted witnesses");
   const auto reverse=CookedContacts(target,query,margin);Hash(reverse);
   Check(CookedHasContacts(target,query,margin)==bool(reverse.count),"reversed Boolean truth differs from full response");
   if(contacts.count)++positive;else ++negative;
  }
  const auto contacts=CookedContacts(query,target,0);double depth=0;
  for(int k=0;k<contacts.count;++k)depth=std::max(depth,-contacts.points[k].signedSeparation);
  for(double threshold:{0.,.01,.1})Check(CookedHasPenetrationBeyond(query,target,threshold)==(depth>threshold),"deep Boolean truth differs from retained maximum witness");
 }
 CheckNestedScratch();
 CheckPairMembership();
 Check(positive>1000&&negative>1000,"both actual positive and negative geometries exercised");
 std::cout<<"checks "<<checks<<" positive "<<positive<<" negative "<<negative<<" contactDigest "<<std::hex<<digest<<std::dec<<" elapsedMs "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<"\n";
 return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<" after "<<checks<<"\n";return 1;}}
