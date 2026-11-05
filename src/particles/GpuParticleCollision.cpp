#include "GpuParticleCollision.h"
#include "../CollisionAsset.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <glm/gtc/quaternion.hpp>

namespace JudasParticles::GpuDetail {
namespace {
std::array<float,4> V(glm::vec3 p,float w=0){return {p.x,p.y,p.z,w};}
std::array<float,4> Q(glm::quat q){return {q.x,q.y,q.z,q.w};}
void Hash(std::uint64_t& h,const void* p,std::size_t n){for(auto b=static_cast<const unsigned char*>(p);n;--n,++b){h^=*b;h*=1099511628211ull;}}
template<class T> void Hash(std::uint64_t& h,const T& v){Hash(h,&v,sizeof(v));}
bool Supported(const CollisionSnapshot& snapshot,std::string& error){
 for(std::size_t body=0;body<snapshot.Bodies().size();++body)if(!snapshot.IsStationary(body)){
  error="GPU particles require stationary collision snapshots; moving or repaired bodies use CPU collision.";return false;
 }
 for(const auto& collider:snapshot.Colliders()){
  if(collider.terrain){error="GPU particles do not evaluate radial-terrain CPU callbacks.";return false;}
  if(collider.kind!=ShapeType::Sphere&&collider.kind!=ShapeType::Capsule&&collider.kind!=ShapeType::Box&&collider.kind!=ShapeType::TriangleMesh){
   error="GPU particles currently support spheres, capsules, boxes and static cooked triangle meshes; this shape uses CPU collision.";return false;
  }
  if(collider.kind==ShapeType::TriangleMesh&&(!collider.asset||collider.asset->nodes.empty())){
   error="GPU mesh collision requires the actual immutable cooked triangle BVH.";return false;
  }
 }
 return true;
}
}
std::uint64_t GeometryFingerprint(const CollisionSnapshot& snapshot,std::string& error){
 error.clear();if(!Supported(snapshot,error))return 0;
 std::uint64_t hash=14695981039346656037ull;
 for(const auto& collider:snapshot.Colliders()){
  const auto& body=snapshot.Bodies()[collider.bodyIndex];
  Hash(hash,body.pose.position);Hash(hash,body.pose.rotation);Hash(hash,body.handle.id);Hash(hash,body.entity);
  Hash(hash,collider.geometryRevision);Hash(hash,collider.childKey);Hash(hash,collider.kind);
  Hash(hash,collider.radius);Hash(hash,collider.halfHeight);Hash(hash,collider.halfExtents);
  Hash(hash,collider.localCenter);Hash(hash,collider.localRotation);
  const auto identity=reinterpret_cast<std::uintptr_t>(collider.asset.get());Hash(hash,identity);
 }
 return hash;
}

bool PackGeometry(const CollisionSnapshot& snapshot,PackedGeometry& output,std::string& error){
 const auto fingerprint=GeometryFingerprint(snapshot,error);if(!error.empty())return false;
 PackedGeometry next;next.fingerprint=fingerprint;
 std::unordered_map<const CollisionAsset*,unsigned> assets;
 std::vector<glm::vec3> minimum,maximum;
 for(std::size_t index=0;index<snapshot.Colliders().size();++index){
  const auto& source=snapshot.Colliders()[index];const auto& body=snapshot.Bodies()[source.bodyIndex];
  const auto rotation=glm::normalize(body.pose.rotation*source.localRotation);
  const auto center=body.pose.position+body.pose.rotation*source.localCenter;
  Collider collider;collider.centerRadius=V(center,source.radius);collider.rotation=Q(rotation);
  collider.extents=V(source.halfExtents,source.halfHeight);collider.meta[2]=unsigned(index);
  glm::vec3 localCenter(0),extent;
  if(source.kind==ShapeType::Sphere){collider.meta[0]=0;extent=glm::vec3(source.radius);}
  else if(source.kind==ShapeType::Capsule){collider.meta[0]=1;extent={source.radius,source.radius+source.halfHeight,source.radius};}
  else if(source.kind==ShapeType::Box){collider.meta[0]=2;extent=source.halfExtents;}
  else{
   collider.meta[0]=3;const auto& asset=*source.asset;collider.meta[3]=asset.twoSided?1:0;
   auto found=assets.find(&asset);
   if(found==assets.end()){
    const auto nodeBase=unsigned(next.nodes.size()),triangleBase=unsigned(next.triangles.size());
    if(asset.order.size()!=asset.faces.size()){error="GPU collision BVH does not cover every cooked face.";return false;}
    for(auto faceIndex:asset.order){
     if(faceIndex>=asset.faces.size()){error="Invalid cooked triangle order.";return false;}
     const auto& face=asset.faces[faceIndex];Triangle triangle;
     triangle.a=V(glm::vec3(asset.vertices[face.vertices[0]]));triangle.b=V(glm::vec3(asset.vertices[face.vertices[1]]));triangle.c=V(glm::vec3(asset.vertices[face.vertices[2]]));triangle.normal=V(glm::vec3(face.normal));triangle.meta[0]=face.source;
     next.triangles.push_back(triangle);
    }
    for(const auto& sourceNode:asset.nodes){
     Node node;auto lo=glm::vec3(sourceNode.minimum),hi=glm::vec3(sourceNode.maximum);
     for(unsigned axis=0;axis<3;++axis){lo[axis]=std::nextafter(lo[axis],-std::numeric_limits<float>::infinity());hi[axis]=std::nextafter(hi[axis],std::numeric_limits<float>::infinity());}
     node.minimum=V(lo);node.maximum=V(hi);node.links={triangleBase+sourceNode.first,sourceNode.count,nodeBase+sourceNode.left,nodeBase+sourceNode.right};next.nodes.push_back(node);
    }
    found=assets.emplace(&asset,nodeBase).first;
   }
   collider.meta[1]=found->second;
   localCenter=glm::vec3((asset.minimum+asset.maximum)*.5);extent=glm::vec3((asset.maximum-asset.minimum)*.5);
  }
  const auto r=glm::mat3_cast(rotation);const auto worldExtent=glm::abs(r[0])*extent.x+glm::abs(r[1])*extent.y+glm::abs(r[2])*extent.z;
  const auto worldCenter=center+rotation*localCenter;
  minimum.push_back(worldCenter-worldExtent);maximum.push_back(worldCenter+worldExtent);next.colliders.push_back(collider);
 }
 if(!next.colliders.empty()){
  std::vector<unsigned> order(next.colliders.size());std::iota(order.begin(),order.end(),0);
  std::function<unsigned(unsigned,unsigned,unsigned)> build=[&](unsigned begin,unsigned end,unsigned depth){
   const auto index=unsigned(next.nodes.size());next.nodes.emplace_back();
   auto lo=minimum[order[begin]],hi=maximum[order[begin]];
   for(unsigned i=begin+1;i<end;++i){lo=glm::min(lo,minimum[order[i]]);hi=glm::max(hi,maximum[order[i]]);}
   next.nodes[index].minimum=V(lo);next.nodes[index].maximum=V(hi);
   if(end-begin<=4){next.nodes[index].links={begin,end-begin,0,0};return index;}
   const auto size=hi-lo;unsigned axis=size.y>size.x?1:0;if(size.z>size[axis])axis=2;
   const auto middle=begin+(end-begin)/2;
   std::nth_element(order.begin()+begin,order.begin()+middle,order.begin()+end,[&](auto a,auto b){const auto ca=minimum[a][axis]+maximum[a][axis],cb=minimum[b][axis]+maximum[b][axis];return ca==cb?a<b:ca<cb;});
   if(depth>=30){error="GPU collision hierarchy exceeds its bounded traversal stack.";return index;}
   const auto left=build(begin,middle,depth+1),right=build(middle,end,depth+1);
   next.nodes[index].links={0,0,left,right};return index;
  };
  next.root=build(0,unsigned(order.size()),0);if(!error.empty())return false;
  auto original=next.colliders;for(std::size_t i=0;i<order.size();++i)next.colliders[i]=original[order[i]];
 }
 output=std::move(next);return true;
}

std::string CollisionShaderSource(){return R"GLSL(
struct Collider {vec4 centerRadius;vec4 rotation;vec4 extents;uvec4 meta;};
struct Node {vec4 minimum;vec4 maximum;uvec4 links;};
struct Triangle {vec4 a;vec4 b;vec4 c;vec4 normal;uvec4 meta;};
layout(std430,binding=4) readonly buffer Colliders {Collider colliders[];};
layout(std430,binding=5) readonly buffer Nodes {Node nodes[];};
layout(std430,binding=6) readonly buffer Triangles {Triangle triangles[];};
vec3 rotateQ(vec4 q,vec3 p){return p+2.0*cross(q.xyz,cross(q.xyz,p)+q.w*p);}
vec3 closestTriangle(vec3 p,Triangle t){
 vec3 a=t.a.xyz,b=t.b.xyz,c=t.c.xyz,ab=b-a,ac=c-a,ap=p-a;
 float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0.0&&d2<=0.0)return a;
 vec3 bp=p-b;float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0.0&&d4<=d3)return b;
 float vc=d1*d4-d3*d2;if(vc<=0.0&&d1>=0.0&&d3<=0.0)return a+ab*(d1/(d1-d3));
 vec3 cp=p-c;float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0.0&&d5<=d6)return c;
 float vb=d5*d2-d1*d6;if(vb<=0.0&&d2>=0.0&&d6<=0.0)return a+ac*(d2/(d2-d6));
 float va=d3*d6-d5*d4;if(va<=0.0&&(d4-d3)>=0.0&&(d5-d6)>=0.0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
 float denominator=va+vb+vc;if(abs(denominator)<1e-24)return a;
 return a+ab*(vb/denominator)+ac*(vc/denominator);
}
bool sweptBounds(vec3 p,vec3 movement,float remaining,Node node){
 vec3 end=p+movement*remaining,radius=vec3(params.accelerationRadius.w+1e-5);
 return all(greaterThanEqual(max(p,end)+radius,node.minimum.xyz))&&all(lessThanEqual(min(p,end)-radius,node.maximum.xyz));
}
struct Nearest {float gap;float advance;vec3 normal;vec3 point;uint collider;uint feature;bool failed;};
// Every candidate is convex (an analytic shape or one finite triangle). Its
// closest-point normal defines a genuine supporting plane of THAT candidate.
// Take the earliest plane-crossing certificate across the complete union; an
// away/tangent candidate cannot hide a different obstacle farther along the ray.
void offer(float gap,vec3 normal,vec3 point,uint collider,uint feature,vec3 movement,inout Nearest hit){
 if(gap<=1e-5){if(hit.advance!=0.0||gap<hit.gap){hit.gap=gap;hit.advance=0.0;hit.normal=normal;hit.point=point;hit.collider=collider;hit.feature=feature;}return;}
 if(hit.advance==0.0)return;
 float closing=-dot(movement,normal);if(closing<=0.0)return;
 float certificate=gap/closing;
 if(certificate<hit.advance){hit.gap=gap;hit.advance=certificate;hit.normal=normal;hit.point=point;hit.collider=collider;hit.feature=feature;}
}
void meshNearest(vec3 local,vec3 movement,float remaining,Collider collider,inout Nearest hit){
 vec3 localMovement=rotateQ(vec4(-collider.rotation.xyz,collider.rotation.w),movement);
 uint stack[32];int count=1;stack[0]=collider.meta.y;uint visits=0u;
 while(count>0){
  if(++visits>1048576u){hit.failed=true;return;}
  Node node=nodes[stack[--count]];if(!sweptBounds(local,localMovement,remaining,node))continue;
  if(node.links.y==0u){if(count>29){hit.failed=true;return;}stack[count++]=node.links.z;stack[count++]=node.links.w;continue;}
  for(uint j=0u;j<node.links.y;++j){Triangle triangle=triangles[node.links.x+j];
   if(collider.meta.w==0u&&dot(local-triangle.a.xyz,triangle.normal.xyz)<-1e-6)continue;
   vec3 nearest=closestTriangle(local,triangle),delta=local-nearest;float distance=length(delta),gap=distance-params.accelerationRadius.w;
   vec3 normal=rotateQ(collider.rotation,distance>1e-8?delta/distance:triangle.normal.xyz);
   offer(gap,normal,collider.centerRadius.xyz+rotateQ(collider.rotation,nearest),collider.meta.z,triangle.meta.x,movement,hit);
  }
 }
}
Nearest nearestScene(vec3 p,vec3 movement,float remaining){
 Nearest hit;hit.gap=1e30;hit.advance=1e30;hit.normal=vec3(0,1,0);hit.point=p;hit.collider=0u;hit.feature=0xffffffffu;hit.failed=false;
 if(params.query.y==0u)return hit;
 uint stack[32];int count=1;stack[0]=params.query.x;
 while(count>0){Node node=nodes[stack[--count]];if(!sweptBounds(p,movement,remaining,node))continue;
  if(node.links.y==0u){if(count>29){hit.failed=true;return hit;}stack[count++]=node.links.z;stack[count++]=node.links.w;continue;}
  for(uint j=0u;j<node.links.y;++j){Collider collider=colliders[node.links.x+j];vec4 inverseQ=vec4(-collider.rotation.xyz,collider.rotation.w);vec3 local=rotateQ(inverseQ,p-collider.centerRadius.xyz);
   if(collider.meta.x==3u){meshNearest(local,movement,remaining,collider,hit);continue;}
   vec3 delta=local;float gap=0;vec3 normal,point;
   if(collider.meta.x<=1u){if(collider.meta.x==1u)delta.y-=clamp(delta.y,-collider.extents.w,collider.extents.w);float lengthDelta=length(delta);normal=lengthDelta>1e-8?delta/lengthDelta:vec3(1,0,0);gap=lengthDelta-collider.centerRadius.w;point=local-normal*gap;}
   else {vec3 nearest=clamp(local,-collider.extents.xyz,collider.extents.xyz);delta=local-nearest;float lengthDelta=length(delta);
    if(lengthDelta>1e-8){normal=delta/lengthDelta;gap=lengthDelta;point=nearest;}
    else{vec3 clearance=collider.extents.xyz-abs(local);uint axis=clearance.y<clearance.x?1u:0u;if(clearance.z<clearance[axis])axis=2u;normal=vec3(0);normal[axis]=local[axis]<0.0?-1.0:1.0;gap=-clearance[axis];point=local-normal*gap;}
   }
   offer(gap-params.accelerationRadius.w,rotateQ(collider.rotation,normal),collider.centerRadius.xyz+rotateQ(collider.rotation,point),collider.meta.z,0xffffffffu,movement,hit);
  }
 }
 return hit;
}
bool sweepScene(vec3 from,vec3 to,out float fraction,out Nearest hit){
 vec3 movement=to-from;fraction=0.0;
 if(any(lessThan(min(from,to)-vec3(params.accelerationRadius.w),params.domainMinimum.xyz))||any(greaterThan(max(from,to)+vec3(params.accelerationRadius.w),params.domainMaximum.xyz))){hit=nearestScene(from,movement,1.0);hit.failed=true;return false;}
 for(uint iteration=0u;iteration<96u;++iteration){hit=nearestScene(from+movement*fraction,movement,1.0-fraction);if(hit.failed)return false;
  if(hit.advance==0.0)return true;
  if(hit.advance>1.0-fraction){fraction=1.0;return false;}
  float next=fraction+hit.advance;if(!(next>fraction)){hit.failed=true;return false;}fraction=next;
 }
 hit.failed=true;return false;
}
)GLSL";}
} // namespace JudasParticles::GpuDetail
