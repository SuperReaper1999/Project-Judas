#include "CutFastResidual.h"
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 std::size_t count;std::cin>>count;std::cout<<std::setprecision(17);
 for(std::size_t sample=0;sample<count;++sample){
  std::size_t n,m;std::cin>>n>>m;
  std::vector<double> capacity(n),target(n);std::vector<CutCompensated> exterior(n),rhs(n),state(n);std::vector<CutDirectedEdge> edges(m);
  for(std::size_t i=0;i<n;++i)std::cin>>capacity[i]>>exterior[i].hi>>exterior[i].lo>>rhs[i].hi>>rhs[i].lo>>state[i].hi>>state[i].lo>>target[i];
  for(auto& e:edges)std::cin>>e.from>>e.to>>e.amount.hi>>e.amount.lo;
  std::vector<CutResidualInterval> bounds;std::string error;
  bool ok=BoundCutDirectedResidualFiltered(capacity,edges,exterior,rhs,state,target,bounds,error);
  std::cout<<ok<<'\n';if(ok)for(auto r:bounds)std::cout<<r.lo<<' '<<r.hi<<'\n';
 }
 return std::cin.fail()?1:0;
}
