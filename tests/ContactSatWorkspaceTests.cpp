#include "Contacts.h"
#include "ContactGeometryInternal.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>

using namespace contact_geometry;
namespace {
std::size_t checks=0,failures=0,exactBoundaryCases=0;
void Check(bool condition,const char* message){++checks;if(!condition){++failures;if(failures<12)std::cerr<<message<<'\n';}}
bool Same(double a,double b){return std::memcmp(&a,&b,sizeof a)==0;}
bool Same(const Iv& a,const Iv& b){return Same(a.lo,b.lo)&&Same(a.hi,b.hi);}
bool Same(const Exact& a,const Exact& b){
 if(a.e.size()!=b.e.size())return false;
 for(std::size_t i=0;i<a.e.size();++i)if(!Same(a.e[i],b.e[i]))return false;
 return true;
}
// Independent frozen pre-optimization implementation. Keep its eager order and
// literal expressions so a lazy-cache error cannot agree with itself.
template<class T>struct EagerWorkspace {
 const Pair<T>& p;
 std::array<std::array<T,3>,3> ab;
 T da2,db2;
 explicit EagerWorkspace(const Pair<T>& pair):p(pair),da2(p.a.d*p.a.d),db2(p.b.d*p.b.d){
  for(int i=0;i<3;++i)for(int j=0;j<3;++j)ab[i][j]=Dot(p.a.n[i],p.b.n[j]);
 }
 T Evaluate(const glm::vec3&ha,const glm::vec3&hb,int axis,const V<T>&t)const{
  T s=Abs(Dot(p.delta,t));
  if(axis<3){
   for(int k=0;k<3;++k){
    const T own=k==axis?da2:T(0);
    s=s-T(double(ha[k]))*own*p.b.d-T(double(hb[k]))*Abs(ab[axis][k])*p.a.d;
   }
  }else if(axis<6){
   const int j=axis-3;
   for(int k=0;k<3;++k){
    const T own=k==j?db2:T(0);
    s=s-T(double(ha[k]))*Abs(ab[k][j])*p.b.d-T(double(hb[k]))*own*p.a.d;
   }
  }else{
   const int i=(axis-6)/3,j=(axis-6)%3;
   for(int k=0;k<3;++k){
    const T aa=k==i?T(0):p.a.d*Abs(ab[3-k-i][j]);
    const T bb=k==j?T(0):p.b.d*Abs(ab[i][3-k-j]);
    s=s-T(double(ha[k]))*aa*p.b.d-T(double(hb[k]))*bb*p.a.d;
   }
  }
  return s;
 }
};
template<class T>void AllAxes(const Pair<T>& pair,const glm::vec3& ha,const glm::vec3& hb,std::mt19937& random){
 const EagerWorkspace<T> eager(pair);
 std::array<int,15> order{};for(int i=0;i<15;++i)order[std::size_t(i)]=i;
 for(unsigned pass=0;pass<3;++pass){
  if(pass)std::shuffle(order.begin(),order.end(),random);
  const SatWorkspace<T> lazy(pair);
  Check(lazy.preparedRows==0,"workspace constructed unused orientation rows");
  for(int axis:order){
   const auto t=Axis(pair,axis);const auto expected=eager.Evaluate(ha,hb,axis,t);
   const auto actual=lazy.Evaluate(ha,hb,axis,t);
   Check(Same(actual,expected),"SAT evaluation representation differs from eager workspace");
   Check(Same(actual,lazy.Evaluate(ha,hb,axis,t)),"repeated SAT axis changed representation");
   if(axis>=3)Check(lazy.preparedRows==7,"B-face/edge axis did not complete orientation rows");
  }
 }
 const SatWorkspace<T> first(pair);first.Evaluate(ha,hb,0,Axis(pair,0));
 Check(first.preparedRows==1,"first A-face axis prepared unrelated rows");
 first.Evaluate(ha,hb,2,Axis(pair,2));Check(first.preparedRows==5,"second A-face axis prepared unrelated row");
 first.Evaluate(ha,hb,1,Axis(pair,1));Check(first.preparedRows==7,"A-face rows missing after all requested");
}
ContactPose RandomPose(std::mt19937& r){
 auto value=[&](int range){return float(int(r()%unsigned(range*2+1))-range)/32.f;};
 ContactPose p;p.position={value(1024),value(1024),value(1024)};p.localCenter={value(16),value(16),value(16)};
 p.orientation={value(64),value(64),value(64),value(64)};
 if(glm::dot(p.orientation,p.orientation)==0)p.orientation.w=1;
 return p;
}
void BoundaryExact(const ContactPose& a,const ContactPose& b,const glm::vec3& half){
 const Pair<Iv> interval(a,b);const Pair<Exact> exact(a,b);const SatWorkspace<Iv> lazy(interval);const EagerWorkspace<Iv> eager(interval);
 for(int axis=0;axis<15;++axis){
  const auto t=Axis(interval,axis);const auto fresh=lazy.Evaluate(half,half,axis,t);const auto old=eager.Evaluate(half,half,axis,t);
  Check(Same(fresh,old),"boundary interval differs from eager workspace");
  const auto exactAxis=Axis(exact,axis);
  if(fresh.lo<=0&&fresh.hi>=0&&Dot(exactAxis,exactAxis).Sign()!=0){
   ++exactBoundaryCases;
   const int reference=Sat(exact,half,half,axis).Sign();
   const SatWorkspace<Exact> precise(exact);const EagerWorkspace<Exact> preciseOld(exact);
   const auto ex=precise.Evaluate(half,half,axis,Axis(exact,axis));
   Check(Same(ex,preciseOld.Evaluate(half,half,axis,Axis(exact,axis))),"exact fallback representation differs from eager workspace");
   Check(ex.Sign()==reference,"exact fallback sign differs from original unfactored SAT");
  }
 }
}
}
int main(){
 std::mt19937 random(0x87ab19u);
 for(unsigned i=0;i<1200;++i){
  const auto a=RandomPose(random),b=RandomPose(random);
  const glm::vec3 ha(float(1+random()%128)/32.f,float(1+random()%128)/32.f,float(1+random()%128)/32.f);
  const glm::vec3 hb(float(1+random()%128)/32.f,float(1+random()%128)/32.f,float(1+random()%128)/32.f);
  AllAxes(Pair<double>(a,b),ha,hb,random);AllAxes(Pair<Iv>(a,b),ha,hb,random);
  // Also test genuine intervals wider than generated roundoff enclosures.
  if(i<120){Pair<Iv> p(a,b);for(auto& column:p.a.n)for(auto& x:column)x=Iv(Down(x.lo),Up(x.hi));AllAxes(p,ha,hb,random);}
  if(i<40)AllAxes(Pair<Exact>(a,b),ha,hb,random);
 }
 for(float coordinate:{0.f,.5f,1.f,2.f,16.f}){
  ContactPose a,b;a.position={coordinate,coordinate,coordinate};
  for(float offset:{0.f,1.f,std::nextafter(1.f,0.f),std::nextafter(1.f,2.f)}){
   b.position=a.position+glm::vec3(offset,0,0);BoundaryExact(a,b,glm::vec3(.5f));
  }
 }
 Check(exactBoundaryCases>0,"no uncertain intervals exercised exact fallback comparison");
 std::cout<<"Contact SAT workspace parity: "<<checks<<" checks, "<<failures<<" failures, "<<exactBoundaryCases<<" exact boundary cases\n";
 return failures?1:0;
}
