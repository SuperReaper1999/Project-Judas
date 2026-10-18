#include "PerformanceProfiler.h"
#include "LiquidSurface.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <limits>
#include <queue>
#include <functional>
#include <istream>
#include <ostream>


namespace {
using V=glm::dvec3;
double capacity(const LiquidBasinData& b,double q){
 if(q<=b.curve.front().q)return 0;
 if(q>=b.curve.back().q)return b.capacity;
 size_t i=size_t(std::upper_bound(b.curve.begin(),b.curve.end(),q,[](double v,auto p){return v<p.q;})-b.curve.begin()-1);
 double t=(q-b.curve[i].q)/(b.curve[i+1].q-b.curve[i].q);
 if(!b.capacityPolynomials.empty()){auto c=b.capacityPolynomials[i];return c[0]+t*(c[1]+t*(c[2]+t*c[3]));}
 return glm::mix(b.curve[i].volume,b.curve[i+1].volume,t);
}
double slope(const LiquidBasinData& b,double q){
 size_t i=size_t(std::upper_bound(b.curve.begin(),b.curve.end(),q,[](double v,auto p){return v<p.q;})-b.curve.begin());
 i=std::clamp(i,size_t(1),b.curve.size()-1)-1;
 double width=b.curve[i+1].q-b.curve[i].q;
 if(!b.capacityPolynomials.empty()){auto c=b.capacityPolynomials[i];double t=std::clamp((q-b.curve[i].q)/width,0.,1.);return (c[1]+t*(2*c[2]+3*t*c[3]))/width;}
 return (b.curve[i+1].volume-b.curve[i].volume)/width;
}
std::vector<glm::dvec4> planes(const LiquidSurfaceData& d,unsigned col,unsigned row){double u0=glm::mix(d.minimum.x,d.maximum.x,double(col)/d.settings.columns),u1=glm::mix(d.minimum.x,d.maximum.x,double(col+1)/d.settings.columns),v0=glm::mix(d.minimum.y,d.maximum.y,double(row)/d.settings.rows),v1=glm::mix(d.minimum.y,d.maximum.y,double(row+1)/d.settings.rows);if(d.equilibrium.kind==GravityEquilibrium::Kind::Plane)return {{-d.x,-u0},{d.x,u1},{-d.y,-v0},{d.y,v1}};std::vector<glm::dvec4> p;for(V n:{-d.x+u0*d.axis,d.x-u1*d.axis,-d.y+v0*d.axis,d.y-v1*d.axis})p.push_back({n,glm::dot(n,d.equilibrium.center)});return p;}
// The distribution of a linear coordinate over a tetrahedron is a cubic
// simplex spline. This positive recurrence also handles repeated coordinates;
// it avoids fragile divided differences and repeated polygon allocations.
// Face-space subtraction keeps actual open apertures, including a solid whose
// boundary lies exactly on the shared face (zero 3D overlap, finite blocked area).
std::vector<LiquidPolygon> subtractFace(const std::vector<LiquidPolygon>& faces,const std::vector<glm::dvec4>& solid){
 auto clip=[](const LiquidPolygon& p,glm::dvec4 plane,bool inside){LiquidPolygon out;
  for(size_t i=0;i<p.size();++i){V a=p[i],b=p[(i+1)%p.size()];double x=glm::dot(V(plane),a)-plane.w,y=glm::dot(V(plane),b)-plane.w;bool ia=inside?x<=1e-12:x>1e-12,ib=inside?y<=1e-12:y>1e-12;
   if(ia)out.push_back(a);
   if(ia!=ib)out.push_back(a+(b-a)*(x/(x-y)));
  }return out;
 };
 auto area=[](const LiquidPolygon& p){double result=0;for(size_t i=1;i+1<p.size();++i)result+=glm::length(glm::cross(p[i]-p[0],p[i+1]-p[0]))*.5;return result;};
 std::vector<LiquidPolygon> result;
 for(auto& p:faces){bool outside=false;for(auto plane:solid){bool all=true;for(V v:p)all&=glm::dot(V(plane),v)>plane.w+1e-12;outside|=all;}if(outside){result.push_back(p);continue;}
  auto overlap=p;for(auto plane:solid)overlap=clip(overlap,plane,true);if(area(overlap)<=1e-12){result.push_back(p);continue;}
  auto remaining=p;for(auto plane:solid){auto portion=clip(remaining,plane,false);if(area(portion)>1e-12)result.push_back(std::move(portion));remaining=clip(remaining,plane,true);if(remaining.empty())break;}
 }
 return result;
}
using Polynomial=std::array<long double,4>;
// Stable simplex-spline recurrence in the interval's local coordinate t. No
// global cubic coefficients or divided differences of almost equal heights.
Polynomial tetraPolynomial(const std::array<double,4>& q,double a,double h){
 std::array<Polynomial,4> f{};long double middle=static_cast<long double>(a)+static_cast<long double>(h)*.5L;
 for(unsigned i=0;i<4;++i)f[i][0]=middle>=q[i]?1:0;
 for(unsigned width=1;width<4;++width)for(unsigned i=0;i+width<4;++i){
  if(middle<=q[i])f[i]={};
  else if(middle>=q[i+width])f[i]={1,0,0,0};
  else{Polynomial next{};long double denominator=q[i+width]-q[i];
   for(unsigned k=0;k<4;++k){next[k]+=((a-q[i])*f[i][k]+(q[i+width]-a)*f[i+1][k])/denominator;
    if(k)next[k]+=h*(f[i][k-1]-f[i+1][k-1])/denominator;
   }f[i]=next;
  }
 }
 return f[0];
}
bool curve(const LiquidGeometry& g,const GravityEquilibrium& e,LiquidBasinData& out){
 if(g.cells.empty())return false;
 struct Tet {std::array<double,4> q;long double volume;};std::vector<Tet> tetrahedra;
 LiquidBasinData b;b.geometry=g;b.equilibrium=e;std::vector<double> knots;
 for(auto& t:g.cells){Tet item;for(unsigned i=0;i<4;++i)item.q[i]=e.Coordinate(t[i]);std::sort(item.q.begin(),item.q.end());
  item.volume=std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))/6;
  if(item.q[0]==item.q[3])return false;
  b.capacity+=double(item.volume);tetrahedra.push_back(item);knots.insert(knots.end(),item.q.begin(),item.q.end());
 }
 std::sort(knots.begin(),knots.end());knots.erase(std::unique(knots.begin(),knots.end()),knots.end());
 if(knots.size()<2)return false;
 // Range-add the positive local cubic pieces into a balanced interval tree.
 // Each node uses its own normalized coordinate, keeping coefficients bounded.
 // This is O(tetrahedra * log(knots)), without derivative cancellation or the
 // O(tetrahedra * knots) active-set assembly cost measured during body motion.
 const size_t intervals=knots.size()-1;std::vector<Polynomial> tree(intervals*4);
 auto restricted=[](const Polynomial& c,long double a,long double width){return Polynomial{
  c[0]+a*(c[1]+a*(c[2]+a*c[3])),width*(c[1]+a*(2*c[2]+a*3*c[3])),width*width*(c[2]+3*a*c[3]),width*width*width*c[3]};};
 auto add=[&](auto&& self,size_t node,size_t left,size_t right,size_t first,size_t last,double origin,double width,const Polynomial& c)->void{
  if(right<=first||left>=last)return;
  if(first<=left&&right<=last){auto contribution=restricted(c,(static_cast<long double>(knots[left])-origin)/width,(static_cast<long double>(knots[right])-knots[left])/width);for(unsigned k=0;k<4;++k)tree[node][k]+=contribution[k];return;}
  size_t middle=(left+right)/2;if(first<middle)self(self,node*2+1,left,middle,first,last,origin,width,c);if(last>middle)self(self,node*2+2,middle,right,first,last,origin,width,c);
 };
 for(auto& t:tetrahedra){for(unsigned i=1;i<4;++i){double h=t.q[i]-t.q[i-1];if(h<=0)continue;auto c=tetraPolynomial(t.q,t.q[i-1],h);for(auto& value:c)value*=t.volume;
   size_t first=size_t(std::lower_bound(knots.begin(),knots.end(),t.q[i-1])-knots.begin()),last=size_t(std::lower_bound(knots.begin(),knots.end(),t.q[i])-knots.begin());
   add(add,0,0,intervals,first,last,t.q[i-1],h,c);
  }
  size_t last=size_t(std::lower_bound(knots.begin(),knots.end(),t.q[3])-knots.begin());
  if(last<intervals)add(add,0,0,intervals,last,intervals,knots.front(),knots.back()-knots.front(),{t.volume,0,0,0});
 }
 auto resolve=[&](auto&& self,size_t node,size_t left,size_t right)->void{auto c=tree[node];
  if(right-left==1){b.curve.push_back({knots[left],double(c[0])});b.capacityPolynomials.push_back({double(c[0]),double(c[1]),double(c[2]),double(c[3])});return;}
  size_t middle=(left+right)/2;long double split=(static_cast<long double>(knots[middle])-knots[left])/(static_cast<long double>(knots[right])-knots[left]);
  auto a=restricted(c,0,split),d=restricted(c,split,1-split);for(unsigned k=0;k<4;++k){tree[node*2+1][k]+=a[k];tree[node*2+2][k]+=d[k];}
  self(self,node*2+1,left,middle);self(self,node*2+2,middle,right);
 };
 resolve(resolve,0,0,intervals);b.curve.push_back({knots.back(),b.capacity});b.curve.front().volume=0;
 b.volumeTolerance=std::max(1e-12,b.capacity*1e-10);b.heightTolerance=1e-12;
 out=std::move(b);return true;
}
}
glm::dvec2 LiquidSurfaceData::Chart(V p)const{if(equilibrium.kind==GravityEquilibrium::Kind::Plane)return {glm::dot(p,x),glm::dot(p,y)};V v=p-equilibrium.center;double h=glm::dot(v,axis);return {glm::dot(v,x)/h,glm::dot(v,y)/h};}
V LiquidSurfaceData::Point(glm::dvec2 uv,double q)const{return equilibrium.kind==GravityEquilibrium::Kind::Plane?x*uv.x+y*uv.y+axis*q:equilibrium.center+glm::normalize(axis+x*uv.x+y*uv.y)*q;}
int LiquidSurfaceData::Cell(V p)const{if(!std::isfinite(glm::dot(p,p)))return -1;auto uv=Chart(p);if(uv.x<minimum.x||uv.y<minimum.y||uv.x>maximum.x||uv.y>maximum.y)return -1;unsigned i=std::min(settings.columns-1,unsigned((uv.x-minimum.x)/(maximum.x-minimum.x)*settings.columns)),j=std::min(settings.rows-1,unsigned((uv.y-minimum.y)/(maximum.y-minimum.y)*settings.rows));return grid[j*settings.columns+i];}
bool BakeLiquidSurface(const LiquidBasinData& b,const LiquidSurfaceSettings& s,std::shared_ptr<const LiquidSurfaceData>& out,std::string& error,const LiquidGeometry* topology){if(!s.enabled){out.reset();return true;}if(s.columns<2||s.rows<2||s.columns>32||s.rows>32||s.friction<0||s.tolerance<=0||s.iterations<4||s.iterations>512){error="invalid bounded surface settings";return false;}auto d=std::make_shared<LiquidSurfaceData>();d->settings=s;d->equilibrium=b.equilibrium;V centre(0);for(auto& t:b.geometry.cells)for(V v:t)centre+=v;centre/=double(b.geometry.cells.size()*4);d->axis=b.equilibrium.Up(centre);V reference=std::abs(d->axis.x)<.8?V(1,0,0):V(0,0,1);d->x=glm::normalize(reference-d->axis*glm::dot(reference,d->axis));d->y=glm::cross(d->axis,d->x);d->minimum={1e30,1e30};d->maximum={-1e30,-1e30};for(auto& t:b.geometry.cells)for(V p:t){if(b.equilibrium.kind==GravityEquilibrium::Kind::Radius&&glm::dot(p-b.equilibrium.center,d->axis)<=0){error="surface needs a single radial cap smaller than a hemisphere";return false;}auto uv=d->Chart(p);d->minimum=glm::min(d->minimum,uv);d->maximum=glm::max(d->maximum,uv);}d->grid.assign(s.columns*s.rows,-1);const bool convexTopology=LiquidConvexCell(topology?*topology:b.geometry);
 for(unsigned j=0;j<s.rows;++j)for(unsigned i=0;i<s.columns;++i){auto g=LiquidCutGeometry(b.geometry,planes(*d,i,j));if(g.cells.empty())continue;auto authored=topology?LiquidCutGeometry(*topology,planes(*d,i,j)):g;if(!convexTopology&&!LiquidConvexCell(authored)){error="disconnected/nonconvex subregions within surface cell "+std::to_string(i)+","+std::to_string(j)+"; refine or partition the physical basin";return false;}
 // Validate topology before quadrature. Refine the already partitioned physical
 // cell, avoiding a fan subdivision of thousands of globally refined tets.
 // The original declared radial coordinate error remains an upper bound.
 if(topology&&b.equilibrium.kind==GravityEquilibrium::Kind::Radius){double bound=0;if(!RefineLiquidGeometry(LiquidSimplifyConvex(authored),b.equilibrium,std::min(b.heightTolerance,4*b.coordinateError),g,bound,error))return false;}
 if(b.equilibrium.kind==GravityEquilibrium::Kind::Plane)g=LiquidSimplifyConvex(g);
 LiquidSurfaceCellData cell;cell.column=i;cell.row=j;cell.chart=glm::mix(d->minimum,d->maximum,glm::dvec2((i+.5)/s.columns,(j+.5)/s.rows));cell.centre=d->Point(cell.chart,(b.curve.front().q+b.curve.back().q)*.5);if(!curve(g,b.equilibrium,cell.storage)){error="empty surface cell storage";return false;}for(auto& tet:cell.storage.geometry.cells)cell.loadingGeometry.push_back(CompileLiquidTetQuery(tet,b.equilibrium));d->grid[j*s.columns+i]=int(d->cells.size());d->cells.push_back(std::move(cell));}
 for(unsigned a=0;a<d->cells.size();++a){auto& c=d->cells[a];for(unsigned direction=0;direction<2;++direction){unsigned i=c.column+(direction==0),j=c.row+(direction==1);if(i>=s.columns||j>=s.rows)continue;int other=d->grid[j*s.columns+i];if(other<0)continue;auto p=planes(*d,c.column,c.row)[direction==0?1:3];LiquidSurfaceFaceData f;f.a=a;f.b=unsigned(other);f.normal=V(p);f.offset=p.w;f.polygons=LiquidBoundary(c.storage.geometry,f.normal,f.offset);if(LiquidFaceArea(f.polygons,b.equilibrium,b.curve.back().q)<=1e-10)continue;V pa=d->Point(c.chart,b.curve.back().q),pb=d->Point(d->cells[other].chart,b.curve.back().q);f.distance=b.equilibrium.kind==GravityEquilibrium::Kind::Plane?glm::length(pa-pb):b.curve.back().q*std::acos(std::clamp(glm::dot(glm::normalize(pa-b.equilibrium.center),glm::normalize(pb-b.equilibrium.center)),-1.,1.));d->faces.push_back(std::move(f));}}
 if(d->cells.empty()){error="no occupied surface cells";return false;}out=d;return true;}
