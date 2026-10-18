#include "LiquidGeometry.h"
#include "LiquidSurface.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <cmath>
namespace {
using V=glm::dvec3;using Face=std::vector<V>;using Poly=std::vector<Face>;
const int faces[4][3]={{1,2,3},{0,3,2},{0,1,3},{0,2,1}};
double tetVolume(const LiquidTet& t){return std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))/6;}
Poly poly(const LiquidTet& t){Poly p;V c=(t[0]+t[1]+t[2]+t[3])*.25;for(auto& f:faces){Face a{t[f[0]],t[f[1]],t[f[2]]};if(glm::dot(glm::cross(a[1]-a[0],a[2]-a[0]),c-a[0])>0)std::reverse(a.begin(),a.end());p.push_back(a);}return p;}
// Convex clipping retains actual faces and a consistently oriented cap.
Poly cut(const Poly& p,V n,double d,Face* cap=nullptr){
 // Most clipping planes miss or contain a fragment. Classify first so these
 // cases avoid per-face allocation and preserve the original tessellation.
 bool anyInside=false,anyOutside=false;for(const auto& f:p)for(V v:f){bool inside=glm::dot(n,v)-d<=1e-12;anyInside|=inside;anyOutside|=!inside;}
 if(!anyInside)return {};
 if(!anyOutside)return p;
 Poly out;Face rim;for(auto& f:p){Face a;for(size_t i=0;i<f.size();++i){V x=f[i],y=f[(i+1)%f.size()];double sx=glm::dot(n,x)-d,sy=glm::dot(n,y)-d;bool ix=sx<=1e-12,iy=sy<=1e-12;if(ix)a.push_back(x);if(ix!=iy){V v=x+(y-x)*(sx/(sx-sy));a.push_back(v);bool unique=true;for(V r:rim)if(glm::length(r-v)<1e-9)unique=false;if(unique)rim.push_back(v);}}if(a.size()>=3)out.push_back(a);}
 if(rim.size()>=3){V c(0);for(V v:rim)c+=v;c/=double(rim.size());V u=glm::normalize(rim[0]-c),v=glm::cross(glm::normalize(n),u);std::sort(rim.begin(),rim.end(),[&](V a,V b){return std::atan2(glm::dot(a-c,v),glm::dot(a-c,u))<std::atan2(glm::dot(b-c,v),glm::dot(b-c,u));});out.push_back(rim);if(cap)*cap=rim;}return out;}
