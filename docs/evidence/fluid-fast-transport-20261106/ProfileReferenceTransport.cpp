#include "CutDirectedTransport.h"
#include "CutExactResidual.h"
#include <cmath>
#include <sstream>
#include <limits>
#include <algorithm>
#include <chrono>
#include <iostream>
namespace judas::volume_grid {
namespace {
struct Profile { double prepare=0.,solve=0.,barrierAudit=0.,residualAudit=0.,sweeps=0.; long builds=0,calls=0,iterations=0,sweepCount=0; ~Profile(){std::cerr<<"TRANSPORT_PROFILE prepare_ms="<<prepare<<" solve_ms="<<solve<<" barrier_audit_ms="<<barrierAudit<<" residual_audit_ms="<<residualAudit<<" sweeps_ms="<<sweeps<<" builds="<<builds<<" solves="<<calls<<" refinements="<<iterations<<" sweepCount="<<sweepCount<<"\n";} } profile;
struct Timer {double& ms;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();~Timer(){ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}};
using R=CutCompensated;bool Fail(std::string& e,const char* s){e=s;return false;}}
bool CutDirectedTransport::Build(const std::vector<double>& capacity,const std::vector<CutDirectedEdge>& edges,const std::vector<R>& exterior,std::string& error){
 CutDirectedTransport prepared;if(!prepared.Prepare(capacity,edges,exterior,error))return false;*this=std::move(prepared);return true;
}
bool CutDirectedTransport::Prepare(const std::vector<double>& capacity,const std::vector<CutDirectedEdge>& edges,const std::vector<R>& exterior,std::string& error){
 Timer timer{profile.prepare};++profile.builds;
 const auto n=capacity.size();if(n==0||exterior.size()!=n)return Fail(error,"invalid directed transport dimensions");
 barrierCache_.clear();capacity_=capacity;edges_=edges;exterior_=exterior;ground_.resize(n);in_.assign(n,{});out_.assign(n,{});removed_.assign(n,false);steps_.clear();
 for(std::size_t i=0;i<n;++i){if(!(capacity[i]>0.&&std::isfinite(capacity[i])&&exterior[i].Value()>=0.&&std::isfinite(exterior[i].Value())))return Fail(error,"invalid directed transport capacity");ground_[i]=R(capacity[i])+exterior[i];}
 for(auto& e:edges){if(e.from>=n||e.to>=n||e.from==e.to||!(e.amount.Value()>=0.&&std::isfinite(e.amount.Value())))return Fail(error,"invalid directed transport edge");if(e.amount.Value()==0.)continue;out_[e.from][e.to]=out_[e.from][e.to]+e.amount;in_[e.to][e.from]=in_[e.to][e.from]+e.amount;}
 for(std::size_t i=0;i<n;++i){R diagonal=ground_[i];for(auto& [j,q]:out_[i]){(void)j;diagonal=diagonal+q;}
  if(diagonal.Value()<4.*capacity[i]||in_[i].size()*out_[i].size()>64)continue;
  Step step{i,diagonal,{in_[i].begin(),in_[i].end()},{out_[i].begin(),out_[i].end()}};
  for(auto& [k,qki]:step.in){out_[k].erase(i);ground_[k]=ground_[k]+qki*(ground_[i]/diagonal);}
  for(auto& [j,qij]:step.out)in_[j].erase(i);
  for(auto& [k,qki]:step.in)for(auto& [j,qij]:step.out)if(k!=j){R q=qki*(qij/diagonal);out_[k][j]=out_[k][j]+q;in_[j][k]=in_[j][k]+q;}
  in_[i].clear();out_[i].clear();removed_[i]=true;steps_.push_back(std::move(step));
 }
 error.clear();return true;
}
bool CutDirectedTransport::SolveInternal(const std::vector<R>& rhs,double budget,std::vector<R>& output,std::string& error,const std::vector<R>* barrier)const{
 Timer timer{profile.solve};++profile.calls;
 const auto n=capacity_.size();if(rhs.size()!=n||!(budget>0.&&std::isfinite(budget)))return Fail(error,"invalid directed transport right hand side");
 for(auto value:rhs)if(!std::isfinite(value.hi)||!std::isfinite(value.lo))return Fail(error,"nonfinite directed transport source");
 std::vector<double> lower(n);double barrierMax=0.;
 if(barrier){Timer auditTimer{profile.barrierAudit};std::vector<CutResidualInterval> az;std::vector<R> zero(n);
  if(!BoundCutDirectedResidual(capacity_,edges_,exterior_,zero,*barrier,az,error))return false;
  for(std::size_t i=0;i<n;++i){lower[i]=-az[i].hi;barrierMax=std::max(barrierMax,std::nextafter(std::abs((*barrier)[i].Value()),std::numeric_limits<double>::infinity()));if(!(lower[i]>0.&&(*barrier)[i].Value()>0.))return Fail(error,"transport forward-error barrier is not positive");}
 }

 std::vector<R> result(n);
 double lastTotal=0.;
 for(int refinement=0;refinement<12;++refinement){++profile.iterations;
  std::vector<R> residual=rhs;
  for(std::size_t i=0;i<n;++i)residual[i]=residual[i]-(R(capacity_[i])+exterior_[i])*result[i];
  for(auto& e:edges_){R q=e.amount*result[e.from];residual[e.from]=residual[e.from]-q;residual[e.to]=residual[e.to]+q;}
  double total=0.;for(auto r:residual)total+=std::abs(r.Value());
  lastTotal=total;
  bool certified=total<=budget;
  if(barrier){Timer auditTimer{profile.residualAudit};
   std::vector<CutResidualInterval> bounds;
   if(!BoundCutDirectedResidual(capacity_,edges_,exterior_,rhs,result,bounds,error))return false;
   double ratio=0.,totalUpper=0.;
   for(std::size_t i=0;i<n;++i){double magnitude=std::max(std::abs(bounds[i].lo),std::abs(bounds[i].hi));totalUpper=std::nextafter(totalUpper+magnitude,std::numeric_limits<double>::infinity());ratio=std::max(ratio,std::nextafter(magnitude/lower[i],std::numeric_limits<double>::infinity()));}
   const double minimumCapacity=*std::min_element(capacity_.begin(),capacity_.end());
   certified=std::nextafter(ratio*barrierMax,std::numeric_limits<double>::infinity())<=budget||std::nextafter(totalUpper/minimumCapacity,std::numeric_limits<double>::infinity())<=budget;
  }
  if(certified&&(refinement>0||std::all_of(rhs.begin(),rhs.end(),[](R x){return x.hi==0.&&x.lo==0.;}))){output=std::move(result);error.clear();return true;}
  for(auto& step:steps_)for(auto& [j,q]:step.out)residual[j]=residual[j]+(q/step.diagonal)*residual[step.node];
  std::vector<R> delta(n),diagonal=ground_;
  for(std::size_t i=0;i<n;++i)for(auto& [j,q]:out_[i]){(void)j;diagonal[i]=diagonal[i]+q;}
  bool solved=false;
  {Timer sweepTimer{profile.sweeps};for(int sweep=0;sweep<20000;++sweep){++profile.sweepCount;
   for(std::size_t i=0;i<n;++i)if(!removed_[i]){R v=residual[i];for(auto& [k,q]:in_[i])v=v+q*delta[k];delta[i]=v/diagonal[i];}
   double reducedError=0.;for(std::size_t i=0;i<n;++i)if(!removed_[i]){R r=residual[i]-diagonal[i]*delta[i];for(auto& [k,q]:in_[i])r=r+q*delta[k];reducedError+=std::abs(r.Value());}
   if(reducedError<=std::max(barrier?total*1e-8:budget*.01,total*1e-5)){solved=true;break;}
  }
  }if(!solved)return Fail(error,"directed reduced transport exhausted");
  for(std::size_t j=steps_.size();j-->0;){auto& step=steps_[j];R v=residual[step.node];for(auto& [k,q]:step.in)v=v+q*delta[k];delta[step.node]=v/step.diagonal;}
  for(std::size_t i=0;i<n;++i)result[i]=result[i]+delta[i];
 }
 std::ostringstream report;report.precision(17);report<<"directed original quantity audit exhausted total="<<lastTotal<<" budget="<<budget;error=report.str();return false;
}
bool CutDirectedTransport::Solve(const std::vector<R>& rhs,double budget,std::vector<R>& out,std::string& error)const{return SolveInternal(rhs,budget,out,error);}
bool CutDirectedTransport::SolveFraction(const std::vector<R>& rhs,double budget,std::vector<R>& out,std::string& error){
 std::vector<R> capacity(capacity_.begin(),capacity_.end()),barrier;double sum=0.;for(double c:capacity_)sum+=c;
 if(barrierCache_.empty()){if(!SolveInternal(capacity,sum*1e-14,barrier,error))return false;barrierCache_=std::move(barrier);}
 return SolveInternal(rhs,budget,out,error,&barrierCache_);
}

}
