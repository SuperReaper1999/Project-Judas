#include "CollisionGeometry.h"
#include "CollisionAsset.h"
#include "RadialTerrain.h"
#include "ReentrantQueryScratch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace {
using V=glm::dvec3;using M=glm::dmat3;
thread_local CollisionGeometryStats stats;
constexpr double eps=1e-10;
V unit(V v,V fallback=V(1,0,0)){double l=glm::length(v);return l>eps?v/l:fallback;}
struct Poly {std::vector<V> points;std::vector<std::vector<unsigned>> faces;std::vector<V> normals;std::vector<std::pair<unsigned,unsigned>> edges;std::vector<unsigned> satEdges;bool triangle=false;std::array<bool,3> active{{true,true,true}};};
V nearestTriangle(V p,V a,V b,V c) {
 const V ab=b-a,ac=c-a,ap=p-a;const double d1=glm::dot(ab,ap),d2=glm::dot(ac,ap);
 if(d1<=0&&d2<=0)return a;
 V bp=p-b;double d3=glm::dot(ab,bp),d4=glm::dot(ac,bp);if(d3>=0&&d4<=d3)return b;
 double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+(d1/(d1-d3))*ab;
 V cp=p-c;double d5=glm::dot(ab,cp),d6=glm::dot(ac,cp);if(d6>=0&&d5<=d6)return c;
 double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+(d2/(d2-d6))*ac;
 double va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return b+((d4-d3)/((d4-d3)+(d5-d6)))*(c-b);
 double inv=1/(va+vb+vc);return a+ab*(vb*inv)+ac*(vc*inv);
}
void segmentPair(V a,V b,V c,V d,V& p,V& q) {
 V v=b-a,w=d-c,r=a-c;double A=glm::dot(v,v),B=glm::dot(v,w),C=glm::dot(w,w),D=glm::dot(v,r),E=glm::dot(w,r),s=0,t=0;
 if(A<=eps){t=C>eps?std::clamp(E/C,0.,1.):0;}else if(C<=eps)s=std::clamp(-D/A,0.,1.);
 else {double den=A*C-B*B;s=den>eps?std::clamp((B*E-C*D)/den,0.,1.):0;t=(B*s+E)/C;if(t<0){t=0;s=std::clamp(-D/A,0.,1.);}else if(t>1){t=1;s=std::clamp((B-D)/A,0.,1.);}}
 p=a+s*v;q=c+t*w;
}
std::pair<V,V> segmentTriangle(V a,V b,V x,V y,V z) {
 V n=unit(glm::cross(y-x,z-x));double da=glm::dot(n,a-x),db=glm::dot(n,b-x);
 if(da*db<=0&&std::abs(da-db)>eps){V p=a+(b-a)*(da/(da-db));V q=nearestTriangle(p,x,y,z);if(glm::length(p-q)<eps)return {p,p};}
 V p=a,q=nearestTriangle(a,x,y,z);double best=glm::dot(p-q,p-q);
 auto test=[&](V u,V v){double dist=glm::dot(u-v,u-v);if(dist<best){best=dist;p=u;q=v;}};
 test(b,nearestTriangle(b,x,y,z));for(auto edge:std::array<std::pair<V,V>,3>{{{x,y},{y,z},{z,x}}}){V u,v;segmentPair(a,b,edge.first,edge.second,u,v);test(u,v);}return {p,q};
}