LiquidMoment moment(const Poly& p){LiquidMoment r;V c(0);size_t count=0;for(auto& f:p)for(V v:f){c+=v;++count;}if(!count)return r;c/=double(count);for(auto& f:p)for(size_t i=1;i+1<f.size();++i){double v=std::abs(glm::dot(f[0]-c,glm::cross(f[i]-c,f[i+1]-c)))/6;r.volume+=v;r.first+=v*(c+f[0]+f[i]+f[i+1])*.25;}return r;}
bool affine(const LiquidTet& t,const std::array<double,4>& values,V& n,double& d){glm::dmat3 m(t[1]-t[0],t[2]-t[0],t[3]-t[0]);if(std::abs(glm::determinant(m))<1e-15)return false;n=glm::transpose(glm::inverse(m))*V(values[1]-values[0],values[2]-values[0],values[3]-values[0]);d=values[0]-glm::dot(n,t[0]);return true;}
std::array<double,4> coords(const LiquidTet& t,const GravityEquilibrium& e){return {e.Coordinate(t[0]),e.Coordinate(t[1]),e.Coordinate(t[2]),e.Coordinate(t[3])};}
}
LiquidGeometry LiquidBox(V a,V b){std::array<V,8> p;for(int i=0;i<8;++i)p[i]=V(i&1?b.x:a.x,i&2?b.y:a.y,i&4?b.z:a.z);LiquidGeometry g;const int ids[6][4]={{0,1,3,7},{0,3,2,7},{0,2,6,7},{0,6,4,7},{0,4,5,7},{0,5,1,7}};for(auto& t:ids)g.cells.push_back({p[t[0]],p[t[1]],p[t[2]],p[t[3]]});return g;}
LiquidMoment LiquidClip(const LiquidTet& t,const std::array<double,4>& q,double level,MeshData* mesh){double lo=*std::min_element(q.begin(),q.end()),hi=*std::max_element(q.begin(),q.end());if(level<=lo)return {};if(level>=hi){if(mesh&&std::abs(level-hi)<1e-9)for(auto& f:faces)if(std::abs(q[f[0]]-level)<1e-9&&std::abs(q[f[1]]-level)<1e-9&&std::abs(q[f[2]]-level)<1e-9){V n;double d;if(affine(t,q,n,d))for(int index:f)mesh->vertices.push_back({glm::vec3(t[index]),glm::vec3(glm::normalize(n)),{0,0}});}return {tetVolume(t),tetVolume(t)*(t[0]+t[1]+t[2]+t[3])*.25};}V n;double d;if(!affine(t,q,n,d))return {};Face cap;auto result=moment(cut(poly(t),n,level-d,&cap));if(mesh&&cap.size()>=3)for(size_t i=1;i+1<cap.size();++i)for(V v:{cap[0],cap[i],cap[i+1]})mesh->vertices.push_back({glm::vec3(v),glm::vec3(glm::normalize(n)),{0,0}});return result;}
double LiquidCapacity(const LiquidGeometry& g,const GravityEquilibrium& e,double q){double v=0;for(auto& t:g.cells)v+=LiquidClip(t,coords(t,e),q).volume;return v;}
double LiquidCoordinate(const LiquidTet& t,const GravityEquilibrium& e,V p,bool& inside){auto m=glm::dmat3(t[1]-t[0],t[2]-t[0],t[3]-t[0]);V b=glm::inverse(m)*(p-t[0]);inside=b.x>=-1e-8&&b.y>=-1e-8&&b.z>=-1e-8&&b.x+b.y+b.z<=1+1e-8;auto q=coords(t,e);return q[0]+b.x*(q[1]-q[0])+b.y*(q[2]-q[0])+b.z*(q[3]-q[0]);}
LiquidTetQuery CompileLiquidTetQuery(const LiquidTet& basin,const GravityEquilibrium& e){
 LiquidTetQuery query;query.minimum=V(1e30);query.maximum=V(-1e30);for(V p:basin){query.minimum=glm::min(query.minimum,p);query.maximum=glm::max(query.maximum,p);}
 V centre=(basin[0]+basin[1]+basin[2]+basin[3])*.25;unsigned index=0;
 for(auto& f:faces){V n=glm::cross(basin[f[1]]-basin[f[0]],basin[f[2]]-basin[f[0]]);if(glm::dot(n,centre-basin[f[0]])>0)n=-n;query.planes[index++]={n,glm::dot(n,basin[f[0]])};}
 V n;double d;if(!affine(basin,coords(basin,e),n,d))return query;query.planes[4]={n,-d};query.valid=true;return query;
}
LiquidMoment LiquidIntersection(const LiquidTet& body,const LiquidTetQuery& query,double q){
 if(!query.valid)return {};
 auto planes=query.planes;planes[4].w+=q;bool complete=true;
 for(auto plane:planes){bool outside=true;for(V v:body){double value=glm::dot(V(plane),v)-plane.w;outside&=value>1e-12;complete&=value<=0;}if(outside)return {};}
 if(complete)return {tetVolume(body),tetVolume(body)*(body[0]+body[1]+body[2]+body[3])*.25};
 Poly p=poly(body);for(auto plane:planes){p=cut(p,V(plane),plane.w);if(p.empty())return {};}return moment(p);
}
LiquidMoment LiquidIntersection(const LiquidTet& body,const LiquidTet& basin,const GravityEquilibrium& e,double q){return LiquidIntersection(body,CompileLiquidTetQuery(basin,e),q);}
bool ValidateLiquidGeometry(const LiquidGeometry& g,std::string& error){if(g.cells.empty()||g.cells.size()>16384){error="cavity needs 1..16384 nonoverlapping tetrahedra";return false;}for(auto& t:g.cells){for(V p:t)if(!std::isfinite(glm::dot(p,p))||glm::length(p)>1e6){error="invalid cavity coordinate";return false;}if(tetVolume(t)<1e-12){error="degenerate cavity tetrahedron";return false;}}
 // Full authored validation only, not per frame. Broad AABB rejection avoids most pairs.
 for(size_t i=0;i<g.cells.size();++i)for(size_t j=0;j<i;++j){V a(1e30),b(-1e30),c=a,d=b;for(V v:g.cells[i]){a=glm::min(a,v);b=glm::max(b,v);}for(V v:g.cells[j]){c=glm::min(c,v);d=glm::max(d,v);}if(b.x<=c.x+1e-9||d.x<=a.x+1e-9||b.y<=c.y+1e-9||d.y<=a.y+1e-9||b.z<=c.z+1e-9||d.z<=a.z+1e-9)continue;Poly p=poly(g.cells[i]);for(auto& f:poly(g.cells[j])){V n=glm::cross(f[1]-f[0],f[2]-f[0]);p=cut(p,n,glm::dot(n,f[0]));}if(moment(p).volume>1e-9){error="overlapping cavity tetrahedra would double-count liquid";return false;}}
 return true;}
