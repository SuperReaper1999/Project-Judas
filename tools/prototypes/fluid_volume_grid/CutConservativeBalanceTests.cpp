#include "CutConservativeBalance.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 int checks=0,failures=0;
 auto check=[&](bool ok,const char* name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
 using R=CutCompensated;std::string error;std::vector<R> result;
 std::vector<double> capacity{1.,2.};std::vector<CutTransportFace> faces{{0,1,.3}};std::vector<CutTransportGasPort> ports{{1,.1}};
 std::vector<R> residual{.2,-.1},derivative{.03,.04},dq{.02},dv{-.03};
 check(ApplyCutConservativeBalance(capacity,faces,ports,residual,result,error),"conservative balance assembles");
 check(std::abs((result[0]-(R(.2)+R(.3)*R(.2))).Value())<1e-28&&std::abs((result[1]-(R(-.1)-R(.3)*R(.2)+R(.1)*R(-.1)/R(2.))).Value())<1e-28,"matches independent two-cell balance");
 for(double q:{.3,-.3,0.}){
  faces[0].volume=q;if(q==0.)derivative={0.,0.};
  std::vector<R> tangent,plus,minus;check(ApplyCutConservativeBalanceTangent(capacity,faces,ports,residual,derivative,dq,dv,tangent,error),"full tangent assembles for either donor direction");
  R epsilon=1e-6;auto pf=faces,mf=faces;auto pp=ports,mp=ports;std::vector<R> pr=residual,mr=residual;
  pf[0].volume=pf[0].volume+epsilon*dq[0];mf[0].volume=mf[0].volume-epsilon*dq[0];pp[0].volume=pp[0].volume+epsilon*dv[0];mp[0].volume=mp[0].volume-epsilon*dv[0];
  for(std::size_t i=0;i<2;++i){pr[i]=pr[i]+epsilon*derivative[i];mr[i]=mr[i]-epsilon*derivative[i];}
  bool ok=ApplyCutConservativeBalance(capacity,pf,pp,pr,plus,error)&&ApplyCutConservativeBalance(capacity,mf,mp,mr,minus,error);
  for(std::size_t i=0;ok&&i<2;++i)ok=std::abs(((plus[i]-minus[i])/(R(2.)*epsilon)-tangent[i]).Value())<1e-22;
  check(ok,"tangent agrees with independent centered differences including dT term");
 }
 faces[0].volume=.3;derivative={.03,.04};std::vector<R> correct,frozen;
 bool ok=ApplyCutConservativeBalanceTangent(capacity,faces,ports,residual,derivative,dq,dv,correct,error)&&ApplyCutConservativeBalance(capacity,faces,ports,derivative,frozen,error);
 check(ok&&std::abs((correct[0]-frozen[0]).Value())>.001,"negative control detects missing derivative of transport");
 result={-7.};check(!ApplyCutConservativeBalance({0.,2.},faces,ports,residual,result,error)&&result.size()==1&&result[0].Value()==-7.,"invalid capacity rejects transactionally");
 auto invalid=faces;invalid[0].right=4;check(!ApplyCutConservativeBalance(capacity,invalid,ports,residual,result,error)&&result[0].Value()==-7.,"invalid topology preserves previous output");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
