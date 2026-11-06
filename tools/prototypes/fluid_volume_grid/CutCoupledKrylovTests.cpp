#include "CutCoupledKrylov.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
namespace {int checks=0,failures=0;void Check(bool ok,const char* s){++checks;if(!ok){++failures;std::cerr<<s<<'\n';}}}
int main(){
 std::vector<double> rhs{1.,2.,3.,4.,5.,6.,7.,8.};std::vector<CutCompensated> result{CutCompensated(777.)};CutKrylovStats stats;std::string error;
 auto identity=[](const std::vector<double>& x,std::vector<double>& y){y=x;return true;};
 auto matrix=[](const std::vector<double>& x,std::vector<double>& y){y=x;for(std::size_t i=0;i<x.size();++i){y[i]*=2.;if(i)y[i]-=.8*x[i-1];if(i+1<x.size())y[i]+=.1*x[i+1];}return true;};
 Check(!SolveCutCoupledKrylov(rhs,matrix,identity,result,stats,error,2,1,1e-10),"one cycle cannot be reported as a converged Newton correction");
 Check(result.size()==1&&result[0].Value()==777.,"failed linear solve leaves prior correction intact");
 Check(SolveCutCoupledKrylov(rhs,matrix,identity,result,stats,error,2,40,1e-10),"restart converges on independently specified nonsymmetric matrix");
 Check(stats.restarts>0&&stats.relativeResidual<=1e-10,"restart and verified true residual are reported");
 std::vector<double> point,actual;for(auto x:result)point.push_back(x.Value());matrix(point,actual);double maximum=0.;for(std::size_t i=0;i<rhs.size();++i)maximum=std::max(maximum,std::abs(actual[i]-rhs[i]));
 Check(maximum<1e-8,"independent original matrix agrees with correction");
 auto invalid=[](const std::vector<double>&,std::vector<double>& y){y={std::numeric_limits<double>::quiet_NaN()};return true;};
 Check(!SolveCutCoupledKrylov(rhs,invalid,identity,result,stats,error),"invalid callback output rejects");
 Check(SolveCutCoupledKrylov(std::vector<double>(8),matrix,identity,result,stats,error)&&stats.applications==0,"zero RHS has an exact zero correction");
 auto zero=[](const std::vector<double>& x,std::vector<double>& y){y.assign(x.size(),0.);return true;};
 Check(!SolveCutCoupledKrylov(rhs,zero,identity,result,stats,error),"singular nonzero system rejects");
 Check(!SolveCutCoupledKrylov(rhs,matrix,identity,result,stats,error,0),"invalid work budget rejects");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