bool LoadLiquidGeometry(const std::string& path,LiquidGeometry& out,std::string& error){std::ifstream s(path);std::string magic;size_t n;LiquidGeometry g;if(!(s>>magic>>n)||magic!="JudasCavity1"||n>16384){error="invalid JudasCavity1 header";return false;}g.cells.resize(n);for(auto& t:g.cells)for(V& v:t)if(!(s>>v.x>>v.y>>v.z)){error="truncated cavity";return false;}s>>std::ws;if(!s.eof()||!ValidateLiquidGeometry(g,error))return false;out=std::move(g);return true;}
double LiquidInverse(const LiquidBasinData& b,double v){if(v<=0)return b.curve.front().q;if(v>=b.capacity)return b.curve.back().q;auto it=std::lower_bound(b.curve.begin(),b.curve.end(),v,[](auto p,double target){return p.volume<target;});auto a=*(it-1);if(!b.capacityPolynomials.empty()){size_t index=size_t(it-b.curve.begin()-1);auto c=b.capacityPolynomials[index];double low=0,high=1;for(unsigned k=0;k<48;++k){double t=(low+high)*.5,value=c[0]+t*(c[1]+t*(c[2]+t*c[3]));if(value<v)low=t;else high=t;}return a.q+(it->q-a.q)*(low+high)*.5;}return a.q+(it->q-a.q)*(v-a.volume)/(it->volume-a.volume);}
bool RefineLiquidGeometry(const LiquidGeometry& source,const GravityEquilibrium& e,double ht,LiquidGeometry& out,double& coordinateError,std::string& error){out=source;coordinateError=0;
 if(e.kind==GravityEquilibrium::Kind::Radius){std::vector<LiquidTet> pending=source.cells;out.cells.clear();while(!pending.empty()){auto t=pending.back();pending.pop_back();double length=0;int a=0,c=1;for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)if(glm::length(t[i]-t[j])>length){length=glm::length(t[i]-t[j]);a=i;c=j;}double radius=1e30;for(V v:t)radius=std::min(radius,e.Coordinate(v));double lower=radius-length;if(lower<=0){error="radial cavity crosses gravity centre or exceeds local radial domain";return false;}double bound=length*length/(2*lower);if(bound>ht*.25){V mid=(t[a]+t[c])*.5;auto left=t,right=t;left[a]=mid;right[c]=mid;pending.push_back(left);pending.push_back(right);}else{coordinateError=std::max(coordinateError,bound);out.cells.push_back(t);}if(pending.size()+out.cells.size()>262144){error="radial refinement exceeded 262144 cells; loosen declared height tolerance";return false;}}}
 return true;
}
bool BakeLiquidBasin(const LiquidGeometry& source,const GravityEquilibrium& e,double vt,double ht,const std::string& hash,LiquidBasinData& out,std::string& error){if(!ValidateLiquidGeometry(source,error)||vt<=0||ht<=0||!std::isfinite(vt+ht)||e.magnitude<=0){error="invalid cavity/gravity/bake tolerances: "+error;return false;}LiquidBasinData b;b.geometry=source;b.equilibrium=e;b.fingerprint=hash;b.volumeTolerance=vt;b.heightTolerance=ht;
 if(!RefineLiquidGeometry(source,e,ht,b.geometry,b.coordinateError,error))return false;
 std::vector<double> knots;for(auto& t:b.geometry.cells)for(V v:t)knots.push_back(e.Coordinate(v));std::sort(knots.begin(),knots.end());knots.erase(std::unique(knots.begin(),knots.end(),[](double a,double c){return std::abs(a-c)<1e-10;}),knots.end());double min=knots.front(),max=knots.back();b.capacity=LiquidCapacity(b.geometry,e,max);b.curve.push_back({min,0});
 // Every interval is refined at quarter/mid/three-quarter positions. Bernstein
 // cubic interpolation bound: capacity is piecewise cubic between vertex knots.
 std::function<bool(double,double,double,double,int)> refine=[&](double a,double va,double c,double vc,int depth){double mid=(a+c)*.5,vm=LiquidCapacity(b.geometry,e,mid),v1=LiquidCapacity(b.geometry,e,(3*a+c)*.25),v3=LiquidCapacity(b.geometry,e,(a+3*c)*.25);double err=std::max({std::abs(vm-(va+vc)*.5),std::abs(v1-(3*va+vc)*.25),std::abs(v3-(va+3*vc)*.25)})*4;bool good=err<=vt&&((c-a)<=ht||err<=std::max(1e-13,(vc-va)*1e-10));if(good){b.curve.push_back({c,vc});return true;}if(depth>30||b.curve.size()>65536){error="capacity curve refinement exceeded budget";return false;}return refine(a,va,mid,vm,depth+1)&&refine(mid,vm,c,vc,depth+1);};
 // Retain every vertex knot: the cubic interpolation error bound only applies
 // between these knots. Never replace them with an unverified coarse grid.
 for(size_t i=1;i<knots.size();++i)if(!refine(knots[i-1],b.curve.back().volume,knots[i],LiquidCapacity(b.geometry,e,knots[i]),0))return false;
 out=std::move(b);return true;}
