#include "ImpactSupportSets.h"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace JudasPhysicsDetail;
static void Old(std::vector<ImpactSupportKey>& s,std::vector<ImpactSupportKey>& p,
                const std::vector<ImpactSupportKey>& e,const std::vector<bool>& spin,int stop) {
 for(std::size_t i=0;i<e.size();++i){
  if(int(i)==stop)throw std::runtime_error("predicate");
  auto& into=spin[i]?p:s;auto& from=spin[i]?s:p;
  from.erase(std::remove(from.begin(),from.end(),e[i]),from.end());
  into.insert(std::upper_bound(into.begin(),into.end(),e[i]),e[i]);
  into.erase(std::unique(into.begin(),into.end()),into.end());
 }
}
int main(int argc,char**){
 const bool incremental=argc>1;
 std::mt19937 rng(17791);unsigned checks=0,fail=0;
 for(int trial=0;trial<12000;++trial){
  std::vector<ImpactSupportKey> s,p,e;std::vector<bool> spin;
  const unsigned population=rng()%120;
  for(unsigned i=0;i<population;++i){ImpactSupportKey key{i/4,i/4+1,i%2,(i/2)%2};if(rng()%3==0)s.push_back(key);if(rng()%3==0)p.push_back(key);}
  for(int i=int(rng()%180);i;--i){unsigned k=rng()%150;e.push_back({k/4,k/4+1,k%2,(k/2)%2});spin.push_back(rng()%2);}
  std::sort(s.begin(),s.end());std::sort(p.begin(),p.end());
  const int stop=trial%2||e.empty()?-1:int(rng()%e.size());
  auto so=s,po=p;bool oldError=false,newError=false;
  try{Old(so,po,e,spin,stop);}catch(const std::runtime_error&){oldError=true;}
  std::size_t at=0;
  try{auto classify=[&](const auto& key){
   if(int(at)==stop)throw std::runtime_error("predicate");
   if(key!=e[at])throw std::logic_error("encounter order");
   return bool(spin[at++]);
  };
  if(incremental)for(const auto& key:e){const bool spin=classify(key);SetImpactSupport(s,p,key,spin);}
  else ReclassifyImpactSupports(s,p,e,classify);
  }catch(const std::runtime_error&){newError=true;}
  for(bool ok:{oldError==newError,s==so,p==po,std::is_sorted(s.begin(),s.end()),std::is_sorted(p.begin(),p.end())}){++checks;if(!ok)++fail;}
 }
 std::cout<<checks<<" checks, "<<fail<<" failures\n";return fail?1:0;
}
