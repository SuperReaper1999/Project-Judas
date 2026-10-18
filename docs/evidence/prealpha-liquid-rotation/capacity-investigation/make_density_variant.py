from pathlib import Path
folder=Path('.cache/prealpha-liquid-rotation/capacity-investigation');s=(folder/'LiquidSurface.before.cpp').read_text();start=s.index('using Polynomial=');end=s.index('\n}\nglm::dvec2 LiquidSurfaceData::Chart',start)
replacement=r'''using Density = std::array<long double, 3>;
// The volume density of an affine coordinate on a tetrahedron is a positive
// quadratic B-spline, scaled by 3*volume/(q3-q0). Build it in Bernstein form.
Density tetraDensity(const std::array<double, 4>& q, double a, double b) {
 std::array<Density, 3> basis{};
 for(unsigned i=0;i<3;++i)if(q[i]<=a&&b<=q[i+1]&&q[i]<q[i+1])basis[i][0]=1;
 for(unsigned degree=1;degree<=2;++degree)for(unsigned i=0;i+degree<3;++i){
  Density next{};
  auto term=[&](const Density& source,long double w0,long double w1){
   for(unsigned k=0;k<=degree;++k){
    if(k<degree)next[k]+=(long double(degree-k)/degree)*w0*source[k];
    if(k)next[k]+=(long double(k)/degree)*w1*source[k-1];
   }
  };
  // A nonzero lower-degree basis has support covering this entire interval.
  // Its linear weights are nonnegative; repeated knots contribute no term.
  long double first=(long double)q[i+degree]-q[i];
  if(first>0&&std::any_of(basis[i].begin(),basis[i].end(),[](auto v){return v!=0;}))
   term(basis[i],((long double)a-q[i])/first,((long double)b-q[i])/first);
  long double second=(long double)q[i+degree+1]-q[i+1];
  if(second>0&&std::any_of(basis[i+1].begin(),basis[i+1].end(),[](auto v){return v!=0;}))
   term(basis[i+1],((long double)q[i+degree+1]-a)/second,((long double)q[i+degree+1]-b)/second);
  basis[i]=next;
 }
 return basis[0];
}
Density restrictedDensity(const Density& c,long double a,long double b){
 const long double l0=(1-a)*c[0]+a*c[1],l1=(1-a)*c[1]+a*c[2];
 const long double r0=(1-b)*c[0]+b*c[1],r1=(1-b)*c[1]+b*c[2];
 return {(1-a)*l0+a*l1,(1-b)*l0+b*l1,(1-b)*r0+b*r1};
}
bool curve(const LiquidGeometry& g,const GravityEquilibrium& e,LiquidBasinData& out){
 if(g.cells.empty())return false;
 struct Tet {std::array<double,4> q;long double volume;};std::vector<Tet> tetrahedra;
 LiquidBasinData b;b.geometry=g;b.equilibrium=e;std::vector<double> knots;
 for(auto& t:g.cells){Tet item;for(unsigned i=0;i<4;++i)item.q[i]=e.Coordinate(t[i]);std::sort(item.q.begin(),item.q.end());
  item.volume=std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))/6;
  if(item.q[0]==item.q[3])return false;
  tetrahedra.push_back(item);knots.insert(knots.end(),item.q.begin(),item.q.end());
 }
 std::sort(knots.begin(),knots.end());knots.erase(std::unique(knots.begin(),knots.end()),knots.end());
 if(knots.size()<2)return false;
 const size_t intervals=knots.size()-1;std::vector<Density> tree(intervals*4);
 auto add=[&](auto&& self,size_t node,size_t left,size_t right,size_t first,size_t last,double origin,double finish,const Density& c)->void{
  if(right<=first||left>=last)return;
  if(first<=left&&right<=last){long double width=(long double)finish-origin;auto contribution=restrictedDensity(c,((long double)knots[left]-origin)/width,((long double)knots[right]-origin)/width);for(unsigned k=0;k<3;++k)tree[node][k]+=contribution[k];return;}
  size_t middle=(left+right)/2;if(first<middle)self(self,node*2+1,left,middle,first,last,origin,finish,c);if(last>middle)self(self,node*2+2,middle,right,first,last,origin,finish,c);
 };
 for(auto& t:tetrahedra)for(unsigned i=1;i<4;++i){if(t.q[i]<=t.q[i-1])continue;auto c=tetraDensity(t.q,t.q[i-1],t.q[i]);for(auto& value:c)value*=3*t.volume/((long double)t.q[3]-t.q[0]);
  size_t first=size_t(std::lower_bound(knots.begin(),knots.end(),t.q[i-1])-knots.begin()),last=size_t(std::lower_bound(knots.begin(),knots.end(),t.q[i])-knots.begin());
  add(add,0,0,intervals,first,last,t.q[i-1],t.q[i],c);
 }
 long double cumulative=0;
 auto resolve=[&](auto&& self,size_t node,size_t left,size_t right)->void{auto c=tree[node];
  if(right-left==1){long double width=(long double)knots[right]-knots[left];b.curve.push_back({knots[left],double(cumulative)});b.capacityPolynomials.push_back({double(cumulative),double(width*c[0]),double(width*(c[1]-c[0])),double(width*(c[0]-2*c[1]+c[2])/3)});cumulative+=width*(c[0]+c[1]+c[2])/3;return;}
  size_t middle=(left+right)/2;long double split=((long double)knots[middle]-knots[left])/((long double)knots[right]-knots[left]);
  auto a=restrictedDensity(c,0,split),d=restrictedDensity(c,split,1);for(unsigned k=0;k<3;++k){tree[node*2+1][k]+=a[k];tree[node*2+2][k]+=d[k];}
  self(self,node*2+1,left,middle);self(self,node*2+2,middle,right);
 };
 resolve(resolve,0,0,intervals);b.capacity=double(cumulative);b.curve.push_back({knots.back(),b.capacity});
 b.volumeTolerance=std::max(1e-12,b.capacity*1e-10);b.heightTolerance=1e-12;
 out=std::move(b);return true;
}'''
replacement=replacement.replace('long double(degree-k)','static_cast<long double>(degree-k)').replace('long double(k)','static_cast<long double>(k)')
(folder/'LiquidSurface.density.cpp').write_text(s[:start]+replacement+s[end:])
for name in ['seeded','probe']:
 s=(folder/(name+'.cpp')).read_text().replace('#include "LiquidSurface.cpp"','#include "LiquidSurface.density.cpp"');(folder/(name+'-density.cpp')).write_text(s)