bool SaveLiquidBasin(const std::string& path,const LiquidBasinData& b,std::string& error){std::ofstream s(path);if(!s){error="cannot write basin asset";return false;}s<<std::setprecision(17)<<(b.dynamicSurface?"JudasBasin2 ":"JudasBasin1 ")<<std::quoted(b.fingerprint)<<' '<<int(b.equilibrium.kind)<<' '<<b.equilibrium.up.x<<' '<<b.equilibrium.up.y<<' '<<b.equilibrium.up.z<<' '<<b.equilibrium.center.x<<' '<<b.equilibrium.center.y<<' '<<b.equilibrium.center.z<<' '<<b.equilibrium.magnitude<<' '<<b.volumeTolerance<<' '<<b.heightTolerance<<' '<<b.coordinateError<<' '<<b.capacity<<' '<<b.geometry.cells.size()<<' '<<b.curve.size()<<'\n';for(auto& t:b.geometry.cells){for(V v:t)s<<v.x<<' '<<v.y<<' '<<v.z<<' ';s<<'\n';}for(auto p:b.curve)s<<p.q<<' '<<p.volume<<'\n';if(b.dynamicSurface&&!SaveLiquidSurface(s,*b.dynamicSurface)){error="failed surface write";return false;}if(!s){error="failed basin write";return false;}return true;}
bool DecodeLiquidBasin(const std::vector<unsigned char>& bytes,LiquidBasinData& out,std::string& error){if(bytes.size()>128*1024*1024){error="basin asset exceeds 128 MiB";return false;}std::istringstream s(std::string(bytes.begin(),bytes.end()));LiquidBasinData b;std::string magic;int kind;size_t nt,nc;auto& e=b.equilibrium;if(!(s>>magic>>std::quoted(b.fingerprint)>>kind>>e.up.x>>e.up.y>>e.up.z>>e.center.x>>e.center.y>>e.center.z>>e.magnitude>>b.volumeTolerance>>b.heightTolerance>>b.coordinateError>>b.capacity>>nt>>nc)||(magic!="JudasBasin1"&&magic!="JudasBasin2")||kind<0||kind>1||nt==0||nt>262144||nc<2||nc>65536){error="invalid JudasBasin1 header";return false;}e.kind=GravityEquilibrium::Kind(kind);b.geometry.cells.resize(nt);b.curve.resize(nc);for(auto& t:b.geometry.cells)for(V& p:t)if(!(s>>p.x>>p.y>>p.z)||!std::isfinite(glm::dot(p,p))){error="invalid basin tetrahedra";return false;}for(auto& t:b.geometry.cells)if(tetVolume(t)<1e-12){error="degenerate basin cell";return false;}double prevQ=-1e30,prevV=-1;for(auto& p:b.curve){if(!(s>>p.q>>p.volume)||!std::isfinite(p.q+p.volume)||p.q<=prevQ||p.volume<prevV){error="nonmonotonic basin capacity";return false;}prevQ=p.q;prevV=p.volume;}if(magic=="JudasBasin2"&&!ReadLiquidSurface(s,e,b.dynamicSurface,error))return false;s>>std::ws;if(!s.eof()||b.capacity<=0||b.volumeTolerance<=0||b.heightTolerance<=0||e.magnitude<=0||b.coordinateError<0||!std::isfinite(e.magnitude+b.volumeTolerance+b.heightTolerance+b.coordinateError+b.capacity+glm::dot(e.center,e.center))||std::abs(glm::length(e.up)-1)>1e-6||b.curve.front().volume!=0||std::abs(b.curve.back().volume-b.capacity)>1e-9){error="invalid basin metadata/endpoints";return false;}out=std::move(b);return true;}
bool LoadLiquidBasin(const std::string& path,LiquidBasinData& out,std::string& error){std::ifstream s(path,std::ios::binary);if(!s){error="missing basin asset";return false;}std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(s),{}};return DecodeLiquidBasin(bytes,out,error);}
namespace {
// A clipped fragment is convex. Fan its boundary triangles from an existing
// vertex; incident faces give zero-volume tets. This partitions exactly the same
// fragment without inserting a centroid and subdividing every boundary face.
void appendTets(const Poly& p,LiquidGeometry& out){if(p.empty()||p.front().empty())return;V c=p.front().front();for(auto& f:p){
 bool incident=false;for(V v:f)incident|=glm::length(v-c)<1e-12;if(incident)continue;
 for(size_t i=1;i+1<f.size();++i){LiquidTet t{c,f[0],f[i],f[i+1]};if(tetVolume(t)>1e-15/6)out.cells.push_back(t);}
}}

}
LiquidGeometry LiquidCutGeometry(const LiquidGeometry& g,const std::vector<glm::dvec4>& planes){LiquidGeometry out;for(auto& t:g.cells){bool complete=true;for(auto plane:planes)for(V v:t)complete&=glm::dot(V(plane),v)<=plane.w+1e-12;if(complete){out.cells.push_back(t);continue;}Poly p=poly(t);for(auto plane:planes){p=cut(p,V(plane),plane.w);if(p.empty())break;}appendTets(p,out);}return out;}
LiquidGeometry LiquidSubtractSolids(const LiquidGeometry& g,const std::vector<std::vector<glm::dvec4>>& solids){
 LiquidGeometry out;
 for(auto& t:g.cells){std::vector<const std::vector<glm::dvec4>*> candidates;bool covered=false;
  for(auto& planes:solids){bool separated=false,inside=true;for(auto plane:planes){bool outside=true;for(V v:t){double value=glm::dot(V(plane),v)-plane.w;outside&=value>1e-12;inside&=value<=0;}separated|=outside;}if(inside){covered=true;break;}if(!separated)candidates.push_back(&planes);}
  if(covered)continue;
  if(candidates.empty()){out.cells.push_back(t);continue;}
  std::vector<Poly> fragments{poly(t)};bool changed=false;
  for(auto candidate:candidates){auto& planes=*candidate;std::vector<Poly> next;
   for(auto& fragment:fragments){bool separated=false;for(auto plane:planes){bool outside=true;for(auto& face:fragment)for(V v:face)outside&=glm::dot(V(plane),v)>plane.w+1e-12;separated|=outside;}
    if(separated){next.push_back(std::move(fragment));continue;}
    auto overlap=fragment;for(auto plane:planes){overlap=cut(overlap,V(plane),plane.w);if(overlap.empty())break;}
    if(moment(overlap).volume<=1e-13){next.push_back(std::move(fragment));continue;}
    changed=true;auto remaining=std::move(fragment);
    for(auto plane:planes){auto outside=cut(remaining,-V(plane),-plane.w);if(moment(outside).volume>1e-15/6)next.push_back(std::move(outside));remaining=cut(remaining,V(plane),plane.w);if(remaining.empty())break;}
   }
   fragments=std::move(next);if(fragments.empty())break;
  }
  if(!changed)out.cells.push_back(t);else for(auto& fragment:fragments)appendTets(fragment,out);
 }
 return out;
}
LiquidGeometry LiquidSubtractConvex(const LiquidGeometry& g,const std::vector<glm::dvec4>& planes){return LiquidSubtractSolids(g,{planes});}
std::vector<LiquidPolygon> LiquidBoundary(const LiquidGeometry& g,V n,double d){std::vector<LiquidPolygon> out;for(auto& t:g.cells)for(auto& f:poly(t)){bool on=true;for(V v:f)on&=std::abs(glm::dot(n,v)-d)<1e-8;if(on)out.push_back(f);}return out;}
double LiquidFaceArea(const std::vector<LiquidPolygon>& faces,const GravityEquilibrium& e,double q){double area=0;for(auto& f:faces){Face wet;for(size_t i=0;i<f.size();++i){V a=f[i],b=f[(i+1)%f.size()];double x=e.Coordinate(a)-q,y=e.Coordinate(b)-q;if(x<=0)wet.push_back(a);if((x<=0)!=(y<=0))wet.push_back(a+(b-a)*(x/(x-y)));}for(size_t i=1;i+1<wet.size();++i)area+=glm::length(glm::cross(wet[i]-wet[0],wet[i+1]-wet[0]))*.5;}return area;}
bool LiquidConvexCell(const LiquidGeometry& g){
 if(g.cells.empty())return false;
 struct Boundary {Face triangle;V normal;double offset,area;};std::vector<Boundary> faces;
 V lo(1e30),hi(-1e30);for(auto& t:g.cells)for(V v:t){lo=glm::min(lo,v);hi=glm::max(hi,v);}
 double eps=std::max(1e-8,glm::length(hi-lo)*1e-8);
 for(auto& t:g.cells)for(auto& f:poly(t)){V n=glm::cross(f[1]-f[0],f[2]-f[0]);double length=glm::length(n);if(length<1e-14)continue;n/=length;faces.push_back({f,n,glm::dot(n,f[0]),length*.5});}
 auto overlap=[&](const Boundary& a,const Boundary& b){
  Face clipped=a.triangle;V centre=(b.triangle[0]+b.triangle[1]+b.triangle[2])/3.;
  for(unsigned edge=0;edge<3&&!clipped.empty();++edge){V n=glm::cross(b.normal,b.triangle[(edge+1)%3]-b.triangle[edge]);double d=glm::dot(n,b.triangle[edge]);if(glm::dot(n,centre)>d){n=-n;d=-d;}
   Face next;for(size_t i=0;i<clipped.size();++i){V x=clipped[i],y=clipped[(i+1)%clipped.size()];double sx=glm::dot(n,x)-d,sy=glm::dot(n,y)-d;
    if(sx<=1e-14)next.push_back(x);
    if((sx<=0)!=(sy<=0))next.push_back(x+(y-x)*(sx/(sx-sy)));
   }clipped=std::move(next);
  }
  double area=0;for(size_t i=1;i+1<clipped.size();++i)area+=glm::length(glm::cross(clipped[i]-clipped[0],clipped[i+1]-clipped[0]))*.5;return area;
 };
 // Radial refinement has nonconforming triangle subdivisions at shared faces.
 // Test covered area, not a single epsilon-offset point which can cross a thin
 // tetrahedron and incorrectly identify an internal face as an exterior wall.
 for(auto& a:faces){bool supports=true;for(auto& t:g.cells)for(V p:t)if(glm::dot(a.normal,p)-a.offset>eps){supports=false;break;}
  if(supports)continue;
  double covered=0;for(auto& b:faces)if(glm::dot(a.normal,b.normal)<-1+1e-8&&std::abs(a.offset+b.offset)<eps)covered+=overlap(a,b);
  if(covered<a.area*(1-1e-6))return false;
 }
 return true;
}

