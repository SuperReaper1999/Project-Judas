#include "BrowserSurface.h"
#include <iostream>
#include <limits>
#include <utility>
#include <stdexcept>
int main(){unsigned checks=0;auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("browser surface budget");};try{
    for(auto pair:{std::pair<double,double>{800,600},{361,801},{1200,540},{8000,8000},{1,1},{1e308,1e308}})
      for(double ratio:{.5,1.,2.,3.,8.}){
        const auto s=ChooseBrowserSurface(pair.first,pair.second,ratio);
        check(s.width>0&&s.height>0&&s.width<=4096&&s.height<=4096);
        check(static_cast<long long>(s.width)*s.height<=2*1024*1024);
      }
    check(ChooseBrowserSurface(800,600,1).width==800);
    check(ChooseBrowserSurface(800,600,2).width>800);
    check(ChooseBrowserSurface(800,600,3).width==ChooseBrowserSurface(800,600,2).width);
    check(ChooseBrowserSurface(800,600,0).width==800);
    for(double bad:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
      check(ChooseBrowserSurface(bad,600,1).width==0);check(ChooseBrowserSurface(800,bad,1).height==0);
    }
    std::cout<<"Browser surface: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