LiquidSurface::LiquidSurface(std::shared_ptr<const LiquidSurfaceData> d,double volume):data(std::move(d)){cells.resize(data->cells.size());m_adjacency.resize(cells.size());m_caps.resize(cells.size());for(size_t k=0;k<data->faces.size();++k){m_adjacency[data->faces[k].a].push_back(k);m_adjacency[data->faces[k].b].push_back(k);}m_cellSolids.resize(cells.size());m_staticSolids.resize(cells.size());m_staticGeometry.resize(cells.size());for(auto& c:data->cells){V lo(1e30),hi(-1e30);for(auto& t:c.storage.geometry.cells)for(V p:t){lo=glm::min(lo,p);hi=glm::max(hi,p);}m_bounds.push_back({lo,hi});}discharge.assign(data->faces.size(),0);for(auto& f:data->faces)m_apertures.push_back(f.polygons);m_faceAreas.assign(data->faces.size(),{std::numeric_limits<double>::quiet_NaN(),0});double low=1e30,high=-1e30;for(size_t i=0;i<cells.size();++i){cells[i].storage=data->cells[i].storage;low=std::min(low,cells[i].storage.curve.front().q);high=std::max(high,cells[i].storage.curve.back().q);}for(int k=0;k<60;++k){double q=(low+high)*.5,sum=0;for(auto& c:cells)sum+=capacity(c.storage,q);if(sum<volume)low=q;else high=q;}for(auto& c:cells)c.volume=capacity(c.storage,(low+high)*.5);double residual=volume-Volume();for(auto& c:cells)if(c.volume+residual>=0&&c.volume+residual<=c.storage.capacity){c.volume+=residual;break;}Resolve();BeginStep();}
double LiquidSurface::Volume()const{double v=0;for(auto& c:cells)v+=c.volume;return v;}
double LiquidSurface::Capacity()const{double v=0;for(auto& c:cells)v+=c.storage.capacity;return v;}
double LiquidSurface::Coordinate(unsigned i,float alpha)const{return glm::mix(cells[i].previousQ,cells[i].q,double(alpha));}
int LiquidSurface::Cell(V p)const{return data->Cell(p);}
// Aperture geometry and the exact head determine area. Plans, inversion,
// loading/splash diagnostics and retries often ask for the same pair. Memoize
// that result; geometry edits explicitly invalidate the affected face cache.
double LiquidSurface::FaceArea(size_t index,double q)const{
 auto& cache=m_faceAreas[index];if(cache.first!=q){cache={q,LiquidFaceArea(m_apertures[index],data->equilibrium,q)};}return cache.second;
}
void LiquidSurface::Resolve(){for(auto& c:cells){c.q=LiquidInverse(c.storage,c.volume);c.velocity=V(0);}std::vector<unsigned> counts(cells.size(),0);for(size_t k=0;k<discharge.size();++k){auto& f=data->faces[k];double area=FaceArea(k,std::max(cells[f.a].q,cells[f.b].q));if(area<1e-12)continue;for(unsigned i:{f.a,f.b}){V from=data->cells[f.b].centre-data->cells[f.a].centre,up=data->equilibrium.Up(data->cells[i].centre);from-=up*glm::dot(from,up);if(glm::length(from)>0){cells[i].velocity+=glm::normalize(from)*(discharge[k]/area);++counts[i];}}}for(size_t i=0;i<cells.size();++i)if(counts[i])cells[i].velocity/=double(counts[i]);}
void LiquidSurface::BeginStep(){for(auto& c:cells)c.previousQ=c.q;}
std::vector<unsigned> LiquidSurface::Component(unsigned start)const{std::vector<unsigned> out;std::vector<bool> visited(cells.size(),false);std::queue<unsigned> todo;todo.push(start);visited[start]=true;while(!todo.empty()){auto i=todo.front();todo.pop();out.push_back(i);for(size_t k:m_adjacency[i]){auto& f=data->faces[k];int j=f.a==i?int(f.b):f.b==i?int(f.a):-1;if(j<0||visited[j])continue;if(FaceArea(k,std::max(cells[i].q,cells[j].q))<1e-12)continue;visited[j]=true;todo.push(unsigned(j));}}return out;}
LiquidSurfaceAllocation LiquidSurface::Plan(double amount,std::optional<V> point)const{LiquidSurfaceAllocation plan;if(!std::isfinite(amount)||amount==0)return plan;int start=point?Cell(*point):-1;if(point&&start<0)return plan;if(start<0){double best=-1;for(unsigned i=0;i<cells.size();++i){double v=amount<0?cells[i].volume:cells[i].storage.capacity-cells[i].volume;if(v>best){best=v;start=int(i);}}}if(start<0)return plan;auto group=Component(unsigned(start));double available=0;for(auto i:group)available+=amount<0?cells[i].volume:std::max(0.,cells[i].storage.capacity-cells[i].volume);double request=std::min(std::abs(amount),available);if(request<=0)return plan;double remaining=request;for(auto i:group){double room=amount<0?cells[i].volume:std::max(0.,cells[i].storage.capacity-cells[i].volume);double change=std::min(room,point?remaining:request*room/available);plan.changes.push_back({i,change});remaining-=change;}for(auto& change:plan.changes){double room=amount<0?cells[change.first].volume:cells[change.first].storage.capacity-cells[change.first].volume;double extra=std::min(remaining,room-change.second);change.second+=extra;remaining-=extra;if(remaining<=0)break;}plan.amount=request-remaining;return plan;}
void LiquidSurface::Apply(const LiquidSurfaceAllocation& p,bool add){
 // Withdrawal carries away the donor's share of its existing transport
 // momentum. Retaining finite discharge after removing its water is not a
 // wave: it gives the next continuity solve a dry-cell outflow demand.
 // Incoming quantity starts at rest; explicitly supplied arrival impulses
 // are applied separately. Untouched faces and retained waves stay intact.
 if(!add){std::vector<double> retained(cells.size(),1);
  for(auto [i,v]:p.changes)if(cells[i].volume>0)retained[i]=(cells[i].volume-v)/cells[i].volume;
  for(size_t k=0;k<discharge.size();++k){auto& f=data->faces[k];discharge[k]*=retained[discharge[k]>=0?f.a:f.b];}
 }
 for(auto [i,v]:p.changes)cells[i].volume+=add?v:-v;
 Resolve();
}
void LiquidSurface::Impulse(V p,V impulse,double density){int i=Cell(p);if(i<0||cells[i].volume<=0)return;V up=data->equilibrium.Up(p);impulse-=up*glm::dot(impulse,up);V delta=impulse/(density*cells[i].volume);for(size_t k=0;k<discharge.size();++k){auto& f=data->faces[k];if(f.a!=unsigned(i)&&f.b!=unsigned(i))continue;V direction=glm::normalize(data->cells[f.b].centre-data->cells[f.a].centre);double area=FaceArea(k,std::max(cells[f.a].q,cells[f.b].q));discharge[k]+=area*glm::dot(delta,direction);}Resolve();}