std::vector<glm::dvec4> LiquidTetPlanes(const LiquidTet& tet){
 std::vector<glm::dvec4> result;result.reserve(5);V centre=(tet[0]+tet[1]+tet[2]+tet[3])*.25;
 for(auto& f:faces){V n=glm::normalize(glm::cross(tet[f[1]]-tet[f[0]],tet[f[2]]-tet[f[0]]));if(glm::dot(n,centre-tet[f[0]])>0)n=-n;result.push_back({n,glm::dot(n,tet[f[0]])});}return result;
}
LiquidGeometry LiquidOccupiedGeometry(const LiquidGeometry& geometry,const GravityEquilibrium& e,double q){LiquidGeometry result;for(auto& t:geometry.cells){V n;double d;if(affine(t,coords(t,e),n,d))appendTets(cut(poly(t),n,q-d),result);}return result;}

std::vector<glm::dvec4> LiquidOccupiedPlanes(const LiquidTet& tet,const GravityEquilibrium& e,double q){auto levels=coords(tet,e);if(q<=*std::min_element(levels.begin(),levels.end()))return {};auto planes=LiquidTetPlanes(tet);if(q<*std::max_element(levels.begin(),levels.end())){V n;double d;if(affine(tet,levels,n,d))planes.push_back({n,q-d});}return planes;}

