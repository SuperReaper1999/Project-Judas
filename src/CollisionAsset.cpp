#include "CollisionAsset.h"
#include "ModelLoader.h"
#include "QuickHull.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <filesystem>
namespace {
using V=glm::dvec3;
bool finite(V p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
void build(CollisionAsset& a){
 a.minimum=a.maximum=a.vertices.at(0);for(auto p:a.vertices){a.minimum=glm::min(a.minimum,p);a.maximum=glm::max(a.maximum,p);}
 a.order.resize(a.faces.size());std::iota(a.order.begin(),a.order.end(),0);a.nodes.clear();
 auto recurse=[&](auto&& self,unsigned first,unsigned count)->unsigned{
  unsigned index=a.nodes.size();a.nodes.push_back({});CollisionNode node;node.first=first;node.count=count;node.minimum=V(INFINITY);node.maximum=V(-INFINITY);
  for(unsigned i=first;i<first+count;++i)for(auto v:a.faces[a.order[i]].vertices){node.minimum=glm::min(node.minimum,a.vertices[v]);node.maximum=glm::max(node.maximum,a.vertices[v]);}
  if(count>8){V span=node.maximum-node.minimum;int axis=span.y>span.x?1:0;if(span.z>span[axis])axis=2;
   std::stable_sort(a.order.begin()+first,a.order.begin()+first+count,[&](unsigned x,unsigned y){auto center=[&](unsigned f){auto& t=a.faces[f];return (a.vertices[t.vertices[0]][axis]+a.vertices[t.vertices[1]][axis]+a.vertices[t.vertices[2]][axis]);};double c=center(x),d=center(y);return c==d?x<y:c<d;});
   node.left=self(self,first,count/2);node.right=self(self,first+count/2,count-count/2);node.count=0;
  }a.nodes[index]=node;return index;
 };recurse(recurse,0,a.faces.size());
}
void topology(CollisionAsset& a,const MeshData* source=nullptr,CollisionDiagnostic* diagnostic=nullptr,const glm::dmat4& transform=glm::dmat4(1)){
 auto fail=[&](const char* code,const char* message,unsigned face,unsigned first,unsigned second,unsigned third=UINT32_MAX){
  CollisionDiagnostic d;d.code=code;d.sourceFace=a.faces.at(face).source;d.action="Select the reported source location; use explicit collision cleanup or a separate physical source. Visual import remains valid.";d.point=(a.vertices.at(first)+a.vertices.at(second))*.5;d.minimum=glm::min(a.vertices.at(first),a.vertices.at(second));d.maximum=glm::max(a.vertices.at(first),a.vertices.at(second));
  if(third!=UINT32_MAX){d.point=a.vertices.at(third);d.minimum=glm::min(d.minimum,d.point);d.maximum=glm::max(d.maximum,d.point);}
  if(source){unsigned triangle=d.sourceFace;if(triangle<source->faceLocations.size()){auto location=source->faceLocations[triangle];d.sourceFace=location.element;d.node=source->sourceNodes.at(location.node);}
   for(unsigned k=0;k<3;++k){unsigned corner=triangle*3+k;if(corner>= (source->indices.empty()?source->vertices.size():source->indices.size()))break;unsigned vertex=source->indices.empty()?corner:source->indices[corner];auto position=glm::dvec3(transform*glm::dvec4(source->vertices.at(vertex).position,1));if(position!=a.vertices.at(first)&&position!=a.vertices.at(second))continue;if(vertex<source->vertexLocations.size())d.sourceVertices.push_back(source->vertexLocations[vertex].element);else if(vertex<source->sourceVertexIds.size())d.sourceVertices.push_back(source->sourceVertexIds[vertex]);else d.sourceVertices.push_back(vertex);}
   if(third!=UINT32_MAX)for(unsigned v=0;v<source->vertices.size();++v)if(glm::dvec3(transform*glm::dvec4(source->vertices[v].position,1))==a.vertices.at(third)){d.sourceVertices.push_back(v<source->vertexLocations.size()?source->vertexLocations[v].element:v);break;}
  }
  if(diagnostic)*diagnostic=d;
  std::ostringstream text;text<<message<<" [node="<<d.node<<", original face="<<d.sourceFace<<", location=("<<d.point.x<<", "<<d.point.y<<", "<<d.point.z<<")]";throw std::runtime_error(text.str());
 };
 std::map<std::pair<unsigned,unsigned>,std::vector<std::pair<unsigned,unsigned>>> edges;
 for(unsigned f=0;f<a.faces.size();++f){auto& t=a.faces[f];auto x=a.vertices[t.vertices[0]],y=a.vertices[t.vertices[1]],z=a.vertices[t.vertices[2]];t.normal=glm::normalize(glm::cross(y-x,z-x));for(unsigned e=0;e<3;++e)edges[std::minmax(t.vertices[e],t.vertices[(e+1)%3])].push_back({f,e});}
 for(auto& [key,uses]:edges){(void)key;if(uses.size()>2)fail("nonmanifold-edge","nonmanifold collision edge; split/correct source topology",uses.front().first,key.first,key.second);if(uses.size()==2){auto [f,e]=uses[0];auto [g,j]=uses[1];auto& x=a.faces[f];auto& y=a.faces[g];if(x.vertices[e]==y.vertices[j])fail("inconsistent-winding","inconsistent adjacent winding",f,key.first,key.second);x.adjacent[e]=g;y.adjacent[j]=f;
  // Convex crease is active; a coplanar/concave shared edge is not a barrier.
  auto third=a.vertices[y.vertices[(j+2)%3]];double side=glm::dot(x.normal,third-a.vertices[x.vertices[0]]);bool active=side< -1e-9 && glm::dot(x.normal,y.normal)<1-1e-10;x.active[e]=y.active[j]=active;
 }}
 // Report T junctions rather than pretending that ambiguous edge adjacency is welded.
 // O(V*boundary edges), cook only, bounded input. No proximity welding of sheets.
 for(auto& [edge,uses]:edges)if(uses.size()==1){auto p=a.vertices[edge.first],q=a.vertices[edge.second],v=q-p;double l=glm::dot(v,v);for(unsigned i=0;i<a.vertices.size();++i){if(i==edge.first||i==edge.second)continue;double t=glm::dot(a.vertices[i]-p,v)/l;if(t>1e-9&&t<1-1e-9&&glm::length(a.vertices[i]-p-t*v)<1e-9)fail("t-junction","T-junction in collision source; triangulate connected topology before cooking",uses.front().first,edge.first,edge.second,i);}}
}
// Coplanar convex triangles become one physical face. Store the ordered polygon
// and crease edges in the cook so contact clipping never selects half a cube face.
void convexFaces(CollisionAsset& a){
 if(!a.convex)return;
 std::vector<bool> used(a.faces.size());std::set<std::array<uint32_t,2>> edges;
 for(unsigned seed=0;seed<a.faces.size();++seed)if(!used[seed]){
  std::vector<unsigned> group{seed};used[seed]=true;
  for(size_t i=0;i<group.size();++i)for(auto n:a.faces[group[i]].adjacent)if(n>=0&&!used[n]&&glm::dot(a.faces[seed].normal,a.faces[n].normal)>1-1e-9){used[n]=true;group.push_back(n);}
  std::set<unsigned> members(group.begin(),group.end());std::map<unsigned,unsigned> next;
  for(auto f:group)for(unsigned e=0;e<3;++e)if(!members.count(a.faces[f].adjacent[e])){auto x=a.faces[f].vertices[e],y=a.faces[f].vertices[(e+1)%3];if(!next.emplace(x,y).second)throw std::runtime_error("ambiguous convex face boundary");edges.insert({std::min(x,y),std::max(x,y)});}
  CollisionPolygon polygon;polygon.normal=a.faces[seed].normal;unsigned first=next.begin()->first,current=first;
  do{polygon.vertices.push_back(current);auto found=next.find(current);if(found==next.end()||polygon.vertices.size()>next.size())throw std::runtime_error("invalid convex face boundary");current=found->second;}while(current!=first);
  if(polygon.vertices.size()!=next.size())throw std::runtime_error("disconnected convex face boundary");
  a.polygons.push_back(std::move(polygon));
 }a.edges.assign(edges.begin(),edges.end());
}
void mass(CollisionAsset& a){
 V first(0);glm::dmat3 second(0);a.volume=0;
 for(auto& f:a.faces){auto p=a.vertices[f.vertices[0]],q=a.vertices[f.vertices[1]],r=a.vertices[f.vertices[2]];double volume=glm::dot(p,glm::cross(q,r))/6;V s=p+q+r;a.volume+=volume;first+=volume*s/4.;second+=volume*(glm::outerProduct(s,s)+glm::outerProduct(p,p)+glm::outerProduct(q,q)+glm::outerProduct(r,r))/20.;}
 if(!(a.volume>1e-12))throw std::runtime_error("convex cook requires positive closed volume and outward winding");
 a.centerOfMass=first/a.volume;second-=a.volume*glm::outerProduct(a.centerOfMass,a.centerOfMass);double trace=second[0][0]+second[1][1]+second[2][2];a.unitInertia=glm::dmat3(trace)-second;
 if(!(glm::determinant(a.unitInertia)>0))throw std::runtime_error("singular convex inertia");
}
}
void CollisionAsset::Candidates(V lo,V hi,std::vector<uint32_t>& out,uint64_t* tested)const{
 out.clear();if(nodes.empty())return;std::array<unsigned,64> stack{};unsigned count=1;stack[0]=0;
 while(count){auto& n=nodes[stack[--count]];if(tested)++*tested;if(glm::any(glm::lessThan(n.maximum,lo))||glm::any(glm::greaterThan(n.minimum,hi)))continue;if(n.count){for(unsigned i=n.first;i<n.first+n.count;++i)out.push_back(order[i]);}else{if(count+2>stack.size())throw std::runtime_error("collision BVH traversal depth overflow");stack[count++]=n.right;stack[count++]=n.left;}}
 std::sort(out.begin(),out.end());
}
bool CookCollision(const MeshData& mesh,const CollisionCookSettings& settings,CollisionAsset& out,std::string& error,CollisionDiagnostic* diagnostic){
 if(diagnostic)*diagnostic={};
 try{CollisionAsset a;a.convex=settings.convex;a.twoSided=settings.twoSided;a.selectedPrimitive=settings.primitive;a.sourceTransform=settings.transform;
  for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(settings.transform[c][r]))throw std::runtime_error("nonfinite source transform");
  if(settings.transform[0][3]!=0||settings.transform[1][3]!=0||settings.transform[2][3]!=0||settings.transform[3][3]!=1)throw std::runtime_error("collision source requires affine transform");
  glm::dmat3 linear(settings.transform);if(glm::determinant(linear)<=0||glm::length(V(settings.transform[0]))<=0||glm::length(V(settings.transform[1]))<=0||glm::length(V(settings.transform[2]))<=0)throw std::runtime_error("collision source transform requires positive nonsingular scale; mirrors unsupported");
  for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)if(std::abs(glm::dot(glm::normalize(V(linear[i])),glm::normalize(V(linear[j]))))>1e-7)throw std::runtime_error("collision source shear unsupported");
  if(mesh.vertices.empty()||mesh.vertices.size()>65536||mesh.indices.size()>196608)throw std::runtime_error("collision source limit: 65536 vertices / 65536 triangles");
  unsigned begin=0,count=mesh.indices.empty()?mesh.vertices.size():mesh.indices.size();if(!mesh.primitives.empty()){if(settings.primitive>=mesh.primitives.size())throw std::runtime_error("selected collision primitive missing");begin=mesh.primitives[settings.primitive].first;count=mesh.primitives[settings.primitive].count;}
  if(!count||count%3||begin+count>(mesh.indices.empty()?mesh.vertices.size():mesh.indices.size()))throw std::runtime_error("collision selected primitive index range");
  // Importer position IDs weld visual seams without merging separately authored
  // coincident sheets. ID-free procedural sources opt into exact-position welding.
  std::map<std::array<double,3>,unsigned> weld;std::map<uint32_t,unsigned> identities;std::set<std::array<unsigned,3>> seen;unsigned dropped=0;
  for(unsigned i=begin;i<begin+count;i+=3){CollisionFace f;f.source=i/3;for(unsigned k=0;k<3;++k){unsigned id=mesh.indices.empty()?i+k:mesh.indices[i+k];if(id>=mesh.vertices.size())throw std::runtime_error("collision source index out of range");V p=V(settings.transform*glm::dvec4(mesh.vertices[id].position,1));if(!finite(p))throw std::runtime_error("nonfinite collision vertex");std::array<double,3> key{p.x,p.y,p.z};if(!mesh.sourceVertexIds.empty()){if(mesh.sourceVertexIds.size()!=mesh.vertices.size())throw std::runtime_error("source identity count mismatch");auto [it,fresh]=identities.emplace(mesh.sourceVertexIds[id],a.vertices.size());if(fresh)a.vertices.push_back(p);else if(a.vertices[it->second]!=p)throw std::runtime_error("one source vertex identity has inconsistent positions");f.vertices[k]=it->second;}else{auto [it,fresh]=weld.emplace(key,a.vertices.size());if(fresh)a.vertices.push_back(p);f.vertices[k]=it->second;}}
   auto key=f.vertices;std::sort(key.begin(),key.end());if(glm::length(glm::cross(a.vertices[f.vertices[1]]-a.vertices[f.vertices[0]],a.vertices[f.vertices[2]]-a.vertices[f.vertices[0]]))<=1e-12||!seen.insert(key).second){++dropped;continue;}a.faces.push_back(f);}
  if(dropped)a.warnings.push_back("removed "+std::to_string(dropped)+" duplicate/degenerate triangles");
  if(a.faces.empty())throw std::runtime_error("no usable collision triangles");
  if(settings.convex){quickhull::QuickHull<double> cooker;auto hull=cooker.getConvexHull(&a.vertices[0].x,a.vertices.size(),true,false,1e-9);auto& points=hull.getVertexBuffer();auto& indices=hull.getIndexBuffer();a.vertices.clear();a.faces.clear();for(auto p:points)a.vertices.push_back({p.x,p.y,p.z});if(a.vertices.size()>128||indices.size()/3>252)throw std::runtime_error("convex hull limit: 128 vertices / 252 faces; use lower-detail source/compound");for(unsigned i=0;i<indices.size();i+=3){CollisionFace f;f.vertices={uint32_t(indices[i]),uint32_t(indices[i+1]),uint32_t(indices[i+2])};f.source=UINT32_MAX;a.faces.push_back(f);}if(a.faces.empty())throw std::runtime_error("convex hull has no volume");
   // Normalize the cooker's winding convention to outward Judas surface normals.
   V interior(0);for(auto p:a.vertices)interior+=p;interior/=double(a.vertices.size());
   for(auto& f:a.faces){auto p=a.vertices[f.vertices[0]],q=a.vertices[f.vertices[1]],r=a.vertices[f.vertices[2]];if(glm::dot(glm::cross(q-p,r-p),p-interior)<0)std::swap(f.vertices[1],f.vertices[2]);}
  }
  topology(a,&mesh,diagnostic,settings.transform);if(a.convex){for(auto& f:a.faces)for(auto n:f.adjacent)if(n<0)throw std::runtime_error("convex cook not closed");mass(a);convexFaces(a);}build(a);out=std::move(a);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
// Versioned cooked file stores acceleration/adjacency, never rebuilds it at runtime.
bool SaveCollisionAsset(const std::string& path,const CollisionAsset& a,std::string& error){
 std::ostringstream s;s<<std::setprecision(17)<<"JudasCollision 1\n"<<a.convex<<' '<<a.twoSided<<' '<<a.selectedPrimitive<<' '<<a.weldTolerance<<'\n'<<std::quoted(a.sourceAsset)<<' '<<std::quoted(a.sourceFingerprint)<<' '<<std::quoted(a.settingsFingerprint)<<'\n';for(int c=0;c<4;++c)for(int r=0;r<4;++r)s<<a.sourceTransform[c][r]<<' ';s<<'\n'<<a.vertices.size()<<' '<<a.faces.size()<<' '<<a.nodes.size()<<'\n';
 for(auto p:a.vertices)s<<p.x<<' '<<p.y<<' '<<p.z<<'\n';
 for(auto& f:a.faces){for(auto v:f.vertices)s<<v<<' ';for(auto v:f.adjacent)s<<v<<' ';for(auto v:f.active)s<<v<<' ';s<<f.source<<' '<<f.normal.x<<' '<<f.normal.y<<' '<<f.normal.z<<'\n';}for(auto& n:a.nodes)s<<n.minimum.x<<' '<<n.minimum.y<<' '<<n.minimum.z<<' '<<n.maximum.x<<' '<<n.maximum.y<<' '<<n.maximum.z<<' '<<n.first<<' '<<n.count<<' '<<n.left<<' '<<n.right<<'\n';for(auto i:a.order)s<<i<<' ';s<<'\n';for(auto p:{a.minimum,a.maximum,a.centerOfMass})s<<p.x<<' '<<p.y<<' '<<p.z<<' ';s<<a.volume<<' ';for(int c=0;c<3;++c)for(int r=0;r<3;++r)s<<a.unitInertia[c][r]<<' ';s<<'\n'<<a.polygons.size()<<' '<<a.edges.size()<<'\n';for(auto& p:a.polygons){s<<p.vertices.size()<<' ';for(auto v:p.vertices)s<<v<<' ';s<<p.normal.x<<' '<<p.normal.y<<' '<<p.normal.z<<'\n';}for(auto e:a.edges)s<<e[0]<<' '<<e[1]<<'\n';
 auto text=s.str();CollisionAsset check;if(!DecodeCollisionAsset(std::vector<uint8_t>(text.begin(),text.end()),check,error))return false;
 auto temporary=path+".cooking";{std::ofstream out(temporary,std::ios::binary);if(!out||!out.write(text.data(),text.size())){error="collision cook write failed";return false;}}std::error_code ec;std::filesystem::rename(temporary,path,ec);if(ec){std::filesystem::remove(temporary);error="collision cook promotion failed: "+ec.message();return false;}return true;
}
bool DecodeCollisionAsset(const std::vector<uint8_t>& bytes,CollisionAsset& out,std::string& error){
 try{if(bytes.size()>32*1024*1024)throw std::runtime_error("collision asset byte limit");std::istringstream s(std::string(bytes.begin(),bytes.end()));std::string magic;unsigned version;CollisionAsset a;size_t vertices,faces,nodes;s>>magic>>version;if(magic!="JudasCollision"||version!=1)throw std::runtime_error("collision asset header/version");s>>a.convex>>a.twoSided>>a.selectedPrimitive>>a.weldTolerance>>std::quoted(a.sourceAsset)>>std::quoted(a.sourceFingerprint)>>std::quoted(a.settingsFingerprint);for(int c=0;c<4;++c)for(int r=0;r<4;++r)s>>a.sourceTransform[c][r];s>>vertices>>faces>>nodes;if(!vertices||vertices>65536||!faces||faces>65536||!nodes||nodes>faces*2|| (a.convex&&(vertices>128||faces>252)))throw std::runtime_error("collision asset geometry limits");a.vertices.resize(vertices);a.faces.resize(faces);a.nodes.resize(nodes);a.order.resize(faces);
 for(auto& p:a.vertices){s>>p.x>>p.y>>p.z;if(!finite(p))throw std::runtime_error("nonfinite cooked vertex");}for(auto& f:a.faces){for(auto& v:f.vertices){s>>v;if(v>=vertices)throw std::runtime_error("cooked collision index");}for(auto& v:f.adjacent){s>>v;if(v< -1||v>=int(faces))throw std::runtime_error("cooked adjacency index");}for(auto& v:f.active)s>>v;s>>f.source>>f.normal.x>>f.normal.y>>f.normal.z;if(!finite(f.normal)||std::abs(glm::length(f.normal)-1)>1e-7)throw std::runtime_error("cooked face normal");}
 for(unsigned i=0;i<nodes;++i){auto& n=a.nodes[i];s>>n.minimum.x>>n.minimum.y>>n.minimum.z>>n.maximum.x>>n.maximum.y>>n.maximum.z>>n.first>>n.count>>n.left>>n.right;if(!finite(n.minimum)||!finite(n.maximum)||glm::any(glm::greaterThan(n.minimum,n.maximum))||(n.count?n.first+n.count>faces:n.left<=i||n.right<=i||n.left>=nodes||n.right>=nodes))throw std::runtime_error("invalid cooked BVH");}std::set<unsigned> ordering;for(auto& i:a.order){s>>i;if(i>=faces||!ordering.insert(i).second)throw std::runtime_error("invalid cooked BVH feature mapping");}for(auto* p:{&a.minimum,&a.maximum,&a.centerOfMass})s>>p->x>>p->y>>p->z;s>>a.volume;for(int c=0;c<3;++c)for(int r=0;r<3;++r)s>>a.unitInertia[c][r];if(!s||!finite(a.minimum)||!finite(a.maximum)||!finite(a.centerOfMass)||!std::isfinite(a.volume)||(a.convex&&a.volume<=0))throw std::runtime_error("truncated/invalid collision cook");size_t polygons,edges;s>>polygons>>edges;if(polygons>252||edges>378|| (a.convex&&polygons<4)||(!a.convex&&(polygons||edges)))throw std::runtime_error("cooked convex topology limits");a.polygons.resize(polygons);a.edges.resize(edges);for(auto& p:a.polygons){size_t count;s>>count;if(count<3||count>128)throw std::runtime_error("cooked polygon limit");p.vertices.resize(count);for(auto& v:p.vertices){s>>v;if(v>=vertices)throw std::runtime_error("cooked polygon index");}s>>p.normal.x>>p.normal.y>>p.normal.z;if(!finite(p.normal)||std::abs(glm::length(p.normal)-1)>1e-7)throw std::runtime_error("cooked polygon normal");}for(auto& e:a.edges){s>>e[0]>>e[1];if(e[0]>=vertices||e[1]>=vertices||e[0]==e[1])throw std::runtime_error("cooked convex edge");}if(!s)throw std::runtime_error("truncated convex topology");
 for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(a.sourceTransform[c][r]))throw std::runtime_error("cooked transform nonfinite");
 for(int c=0;c<3;++c)for(int r=0;r<3;++r)if(!std::isfinite(a.unitInertia[c][r]))throw std::runtime_error("cooked inertia nonfinite");
 if(a.convex&&!(glm::determinant(a.unitInertia)>0))throw std::runtime_error("invalid cooked inertia");
 std::vector<unsigned> parents(nodes),coverage(faces);for(unsigned i=0;i<nodes;++i){auto& n=a.nodes[i];if(n.count){for(unsigned j=n.first;j<n.first+n.count;++j){++coverage[j];for(auto v:a.faces[a.order[j]].vertices)if(glm::any(glm::lessThan(a.vertices[v],n.minimum))||glm::any(glm::greaterThan(a.vertices[v],n.maximum)))throw std::runtime_error("BVH leaf excludes geometry");}}else for(auto child:{n.left,n.right}){++parents[child];if(glm::any(glm::lessThan(a.nodes[child].minimum,n.minimum))||glm::any(glm::greaterThan(a.nodes[child].maximum,n.maximum)))throw std::runtime_error("BVH parent excludes child");}}
 for(unsigned i=1;i<nodes;++i)if(parents[i]!=1)throw std::runtime_error("cooked BVH is not a tree");
 for(auto count:coverage)if(count!=1)throw std::runtime_error("cooked BVH leaf coverage");
 for(auto v:a.vertices)if(glm::any(glm::lessThan(v,a.minimum))||glm::any(glm::greaterThan(v,a.maximum)))throw std::runtime_error("cooked bounds exclude vertex");
 for(auto& f:a.faces){auto normal=glm::cross(a.vertices[f.vertices[1]]-a.vertices[f.vertices[0]],a.vertices[f.vertices[2]]-a.vertices[f.vertices[0]]);if(glm::length(normal)<=1e-12||glm::dot(glm::normalize(normal),f.normal)<1-1e-7)throw std::runtime_error("cooked winding/normal mismatch");}
 // Validate the serialized physical topology without recooking it at runtime.
 for(unsigned i=0;i<a.faces.size();++i){const auto& f=a.faces[i];for(unsigned e=0;e<3;++e){
  int other=f.adjacent[e];if(other<0){if(!f.active[e])throw std::runtime_error("inactive open boundary");continue;}
  if(other==int(i))throw std::runtime_error("self adjacency");
  const auto& g=a.faces[other];bool reciprocal=false;
  for(unsigned j=0;j<3;++j)if(g.vertices[j]==f.vertices[(e+1)%3]&&g.vertices[(j+1)%3]==f.vertices[e]){
   reciprocal=g.adjacent[j]==int(i)&&g.active[j]==f.active[e];
   double side=glm::dot(f.normal,a.vertices[g.vertices[(j+2)%3]]-a.vertices[f.vertices[0]]);
   bool active=side< -1e-9&&glm::dot(f.normal,g.normal)<1-1e-10;
   if(active!=f.active[e])throw std::runtime_error("cooked edge classification mismatch");
  }
  if(!reciprocal)throw std::runtime_error("nonreciprocal cooked adjacency");
 }}
 if(a.convex){for(const auto& p:a.polygons){const auto base=a.vertices[p.vertices[0]];
  for(unsigned i=0;i<p.vertices.size();++i){auto x=a.vertices[p.vertices[i]],y=a.vertices[p.vertices[(i+1)%p.vertices.size()]],z=a.vertices[p.vertices[(i+2)%p.vertices.size()]];
   if(std::abs(glm::dot(x-base,p.normal))>1e-7||glm::dot(glm::cross(y-x,z-y),p.normal)<-1e-9)throw std::runtime_error("invalid convex polygon");
  }
  for(auto v:a.vertices)if(glm::dot(v-base,p.normal)>1e-7)throw std::runtime_error("nonconvex cooked hull");
 }}
 s>>std::ws;if(!s.eof())throw std::runtime_error("trailing collision asset records");out=std::move(a);return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool LoadCollisionAsset(const std::string& path,CollisionAsset& out,std::string& error){std::ifstream file(path,std::ios::binary);if(!file){error="cannot open cooked collision "+path;return false;}return DecodeCollisionAsset(std::vector<uint8_t>{std::istreambuf_iterator<char>(file),{}},out,error);}
