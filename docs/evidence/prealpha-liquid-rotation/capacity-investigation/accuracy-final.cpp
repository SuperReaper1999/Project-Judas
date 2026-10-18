#include <iostream>
#include <iomanip>
#include <random>
#include "LiquidSurface.cpp"
int main(){std::mt19937_64 random(0x41cab21ULL);std::uniform_real_distribution<double> draw(-1,1);double clipError=0,analyticError=0,rotatedError=0,massError=0,inverseError=0;double integralError=0;size_t checks=0;bool ok=true;GravityEquilibrium e;e.magnitude=9.81;
 auto check=[&](LiquidGeometry g,GravityEquilibrium eq,auto analytic){LiquidBasinData b;if(!curve(g,eq,b)){ok=false;return;}long double sum=0;for(auto&t:g.cells)sum+=std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))/6;massError=std::max(massError,std::abs(b.capacity-double(sum)));double lo=b.curve.front().q,hi=b.curve.back().q;
  for(const auto& interval:b.capacityPolynomials)for(unsigned j=0;j<=8;++j){double t=double(j)/8;double expected=t*(interval.Volume(0)+4*interval.Volume(t/2)+interval.Volume(t))/6;integralError=std::max(integralError,std::abs(interval.Integral(t)-expected));++checks;}

  for(unsigned k=0;k<=64;++k){double q=lo+(hi-lo)*k/64;double v=capacity(b,q);double physical=LiquidCapacity(g,eq,q);clipError=std::max(clipError,std::abs(v-physical));analyticError=std::max(analyticError,std::abs(v-analytic(q)));++checks;if(v<0||v>b.capacity||slope(b,q)<0)ok=false;
   if(k>0&&k<64){auto it=std::lower_bound(b.curve.begin(),b.curve.end(),v,[](auto p,double target){return p.volume<target;});size_t i=size_t(it-b.curve.begin()-1);double low=0,high=1;for(unsigned n=0;n<48;++n){double t=(low+high)*.5;auto c=b.capacityPolynomials[i];if(c.Volume(t)<v)low=t;else high=t;}double restored=b.curve[i].q+(b.curve[i+1].q-b.curve[i].q)*(low+high)*.5;inverseError=std::max(inverseError,std::abs(restored-q));}
  }
 };
 const std::array<LiquidTet,4> tets{
 LiquidTet{V(0,0,0),V(1,0,0),V(0,0,1),V(0,1,0)},
 LiquidTet{V(0,0,0),V(1,1,0),V(0,1,1),V(0,1,0)},
 LiquidTet{V(0,0,0),V(1,0,0),V(0,1,1),V(0,1,0)},
 LiquidTet{V(0,0,0),V(1,1,0),V(0,2,1),V(0,3,0)}};

 for(unsigned i=0;i<4;++i){LiquidGeometry g;g.cells.push_back(tets[i]);double volume=std::abs(glm::dot(tets[i][1]-tets[i][0],glm::cross(tets[i][2]-tets[i][0],tets[i][3]-tets[i][0])))/6;
  check(g,e,[&](double q){if(i==0)return volume*(1-std::pow(1-q,3));if(i==1)return volume*q*q*q;if(i==2)return volume*q*q*(3-2*q);double sum=0;for(unsigned j=0;j<4;++j){double den=1;for(unsigned k=0;k<4;++k)if(k!=j)den*=double(k)-j;sum+=std::pow(std::max(0.,q-j),3)/den;}return volume*sum;});
 }
 for(unsigned i=0;i<128;++i){glm::dquat rotation=glm::normalize(glm::dquat(draw(random),draw(random),draw(random),draw(random)));auto g=LiquidBox({-1,0,-1},{1,1,1});for(auto&t:g.cells)for(auto&p:t)p=rotation*p;GravityEquilibrium eq=e;eq.up=rotation*V(0,1,0);check(g,eq,[](double q){return std::max(0.,std::min(4.,4*q));});LiquidBasinData b;curve(g,eq,b);for(unsigned k=0;k<=32;++k){double q=double(k)/32;rotatedError=std::max(rotatedError,std::abs(capacity(b,q)-4*q));}}
 std::cout<<std::setprecision(18)<<"SUMMARY analyticPackets=4 rotatedBoxes=128 checks="<<checks<<" geometricMassError="<<massError<<" independentClipError="<<clipError<<" analyticError="<<analyticError<<" rotatedCapacityError="<<rotatedError<<" inverseCoordinateError="<<inverseError<<" integralError="<<integralError<<" positiveAndBounded="<<ok<<"\n";return ok?0:1;
}