void LiquidCap(const LiquidTet& t,const std::array<double,4>& q,double level,MeshData& mesh,V n){
 double lo=*std::min_element(q.begin(),q.end()),hi=*std::max_element(q.begin(),q.end());
 if(level<=lo||level>hi+1e-9)return;
 double d;if(glm::length(n)<1e-14&&!affine(t,q,n,d))return;
 if(glm::length(n)<1e-14)return;
 n=glm::normalize(n);Face cap;
 auto add=[&](V p){for(V v:cap)if(glm::length(v-p)<1e-9)return;cap.push_back(p);};
 for(unsigned i=0;i<4;++i){if(std::abs(q[i]-level)<1e-10)add(t[i]);for(unsigned j=i+1;j<4;++j)if((q[i]<level)!=(q[j]<level)&&q[i]!=q[j])add(t[i]+(t[j]-t[i])*((level-q[i])/(q[j]-q[i])));}
 if(cap.size()<3)return;
 V centre(0);for(V p:cap)centre+=p;centre/=double(cap.size());
 V u=glm::normalize(cap.front()-centre),v=glm::cross(n,u);
 std::sort(cap.begin(),cap.end(),[&](V a,V b){return std::atan2(glm::dot(a-centre,v),glm::dot(a-centre,u))<std::atan2(glm::dot(b-centre,v),glm::dot(b-centre,u));});
 for(size_t i=1;i+1<cap.size();++i)for(V p:{cap[0],cap[i],cap[i+1]})mesh.vertices.push_back({glm::vec3(p),glm::vec3(n),{0,0}});
}