bool LiquidSurface::Once(double dt,std::string& error){
    JUDAS_PROFILE_SCOPE("Surface pressure continuity attempt");
 const size_t n=cells.size(),nf=discharge.size();
 std::vector<double> q(n),old(n),pred(nf),weight(nf),area(nf),diagonal(n),r(n);
 double total=Volume(),target=data->settings.tolerance*std::max(1.,total);
 for(size_t i=0;i<n;++i){q[i]=cells[i].q;old[i]=cells[i].volume;}
 for(size_t f=0;f<nf;++f){
  const auto& face=data->faces[f];
  area[f]=FaceArea(f,std::max(q[face.a],q[face.b]));
  pred[f]=area[f]>1e-12?discharge[f]*std::exp(-data->settings.friction*dt):0;
  weight[f]=dt*dt*data->equilibrium.magnitude*area[f]/face.distance;
 }
 auto residual=[&](const std::vector<double>& level,std::vector<double>& result){
  for(size_t i=0;i<n;++i)result[i]=capacity(cells[i].storage,level[i])-old[i];
  for(size_t f=0;f<nf;++f){auto& face=data->faces[f];
   double transfer=dt*pred[f]+weight[f]*(level[face.a]-level[face.b]);
   result[face.a]+=transfer;result[face.b]-=transfer;
  }
  double norm=0;for(double v:result)norm=std::max(norm,std::abs(v));return norm;
 };
 auto product=[&](const std::vector<double>& v,std::vector<double>& out){
  for(size_t i=0;i<n;++i)out[i]=diagonal[i]*v[i];
  for(size_t f=0;f<nf;++f){auto& face=data->faces[f];double d=weight[f]*(v[face.a]-v[face.b]);out[face.a]+=d;out[face.b]-=d;}
 };
 bool converged=false;
 for(unsigned outer=0;outer<12;++outer){
  double norm=residual(q,r);stats.residual=norm;
  if(norm<=target){converged=true;break;}
  std::vector<double> jacobi(n),correction(n,0),rr(n),z(n),direction(n),ad(n);
  for(size_t i=0;i<n;++i){
   const auto& b=cells[i].storage;
   // At an exact storage boundary use the physical one-sided derivative.
   // Treating a newly full cell as saturated gives it almost no diagonal and
   // an ill-conditioned initial Newton correction. Outside storage, only the
   // Jacobian is regularized; C(q) remains the exact physical capacity.
   diagonal[i]=(q[i]>=b.curve.front().q&&q[i]<=b.curve.back().q)?std::max(1e-9,slope(b,q[i])):1e-9;
   jacobi[i]=diagonal[i];rr[i]=-r[i];
  }
  for(size_t f=0;f<nf;++f){jacobi[data->faces[f].a]+=weight[f];jacobi[data->faces[f].b]+=weight[f];}
  for(size_t i=0;i<n;++i){z[i]=rr[i]/jacobi[i];direction[i]=z[i];}
  double rz=std::inner_product(rr.begin(),rr.end(),z.begin(),0.);
  for(unsigned iteration=0;iteration<data->settings.iterations;++iteration){
   double maximum=0;for(double v:rr)maximum=std::max(maximum,std::abs(v));if(maximum<=target*.1)break;
   product(direction,ad);double denominator=std::inner_product(direction.begin(),direction.end(),ad.begin(),0.);
   if(!(denominator>0)||!std::isfinite(denominator)){error="surface pressure matrix is not finite positive definite";return false;}
   double alpha=rz/denominator;
   for(size_t i=0;i<n;++i){correction[i]+=alpha*direction[i];rr[i]-=alpha*ad[i];z[i]=rr[i]/jacobi[i];}
   double next=std::inner_product(rr.begin(),rr.end(),z.begin(),0.),beta=next/rz;rz=next;
   for(size_t i=0;i<n;++i)direction[i]=z[i]+beta*direction[i];
   ++stats.iterations;
  }
  std::vector<double> trial(n),trialR(n);double alpha=1;bool improved=false;
  for(unsigned search=0;search<16;++search){
   for(size_t i=0;i<n;++i)trial[i]=q[i]+alpha*correction[i];
   double next=residual(trial,trialR);
   if(std::isfinite(next)&&next<norm){q.swap(trial);improved=true;break;}
   alpha*=.5;
  }
  if(!improved){error="surface nonlinear pressure iteration did not reduce residual";return false;}
 }
 if(!converged){stats.residual=residual(q,r);if(stats.residual>target){error="surface pressure/continuity solve exceeded bounded iteration budget";return false;}}
 // Positivity belongs to the shared face transfer. Never clamp an updated cell.
 std::vector<double> flows(nf),outgoing(n,0),incoming(n,0),donor(n,1),receiver(n,1);
 for(size_t f=0;f<nf;++f){auto& face=data->faces[f];flows[f]=dt*pred[f]+weight[f]*(q[face.a]-q[face.b]);
  unsigned a=flows[f]>=0?face.a:face.b,b=flows[f]>=0?face.b:face.a;
  outgoing[a]+=std::abs(flows[f]);incoming[b]+=std::abs(flows[f]);
 }
 for(size_t i=0;i<n;++i){
  if(outgoing[i]>old[i])donor[i]=old[i]/outgoing[i];
  // Conservative receiver bound does not count simultaneous outgoing space.
  if(incoming[i]>cells[i].storage.capacity-old[i])receiver[i]=(cells[i].storage.capacity-old[i])/incoming[i];
 }
 // The proportional limits are mathematically bounded, but summing rounded
 // face products can overspend a nearly empty donor by one ulp. Consume the
 // remaining budgets before each paired debit/credit, rather than repairing
 // negative cell storage afterwards. Incoming water is accumulated separately
 // and cannot be spent again within this step.
 std::vector<double> remaining=old,room(n),received(n,0),next(n);
 for(size_t i=0;i<n;++i)room[i]=std::max(0.,cells[i].storage.capacity-old[i]);
 for(size_t f=0;f<nf;++f){auto& face=data->faces[f];
  unsigned a=flows[f]>=0?face.a:face.b,b=flows[f]>=0?face.b:face.a;
  double scale=std::min(donor[a],receiver[b]);if(scale<1)++stats.limitedFaces;
  double amount=std::min({std::abs(flows[f])*scale,remaining[a],room[b]});
  remaining[a]-=amount;room[b]-=amount;received[b]+=amount;
  discharge[f]=(flows[f]>=0?amount:-amount)/dt;
 }
 for(size_t i=0;i<n;++i)next[i]=remaining[i]+received[i];
 for(size_t i=0;i<n;++i)if(!std::isfinite(next[i])||next[i]<0||next[i]>cells[i].storage.capacity+target){error="surface accepted face transfers violate physical storage";return false;}
 for(size_t i=0;i<n;++i)cells[i].volume=next[i];
 stats.partitionError=Volume()-total;Resolve();return true;
}
bool LiquidSurface::Advance(double dt,unsigned depth,std::string& error){
 struct Motion {double volume,q;V velocity;};std::vector<Motion> previous;previous.reserve(cells.size());for(auto& c:cells)previous.push_back({c.volume,c.q,c.velocity});
 auto previousDischarge=discharge;
 auto restore=[&](){for(size_t i=0;i<cells.size();++i){cells[i].volume=previous[i].volume;cells[i].q=previous[i].q;cells[i].velocity=previous[i].velocity;}discharge=previousDischarge;};
 if(Once(dt,error))return true;
 restore();if(depth==3)return false;
 ++stats.retries;
 if(Advance(dt*.5,depth+1,error)&&Advance(dt*.5,depth+1,error))return true;
 restore();return false;
}

