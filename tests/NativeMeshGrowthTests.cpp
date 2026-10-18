// Focused controls for the MINIMAL native-only feature budget API. No ownership,
// episode, swept-facet table, world state, test budget or quality override exists.
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "Narrowphase.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
int checks=0,failures=0;
constexpr double amount=.0025;
void Check(bool value,const char* label){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",label);}
std::shared_ptr<CollisionAsset> Cook(MeshData source,bool convex){
    auto result=std::make_shared<CollisionAsset>();CollisionCookSettings settings;settings.convex=convex;
    std::string error;if(!CookCollision(source,settings,*result,error))throw std::runtime_error(error);return result;
}
void Add(MeshData& mesh,glm::vec3 a,glm::vec3 b,glm::vec3 c){
    const auto index=uint32_t(mesh.vertices.size());for(auto p:{a,b,c}){MeshVertex v;v.position=p;mesh.vertices.push_back(v);}
    mesh.indices.insert(mesh.indices.end(),{index,index+1,index+2});
}
MeshData FiniteTriangle(){MeshData mesh;Add(mesh,{0,-10,1},{0,10,1},{0,0,10});return mesh;}
MeshData TwoCurrentFacets(float secondPlane=-.49f){
    MeshData mesh;
    Add(mesh,{0,-10,-10},{0,10,-10},{0,0,10});             // front+x, real depth .4
    Add(mesh,{-10,-10,secondPlane},{10,-10,secondPlane},{0,10,secondPlane}); // front+z
    return mesh;
}
MeshData Hull(){
    MeshData mesh;for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1}){MeshVertex v;v.position=glm::vec3(.5f*x,.01f*y,.5f*z);mesh.vertices.push_back(v);}
    mesh.indices={0,4,6,0,6,2,1,3,7,1,7,5,0,1,5,0,5,4,2,6,7,2,7,3,0,2,3,0,3,1,4,5,7,4,7,6};return mesh;
}
double Limit(const MeshFeatureBudget& budget,uint32_t feature){
    for(const auto& entry:budget.limits)if(entry.first==feature)return entry.second;
    return budget.newFeatureLimit;
}
double Depth(const ContactManifold& manifold){double result=0;for(int i=0;i<manifold.count;++i)result=std::max(result,-manifold.points[i].signedSeparation);return result;}
void Case(const Shape& shape,glm::quat rotation,glm::vec3 origin,const char* label){
    std::printf("CASE %s\n",label);
    PrimitivePose mesh;mesh.shape=Shape::Cooked(Cook(FiniteTriangle(),false),"native finite triangle");mesh.parentPosition=origin;mesh.parentOrientation=rotation;
    PrimitivePose query;query.shape=shape;query.parentOrientation=rotation;
    const auto place=[&](glm::vec3 local){auto pose=query;pose.parentPosition=origin+rotation*local;return pose;};

    // Authored ranges are disjoint: initial query z<=.5, triangle z>=1.
    // Its .4m infinite-plane depth is NOT an initial finite contact depth.
    const auto initial=place({.1f,0,0});const auto budget=BuildMeshFeatureBudget(initial,mesh,amount);
    Check(ComputeContacts(initial,mesh,0).count==0&&!CookedHasContacts(initial,mesh,0),"initial finite-footprint contact is absent despite .4m infinite-plane depth");
    Check(!budget.HasContacts()&&budget.limits.empty(),"initial absent finite facet cannot enter actual current-face budget");
    Check(budget.newFeatureLimit==amount&&Limit(budget,0)==amount,"unknown future face receives exactly amount without infinite-plane inheritance");
    Check(!MeshFeatureBudgetExceeded(initial,mesh,budget),"infinite-plane overlap alone never schedules finite contact");
    const auto shallow=place({.1f,0,float(.5+amount*.5)});
    Check(ComputeContacts(shallow,mesh,0).count>0&&CookedHasContacts(shallow,mesh,0),"below-threshold shallow query has real finite edge contact");
    Check(Depth(ComputeContacts(shallow,mesh,0))<amount&&!MeshFeatureBudgetExceeded(shallow,mesh,budget),"new actual finite depth below amount remains below event threshold");
    const auto deep=place({.1f,0,.52f});const auto actual=ComputeContacts(deep,mesh,0);
    Check(actual.count>0&&std::abs(Depth(actual)-.02)<1e-5,"authored finite z edge yields actual .02m penetration");
    Check(CookedHasPenetrationBeyond(deep,mesh,amount),"independent native global predicate sees real finite penetration beyond amount");
    Check(MeshFeatureBudgetExceeded(deep,mesh,budget),"future face uses default amount without needing a preselected swept-facet entry");
    Check(ComputeContacts(mesh,deep,0).count>0&&CookedHasContacts(mesh,deep,0),"reverse native dispatcher retains finite actual contact");

    // Real existing depth is preserved; it is not replaced by a zero-depth
    // first-arrival threshold or the rejected half-extent cap.
    const auto old=place({.1f,0,.6f});const auto oldContacts=ComputeContacts(old,mesh,0);
    const auto oldBudget=BuildMeshFeatureBudget(old,mesh,amount);
    Check(oldBudget.HasContacts()&&oldContacts.count>0&&std::abs(Depth(oldContacts)-.1)<1e-5,"current finite facet starts with actual .1m penetration");
    Check(oldBudget.limits.size()==1&&std::abs(Limit(oldBudget,0)-(Depth(oldContacts)+amount))<1e-9,"current face keeps maximum actual accepted depth plus amount");
    Check(!MeshFeatureBudgetExceeded(old,mesh,oldBudget),"own actual-depth anchor cannot exceed its budget");
    Check(!MeshFeatureBudgetExceeded(place({.1f,0,.601f}),mesh,oldBudget),"existing actual depth growth below amount does not trigger");
    Check(MeshFeatureBudgetExceeded(place({.1f,0,.61f}),mesh,oldBudget),"existing actual depth growth beyond amount triggers");
    const auto deepOld=place({.1f,0,1.f});const auto deepOldContacts=ComputeContacts(deepOld,mesh,0);
    const auto deepOldBudget=BuildMeshFeatureBudget(deepOld,mesh,amount);
    Check(deepOldContacts.count>0&&std::abs(Depth(deepOldContacts)-.4)<1e-5,"deep current facet has actual .4m penetration");
    Check(Limit(deepOldBudget,0)>.25&&std::abs(Limit(deepOldBudget,0)-(Depth(deepOldContacts)+amount))<1e-9,"actual deep current contact is not restricted by rejected .25m cap");
    Check(!MeshFeatureBudgetExceeded(place({.099f,0,1.f}),mesh,deepOldBudget)&&MeshFeatureBudgetExceeded(place({.09f,0,1.f}),mesh,deepOldBudget),"deep current facet keeps unchanged actual-depth growth threshold");

    // These authored points straddle x=0 and overlap the finite z edge,
    // but the query mean lies behind the one-sided front, so native contact
    // remains absent. No history or plane extension may manufacture support.
    const auto rear=place({-.1f,0,.52f});const auto rearBudget=BuildMeshFeatureBudget(rear,mesh,amount);
    Check(ComputeContacts(rear,mesh,0).count==0&&!CookedHasContacts(rear,mesh,0),"rear-entry native contact stays absent during geometric overlap");
    Check(!rearBudget.HasContacts()&&!MeshFeatureBudgetExceeded(rear,mesh,budget),"rear entry acquires neither current budget nor future support");
    const auto miss=place({.1f,30,.52f});const auto missBudget=BuildMeshFeatureBudget(miss,mesh,amount);
    Check(ComputeContacts(miss,mesh,0).count==0&&!CookedHasContacts(miss,mesh,0)&&!missBudget.HasContacts()&&!MeshFeatureBudgetExceeded(miss,mesh,budget),"finite-edge miss remains absent while cutting the same infinite plane");

    // Causal distinction from the old GLOBAL initial-depth allowance:
    // A remains at .4m while shallow facet B grows from .01m to .02m.
    // The old global threshold .4025m stays false, while B's own .0125m
    // allowance must trigger. B may be discarded by response reduction;
    // the observer must retain all accepted native features before reduction.
    PrimitivePose two=mesh;two.shape=Shape::Cooked(Cook(TwoCurrentFacets(),false),"two native growth facets");
    const auto anchor=place({.1f,0,0});const auto twoBudget=BuildMeshFeatureBudget(anchor,two,amount);
    const double globalAnchor=Depth(ComputeContacts(anchor,two,0));
    Check(twoBudget.HasContacts()&&twoBudget.limits.size()==2,"all actual finite features survive regardless of reduced response manifold membership");
    Check(std::abs(Limit(twoBudget,0)-(.4+amount))<1e-5&&std::abs(Limit(twoBudget,1)-(.01+amount))<1e-5,"two current facets retain distinct actual per-feature depth allowances");
    Check(std::abs(globalAnchor-.4)<1e-5&&!MeshFeatureBudgetExceeded(anchor,two,twoBudget),"deep global anchor remains .4m and does not exceed itself");
    Check(!MeshFeatureBudgetExceeded(place({.1f,0,-.001f}),two,twoBudget),"shallow current facet growth .001m remains below unchanged amount");
    const auto otherGrowth=place({.1f,0,-.01f});
    Check(std::abs(Depth(ComputeContacts(otherGrowth,two,0))-globalAnchor)<1e-5&&!CookedHasPenetrationBeyond(otherGrowth,two,globalAnchor+amount),"old global-depth criterion misses shallower facet growth despite unchanged deepest facet");
    Check(MeshFeatureBudgetExceeded(otherGrowth,two,twoBudget),"per-feature growth catches .02m shallow facet that old global .4025m allowance misses");

    // Here A still starts deep, but B is initially absent by .01m. Unlike
    // the isolated absent-facet example, HasContacts is true at this anchor,
    // so this is directly the default-new-face truth used by meshContinuation.
    PrimitivePose introduced=mesh;introduced.shape=Shape::Cooked(Cook(TwoCurrentFacets(-.51f),false),"new facet beside deep native contact");
    const auto introductionBudget=BuildMeshFeatureBudget(anchor,introduced,amount);
    const double introductionGlobal=Depth(ComputeContacts(anchor,introduced,0));
    Check(introductionBudget.HasContacts()&&introductionBudget.limits.size()==1&&introductionBudget.limits.front().first==0,"deep current face anchors continuation while nearby new finite face is absent");
    Check(introductionBudget.newFeatureLimit==amount&&Limit(introductionBudget,1)==amount,"new finite face inherits amount even beside .4m current-face depth");
    Check(!MeshFeatureBudgetExceeded(place({.1f,0,-.011f}),introduced,introductionBudget),"new face with .001m real penetration remains below unchanged amount");
    const auto newGrowth=place({.1f,0,-.02f});
    Check(std::abs(introductionGlobal-.4)<1e-5&&std::abs(Depth(ComputeContacts(newGrowth,introduced,0))-introductionGlobal)<1e-5&&!CookedHasPenetrationBeyond(newGrowth,introduced,introductionGlobal+amount),"old global initial-depth allowance misses newly introduced finite-face depth");
    Check(MeshFeatureBudgetExceeded(newGrowth,introduced,introductionBudget),"new .01m finite-face penetration exceeds default amount inside active continuation");
}
}
int main(){
    try{
        const auto box=Shape::Box({.5f,.01f,.5f});const auto hull=Shape::Cooked(Cook(Hull(),true),"thin native growth hull");
        const auto rotated=glm::angleAxis(.73f,glm::normalize(glm::vec3(1,2,-3)));
        Case(box,{1,0,0,0},{},"box identity");Case(box,rotated,{10,-4,7},"box arbitrary rotation and translation");
        Case(hull,{1,0,0,0},{},"convex hull identity");Case(hull,rotated,{10,-4,7},"convex hull arbitrary rotation and translation");
    }catch(const std::exception& e){++failures;std::printf("FAIL exception %s\n",e.what());}
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