LiquidGeometry LiquidSimplifyConvex(const LiquidGeometry& geometry){
 if(!LiquidConvexCell(geometry))return geometry;
 std::vector<V> vertices;for(auto& tet:geometry.cells)for(V p:tet){bool found=false;for(V v:vertices)found|=glm::length(v-p)<1e-9;if(!found)vertices.push_back(p);}
 Poly boundary;std::vector<glm::dvec4> planes;
 for(auto& tet:geometry.cells)for(auto plane:LiquidTetPlanes(tet)){
  bool supports=true;for(V p:vertices)if(glm::dot(V(plane),p)>plane.w+1e-8){supports=false;break;}if(!supports)continue;
  bool found=false;for(auto other:planes)found|=glm::length(V(other)-V(plane))<1e-8&&std::abs(other.w-plane.w)<1e-8;if(found)continue;planes.push_back(plane);
  V normal=V(plane),reference=std::abs(normal.x)<.8?V(1,0,0):V(0,1,0),u=glm::normalize(glm::cross(reference,normal)),v=glm::cross(normal,u);
  struct Point {glm::dvec2 uv;V position;};std::vector<Point> points;
  for(V p:vertices)if(std::abs(glm::dot(normal,p)-plane.w)<1e-8)points.push_back({{glm::dot(u,p),glm::dot(v,p)},p});
  std::sort(points.begin(),points.end(),[](auto a,auto b){return a.uv.x==b.uv.x?a.uv.y<b.uv.y:a.uv.x<b.uv.x;});if(points.size()<3)continue;
  auto turn=[](const Point& a,const Point& b,const Point& c){auto x=b.uv-a.uv,y=c.uv-a.uv;return x.x*y.y-x.y*y.x;};
  std::vector<Point> hull;
  for(auto p:points){while(hull.size()>1&&turn(hull[hull.size()-2],hull.back(),p)<=1e-14)hull.pop_back();hull.push_back(p);}
  size_t lower=hull.size();for(size_t i=points.size()-1;i-->0;){auto p=points[i];while(hull.size()>lower&&turn(hull[hull.size()-2],hull.back(),p)<=1e-14)hull.pop_back();hull.push_back(p);}hull.pop_back();
  if(hull.size()<3)continue;
  Face face;for(auto p:hull)face.push_back(p.position);boundary.push_back(std::move(face));
 }
 LiquidGeometry result;appendTets(boundary,result);double before=0,after=0;for(auto& tet:geometry.cells)before+=tetVolume(tet);for(auto& tet:result.cells)after+=tetVolume(tet);
 if(result.cells.empty()||result.cells.size()>=geometry.cells.size()||std::abs(after-before)>std::max(1e-10,before*1e-10))return geometry;
 return result;
}
