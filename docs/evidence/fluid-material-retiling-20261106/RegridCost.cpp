#define main MaterialRegridControlMain
#include "WaterMaterialRegridTests.cpp"
#undef main
#include <chrono>
#include <iomanip>
int main(){std::cout<<std::setprecision(17);auto field=Cell();for(int cuts:{1,4,8,16}){
 WaterMaterialState source;for(int x=0;x<cuts;++x)for(int y=0;y<cuts;++y){source.fragments.push_back(Box(double(x)/cuts,double(x+1)/cuts,double(y)/cuts,double(y+1)/cuts,0.,.5,true,0));source.fragments.push_back(Box(double(x)/cuts,double(x+1)/cuts,double(y)/cuts,double(y+1)/cuts,.5,1.,false,1));}
 for(int repeat=0;repeat<5;++repeat){WaterMaterialState after;WaterMaterialRegridStats stats;std::string error;auto start=std::chrono::steady_clock::now();bool ok=RegridWaterMaterial(field,source,after,stats,error);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"RETILE_COST input="<<source.fragments.size()<<" output="<<after.fragments.size()<<" repeat="<<repeat<<" ok="<<ok<<" work="<<stats.work<<" ms="<<ms<<"\n";if(!ok||after.fragments.size()!=2){std::cerr<<error;return 1;}
 }
}return 0;}
