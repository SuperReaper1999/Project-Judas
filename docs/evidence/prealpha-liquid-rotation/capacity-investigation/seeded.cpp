#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include "LiquidSurface.cpp"
int main(){
 std::mt19937_64 random(0x91d8ab01ULL);std::uniform_real_distribution<double> sample(-1,1);int bad=0,baked=0,decoded=0;double maxdrop=0,volumeError=0;std::cout<<std::setprecision(18);
 for(unsigned n=0;n<512;++n){
  auto g=LiquidBox({-1,0,-1},{1,1,1});GravityEquilibrium e;e.magnitude=9.81;
  if(n<128){auto a=sample(random)*1e-7,c=sample(random)*1e-7;e.up=glm::normalize(V(a,1,c));}else e.up=glm::normalize(V(sample(random),sample(random),sample(random)));
  LiquidBasinData b;b.geometry=g;b.equilibrium=e;b.curve={{0,0},{1,4}};b.capacity=4;LiquidSurfaceSettings s;s.enabled=true;s.columns=s.rows=4;std::shared_ptr<const LiquidSurfaceData> data;std::string err;
  if(!BakeLiquidSurface(b,s,data,err)){std::cout<<"BAKE_FAIL "<<n<<" "<<err<<"\n";++bad;continue;}++baked;double volume=0;
  for(size_t i=0;i<data->cells.size();++i){auto& c=data->cells[i].storage;volume+=c.capacity;for(size_t j=1;j<c.curve.size();++j)if(c.curve[j].volume<c.curve[j-1].volume){++bad;double drop=c.curve[j-1].volume-c.curve[j].volume;maxdrop=std::max(maxdrop,drop);std::cout<<"DROP "<<n<<" cell="<<i<<" knot="<<j<<" qprev="<<c.curve[j-1].q<<" q="<<c.curve[j].q<<" vprev="<<c.curve[j-1].volume<<" v="<<c.curve[j].volume<<"\n";}}
  volumeError=std::max(volumeError,std::abs(volume-4));std::stringstream stream;stream<<std::setprecision(17);SaveLiquidSurface(stream,*data);std::shared_ptr<const LiquidSurfaceData> loaded;if(!ReadLiquidSurface(stream,e,loaded,err)){++bad;std::cout<<"READ_FAIL "<<n<<" "<<err<<"\n";}else ++decoded;
 }
 std::cout<<"SUMMARY cases=512 baked="<<baked<<" decoded="<<decoded<<" failures="<<bad<<" maxdrop="<<maxdrop<<" capacity_error="<<volumeError<<"\n";return bad?1:0;
}
