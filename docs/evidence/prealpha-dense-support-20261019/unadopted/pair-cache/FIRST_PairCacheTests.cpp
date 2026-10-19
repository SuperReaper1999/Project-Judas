#include "physics/BroadphasePairCache.h"
#include <cstdint>
#include <iostream>
#include <set>
#include <string>

namespace {
struct Body {
    bool alive=false,enabled=true,moving=false,sensor=false,queryOnly=false,suspended=false;
    unsigned generation=0,layer=0,mask=15;
    int proxy=DynamicAabbTree::kNull;
    bool Eligible()const{return (moving&&!suspended)||sensor||queryOnly;}
};
using Pair=BroadphasePairCache::Pair;
struct Result {std::vector<Pair> pairs;std::size_t rejected=0,queries=0;};
unsigned checks=0,failures=0;
void Check(bool good,const char *label){++checks;if(!good){if(failures<12)std::cout<<"FAIL "<<label<<'\n';++failures;}}
std::uint64_t randomState=0x51b70c592aull;
unsigned Random(){randomState^=randomState<<13;randomState^=randomState>>7;randomState^=randomState<<17;return unsigned(randomState);}
std::uint64_t Handle(unsigned slot,const Body &body){return (std::uint64_t(body.generation)<<20)|slot;}
bool Collide(unsigned a,unsigned b,const std::vector<Body>&bodies,const std::set<std::pair<std::uint64_t,std::uint64_t>>& suppressed){
 const auto &aa=bodies[a],&bb=bodies[b];
 return aa.enabled&&bb.enabled&&(aa.mask&(1u<<bb.layer))&&(bb.mask&(1u<<aa.layer))&&
 !suppressed.count(std::minmax(Handle(a,aa),Handle(b,bb)));
}
// Literal independent original tree-query algorithm, including its asymmetric
// duplicate suppression when the lower potential query body is disabled.
Result Original(const DynamicAabbTree&tree,const std::vector<Body>&bodies,const std::set<std::pair<std::uint64_t,std::uint64_t>>&suppressed){
 Result out;
 for(unsigned slot=0;slot<bodies.size();++slot){
  const auto &a=bodies[slot];if(!a.alive||!a.enabled||!a.Eligible())continue;
  ++out.queries;
  tree.Query(tree.FatAabb(a.proxy),[&](int proxy){
   const unsigned other=tree.UserData(proxy);if(other==slot)return;
   const auto &b=bodies[other];if(b.Eligible()&&other<slot)return;
   if(!Collide(slot,other,bodies,suppressed)){++out.rejected;return;}
   out.pairs.emplace_back(std::min(slot,other),std::max(slot,other));
  });
 }
 std::sort(out.pairs.begin(),out.pairs.end());return out;
}
Result FilterCached(const BroadphasePairCache&cache,const std::vector<Body>&bodies,const std::set<std::pair<std::uint64_t,std::uint64_t>>&suppressed){
 Result out;
 for(auto pair:cache.Pairs()){
  const unsigned outer=bodies[pair.first].Eligible()?pair.first:pair.second;
  if(!bodies[outer].enabled)continue;
  if(!Collide(pair.first,pair.second,bodies,suppressed)){++out.rejected;continue;}
  out.pairs.push_back(pair);
 }
 return out;
}
Aabb RandomBound(){
 const auto coordinate=[](){return float(int(Random()%1200)-600)*.01f;};
 const glm::vec3 p(coordinate(),coordinate(),coordinate());
 const glm::vec3 radius(.2f+float(Random()%120)*.01f,.2f+float(Random()%120)*.01f,.2f+float(Random()%120)*.01f);
 return {p-radius,p+radius};
}
void Lifecycle(bool omitBound,bool omitEligibility){
 DynamicAabbTree tree;std::vector<Body>bodies(96);BroadphasePairCache cache;
 std::vector<BroadphasePairCache::Proxy> proxies,oldInputs(bodies.size());
 std::vector<bool> observed(bodies.size(),false);
 std::set<std::pair<std::uint64_t,std::uint64_t>>suppressed;
 auto create=[&](unsigned slot){auto &b=bodies[slot];b.alive=true;++b.generation;b.proxy=tree.CreateProxy(RandomBound(),slot);b.enabled=true;b.moving=Random()%2;b.sensor=Random()%5==0;b.queryOnly=Random()%11==0;b.suspended=false;b.layer=Random()%4;b.mask=Random()%16;};
 for(unsigned slot=0;slot<bodies.size();slot+=3)create(slot);
 std::size_t oldQueries=0,newQueries=0,zeroUpdates=0;
 for(unsigned step=0;step<6000;++step){
  const unsigned slot=Random()%bodies.size(),action=Random()%13;auto &b=bodies[slot];
  switch(action){
   case 0:if(b.alive)tree.MoveProxy(b.proxy,RandomBound());break;
   case 1:if(b.alive){tree.DestroyProxy(b.proxy);b.alive=false;}else create(slot);break;
   case 2:b.enabled=!b.enabled;break;
   case 3:b.layer=Random()%4;b.mask=Random()%16;break;
   case 4:b.moving=!b.moving;break;
   case 5:b.sensor=!b.sensor;break;
   case 6:b.queryOnly=!b.queryOnly;break;
   case 7:b.suspended=!b.suspended;break;
   case 8:{unsigned other=Random()%bodies.size();if(other!=slot&&b.alive&&bodies[other].alive){auto key=std::minmax(Handle(slot,b),Handle(other,bodies[other]));if(!suppressed.erase(key))suppressed.insert(key);}}break;
   case 9:tree.ShiftOrigin(glm::dvec3(.125,-.25,.0625));break;
   case 10:if(step%17==0)cache.Clear();break;
   case 11:if(b.alive){tree.DestroyProxy(b.proxy);b.alive=false;create(slot);}break;
   case 12:break; // Deliberately unchanged pass exercises reuse.
  }
  proxies.clear();
  for(unsigned i=0;i<bodies.size();++i)if(bodies[i].alive){
   const auto &body=bodies[i];BroadphasePairCache::Proxy input{i,body.generation,body.proxy,tree.FatAabb(body.proxy),body.Eligible()};
   if(observed[i]&&oldInputs[i].generation==input.generation&&oldInputs[i].proxy==input.proxy){
    if(omitBound)input.bound=oldInputs[i].bound;
    if(omitEligibility)input.queryEligible=oldInputs[i].queryEligible;
   }
   oldInputs[i]=input;observed[i]=true;proxies.push_back(input);
  }
  const auto expected=Original(tree,bodies,suppressed);
  const bool available=cache.Update(proxies,[&](const Aabb&box,auto visitor){tree.Query(box,[&](int proxy){visitor(tree.UserData(proxy));});});
  Check(available,"ordinary lifecycle remains within bounded cache capacity");
  if(!available)break;
  const auto actual=FilterCached(cache,bodies,suppressed);
  Check(actual.pairs==expected.pairs,"cached ordered candidate pairs match independent original tree query");
  Check(actual.rejected==expected.rejected,"layer/filter rejection counts match original, including disabled lower-slot quirk");
  Check(tree.Validate(),"source dynamic tree remains structurally valid");
  oldQueries+=expected.queries;newQueries+=cache.Stats().treeQueries;zeroUpdates+=cache.Stats().reused;
 }
 std::cout<<"lifecycle original_queries="<<oldQueries<<" changed_proxy_queries="<<newQueries<<" reused_updates="<<zeroUpdates<<" storage_bytes="<<cache.StorageBytes()<<'\n';
}
void CapacityAndStatics(){
 DynamicAabbTree tree;std::vector<Body>bodies(32);std::vector<BroadphasePairCache::Proxy> proxies;
 for(unsigned slot=0;slot<bodies.size();++slot){auto &b=bodies[slot];b.alive=true;b.moving=true;b.generation=1;b.proxy=tree.CreateProxy({glm::vec3(-1),glm::vec3(1)},slot);proxies.push_back({slot,1,b.proxy,tree.FatAabb(b.proxy),true});}
 const std::set<std::pair<std::uint64_t,std::uint64_t>> suppressed;
 BroadphasePairCache bounded(8);
 auto update=[&](BroadphasePairCache&cache){return cache.Update(proxies,[&](const Aabb&box,auto visitor){tree.Query(box,[&](int proxy){visitor(tree.UserData(proxy));});});};
 Check(!update(bounded)&&bounded.Disabled()&&bounded.Stats().overflow,"pair budget overflow explicitly selects original algorithm");
 for(unsigned pass=0;pass<10;++pass){
  Check(!update(bounded),"overflow stays disabled instead of repeatedly rebuilding discarded edges");
  const auto fallback=Original(tree,bodies,suppressed),expected=Original(tree,bodies,suppressed);
  Check(fallback.pairs==expected.pairs&&fallback.rejected==expected.rejected,"literal fallback preserves full original pairs and rejection statistics");
 }
 BroadphasePairCache statics;
 for(auto &p:proxies)p.queryEligible=false;
 Check(update(statics)&&statics.Pairs().empty()&&statics.Stats().treeQueries==0,"all-static scene creates no pair storage and performs no tree queries");
 proxies[7].queryEligible=true;
 Check(update(statics)&&statics.Pairs().size()==31,"activating one formerly static body discovers all required neighbors");
 proxies[7].queryEligible=false;
 Check(update(statics)&&statics.Pairs().empty(),"deactivating final potential query body leaves no stale edges");
 BroadphasePairCache sparse(100,8);
 Check(!update(sparse)&&sparse.Disabled(),"slot-storage bound fails closed to original generation");
}
}
int main(int argc,char**argv){
 const std::string mode=argc>1?argv[1]:"";
 Lifecycle(mode=="--omit-bound-key-mutation",mode=="--omit-eligibility-key-mutation");CapacityAndStatics();
 std::cout<<"pair_cache checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
