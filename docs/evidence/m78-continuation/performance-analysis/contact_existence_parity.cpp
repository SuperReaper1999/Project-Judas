// Isolated M78 proposal parity probe. No production or fixture mutation.
#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "SceneSerialization.h"
#include "Scene.h"
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

ContactManifold M78OriginalCookedContacts(const PrimitivePose&,const PrimitivePose&,double);
namespace {
std::size_t checks=0,samples=0,positive=0,negative=0;
std::string context;
void require(bool good,const char* text){++checks;if(!good)throw std::runtime_error(context+": "+text);}
template<class T>bool bits(const T& a,const T& b){return std::memcmp(&a,&b,sizeof(T))==0;}
void contactEqual(const Contact& a,const Contact& b){
 require(a.hit==b.hit,"hit unchanged");require(bits(a.point,b.point),"point unchanged");
 require(bits(a.normal,b.normal),"normal unchanged");require(bits(a.penetration,b.penetration),"penetration unchanged");
 require(bits(a.signedSeparation,b.signedSeparation),"separation unchanged");require(bits(a.preciseNormal,b.preciseNormal),"precise normal unchanged");
 require(bits(a.localAnchorA,b.localAnchorA)&&bits(a.localAnchorB,b.localAnchorB),"anchors unchanged");
 require(bits(a.localWitnessA,b.localWitnessA)&&bits(a.localWitnessB,b.localWitnessB),"witnesses unchanged");
 require(a.hasLocalAnchors==b.hasLocalAnchors&&a.anchorAInWorldFrame==b.anchorAInWorldFrame&&a.anchorBInWorldFrame==b.anchorBInWorldFrame,"anchor state unchanged");
 require(a.separationState==b.separationState,"separation state unchanged");
}
void compare(const PrimitivePose& a,const PrimitivePose& b,double margin){
 ++samples;
 const auto ref=M78OriginalCookedContacts(a,b,margin),full=CookedContacts(a,b,margin);
 const bool exists=CookedHasContacts(a,b,margin);
 require(ref.count==full.count&&ref.uncertain==full.uncertain,"full manifold count/state unchanged");
 require(exists==(ref.count>0),"existence equals original full manifold");
 for(int i=0;i<ref.count;++i)contactEqual(ref.points[i],full.points[i]);
 positive+=exists;negative+=!exists;
}
MeshData cube(){MeshData m;for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1}){MeshVertex v;v.position={float(x),float(y),float(z)};m.vertices.push_back(v);}m.indices={0,4,6,0,6,2,1,3,7,1,7,5,0,1,5,0,5,4,2,6,7,2,7,3,0,2,3,0,3,1,4,5,7,4,7,6};return m;}
MeshData actualFloor(){MeshData m;constexpr unsigned n=64;for(unsigned z=0;z<=n;++z)for(unsigned x=0;x<=n;++x){MeshVertex v;v.position={float(x)*60/n-30,0,float(z)*60/n-30};m.vertices.push_back(v);}for(unsigned z=0;z<n;++z)for(unsigned x=0;x<n;++x){unsigned a=z*(n+1)+x,b=a+1,c=a+n+1,d=c+1;m.indices.insert(m.indices.end(),{a,c,b,b,c,d});}return m;}
Shape cook(MeshData mesh,CollisionCookSettings settings,const std::string& name){CollisionAsset a;std::string error;if(!CookCollision(mesh,settings,a,error))throw std::runtime_error(name+": "+error);return Shape::Cooked(std::make_shared<CollisionAsset>(std::move(a)),name);}
uint32_t rng=0x4d783843u;
uint32_t randomBits(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
float randomFloat(float lo,float hi){const float t=float(randomBits()>>8)*(1.0f/16777216.0f);return lo+(hi-lo)*t;}
glm::vec3 randomAxis(){glm::vec3 v{randomFloat(-1,1),randomFloat(-1,1),randomFloat(-1,1)};return glm::dot(v,v)>1e-6?glm::normalize(v):glm::vec3(1,0,0);}
}
int main(){try{
 std::string error;Scene original;context="original imported rig";
 require(LoadSceneFromFile("projects/post_m65_consumers/Scenes/skate/park.judas",original,error),error.c_str());
 const auto* owner=original.Find(12);require(owner&&owner->ragdoll.has_value(),"same authored rig exists");require(owner->ragdoll->bones.size()==13,"original thirteen-body mapping retained");
 std::vector<Shape> rigShapes;std::vector<glm::vec3> offsets;std::vector<glm::quat> childRotations;
 for(const auto& b:owner->ragdoll->bones){rigShapes.push_back(b.shape==RagdollShape::Sphere?Shape::Sphere(b.radius):Shape::Box(b.halfExtents));offsets.push_back(b.offset);childRotations.push_back(b.orientation);}
 CollisionCookSettings settings;auto floor=cook(actualFloor(),settings,"actual-8192-triangle-floor");context="same cooked floor";require(floor.asset->faces.size()==8192,"original mesh triangle count retained");
 auto doubleSettings=settings;doubleSettings.twoSided=true;auto twoSided=cook(actualFloor(),doubleSettings,"two-sided-8192-triangle-floor");
 auto convexSettings=settings;convexSettings.convex=true;auto hull=cook(cube(),convexSettings,"convex-cube");
 std::vector<Shape> queries=rigShapes;queries.push_back(Shape::Capsule(.2f,.4f));queries.push_back(hull);
 auto oblique=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));
 const std::array<double,5> margins={-.01,0.,double(std::numeric_limits<float>::denorm_min()),.001,.2};
 for(unsigned target=0;target<3;++target)for(unsigned shape=0;shape<queries.size();++shape)for(unsigned k=0;k<96;++k){
  PrimitivePose a,b;a.shape=queries[shape];b.shape=target==0?floor:target==1?twoSided:hull;
  b.parentOrientation=k%3==0?glm::quat(1,0,0,0):oblique;
  b.parentPosition=k%3==0?glm::vec3(90,0,0):k%3==1?glm::vec3(10000,-10000,15000):glm::vec3(1000000,-1000000,1000000);
  const glm::vec3 relative{randomFloat(-5,5),randomFloat(-1,1),randomFloat(-5,5)};
  a.parentPosition=b.parentPosition+b.parentOrientation*relative;
  a.parentOrientation=glm::angleAxis(randomFloat(-3.1415926f,3.1415926f),randomAxis());
  if(k%7==0)a.parentOrientation*=1.125f; // Same represented non-unit quaternion accepted by real geometry.
  if(shape<rigShapes.size()){a.parentLocalCenter=offsets[shape];a.childRotation=childRotations[shape];}
  else {a.parentLocalCenter={.17f,-.11f,.09f};a.childRotation=glm::angleAxis(.37f,randomAxis());}
  context="seeded target="+std::to_string(target)+" shape="+std::to_string(shape)+" sample="+std::to_string(k);
  for(double margin:margins){compare(a,b,margin);compare(b,a,margin);}
 }
 // Independent near-boundary witnesses and float nextafter cases.
 PrimitivePose a,b;a.shape=Shape::Box({.4,.2,.3});b.shape=floor;
 for(float y:{std::nextafter(.2f,0.f),.2f,std::nextafter(.2f,1.f),.23f,2.f,-2.f})for(float angle:{0.f,.3f,-.3f}){
  a.parentPosition={0,y,0};a.parentOrientation=glm::angleAxis(angle,glm::vec3(0,0,1));context="inclined/nextafter witness y="+std::to_string(y);
  for(double margin:margins){compare(a,b,margin);compare(b,a,margin);}
 }
 // The own-plane identity applies to clipped FACE witnesses. SAT edge
 // fallbacks intentionally report separating-axis gap, not witness distance.
 // Preserve the original large-grid parity above; use the existing explicit
 // two-triangle sheet fixture to establish the face-witness identity here.
 MeshData sheet;for(auto p:{glm::vec3(-10,0,-10),glm::vec3(10,0,-10),glm::vec3(10,0,10),glm::vec3(-10,0,10)}){MeshVertex v;v.position=p;sheet.vertices.push_back(v);}sheet.indices={0,2,1,0,3,2};b.shape=cook(sheet,settings,"independent-face-sheet");
 a.parentPosition={0,.23f,0};a.parentOrientation=glm::angleAxis(.3f,glm::vec3(0,0,1));context="independent clipped face witness gap";
 for(double margin:{0.,.2}){auto m=CookedContacts(a,b,margin);require(m.count>0,"inclined real witness exists");for(int i=0;i<m.count;++i){const auto& c=m.points[i];auto wa=glm::dvec3(a.parentPosition)+glm::mat3_cast(glm::normalize(glm::dquat(a.parentOrientation)))*c.localWitnessA;auto wb=c.localWitnessB;require(std::abs(c.signedSeparation-glm::dot(c.preciseNormal,wa-wb))<1e-9,"independent own-plane witness gap preserved");require(c.signedSeparation<=margin,"above-margin witness remains rejected");}}
 // Coplanar seam, concave and convex creases viewed from front/back; no loss
 // of active-edge policy or change in winding equivalence.
 MeshData valley;for(auto p:{glm::vec3(-2,1,-2),glm::vec3(-2,1,2),glm::vec3(0,0,-2),glm::vec3(0,0,2),glm::vec3(2,1,-2),glm::vec3(2,1,2)}){MeshVertex v;v.position=p;valley.vertices.push_back(v);}valley.indices={0,1,3,0,3,2,2,3,5,2,5,4};auto reverse=valley;for(unsigned i=0;i<reverse.indices.size();i+=3)std::swap(reverse.indices[i+1],reverse.indices[i+2]);auto valleyShape=cook(valley,doubleSettings,"valley"),reverseShape=cook(reverse,doubleSettings,"reversed valley");
 for(auto target:{valleyShape,reverseShape})for(float y:{-.3f,-.2f,0.f,.2f,.3f,2.f})for(float x:{-2.f,-.4f,0.f,.4f,2.f}){a.parentPosition={x,y,0};a.parentOrientation=glm::angleAxis(.2f,glm::normalize(glm::vec3(1,2,3)));b.shape=target;context="two-sided crease x="+std::to_string(x)+" y="+std::to_string(y);for(double margin:margins){compare(a,b,margin);compare(b,a,margin);}}
 context="coverage";require(positive>0&&negative>0,"both accepted and rejected contact sets exercised");
 std::cout<<"{\"checks\":"<<checks<<",\"samples\":"<<samples<<",\"positive\":"<<positive<<",\"negative\":"<<negative<<",\"originalRigBodies\":13,\"meshTriangles\":8192,\"fullManifoldBitwiseParity\":true,\"seed\":\"0x4d783843\",\"failures\":0}\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" checks="<<checks<<" samples="<<samples<<'\n';return 1;}}