bool LiquidSurface::Step(double dt,std::string& error){
    JUDAS_PROFILE_SCOPE("Dynamic surface solve");
 stats={};if(!enabled)return true;
 if(!std::isfinite(dt)||dt<=0){error="surface timestep must be finite and positive";return false;}
 auto start=std::chrono::steady_clock::now();bool result=Advance(dt,0,error);
 JUDAS_PROFILE_COUNTER("Surface cells",double(cells.size()),ProfileCounterMode::Sum);
 JUDAS_PROFILE_COUNTER("Surface faces",double(discharge.size()),ProfileCounterMode::Sum);
 JUDAS_PROFILE_COUNTER("Surface solve iterations",double(stats.iterations),ProfileCounterMode::Sum);
 JUDAS_PROFILE_COUNTER("Surface solve residual max",stats.residual,ProfileCounterMode::Maximum);
 stats.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 if(result)error.clear();
 return result;
}
glm::dvec3 LiquidSurface::Normal(unsigned i,float)const{
 // Finite-volume levels are piecewise constant. A planar cap's geometric
 // normal is gravity-up, not an invented smooth height-gradient normal.
 return data->equilibrium.Up(data->cells[i].centre);
}
MeshData LiquidSurface::Mesh(float alpha)const{
    JUDAS_PROFILE_SCOPE("Water surface mesh");
 MeshData mesh;for(unsigned i=0;i<cells.size();++i){
  double q=Coordinate(i,alpha);
  auto& cache=m_caps[i];auto& geometry=cells[i].storage.geometry;
  if(cache.first!=cells[i].geometryRevision){cache.second.clear();cache.second.reserve(geometry.cells.size());
   for(auto& tet:geometry.cells){CapTet cap;for(unsigned j=0;j<4;++j)cap.coordinates[j]=data->equilibrium.Coordinate(tet[j]);
    auto range=std::minmax_element(cap.coordinates.begin(),cap.coordinates.end());auto planes=LiquidOccupiedPlanes(tet,data->equilibrium,(*range.first+*range.second)*.5);cap.normal=planes.size()==5?glm::normalize(V(planes.back())):data->equilibrium.Up(tet[0]);cache.second.push_back(cap);}
   cache.first=cells[i].geometryRevision;
  }
  for(size_t j=0;j<geometry.cells.size();++j){auto& tet=geometry.cells[j];auto& cap=cache.second[j];auto& levels=cap.coordinates;
   auto range=std::minmax_element(levels.begin(),levels.end());if(q<=*range.first||q>*range.second+1e-9)continue;
   bool atVertex=false;for(double level:levels)atVertex|=std::abs(level-q)<1e-10;
   if(atVertex){LiquidCap(tet,levels,q,mesh,cap.normal);continue;}
   unsigned mask=0;for(unsigned a=0;a<4;++a)if(levels[a]<q)mask|=1u<<a;
   auto point=[&](std::pair<unsigned,unsigned> edge){auto [a,b]=edge;return tet[a]+(tet[b]-tet[a])*((q-levels[a])/(levels[b]-levels[a]));};
   if(cap.mask!=mask){cap.mask=mask;cap.count=0;
    for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)if(((mask>>a)&1)!=((mask>>b)&1))cap.edges[cap.count++]={a,b};
    if(cap.count<3)continue;
    V centre(0);for(unsigned a=0;a<cap.count;++a)centre+=point(cap.edges[a]);centre/=double(cap.count);
    V u=glm::normalize(point(cap.edges[0])-centre),v=glm::cross(cap.normal,u);
    std::sort(cap.edges.begin(),cap.edges.begin()+cap.count,[&](auto a,auto b){V x=point(a)-centre,y=point(b)-centre;return std::atan2(glm::dot(x,v),glm::dot(x,u))<std::atan2(glm::dot(y,v),glm::dot(y,u));});
   }
   // Within a vertex-coordinate interval, a parallel plane meets the same
   // convex edges in the same cyclic order. Reuse that topology, evaluating
   // the actual intersections at this frame's interpolated coordinate.
   for(unsigned a=1;a+1<cap.count;++a)for(unsigned index:{0u,a,a+1})mesh.vertices.push_back({glm::vec3(point(cap.edges[index])),glm::vec3(cap.normal),{0,0}});
  }

 }
 // Draw exposed steps between neighbouring FV levels. These are the actual
 // occupied shared-face bands, closing gaps without smoothing physical levels.
 for(size_t k=0;k<data->faces.size();++k){auto& face=data->faces[k];double a=Coordinate(face.a,alpha),b=Coordinate(face.b,alpha);
  if(std::abs(a-b)<1e-10)continue;
  auto clip=[&](LiquidPolygon polygon,double level,bool below){LiquidPolygon result;
   for(size_t i=0;i<polygon.size();++i){V x=polygon[i],y=polygon[(i+1)%polygon.size()];double sx=data->equilibrium.Coordinate(x)-level,sy=data->equilibrium.Coordinate(y)-level;
    bool ix=below?sx<=0:sx>=0,iy=below?sy<=0:sy>=0;if(ix)result.push_back(x);if(ix!=iy)result.push_back(x+(y-x)*(sx/(sx-sy)));
   }return result;
  };
  V normal=glm::normalize(face.normal)*(a>b?1.:-1.);
  for(auto polygon:m_apertures[k]){polygon=clip(clip(polygon,std::max(a,b),true),std::min(a,b),false);
   for(size_t i=1;i+1<polygon.size();++i)for(V p:{polygon[0],polygon[i],polygon[i+1]})mesh.vertices.push_back({glm::vec3(p),glm::vec3(normal),{0,0}});
  }
 }
 return mesh;
}
double LiquidSurface::Energy()const{
 double result=0;for(auto& c:cells){
  double integral=0;
  for(size_t i=1;i<c.storage.curve.size();++i){auto a=c.storage.curve[i-1],b=c.storage.curve[i];if(c.q<=a.q)break;
   double width=b.q-a.q,t=std::min(1.,(c.q-a.q)/width);
   if(!c.storage.capacityPolynomials.empty()){auto k=c.storage.capacityPolynomials[i-1];integral+=width*(k[0]*t+k[1]*t*t/2+k[2]*t*t*t/3+k[3]*t*t*t*t/4);}
   else integral+=width*(a.volume*t+(b.volume-a.volume)*t*t/2);
  }
  result+=data->equilibrium.magnitude*(c.q*c.volume-integral);
 }
 for(size_t f=0;f<discharge.size();++f){auto& face=data->faces[f];double a=FaceArea(f,std::max(cells[face.a].q,cells[face.b].q));if(a>1e-12)result+=.5*face.distance*discharge[f]*discharge[f]/a;}
 return result;
}
double LiquidSurface::SetSolids(const std::vector<std::vector<glm::dvec4>>& solids,std::string& error,unsigned staticCount){
    JUDAS_PROFILE_SCOPE("Solid excluded storage rebuild");
 JUDAS_PROFILE_COUNTER("Surface solid storage calls",1,ProfileCounterMode::Sum);
 if(m_geometryCacheValid&&solids==m_solids)return 0;
 std::vector<std::pair<size_t,LiquidBasinData>> changed;std::vector<bool> touched(cells.size(),false);
 for(size_t i=0;i<cells.size();++i){
  std::vector<std::vector<glm::dvec4>> relevant,stationary,moving;
  for(size_t bodyIndex=0;bodyIndex<solids.size();++bodyIndex){auto& body=solids[bodyIndex];bool separated=false;
   for(auto plane:body){V nearest;for(unsigned axis=0;axis<3;++axis)nearest[axis]=plane[axis]>=0?m_bounds[i].first[axis]:m_bounds[i].second[axis];if(glm::dot(V(plane),nearest)>plane.w+1e-12){separated=true;break;}}
   if(!separated){relevant.push_back(body);(bodyIndex<staticCount?stationary:moving).push_back(body);}
  }
  if(m_geometryCacheValid&&relevant==m_cellSolids[i])continue;
  auto geometry=data->cells[i].storage.geometry;touched[i]=true;
  if(!m_geometryCacheValid||stationary!=m_staticSolids[i]){m_staticGeometry[i]=LiquidSubtractSolids(geometry,stationary);m_staticSolids[i]=stationary;}
  geometry=LiquidSubtractSolids(m_staticGeometry[i],moving);
  if(geometry.cells.size()>262144){error="moving-solid geometry exceeded bounded cell budget";m_geometryCacheValid=false;return -1;}
  m_cellSolids[i]=std::move(relevant);
  if(geometry.cells==cells[i].storage.geometry.cells)continue;
  LiquidBasinData storage;
  if(geometry.cells.empty()){
   storage.geometry.cells.clear();storage.capacity=0;storage.equilibrium=data->equilibrium;
   storage.curve={{data->cells[i].storage.curve.front().q,0},{data->cells[i].storage.curve.back().q,0}};
  }else if(!curve(geometry,data->equilibrium,storage)){error="moving-solid storage construction failed";m_geometryCacheValid=false;return -1;}
  changed.push_back({i,std::move(storage)});

 }
 std::vector<std::pair<size_t,std::vector<LiquidPolygon>>> changedApertures;
 for(size_t k=0;k<data->faces.size();++k){auto& face=data->faces[k];if(!touched[face.a]&&!touched[face.b])continue;
  auto polygons=face.polygons;std::vector<std::vector<glm::dvec4>> solidsOnFace=m_cellSolids[face.a];for(auto& body:m_cellSolids[face.b])if(std::find(solidsOnFace.begin(),solidsOnFace.end(),body)==solidsOnFace.end())solidsOnFace.push_back(body);
  for(auto& body:solidsOnFace){polygons=subtractFace(polygons,body);if(polygons.size()>65536){error="moving-solid aperture exceeded bounded budget";m_geometryCacheValid=false;return -1;}}
  changedApertures.push_back({k,std::move(polygons)});
 }
 JUDAS_PROFILE_COUNTER("Surface storage cells rebaked",double(changed.size()),ProfileCounterMode::Sum);
 JUDAS_PROFILE_COUNTER("Surface apertures rebuilt",double(changedApertures.size()),ProfileCounterMode::Sum);
 for(auto& [i,storage]:changed){cells[i].storage=std::move(storage);++cells[i].geometryRevision;}
 for(auto& [k,polygons]:changedApertures){m_apertures[k]=std::move(polygons);m_faceAreas[k].first=std::numeric_limits<double>::quiet_NaN();}
 auto& next=cells;
 // Displaced liquid is transferred along wet face paths. Every intermediate
 // cell receives/debits the identical amount; this is an atomic redistribution,
 // not global averaging or a change to owner quantity.
 for(unsigned i=0;i<next.size();++i){double excess=std::max(0.,next[i].volume-next[i].storage.capacity);
  if(excess<=0)continue;
  std::vector<int> parent(next.size(),-1);std::vector<unsigned> order;std::queue<unsigned> todo;todo.push(i);parent[i]=int(i);
  while(!todo.empty()){auto a=todo.front();todo.pop();order.push_back(a);
   for(size_t k:m_adjacency[a]){auto& f=data->faces[k];int b=f.a==a?int(f.b):f.b==a?int(f.a):-1;if(b<0||parent[b]>=0)continue;
    if(FaceArea(k,std::max(cells[a].q,cells[b].q))<=1e-12)continue;
    parent[b]=int(a);todo.push(unsigned(b));
   }
  }
  for(auto j:order){if(j==i)continue;double delta=std::min(excess,std::max(0.,next[j].storage.capacity-next[j].volume));
   for(unsigned to=j;to!=i;to=unsigned(parent[to])){unsigned from=unsigned(parent[to]);next[from].volume-=delta;next[to].volume+=delta;}
   excess-=delta;if(excess<=0)break;
  }
 }
 double excess=0;for(auto& c:next)excess+=std::max(0.,c.volume-c.storage.capacity);
 m_solids=solids;m_geometryCacheValid=true;
 Resolve();error.clear();return excess;
}

