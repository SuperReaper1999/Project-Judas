#include "CutCoupledKrylov.h"
#include <algorithm>
#include <cmath>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
double Dot(const std::vector<double>& a,const std::vector<double>& b){long double s=0.;for(std::size_t i=0;i<a.size();++i)s+=static_cast<long double>(a[i])*b[i];return double(s);}
bool Finite(const std::vector<double>& v,std::size_t n){if(v.size()!=n)return false;for(auto x:v)if(!std::isfinite(x))return false;return true;}
}
bool SolveCutCoupledKrylov(const std::vector<double>& rhs,const CutKrylovOperator& apply,
 const CutKrylovOperator& precondition,std::vector<Real>& output,CutKrylovStats& stats,
 std::string& error,int width,int cycles,double tolerance){
 const auto n=rhs.size();if(!n||!Finite(rhs,n)||width<1||cycles<1||!(tolerance>0.&&tolerance<1.)){error="invalid coupled Krylov request";return false;}
 const double initial=std::sqrt(Dot(rhs,rhs));if(!std::isfinite(initial)){error="invalid coupled Krylov norm";return false;}
 std::vector<Real> x(n);std::vector<double> residual=rhs;CutKrylovStats work;
 if(initial==0.){output=std::move(x);stats={0,0,0.};error.clear();return true;}
 auto action=[&](const CutKrylovOperator& operation,const std::vector<double>& v,std::vector<double>& result){
  if(!operation(v,result))return false;
  if(!Finite(result,n)){error="invalid coupled Krylov operator result";return false;}return true;
 };
 for(int restart=0;restart<cycles;++restart){
  const double norm=std::sqrt(Dot(residual,residual));std::vector<std::vector<double>> basis(1,residual),directions;
  for(auto& a:basis[0])a/=norm;
  std::vector<std::vector<double>> h(width+1,std::vector<double>(width));std::vector<double> cosine(width),sine(width),projection(width+1);projection[0]=norm;int used=0;
  for(int k=0;k<width;++k){std::vector<double> d,w;if(!action(precondition,basis[k],d)||!action(apply,d,w)){stats=work;return false;}++work.applications;directions.push_back(std::move(d));
   for(int pass=0;pass<2;++pass)for(int j=0;j<=k;++j){double value=Dot(w,basis[j]);h[j][k]+=value;for(std::size_t i=0;i<n;++i)w[i]-=value*basis[j][i];}
   h[k+1][k]=std::sqrt(Dot(w,w));if(h[k+1][k]>0.){for(auto& a:w)a/=h[k+1][k];basis.push_back(std::move(w));}
   for(int j=0;j<k;++j){double a=h[j][k],b=h[j+1][k];h[j][k]=cosine[j]*a+sine[j]*b;h[j+1][k]=-sine[j]*a+cosine[j]*b;}
   double length=std::hypot(h[k][k],h[k+1][k]);if(!(length>0.&&std::isfinite(length))){error="singular coupled Krylov direction";stats=work;return false;}
   cosine[k]=h[k][k]/length;sine[k]=h[k+1][k]/length;h[k][k]=length;h[k+1][k]=0.;projection[k+1]=-sine[k]*projection[k];projection[k]*=cosine[k];used=k+1;
   if(std::abs(projection[k+1])<=initial*tolerance||basis.size()<=std::size_t(k+1))break;
  }
  std::vector<double> weights(used);for(int k=used;k-->0;){double value=projection[k];for(int j=k+1;j<used;++j)value-=h[k][j]*weights[j];weights[k]=value/h[k][k];}
  for(int k=0;k<used;++k)for(std::size_t i=0;i<n;++i)x[i]=x[i]+Real(directions[k][i])*Real(weights[k]);
  std::vector<double> point(n),actual;for(std::size_t i=0;i<n;++i)point[i]=x[i].Value();
  if(!action(apply,point,actual)){stats=work;return false;}++work.applications;
  for(std::size_t i=0;i<n;++i)residual[i]=rhs[i]-actual[i];
  work.restarts=restart;work.relativeResidual=std::sqrt(Dot(residual,residual))/initial;stats=work;
  if(work.relativeResidual<=tolerance){output=std::move(x);error.clear();return true;}
 }
 error="coupled Krylov budget exhausted with original relative residual="+std::to_string(work.relativeResidual);return false;
}
}