struct PolyScratchFrame {Poly first,second,triangle;std::vector<V> polygon,clipped;};
using PolyScratchLease=ReentrantQueryScratch<PolyScratchFrame>;
void polyInto(Poly& p,const PrimitivePose& pose,V origin=V(0)) {
 p.triangle=false;p.points.clear();p.normals.clear();p.edges.clear();p.satEdges.clear();
 const M r=PrimitiveRotation(pose);const V center=PrimitiveCenter(pose)-origin;
 if(pose.shape.type==ShapeType::Box){
  p.points.reserve(8);p.normals.reserve(6);V h(pose.shape.halfExtents);
  for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1})p.points.push_back(center+r*V(x*h.x,y*h.y,z*h.z));
  static constexpr unsigned faces[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
  p.faces.resize(6);for(unsigned i=0;i<6;++i)p.faces[i].assign(std::begin(faces[i]),std::end(faces[i]));
 }else if(pose.shape.type==ShapeType::ConvexHull&&pose.shape.asset){
  const auto& asset=*pose.shape.asset;
  for(V v:asset.vertices)p.points.push_back(center+r*v);
  p.faces.resize(asset.polygons.size());
  for(unsigned i=0;i<asset.polygons.size();++i){const auto& f=asset.polygons[i];p.faces[i].assign(f.vertices.begin(),f.vertices.end());p.normals.push_back(r*f.normal);}
  for(auto e:asset.edges)p.edges.push_back({e[0],e[1]});
  p.satEdges.assign(asset.satEdges.begin(),asset.satEdges.end());
  if(p.satEdges.empty())for(unsigned i=0;i<p.edges.size();++i)p.satEdges.push_back(i);
  return;
 }else throw std::invalid_argument("poly geometry requires box or cooked convex hull");
 for(const auto& f:p.faces)p.normals.push_back(unit(glm::cross(p.points[f[1]]-p.points[f[0]],p.points[f[2]]-p.points[f[0]])));
 static constexpr std::array<std::pair<unsigned,unsigned>,12> edges{{{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}}};
 p.edges.assign(edges.begin(),edges.end());p.satEdges={0,1,2};
}
// Writes the same triangle Poly as triangle() into storage reused across a mesh's
// candidates, so a contact query does not allocate per triangle. `r` and `center`
// are that function's PrimitiveRotation(pose) and PrimitiveCenter(pose)-origin.
void triangleInto(Poly& p,const CollisionAsset& a,unsigned feature,const M& r,V center) {
 const auto& f=a.faces[feature];p.triangle=true;p.active=f.active;
 p.points.resize(3);for(unsigned k=0;k<3;++k)p.points[k]=center+r*a.vertices[f.vertices[k]];
 p.faces.resize(1);p.faces[0].assign({0,1,2});p.normals.assign(1,r*f.normal);p.edges.assign({{0,1},{1,2},{0,2}});p.satEdges={0,1,2};
}
void reverseTriangleSide(Poly& p,const CollisionAsset& asset,unsigned feature) {
 // Convex and concave creases exchange roles when viewing a two-sided sheet
 // from behind. Coplanar internal edges remain inactive on both sides.
 const auto& face=asset.faces[feature];
 for(unsigned e=0;e<3;++e)if(face.adjacent[e]>=0){
  const auto& neighbor=asset.faces[face.adjacent[e]];
  for(auto v:neighbor.vertices)if(v!=face.vertices[e]&&v!=face.vertices[(e+1)%3]){
   const double side=glm::dot(face.normal,asset.vertices[v]-asset.vertices[face.vertices[0]]);
   p.active[e]=side>1e-9&&glm::dot(face.normal,neighbor.normal)<1-1e-10;
  }
 }
 p.normals[0]=-p.normals[0];std::reverse(p.faces[0].begin(),p.faces[0].end());
}
struct Separation {double gap=-INFINITY;V normal{0};bool edge=false;};
// True when one face normal of `a` or `b` alone separates them by more than `margin`.
// Face normals are always allowed sat() axes and this projection is sat()'s own, so
// then sat(a,b).gap >= that gap > margin: the caller's rejection is unchanged, only
// reached without the edge-pair axes. Anything else falls through to the full sat().
bool faceAxesSeparate(const Poly& a,const Poly& b,double margin) {
 for(const Poly* owner:{&a,&b})for(V n:owner->normals){double len=glm::length(n);if(len<eps)continue;n/=len;double alo=INFINITY,ahi=-INFINITY,blo=INFINITY,bhi=-INFINITY;
  for(V p:a.points){double x=glm::dot(p,n);alo=std::min(alo,x);ahi=std::max(ahi,x);}for(V p:b.points){double x=glm::dot(p,n);blo=std::min(blo,x);bhi=std::max(bhi,x);}double gap=alo-bhi;if(blo-ahi>gap)gap=blo-ahi;
  if(gap>margin)return true;}
 return false;
}
Separation sat(const Poly& a,const Poly& b,bool edgeAware=true) {
 ++stats.convexTests;Separation best;double actual=-INFINITY;
 auto axis=[&](V n,bool allowed,bool edge){double len=glm::length(n);if(len<eps)return;n/=len;double alo=INFINITY,ahi=-INFINITY,blo=INFINITY,bhi=-INFINITY;
  for(V p:a.points){double x=glm::dot(p,n);alo=std::min(alo,x);ahi=std::max(ahi,x);}for(V p:b.points){double x=glm::dot(p,n);blo=std::min(blo,x);bhi=std::max(bhi,x);}double gap=alo-bhi;V direction=n;if(blo-ahi>gap){gap=blo-ahi;direction=-n;}actual=std::max(actual,gap);if(allowed&&gap>best.gap){best={gap,direction,edge};}};
 for(V n:a.normals)axis(n,true,false);
 for(V n:b.normals)axis(n,true,false);
 for(auto index:a.satEdges)for(unsigned i:b.satEdges){auto e=a.edges[index];auto f=b.edges[i];bool active=!edgeAware||!b.triangle||b.active[i==2?2:i];axis(glm::cross(a.points[e.second]-a.points[e.first],b.points[f.second]-b.points[f.first]),active,true);}
 // An inactive triangle edge still bounds its patch; it cannot supply a
 // blocking solver normal. Positive separation remains a real geometric miss.
 if(actual>0&&actual>best.gap)best.gap=actual;
 return best;
}