bool SaveLiquidSurface(std::ostream& out,const LiquidSurfaceData& d){
 auto vector=[&](V v){out<<v.x<<' '<<v.y<<' '<<v.z<<' ';};auto& s=d.settings;
 out<<"Surface1 "<<s.columns<<' '<<s.rows<<' '<<s.friction<<' '<<s.tolerance<<' '<<s.iterations<<' '<<s.splashSpeed<<' '<<s.splashFraction<<' '<<s.parcelBudget<<' ';
 vector(d.axis);vector(d.x);vector(d.y);out<<d.minimum.x<<' '<<d.minimum.y<<' '<<d.maximum.x<<' '<<d.maximum.y<<' '<<d.cells.size()<<' '<<d.faces.size()<<'\n';
 for(int i:d.grid)out<<i<<' ';
 out<<'\n';
 for(auto& c:d.cells){auto& b=c.storage;out<<c.column<<' '<<c.row<<' ';vector(c.centre);out<<c.chart.x<<' '<<c.chart.y<<' '<<b.capacity<<' '<<b.volumeTolerance<<' '<<b.heightTolerance<<' '<<b.geometry.cells.size()<<' '<<b.curve.size()<<'\n';
  for(auto& t:b.geometry.cells){for(V p:t)vector(p);out<<'\n';}for(auto v:b.curve)out<<v.q<<' '<<v.volume<<'\n';
 }
 for(auto& f:d.faces){out<<f.a<<' '<<f.b<<' ';vector(f.normal);out<<f.offset<<' '<<f.distance<<' '<<f.polygons.size()<<'\n';for(auto& p:f.polygons){out<<p.size()<<' ';for(V v:p)vector(v);out<<'\n';}}
 return bool(out);
}
bool ReadLiquidSurface(std::istream& in,const GravityEquilibrium& eq,std::shared_ptr<const LiquidSurfaceData>& result,std::string& error){
 auto d=std::make_shared<LiquidSurfaceData>();auto& s=d->settings;s.enabled=true;d->equilibrium=eq;
 auto vector=[&](V& v){return bool(in>>v.x>>v.y>>v.z)&&std::isfinite(glm::dot(v,v));};
 std::string magic;size_t nc,nf;auto fail=[&](const char* reason="header/geometry bounds"){error=std::string("invalid bounded Surface1 bake: ")+reason;return false;};
 if(!(in>>magic>>s.columns>>s.rows>>s.friction>>s.tolerance>>s.iterations>>s.splashSpeed>>s.splashFraction>>s.parcelBudget)||magic!="Surface1"||s.columns<2||s.rows<2||s.columns>32||s.rows>32||s.iterations<4||s.iterations>512||s.friction<0||s.tolerance<=0||s.splashSpeed<=0||s.splashFraction<0||s.splashFraction>1||s.parcelBudget>1024||!std::isfinite(s.friction+s.tolerance+s.splashSpeed+s.splashFraction))return fail();
 if(!vector(d->axis)||!vector(d->x)||!vector(d->y)||!(in>>d->minimum.x>>d->minimum.y>>d->maximum.x>>d->maximum.y>>nc>>nf)||nc==0||nc>1024||nf>2048||glm::any(glm::lessThanEqual(d->maximum,d->minimum))||!std::isfinite(glm::dot(d->minimum,d->maximum)))return fail();
 if(std::abs(glm::length(d->axis)-1)>1e-6||std::abs(glm::length(d->x)-1)>1e-6||std::abs(glm::length(d->y)-1)>1e-6||std::abs(glm::dot(d->x,d->axis))+std::abs(glm::dot(d->y,d->axis))+std::abs(glm::dot(d->x,d->y))>1e-6)return fail();
 d->grid.resize(s.columns*s.rows);for(auto& index:d->grid)if(!(in>>index)||index<-1||index>=int(nc))return fail();
 d->cells.resize(nc);size_t totalTets=0;
 for(unsigned i=0;i<nc;++i){auto& c=d->cells[i];auto& b=c.storage;size_t nt,nk;b.equilibrium=eq;
  if(!(in>>c.column>>c.row)||!vector(c.centre)||!(in>>c.chart.x>>c.chart.y>>b.capacity>>b.volumeTolerance>>b.heightTolerance>>nt>>nk)||c.column>=s.columns||c.row>=s.rows||d->grid[c.row*s.columns+c.column]!=int(i)||nt==0||nt>262144||nk<2||nk>65536||b.capacity<=0||b.volumeTolerance<=0||b.heightTolerance<=0||!std::isfinite(b.capacity+b.volumeTolerance+b.heightTolerance+c.chart.x+c.chart.y))return fail();
  totalTets+=nt;if(totalTets>2097152)return fail();b.geometry.cells.resize(nt);b.curve.resize(nk);
  for(auto& t:b.geometry.cells){for(auto& p:t)if(!vector(p))return fail();if(std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))<=1e-15)return fail("degenerate tetrahedron");}
  double prevQ=-1e30,prevV=-1;
  for(auto& p:b.curve){if(!(in>>p.q>>p.volume)||!std::isfinite(p.q+p.volume)||p.q<=prevQ||p.volume<prevV)return fail("nonmonotonic capacity knots");prevQ=p.q;prevV=p.volume;}
  if(b.curve.front().volume!=0||std::abs(b.curve.back().volume-b.capacity)>1e-9)return fail("capacity endpoints");
  LiquidBasinData exact;if(!curve(b.geometry,eq,exact))return fail("exact geometry capacity");if(std::abs(exact.capacity-b.capacity)>std::max(1e-9,b.capacity*1e-10))return fail();b=std::move(exact);for(auto& tet:b.geometry.cells)c.loadingGeometry.push_back(CompileLiquidTetQuery(tet,eq));
 }
 d->faces.resize(nf);
 for(auto& f:d->faces){size_t np;if(!(in>>f.a>>f.b)||!vector(f.normal)||!(in>>f.offset>>f.distance>>np)||f.a>=nc||f.b>=nc||f.a==f.b||f.distance<=0||np>65536||!std::isfinite(f.offset+f.distance))return fail();
  f.polygons.resize(np);for(auto& p:f.polygons){size_t n;if(!(in>>n)||n<3||n>16)return fail();p.resize(n);for(V& v:p)if(!vector(v))return fail();}
 }
 result=std::move(d);return true;
}

double LiquidSurface::KineticEnergy(unsigned cell)const{double energy=0;for(size_t f=0;f<discharge.size();++f){auto& face=data->faces[f];if(face.a!=cell&&face.b!=cell)continue;double area=FaceArea(f,std::max(cells[face.a].q,cells[face.b].q));if(area>1e-12)energy+=.5*face.distance*discharge[f]*discharge[f]/area;}return energy;}
bool LiquidSurface::TakeKineticEnergy(unsigned cell,double amount){double energy=KineticEnergy(cell);if(amount<0||amount>energy||!std::isfinite(amount))return false;double scale=energy>0?std::sqrt(std::max(0.,1-amount/energy)):1;for(size_t f=0;f<discharge.size();++f)if(data->faces[f].a==cell||data->faces[f].b==cell)discharge[f]*=scale;Resolve();return true;}
