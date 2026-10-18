#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include "LiquidSurface.cpp"
int main(){
 std::mt19937_64 random(0x91d8ab01ULL);std::uniform_real_distribution<double> sample(-1,1);int bad=0,baked=0,decoded=0;double maxdrop=0,volumeError=0;size_t runtimeDrops=0,derivativeNegative=0,runtimeBound=0;double runtimeDrop=0,derivativeMin=0;std::cout<<std::setprecision(18);
 for(unsigned n=0;n<512;++n){
  auto g=LiquidBox({-1,0,-1},{1,1,1});GravityEquilibrium e;e.magnitude=9.81;
  if(n<128){auto a=sample(random)*1e-7,c=sample(random)*1e-7;e.up=glm::normalize(V(a,1,c));}else e.up=glm::normalize(V(sample(random),sample(random),sample(random)));
  LiquidBasinData b;b.geometry=g;b.equilibrium=e;b.curve={{0,0},{1,4}};b.capacity=4;LiquidSurfaceSettings s;s.enabled=true;s.columns=s.rows=4;std::shared_ptr<const LiquidSurfaceData> data;std::string err;
  if(!BakeLiquidSurface(b,s,data,err)){std::cout<<"BAKE_FAIL "<<n<<" "<<err<<"\n";++bad;continue;}++baked;double volume=0;
  for(size_t i=0;i<data->cells.size();++i){auto& c=data->cells[i].storage;volume+=c.capacity;
   double priorValue=0;
   for(size_t interval=0;interval+1<c.curve.size();++interval){
    double a=c.curve[interval].q,b=c.curve[interval+1].q;
    std::vector<double> samples{a,std::nextafter(a,b),a+(b-a)*.25,a+(b-a)*.5,a+(b-a)*.75,std::nextafter(b,a),b};std::sort(samples.begin(),samples.end());
    for(double q:samples){double value=capacity(c,q),derivative=slope(c,q);if(value<priorValue){++runtimeDrops;runtimeDrop=std::max(runtimeDrop,priorValue-value);}if(value<0||value>c.capacity)++runtimeBound;if(derivative<0){++derivativeNegative;derivativeMin=std::min(derivativeMin,derivative);}priorValue=value;}
   }
   for(size_t j=1;j<c.curve.size();++j)if(c.curve[j].volume<c.curve[j-1].volume){++bad;double drop=c.curve[j-1].volume-c.curve[j].volume;maxdrop=std::max(maxdrop,drop);std::cout<<"DROP "<<n<<" cell="<<i<<" knot="<<j<<" qprev="<<c.curve[j-1].q<<" q="<<c.curve[j].q<<" vprev="<<c.curve[j-1].volume<<" v="<<c.curve[j].volume<<"\n";}}
  volumeError=std::max(volumeError,std::abs(volume-4));std::stringstream stream;stream<<std::setprecision(17);SaveLiquidSurface(stream,*data);std::shared_ptr<const LiquidSurfaceData> loaded;if(!ReadLiquidSurface(stream,e,loaded,err)){++bad;std::cout<<"READ_FAIL "<<n<<" "<<err<<"\n";}else ++decoded;
 }
 std::cout<<"SUMMARY cases=512 baked="<<baked<<" decoded="<<decoded<<" failures="<<bad<<" maxdrop="<<maxdrop<<" capacity_error="<<volumeError<<" runtimeDrops="<<runtimeDrops<<" runtimeMaxDrop="<<runtimeDrop<<" runtimeOutOfBounds="<<runtimeBound<<" negativeDerivatives="<<derivativeNegative<<" minDerivative="<<derivativeMin<<"\n";return bad?1:0;
}