void clipInto(const std::vector<V>& points,V normal,double plane,std::vector<V>& out) {
 out.clear();if(points.empty())return;V previous=points.back();double before=glm::dot(normal,previous)-plane;
 for(V current:points){const double now=glm::dot(normal,current)-plane;
  if((before<=eps)!=(now<=eps))out.push_back(previous+(current-previous)*(before/(before-now)));
  if(now<=eps)out.push_back(current);
  previous=current;before=now;
 }
}
template<class Consumer>
bool visitWitnesses(const Poly& a,const Poly& b,const Separation& s,PolyScratchFrame& scratch,Consumer consume) {
 unsigned reference=0,incident=0;double alignment=-INFINITY,opposite=INFINITY;
 for(unsigned i=0;i<b.normals.size();++i)if(glm::dot(b.normals[i],s.normal)>alignment){alignment=glm::dot(b.normals[i],s.normal);reference=i;}
 for(unsigned i=0;i<a.normals.size();++i)if(glm::dot(a.normals[i],s.normal)<opposite){opposite=glm::dot(a.normals[i],s.normal);incident=i;}
 auto& polygon=scratch.polygon;auto& clipped=scratch.clipped;
 polygon.clear();
 if(alignment>1-1e-6&&!s.edge){
  const auto& face=b.faces[reference];const V n=b.normals[reference];for(auto v:a.faces[incident])polygon.push_back(a.points[v]);
  for(unsigned i=0;i<face.size();++i){const V x=b.points[face[i]],y=b.points[face[(i+1)%face.size()]];const V side=unit(glm::cross(y-x,n));clipInto(polygon,side,glm::dot(side,x),clipped);polygon.swap(clipped);}
  const double plane=glm::dot(n,b.points[face[0]]);
  if(!polygon.empty()){for(V p:polygon)if(consume(p,p-n*(glm::dot(n,p)-plane),true))return true;return false;}
 }
 if(!s.edge&&!a.triangle&&!b.triangle&&opposite< -1+1e-6){
  const auto& face=a.faces[incident];const V n=a.normals[incident];
  for(auto v:b.faces[reference])polygon.push_back(b.points[v]);
  for(unsigned i=0;i<face.size();++i){const V x=a.points[face[i]],y=a.points[face[(i+1)%face.size()]];const V side=unit(glm::cross(y-x,n));clipInto(polygon,side,glm::dot(side,x),clipped);polygon.swap(clipped);}
  const double plane=glm::dot(n,a.points[face[0]]);
  if(!polygon.empty()){for(V p:polygon)if(consume(p-n*(glm::dot(n,p)-plane),p,true))return true;return false;}
 }
 double supportA=INFINITY,supportB=-INFINITY;V pa=a.points[0],pb=b.points[0];
 for(V p:a.points)if(glm::dot(p,s.normal)<supportA){supportA=glm::dot(p,s.normal);pa=p;}
 for(V p:b.points)if(glm::dot(p,s.normal)>supportB){supportB=glm::dot(p,s.normal);pb=p;}
 double nearest=INFINITY;
 for(auto e:a.edges)for(auto f:b.edges){V x,y;segmentPair(a.points[e.first],a.points[e.second],b.points[f.first],b.points[f.second],x,y);const double d=glm::dot(x-y,x-y);if(d<nearest){nearest=d;pa=x;pb=y;}}
 return consume(pa,pb,false);
}
std::vector<unsigned> candidates(const PrimitivePose& target,V lo,V hi,double radius) {
 std::vector<uint32_t> list;const auto& asset=*target.shape.asset;
 if(!std::isfinite(radius)){list.resize(asset.faces.size());for(unsigned i=0;i<list.size();++i)list[i]=i;return list;}
 M inverse=glm::transpose(PrimitiveRotation(target));V center=PrimitiveCenter(target),localLo(INFINITY),localHi(-INFINITY);
 lo-=V(radius);hi+=V(radius);for(int x:{0,1})for(int y:{0,1})for(int z:{0,1}){V p=inverse*(V(x?hi.x:lo.x,y?hi.y:lo.y,z?hi.z:lo.z)-center);localLo=glm::min(localLo,p);localHi=glm::max(localHi,p);}asset.Candidates(localLo,localHi,list,&stats.nodes);return list;
}
Contact make(const PrimitivePose& a,const PrimitivePose& b,V pa,V pb,V normal,double gap) {
 Contact c;c.hit=true;c.signedSeparation=gap;c.penetration=-gap;c.normal=glm::vec3(normal);c.preciseNormal=normal;c.point=glm::vec3((pa+pb)*.5);c.hasLocalAnchors=true;V midpoint=(pa+pb)*.5;
 M ra=glm::transpose(ContactRotation(a.parentOrientation)),rb=glm::transpose(ContactRotation(b.parentOrientation));c.localAnchorA=ra*(midpoint-V(a.parentPosition));c.localAnchorB=rb*(midpoint-V(b.parentPosition));c.localWitnessA=ra*(pa-V(a.parentPosition));c.localWitnessB=rb*(pb-V(b.parentPosition));c.separationState=gap>0?SeparationState::Separated:gap<0?SeparationState::Penetrating:SeparationState::Touching;return c;
}
void reduce(std::vector<Contact>& contacts,ContactManifold& out) {
 std::stable_sort(contacts.begin(),contacts.end(),[](const Contact& a,const Contact& b){return a.signedSeparation<b.signedSeparation;});if(contacts.empty())return;
 // Deepest first, then maximize coverage. Deduplicate shared-face witnesses.
 std::vector<unsigned> chosen;chosen.push_back(0);while(chosen.size()<4){double best=1e-12;unsigned selected=UINT32_MAX;for(unsigned i=0;i<contacts.size();++i){double score=INFINITY;for(auto j:chosen)score=std::min(score,glm::dot(contacts[i].localAnchorB-contacts[j].localAnchorB,contacts[i].localAnchorB-contacts[j].localAnchorB));if(score>best){best=score;selected=i;}}if(selected==UINT32_MAX)break;chosen.push_back(selected);}for(auto i:chosen)out.Add(contacts[i]);
}
// Shared accepted-witness traversal. A full manifold consumer visits every
// witness in the original deterministic order. An existence-only consumer may
// stop after the first accepted witness; it never changes SAT, clipping, margins,
// sidedness, crease handling or the contacts used by a physical solver.
template<class Consumer>
bool visitCookedPolyContacts(const PrimitivePose& a,const PrimitivePose& b,double margin,Consumer consume,double penetrationThreshold=INFINITY,const MeshContactEpisode* episode=nullptr,const std::vector<uint32_t>* excluded=nullptr,std::vector<uint32_t>* features=nullptr) {
 PolyScratchLease lease;auto& scratch=lease.frame;V origin=PrimitiveCenter(b);auto& pa=scratch.first;polyInto(pa,a,origin);uint32_t triangleFeature=UINT32_MAX;auto test=[&](const Poly& pb){
  // A face axis is already a valid SAT axis. Reject shallow convex pairs
  // before enumerating edge axes/clipping; preserve the original full path
  // near the threshold and for every concave triangle response.
  if(!pb.triangle){
   double guard=eps;
   if(std::isfinite(penetrationThreshold)){
    for(V p:pa.points)guard=std::max(guard,eps*glm::length(p));
    for(V p:pb.points)guard=std::max(guard,eps*glm::length(p));
   }
   const double reject=std::isfinite(penetrationThreshold)?-penetrationThreshold+guard:margin;
   if(faceAxesSeparate(pa,pb,reject)){++stats.convexTests;return false;}
  }
  auto s=sat(pa,pb);
  // SAT separation is authoritative. Choosing a response normal must never
  // turn an edge-separated finite triangle into an overlapping infinite plane.
  if(s.gap>margin)return false;
  // Deep in a thin body, a least-overlap axis dominated by the backward face
  // component can drive it through the sheet. Guard that rear 45-degree cone
  // (backward component > total tangential component), using the selected
  // front, or reversed front of a two-sided sheet. Predominantly lateral axes
  // remain real finite-edge responses: a small backward component must not
  // replace a shallow sideways overlap with much deeper face penetration.
  // This is a bounded response policy, not a unique continuum contact law.
  if(pb.triangle&&s.gap<=0&&glm::dot(s.normal,pb.normals[0])<-std::sqrt(.5)){
   const V n=unit(pb.normals[0]);double alo=INFINITY;for(V p:pa.points)alo=std::min(alo,glm::dot(p,n));
   double bhi=-INFINITY;for(V p:pb.points)bhi=std::max(bhi,glm::dot(p,n));
   s={alo-bhi,n,false};
  }
  return visitWitnesses(pa,pb,s,scratch,[&](V x,V y,bool faceWitness){
   const double gap=faceWitness?glm::dot(s.normal,x-y):s.gap;
   if(gap<=margin){
    if(features)features->push_back(triangleFeature);
    return consume(x+origin,y+origin,s.normal,gap,triangleFeature);
   }
   return false;
  });
 };
 if(b.shape.type==ShapeType::ConvexHull||b.shape.type==ShapeType::Box){polyInto(scratch.second,b,origin);return test(scratch.second);}
 if(b.shape.type==ShapeType::TriangleMesh){V lo(INFINITY),hi(-INFINITY),center(0);for(V p:pa.points){lo=glm::min(lo,p+origin);hi=glm::max(hi,p+origin);center+=p;}center/=double(pa.points.size());
  // Most candidates of a dense mesh are rejected by a face axis; skip their edge axes
  // (counted as the same convex test) and reuse one triangle's storage throughout.
  const auto& asset=*b.shape.asset;const M rotation=PrimitiveRotation(b);const V placed=PrimitiveCenter(b)-origin;auto& pb=scratch.triangle;
  const auto selected=candidates(b,lo,hi,margin);
  std::vector<uint32_t> connected;
  bool needsOwnership=false;
  if(episode&&!episode->fronts.empty())for(auto feature:selected){const auto& face=asset.faces[feature];const V normal=rotation*face.normal,point=placed+rotation*asset.vertices[face.vertices[0]];if(glm::dot(normal,center-point)<0){needsOwnership=true;break;}}
  if(needsOwnership){
   auto overlaps=[&](uint32_t feature){triangleInto(pb,asset,feature,rotation,placed);return !faceAxesSeparate(pa,pb,0)&&sat(pa,pb).gap<=0;};
   for(auto feature:episode->fronts)if(std::binary_search(selected.begin(),selected.end(),feature)&&overlaps(feature))connected.push_back(feature);
   auto edgeInside=[&](V x,V y){double enter=0,leave=1;for(unsigned face=0;face<pa.faces.size();++face){const V n=pa.normals[face];const auto& indices=pa.faces[face];const double from=glm::dot(n,x-pa.points[indices[0]]),rate=glm::dot(n,y-x);if(rate==0){if(from>eps)return false;}else{const double at=-from/rate;if(rate<0)enter=std::max(enter,at);else leave=std::min(leave,at);if(enter>leave)return false;}}return true;};
   for(std::size_t at=0;at<connected.size();++at){const auto& face=asset.faces[connected[at]];for(unsigned edge=0;edge<3;++edge){const auto neighbor=face.adjacent[edge];if(neighbor<0||std::find(connected.begin(),connected.end(),uint32_t(neighbor))!=connected.end()||!std::binary_search(selected.begin(),selected.end(),uint32_t(neighbor)))continue;
    const V x=placed+rotation*asset.vertices[face.vertices[edge]],y=placed+rotation*asset.vertices[face.vertices[(edge+1)%3]];
    if(edgeInside(x,y)&&overlaps(uint32_t(neighbor)))connected.push_back(uint32_t(neighbor));}}
   std::sort(connected.begin(),connected.end());
  }
  for(auto i:selected){
   if(excluded&&std::binary_search(excluded->begin(),excluded->end(),i))continue;
   triangleFeature=i;
   ++stats.triangles;triangleInto(pb,asset,i,rotation,placed);
   const double side=glm::dot(pb.normals[0],center-pb.points[0]);
   const bool owned=std::binary_search(connected.begin(),connected.end(),i);
   if(!asset.twoSided&&side<-eps&&!owned)continue;
   if(asset.twoSided&&side<0)reverseTriangleSide(pb,asset,i);
   if(faceAxesSeparate(pa,pb,margin)){++stats.convexTests;continue;}
   if(test(pb))return true;}return false;}
 throw std::invalid_argument("unsupported cooked contact target");
}
}
Shape Shape::Cooked(std::shared_ptr<const CollisionAsset> data,std::string id){if(!data)throw std::invalid_argument("missing cooked collision asset");Shape s;s.type=data->convex?ShapeType::ConvexHull:ShapeType::TriangleMesh;s.asset=std::move(data);s.assetId=std::move(id);return s;}
CollisionGeometryStats GetCollisionGeometryStats(){return stats;}void ResetCollisionGeometryStats(){stats={};}void CountCollisionQuery(uint64_t nodes,uint64_t triangles){stats.nodes+=nodes;stats.triangles+=triangles;}
M PrimitiveRotation(const PrimitivePose& p){return ContactRotation(p.parentOrientation)*ContactRotation(p.childRotation);}
V PrimitiveCenter(const PrimitivePose& p){return V(p.parentPosition)+ContactRotation(p.parentOrientation)*V(p.parentLocalCenter);}
GeometryDistance SegmentGeometry(V a,V b,double radius,const PrimitivePose& target,double range,bool nearestSurface,uint32_t feature) {
 GeometryDistance out;M r=PrimitiveRotation(target),inverse=glm::transpose(r);V center=PrimitiveCenter(target),x=inverse*(a-center),y=inverse*(b-center);
 if(target.shape.type==ShapeType::Capsule){
  V u,v;segmentPair(x,y,V(0,-target.shape.halfHeight,0),V(0,target.shape.halfHeight,0),u,v);
  double length=glm::length(u-v);V normal=unit(u-v);
  double gap=length-radius-target.shape.radius;
  return {gap<=range,length<=target.shape.radius,gap,center+r*(v+normal*double(target.shape.radius)),center+r*u,r*normal,0,length>eps};
 }
 if(!target.shape.asset)return out;
 const auto& asset=*target.shape.asset;
 bool inside=asset.convex;
 if(inside)for(const auto& f:asset.faces)if(glm::dot(f.normal,x-asset.vertices[f.vertices[0]])>eps){inside=false;break;}
 auto list=feature==UINT32_MAX?candidates(target,glm::min(a,b),glm::max(a,b),inside?INFINITY:radius+range):std::vector<unsigned>{feature};
 for(unsigned i:list){++stats.triangles;auto& f=asset.faces[i];V v=asset.vertices[f.vertices[0]],w=asset.vertices[f.vertices[1]],z=asset.vertices[f.vertices[2]];
  if(!nearestSurface&&!asset.convex&&!asset.twoSided&&glm::dot(f.normal,(x+y)*.5-v)<-eps)continue;
  auto [p,q]=x==y?std::pair<V,V>{x,nearestTriangle(x,v,w,z)}:segmentTriangle(x,y,v,w,z);double length=glm::length(p-q),gap=length-radius;V normal=length>eps?(p-q)/length:f.normal;
  if(inside){gap=-length-radius;normal=f.normal;}
  if(out.valid&&std::abs(gap-out.gap)<eps&&glm::dot(out.normal,r*normal)<1-1e-7)out.normalUnique=false;
  else if(!out.valid||(inside?gap>out.gap:gap<out.gap)){out={true,inside,gap,center+r*q,center+r*p,r*normal,i};}
 }
 if(out.valid&&out.gap>range)out.valid=false;
 return out;
}
GeometryDistance PointGeometry(V point,const PrimitivePose& target,double range){
 if(target.shape.asset){auto out=SegmentGeometry(point,point,0,target,range,true);if(out.contains)out.gap=std::abs(out.gap);if(out.gap>range)out.valid=false;return out;}
 M r=PrimitiveRotation(target);V center=PrimitiveCenter(target),p=glm::transpose(r)*(point-center),q(0),n(0);bool inside=false;
 if(target.shape.type==ShapeType::Sphere){double length=glm::length(p);n=unit(p);q=n*double(target.shape.radius);inside=length<=target.shape.radius;}
 else if(target.shape.type==ShapeType::Capsule){V axis(0,std::clamp(p.y,-double(target.shape.halfHeight),double(target.shape.halfHeight)),0);V offset=p-axis;double length=glm::length(offset);n=unit(offset);q=axis+n*double(target.shape.radius);inside=length<=target.shape.radius;}
 else if(target.shape.type==ShapeType::Box){V h(target.shape.halfExtents);inside=glm::all(glm::lessThanEqual(glm::abs(p),h));q=glm::clamp(p,-h,h);if(inside){V depth=h-glm::abs(p);int axis=depth.y<depth.x?1:0;if(depth.z<depth[axis])axis=2;n[axis]=p[axis]>=0?1:-1;q[axis]=n[axis]*h[axis];}else n=unit(p-q);}
 else if(target.shape.type==ShapeType::Terrain&&target.shape.terrain){auto sample=target.shape.terrain->Sample(glm::vec3(p));q=sample.surfacePoint;n=sample.outwardNormal;inside=sample.signedDistance<=0;}
 else return {};
 double distance=glm::length(p-q);GeometryDistance result{distance<=range,inside,distance,center+r*q,point,r*n,0};
 if((target.shape.type==ShapeType::Sphere&&glm::length(p)<eps)||(target.shape.type==ShapeType::Capsule&&glm::length(V(p.x,p.y-std::clamp(p.y,-double(target.shape.halfHeight),double(target.shape.halfHeight)),p.z))<eps))result.normalUnique=false;
 if(target.shape.type==ShapeType::Box&&inside){auto depth=V(target.shape.halfExtents)-glm::abs(p);double minimum=std::min({depth.x,depth.y,depth.z});unsigned tied=0;for(int i=0;i<3;++i)if(std::abs(depth[i]-minimum)<eps)++tied;result.normalUnique=tied==1;}
 return result;
}
GeometryDistance PolyGeometry(const PrimitivePose& query,const PrimitivePose& target,double range,uint32_t feature) {
 PolyScratchLease lease;auto& scratch=lease.frame;auto& a=scratch.first;
 GeometryDistance out;const V origin=PrimitiveCenter(target);polyInto(a,query,origin);
 auto distance=[&](const Poly& b,const Separation& separation,unsigned face){
  GeometryDistance value;
  visitWitnesses(a,b,separation,scratch,[&](V first,V second,bool){value={true,separation.gap<0,separation.gap,second+origin,first+origin,separation.normal,face};return true;});
  return value;
 };
 if(target.shape.type==ShapeType::ConvexHull||target.shape.type==ShapeType::Box){polyInto(scratch.second,target,origin);return distance(scratch.second,sat(a,scratch.second),0);}
 if(target.shape.type!=ShapeType::TriangleMesh||!target.shape.asset)return out;
 V lo(INFINITY),hi(-INFINITY),center(0);for(V p:a.points){lo=glm::min(lo,p+origin);hi=glm::max(hi,p+origin);center+=p;}center/=double(a.points.size());
 const auto list=feature==UINT32_MAX?candidates(target,lo,hi,range):std::vector<unsigned>{feature};
 const M rotation=PrimitiveRotation(target);auto& b=scratch.triangle;
 for(auto i:list){++stats.triangles;triangleInto(b,*target.shape.asset,i,rotation,V(0));const double side=glm::dot(b.normals[0],center-b.points[0]);
  if(!target.shape.asset->twoSided&&side<-eps)continue;
  if(side<0)reverseTriangleSide(b,*target.shape.asset,i);
  const auto separation=sat(a,b,false);if(!out.valid||separation.gap<out.gap){out=distance(b,separation,i);out.contains=false;}
 }
 return out;
}
double MeshSeparationAdvance(const PrimitivePose& query,const PrimitivePose& mesh,
                             V velocity,V angularGenerator,double rotationRadius,double duration,const std::vector<uint32_t>* excluded,const MeshContactEpisode* episode) {
 if(mesh.shape.type!=ShapeType::TriangleMesh||!mesh.shape.asset||
    (query.shape.type!=ShapeType::Box&&query.shape.type!=ShapeType::ConvexHull))
  throw std::invalid_argument("mesh advancement requires a convex polyhedron and triangle mesh");
 PolyScratchLease lease;auto& a=lease.frame.first;const V origin=PrimitiveCenter(mesh);polyInto(a,query,origin);
 V lo(INFINITY),hi(-INFINITY),center(0);
 for(V p:a.points){lo=glm::min(lo,p+origin);hi=glm::max(hi,p+origin);center+=p;}
 center/=double(a.points.size());
 const V displacement=velocity*duration;
 lo=glm::min(lo,lo+displacement);hi=glm::max(hi,hi+displacement);
 const double angular=glm::length(angularGenerator);
 const auto selected=candidates(mesh,lo,hi,angular*rotationRadius*duration);
 const auto& asset=*mesh.shape.asset;const M rotation=PrimitiveRotation(mesh);
 auto& triangleStorage=lease.frame.triangle;double earliest=INFINITY;
 for(unsigned feature:selected){
  if(excluded&&std::binary_search(excluded->begin(),excluded->end(),feature))continue;
  ++stats.triangles;++stats.convexTests;
  triangleInto(triangleStorage,asset,feature,rotation,V(0));
  const auto& b=triangleStorage;
  if(!asset.twoSided&&glm::dot(b.normals[0],center-b.points[0])<-eps&&!(episode&&!episode->fronts.empty()))continue;
  double enter=0,leave=duration;
  auto axis=[&](V n){
   const double length=glm::length(n);if(length<eps)return true;n/=length;
   double alo=INFINITY,ahi=-INFINITY,blo=INFINITY,bhi=-INFINITY;
   for(V p:a.points){const double x=glm::dot(p,n);alo=std::min(alo,x);ahi=std::max(ahi,x);}
   for(V p:b.points){const double x=glm::dot(p,n);blo=std::min(blo,x);bhi=std::max(bhi,x);}
   const double rate=glm::dot(velocity,n);
   if(angular==0){
    // Exact swept SAT for translation. Intersect entry AND exit intervals,
    // including finite edge axes; extended triangle planes cannot block a miss.
    if(rate==0)return alo<=bhi&&blo<=ahi;
    const double first=(rate>0?blo-ahi:bhi-alo)/rate;
    const double last=(rate>0?bhi-alo:blo-ahi)/rate;
    enter=std::max(enter,first);leave=std::min(leave,last);
    return enter<=leave;
   }
   // A fixed world axis remains a valid separating axis while the body turns.
   // Rotation about that axis cannot close its gap. The maximum projected
   // rotational speed is |n x omega| R, including off-centre compound children.
   const double rotationSpeed=glm::length(glm::cross(n,angularGenerator))*rotationRadius;
   double gap=alo-bhi,closing=-rate+rotationSpeed;
   if(blo-ahi>gap){gap=blo-ahi;closing=rate+rotationSpeed;}
   if(gap<=0)return true;
   if(!(closing>0))return false;
   enter=std::max(enter,gap/closing);
   return enter<=leave;
  };
  bool possible=true;
  for(V normal:b.normals)if(!axis(normal)){possible=false;break;}
  if(possible)for(V normal:a.normals)if(!axis(normal)){possible=false;break;}
  if(possible)for(unsigned first:a.satEdges){const auto e=a.edges[first];
   for(unsigned second:b.satEdges){const auto f=b.edges[second];
    if(!axis(glm::cross(a.points[e.second]-a.points[e.first],b.points[f.second]-b.points[f.first]))){possible=false;break;}}
   if(!possible)break;
  }
  if(possible&&leave>=0)earliest=std::min(earliest,std::nextafter(std::max(0.,enter),0.));
 }
 return earliest;
}
ContactManifold EpisodeCookedContacts(const PrimitivePose& a,const PrimitivePose& b,double margin,const MeshContactEpisode* episode) {
 const bool newA=a.shape.asset!=nullptr,newB=b.shape.asset!=nullptr;
 if(!newA&&!newB)throw std::invalid_argument("CookedContacts called without cooked geometry");
 if(a.shape.type==ShapeType::TriangleMesh){auto out=EpisodeCookedContacts(b,a,margin,episode);for(int i=0;i<out.count;++i){auto& c=out.points[i];c.normal=-c.normal;c.preciseNormal=-c.preciseNormal;std::swap(c.localAnchorA,c.localAnchorB);std::swap(c.localWitnessA,c.localWitnessB);}return out;}
 ContactManifold out;std::vector<Contact> contacts;
 if(a.shape.type==ShapeType::Sphere||a.shape.type==ShapeType::Capsule){M r=PrimitiveRotation(a);V center=PrimitiveCenter(a);double half=a.shape.type==ShapeType::Capsule?a.shape.halfHeight:0;auto d=SegmentGeometry(center-r[1]*half,center+r[1]*half,a.shape.radius,b,margin);if(d.valid&&d.gap<=margin)out.Add(make(a,b,d.queryPoint-d.normal*double(a.shape.radius),d.point,d.normal,d.gap));return out;}
 if(b.shape.type==ShapeType::Sphere||b.shape.type==ShapeType::Capsule){auto result=EpisodeCookedContacts(b,a,margin,episode);for(int i=0;i<result.count;++i){auto& c=result.points[i];c.normal=-c.normal;c.preciseNormal=-c.preciseNormal;std::swap(c.localAnchorA,c.localAnchorB);std::swap(c.localWitnessA,c.localWitnessB);}return result;}
 if(a.shape.type!=ShapeType::Box&&a.shape.type!=ShapeType::ConvexHull)throw std::invalid_argument("unsupported cooked contact pair");
 visitCookedPolyContacts(a,b,margin,[&](V x,V y,V normal,double gap,uint32_t){
  contacts.push_back(make(a,b,x,y,normal,gap));return false;
 },INFINITY,episode);
 reduce(contacts,out);return out;
}
// Boolean truth for CCD observations, not a replacement response manifold.
MeshFeatureBudget BuildMeshFeatureBudget(const PrimitivePose& query,const PrimitivePose& mesh,double amount,V velocity,V angular,double radius,double duration,const MeshContactEpisode* episode) {
 MeshFeatureBudget result;std::vector<std::pair<uint32_t,double>> actual;
 visitCookedPolyContacts(query,mesh,0,[&](V,V,V,double gap,uint32_t feature){actual.push_back({feature,-gap});return false;},INFINITY,episode);
 PolyScratchLease lease;auto& pa=lease.frame.first;const V origin=PrimitiveCenter(mesh);polyInto(pa,query,origin);V lo(INFINITY),hi(-INFINITY),center(0);
 for(V p:pa.points){lo=glm::min(lo,p+origin);hi=glm::max(hi,p+origin);center+=p;}center/=double(pa.points.size());const V delta=velocity*duration;lo=glm::min(lo,lo+delta);hi=glm::max(hi,hi+delta);
 const auto selected=candidates(mesh,lo,hi,glm::length(angular)*radius*duration);const auto& asset=*mesh.shape.asset;const M rotation=PrimitiveRotation(mesh);
 for(auto feature:selected){const auto& face=asset.faces[feature];const V n=rotation*face.normal;const double plane=glm::dot(n,rotation*asset.vertices[face.vertices[0]]);double bottom=INFINITY;
  for(V p:pa.points)bottom=std::min(bottom,glm::dot(n,p));const double depth=plane-bottom,half=glm::dot(n,center)-bottom;
  double limit=std::min(std::max(0.,depth)+amount,.5*half);bool found=false;
  for(const auto& old:actual)if(old.first==feature){if(!found){limit=old.second+amount;found=true;}else limit=std::max(limit,old.second+amount);}
  result.limits.push_back({feature,limit});
 }
 return result;
}
bool MeshFeatureBudgetExceeded(const PrimitivePose& query,const PrimitivePose& mesh,const MeshFeatureBudget& budget,const MeshContactEpisode* episode) {
 return visitCookedPolyContacts(query,mesh,0,[&](V,V,V,double gap,uint32_t feature){const auto found=std::lower_bound(budget.limits.begin(),budget.limits.end(),feature,[](const auto& entry,uint32_t id){return entry.first<id;});return found!=budget.limits.end()&&found->first==feature&&-gap>found->second;},INFINITY,episode);
}
ContactManifold CookedContacts(const PrimitivePose& a,const PrimitivePose& b,double margin) { return EpisodeCookedContacts(a,b,margin,nullptr); }
std::vector<uint32_t> MeshContactFeatures(const PrimitivePose& query,const PrimitivePose& mesh,const MeshContactEpisode* episode) {
 std::vector<uint32_t> result;visitCookedPolyContacts(query,mesh,0,[](V,V,V,double,uint32_t){return false;},INFINITY,episode,nullptr,&result);
 std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
bool MeshEpisodeHasContacts(const PrimitivePose& query,const PrimitivePose& mesh,const MeshContactEpisode* episode,const std::vector<uint32_t>* excluded) {
 return visitCookedPolyContacts(query,mesh,0,[](V,V,V,double,uint32_t){return true;},INFINITY,episode,excluded);
}
bool MeshEpisodeHasPenetrationBeyond(const PrimitivePose& query,const PrimitivePose& mesh,double threshold,const MeshContactEpisode* episode) {
 return visitCookedPolyContacts(query,mesh,0,[&](V,V,V,double gap,uint32_t){return -gap>threshold;},threshold,episode);
}
bool CookedHasContacts(const PrimitivePose& a,const PrimitivePose& b,double margin) {
 if(a.shape.type==ShapeType::TriangleMesh)return CookedHasContacts(b,a,margin);
 // Keep unoptimized sphere/capsule and unsupported cases on the same dispatcher.
 if(a.shape.type==ShapeType::Sphere||a.shape.type==ShapeType::Capsule||
    b.shape.type==ShapeType::Sphere||b.shape.type==ShapeType::Capsule)
  return CookedContacts(a,b,margin).count>0;
 if(!a.shape.asset&&!b.shape.asset)throw std::invalid_argument("CookedContacts called without cooked geometry");
 if(a.shape.type!=ShapeType::Box&&a.shape.type!=ShapeType::ConvexHull)throw std::invalid_argument("unsupported cooked contact pair");
 return visitCookedPolyContacts(a,b,margin,[](V,V,V,double,uint32_t){return true;});
}
bool CookedHasPenetrationBeyond(const PrimitivePose& a,const PrimitivePose& b,double threshold) {
 if(!std::isfinite(threshold)||threshold<0)throw std::invalid_argument("invalid penetration threshold");
 const auto polyType=[](ShapeType type){return type==ShapeType::Box||type==ShapeType::ConvexHull;};
 if(!polyType(a.shape.type)||!polyType(b.shape.type)){
  const auto manifold=CookedContacts(a,b,0);
  for(int k=0;k<manifold.count;++k)if(-manifold.points[k].signedSeparation>threshold)return true;
  return false;
 }
 // reduce() retains the deepest witness first. Existence over the same
 // accepted witnesses is therefore exactly the full manifold's maximum test.
 return visitCookedPolyContacts(a,b,0,[&](V,V,V,double gap,uint32_t){return -gap>threshold;},threshold);
}
CollisionMassProperties ShapeMassProperties(const Shape& shape) {
 CollisionMassProperties out;
 if(shape.type==ShapeType::ConvexHull){if(!shape.asset||!shape.asset->convex)throw std::invalid_argument("invalid convex mass geometry");return {shape.asset->volume,shape.asset->centerOfMass,shape.asset->unitInertia};}
 if(shape.type==ShapeType::Sphere){double r=shape.radius;out.volume=4*3.141592653589793*r*r*r/3;out.inertia=M(out.volume*2*r*r/5);}
 else if(shape.type==ShapeType::Box){V h(shape.halfExtents);out.volume=8*h.x*h.y*h.z;out.inertia[0][0]=out.volume*(h.y*h.y+h.z*h.z)/3;out.inertia[1][1]=out.volume*(h.x*h.x+h.z*h.z)/3;out.inertia[2][2]=out.volume*(h.x*h.x+h.y*h.y)/3;}
 else if(shape.type==ShapeType::CompoundBoxes){
  // Uniform density of authored children, SUMMED volumes, not Boolean union.
  // Overlapping children deliberately count twice in this mass representation.
  for(auto& child:shape.boxes){Shape primitive=child.type==ShapeType::Sphere?Shape::Sphere(child.radius):child.type==ShapeType::ConvexHull?Shape::Cooked(child.asset,child.assetId):Shape::Box(child.halfExtents);auto mass=ShapeMassProperties(primitive);M r=ContactRotation(child.rotation);V center=V(child.localCenter)+r*mass.center;out.volume+=mass.volume;out.center+=mass.volume*center;out.inertia+=r*mass.inertia*glm::transpose(r)+mass.volume*(glm::dot(center,center)*M(1)-glm::outerProduct(center,center));}
  if(!(out.volume>0))throw std::invalid_argument("compound has no mass volume");
  out.center/=out.volume;out.inertia-=out.volume*(glm::dot(out.center,out.center)*M(1)-glm::outerProduct(out.center,out.center));
 }else throw std::invalid_argument("unsupported dynamic mass geometry");
 if(!(out.volume>0)||!std::isfinite(out.volume)||!(glm::determinant(out.inertia)>0))throw std::invalid_argument("invalid/singular collision mass geometry");
 return out;
}
