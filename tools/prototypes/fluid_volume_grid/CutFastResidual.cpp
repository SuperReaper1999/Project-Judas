#include "CutFastResidual.h"
#include <array>
#include <cmath>
#include <limits>
namespace judas::volume_grid {
namespace {
struct Expansion {
 std::array<double,16> local{};
 std::vector<double> spill;
 std::size_t count=0;
 double radius=0.;
 double& At(std::size_t i){return spill.empty()?local[i]:spill[i];}
 double At(std::size_t i)const{return spill.empty()?local[i]:spill[i];}
 bool Add(double value){
  if(!std::isfinite(value))return false;
  // Error-free expansion growth in place. Written terms are always behind
  // the read position. Overflow grows storage; it never truncates the audit.
  if(count+1>local.size()&&spill.empty())spill.assign(local.begin(),local.end());
  if(!spill.empty()&&spill.size()<count+1)spill.resize(count+1);
  std::size_t kept=0;
  for(std::size_t i=0;i<count;++i){double e=At(i),sum=value+e,part=sum-value,error=(value-(sum-part))+(e-part);if(!std::isfinite(sum))return false;if(error!=0.)At(kept++)=error;value=sum;}
  if(value!=0.||kept==0)At(kept++)=value;
  count=kept;return true;
 }
 bool Product(CutCompensated a,CutCompensated b,double sign){
  for(double x:{a.hi,a.lo})for(double y:{b.hi,b.lo}){
   if(x==0.||y==0.)continue;
   double product=x*y;
   if(!std::isfinite(product))return false;
   if(std::abs(product)<std::ldexp(1.,-900))radius=std::nextafter(radius+std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::infinity());
   double error=std::fma(x,y,-product);if(!Add(sign*error)||!Add(sign*product))return false;
  }return true;
 }
 CutResidualInterval Bounds()const{
  CutResidualInterval r;
  for(std::size_t i=0;i<count;++i){double term=At(i);r.lo=std::nextafter(r.lo+term,-std::numeric_limits<double>::infinity());r.hi=std::nextafter(r.hi+term,std::numeric_limits<double>::infinity());}r.lo=std::nextafter(r.lo-radius,-std::numeric_limits<double>::infinity());r.hi=std::nextafter(r.hi+radius,std::numeric_limits<double>::infinity());return r;
 }
};
}
static bool ExactRows(const std::vector<double>& capacity,const std::vector<CutDirectedEdge>& edges,
 const std::vector<CutCompensated>& exterior,const std::vector<CutCompensated>& rhs,const std::vector<CutCompensated>& state,
 std::vector<CutResidualInterval>& output,std::string& error,const std::vector<bool>* mask=nullptr){
 const auto n=capacity.size();if(exterior.size()!=n||rhs.size()!=n||state.size()!=n){error="exact residual dimensions mismatch";return false;}
 for(std::size_t i=0;i<n;++i)if(!std::isfinite(capacity[i])||!std::isfinite(exterior[i].hi)||!std::isfinite(exterior[i].lo)||!std::isfinite(rhs[i].hi)||!std::isfinite(rhs[i].lo)||!std::isfinite(state[i].hi)||!std::isfinite(state[i].lo)){error="nonfinite exact residual input";return false;}
 std::vector<Expansion> rows(n);
 for(std::size_t i=0;i<n;++i)if((!mask||(*mask)[i])&&(!rows[i].Add(rhs[i].hi)||!rows[i].Add(rhs[i].lo)||!rows[i].Product(CutCompensated(capacity[i]),state[i],-1.)||!rows[i].Product(exterior[i],state[i],-1.))){error="exact residual arithmetic outside supported range";return false;}
 for(auto& e:edges){if(e.from>=n||e.to>=n){error="exact residual edge outside field";return false;}if(((!mask||(*mask)[e.from])&&!rows[e.from].Product(e.amount,state[e.from],-1.))||((!mask||(*mask)[e.to])&&!rows[e.to].Product(e.amount,state[e.from],1.))){error="exact residual product outside supported range";return false;}}
 std::vector<CutResidualInterval> next(n);for(std::size_t i=0;i<n;++i)if(!mask||(*mask)[i])next[i]=rows[i].Bounds();output=std::move(next);error.clear();return true;
}

bool BoundCutDirectedResidualFast(const std::vector<double>& c,const std::vector<CutDirectedEdge>& e,
 const std::vector<CutCompensated>& o,const std::vector<CutCompensated>& b,const std::vector<CutCompensated>& x,
 std::vector<CutResidualInterval>& result,std::string& error){return ExactRows(c,e,o,b,x,result,error);}

bool BoundCutDirectedResidualFiltered(const std::vector<double>& capacity,
 const std::vector<CutDirectedEdge>& edges,const std::vector<CutCompensated>& exterior,
 const std::vector<CutCompensated>& rhs,const std::vector<CutCompensated>& state,
 const std::vector<double>& targets,std::vector<CutResidualInterval>& output,std::string& error,bool proveNegative){
 const auto n=capacity.size();
 if(exterior.size()!=n||rhs.size()!=n||state.size()!=n||targets.size()!=n){error="filtered residual dimensions mismatch";return false;}
 struct Sum {
  long double value=0.,magnitude=0.;std::size_t operations=0;
  void Add(long double x){value+=x;magnitude+=std::abs(x);operations+=2;}
  void Product(CutCompensated a,CutCompensated b,int sign){
   for(double x:{a.hi,a.lo})for(double y:{b.hi,b.lo}){Add(static_cast<long double>(x)*static_cast<long double>(y)*sign);operations+=2;}
  }
  CutResidualInterval Bounds()const{
   // Standard gamma_n bound with a factor-four margin covers product,
   // accumulation, magnitude accumulation and bound evaluation rounding.
   // Each possible subnormal error is included even on binary64 long double.
   const long double eps=std::numeric_limits<long double>::epsilon();
   if(operations*eps>.125L||!std::isfinite(value)||!std::isfinite(magnitude))return {-INFINITY,INFINITY};
   long double radius=4.L*(operations+8)*(eps*magnitude+std::numeric_limits<long double>::denorm_min());
   return {std::nextafter(double(value-radius),-INFINITY),std::nextafter(double(value+radius),INFINITY)};
  }
 };
 std::vector<Sum> sums(n);
 for(std::size_t i=0;i<n;++i){
  if(!std::isfinite(capacity[i])||!std::isfinite(exterior[i].hi)||!std::isfinite(exterior[i].lo)||!std::isfinite(rhs[i].hi)||!std::isfinite(rhs[i].lo)||!std::isfinite(state[i].hi)||!std::isfinite(state[i].lo)){error="nonfinite filtered residual input";return false;}
  sums[i].Add(rhs[i].hi);sums[i].Add(rhs[i].lo);sums[i].Product(CutCompensated(capacity[i]),state[i],-1);sums[i].Product(exterior[i],state[i],-1);
 }
 for(const auto& e:edges){
  if(e.from>=n||e.to>=n){error="filtered residual edge outside field";return false;}
  sums[e.from].Product(e.amount,state[e.from],-1);sums[e.to].Product(e.amount,state[e.from],1);
 }
 std::vector<CutResidualInterval> result(n);std::vector<bool> ambiguous(n);bool any=false;
 for(std::size_t i=0;i<n;++i){result[i]=sums[i].Bounds();ambiguous[i]=proveNegative?!(result[i].hi<0.):!(std::max(std::abs(result[i].lo),std::abs(result[i].hi))<=targets[i]);any=any||ambiguous[i];}
 if(any){std::vector<CutResidualInterval> exact;if(!ExactRows(capacity,edges,exterior,rhs,state,exact,error,&ambiguous))return false;for(std::size_t i=0;i<n;++i)if(ambiguous[i])result[i]=exact[i];}
 output=std::move(result);error.clear();return true;
}
}
