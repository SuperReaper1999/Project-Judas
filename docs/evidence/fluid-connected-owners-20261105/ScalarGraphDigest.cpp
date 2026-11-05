#include "CutHydroacousticGraph.h"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
int main(){
 std::cout<<std::hexfloat;
 for(std::size_t n=1;n<=48;++n){
  std::vector<double> storage(n),rhs(n),answer(n);std::vector<CutHydroacousticGraphLink> links;
  for(std::size_t i=0;i<n;++i){storage[i]=1.+double(i%5)/7.;answer[i]=.125*double(i)+std::sin(.3*double(i));rhs[i]=storage[i]*answer[i];}
  for(std::size_t i=1;i<n;++i){double g=.1+double(i%7)/3.;links.push_back({i-1,i,g});double q=g*(answer[i-1]-answer[i]);rhs[i-1]+=q;rhs[i]-=q;}
  CutHydroacousticGraphResult result;std::string error;
  if(!SolveCutHydroacousticGraph(storage,links,rhs,1e-13,result,error)){std::cerr<<error;return 1;}
  std::cout<<"GRAPH "<<n<<" iterations="<<result.iterations<<" residual="<<result.maximumResidual;
  for(double p:result.pressure)std::cout<<' '<<p;
  std::cout<<'\n';
 }
}
