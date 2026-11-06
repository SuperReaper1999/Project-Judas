#include "CutExactResidual.h"
#include <cmath>
#include <limits>
namespace judas::volume_grid {
namespace {
struct Expansion {
 std::vector<double> terms;double radius=0.;
 bool Add(double value){
  if(!std::isfinite(value))return false;
  std::vector<double> next;next.reserve(terms.size()+1);
  for(double e:terms){double sum=value+e,part=sum-value,error=(value-(sum-part))+(e-part);if(!std::isfinite(sum))return false;if(error!=0.)next.push_back(error);value=sum;}
  if(value!=0.||next.empty())next.push_back(value);
  terms=std::move(next);return true;
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
  for(double term:terms){r.lo=std::nextafter(r.lo+term,-std::numeric_limits<double>::infinity());r.hi=std::nextafter(r.hi+term,std::numeric_limits<double>::infinity());}r.lo=std::nextafter(r.lo-radius,-std::numeric_limits<double>::infinity());r.hi=std::nextafter(r.hi+radius,std::numeric_limits<double>::infinity());return r;
 }
};
}
bool BoundCutDirectedResidual(const std::vector<double>& capacity,const std::vector<CutDirectedEdge>& edges,
 const std::vector<CutCompensated>& exterior,const std::vector<CutCompensated>& rhs,const std::vector<CutCompensated>& state,
 std::vector<CutResidualInterval>& output,std::string& error){
 const auto n=capacity.size();if(exterior.size()!=n||rhs.size()!=n||state.size()!=n){error="exact residual dimensions mismatch";return false;}
 for(std::size_t i=0;i<n;++i)if(!std::isfinite(capacity[i])||!std::isfinite(exterior[i].hi)||!std::isfinite(exterior[i].lo)||!std::isfinite(rhs[i].hi)||!std::isfinite(rhs[i].lo)||!std::isfinite(state[i].hi)||!std::isfinite(state[i].lo)){error="nonfinite exact residual input";return false;}
 std::vector<Expansion> rows(n);
 for(std::size_t i=0;i<n;++i)if(!rows[i].Add(rhs[i].hi)||!rows[i].Add(rhs[i].lo)||!rows[i].Product(CutCompensated(capacity[i]),state[i],-1.)||!rows[i].Product(exterior[i],state[i],-1.)){error="exact residual arithmetic outside supported range";return false;}
 for(auto& e:edges){if(e.from>=n||e.to>=n){error="exact residual edge outside field";return false;}if(!rows[e.from].Product(e.amount,state[e.from],-1.)||!rows[e.to].Product(e.amount,state[e.from],1.)){error="exact residual product outside supported range";return false;}}
 std::vector<CutResidualInterval> next(n);for(std::size_t i=0;i<n;++i)next[i]=rows[i].Bounds();output=std::move(next);error.clear();return true;
}
}
