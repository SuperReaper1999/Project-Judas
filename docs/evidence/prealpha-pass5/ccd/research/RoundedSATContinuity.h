#pragma once
#include "MeshContinuityProbe.h"
#include "RoundedInterval.h"

namespace rounded_episode_probe {
using V=glm::dvec3;
using IV = std::array<RI,3>;
inline IV Add(const IV&a,const IV&b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
inline IV Sub(const IV&a,const IV&b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline IV Cross(const IV&a,const IV&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline RI Dot(const IV&a,const IV&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline IV Point(V p){return {RI(p[0]),RI(p[1]),RI(p[2])};}
inline IV Times(const contact_geometry::M<RI>&r,V p){
 IV out{RI(0),RI(0),RI(0)};for(unsigned k=0;k<3;++k)for(unsigned j=0;j<3;++j)out[k]=out[k]+r[j][k]*RI(p[j]);return out;
}
inline std::optional<IV> Unit(IV v){
 const auto length=NearestSqrt(Dot(v,v));if(!(length.lo>1e-10))return std::nullopt;
 for(auto&x:v)x=x/length;return v;
}

// All axes and vertices mirror visitCookedPolyContacts' actual represented
// Poly construction. No inverse-R approximation or fixed contact witness.
// This is sufficient-only overlap certification: uncertainty remains explicit.
inline bool NativeOverlapThroughout(const PrimitivePose&query,const PrimitivePose&mesh,
 unsigned feature,const RigidMotionSegment&segment,double first,double last){
 if(query.shape.type!=ShapeType::Box&&query.shape.type!=ShapeType::ConvexHull)return false;
 try{
  const auto from=segment.EvaluateGeometry(first),to=segment.EvaluateGeometry(last);
  auto parent=RoundedRotation(segment,first,last);
  if(segment.driftAngularVelocity==glm::vec3(0)&&rigid_motion_bounds::SameQuat(from.orientation,to.orientation)){
   contact_geometry::M<RI> exact;const auto r=ContactRotation(from.orientation);
   for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k)exact[j][k]=RI(r[j][k]);parent=exact;
  }
  if(!parent)return false;
  const auto child=ContactRotation(query.childRotation);
  contact_geometry::M<RI> rotation;
  for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k){rotation[j][k]=RI(0);for(unsigned l=0;l<3;++l)rotation[j][k]=rotation[j][k]+(*parent)[l][k]*RI(child[j][l]);}
  IV center=Times(*parent,V(query.parentLocalCenter));const V origin=PrimitiveCenter(mesh);
  for(unsigned k=0;k<3;++k)center[k]=(RI(double(std::min(from.position[k],to.position[k])),double(std::max(from.position[k],to.position[k])))+center[k])-RI(origin[k]);
  std::vector<IV> a,b;std::vector<IV> normals;std::vector<std::array<unsigned,2>> edges;
  if(query.shape.type==ShapeType::Box){
   const V h(query.shape.halfExtents);for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1})a.push_back(Add(center,Times(rotation,V(x*h.x,y*h.y,z*h.z))));
   static constexpr unsigned faces[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
   for(const auto&face:faces){auto n=Unit(Cross(Sub(a[face[1]],a[face[0]]),Sub(a[face[2]],a[face[0]])));if(!n)return false;normals.push_back(*n);}
   edges={{{0,1}},{{0,2}},{{0,4}}};
  }else{
   for(V p:query.shape.asset->vertices)a.push_back(Add(center,Times(rotation,p)));
   for(const auto&face:query.shape.asset->polygons)normals.push_back(Times(rotation,face.normal));
   for(auto e:query.shape.asset->satEdges)edges.push_back(query.shape.asset->edges[e]);
   if(edges.empty())edges=query.shape.asset->edges;
  }
  const auto fixed=PrimitiveRotation(mesh);const auto&face=mesh.shape.asset->faces[feature];
  for(auto index:face.vertices)b.push_back(Point(fixed*mesh.shape.asset->vertices[index]));
  normals.push_back(Point(fixed*face.normal));
  const auto overlaps=[&](IV axis){
   const auto length=NearestSqrt(Dot(axis,axis));
   if(length.hi<1e-10)return true;
   if(!(length.lo>=1e-10))return false;
   for(auto&x:axis)x=x/length;
   double minA=INFINITY,maxA=-INFINITY,minB=INFINITY,maxB=-INFINITY;
   for(const auto&p:a){auto value=Dot(p,axis);minA=std::min(minA,value.hi);maxA=std::max(maxA,value.lo);}
   for(const auto&p:b){auto value=Dot(p,axis);minB=std::min(minB,value.hi);maxB=std::max(maxB,value.lo);}
   return minA<=maxB&&minB<=maxA;
  };
  for(const auto&normal:normals)if(!overlaps(normal))return false;
  static constexpr std::array<std::array<unsigned,2>,3> triangleEdges={{{{0,1}},{{1,2}},{{0,2}}}};
  for(auto e:edges)for(auto t:triangleEdges)if(!overlaps(Cross(Sub(a[e[1]],a[e[0]]),Sub(b[t[1]],b[t[0]]))))return false;
  return true;
 }catch(const contact_geometry::ArithmeticFailure&){return false;}
}
} // namespace rounded_episode_probe
