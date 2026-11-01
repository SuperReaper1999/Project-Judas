#include "AISensing.h"
#include "RuntimeWorld.h"
#include "PlanarGeometry.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace {
bool Finite(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool SameOwner(BodyHandle a, BodyHandle b) { return a.id == b.id && a.world == b.world; }
bool SameRequest(const AISensingRequest& a, const AISensingRequest& b) {
    const auto& x=a.targets;const auto& y=b.targets;
    return a.origin==b.origin && a.radius==b.radius && a.visibility==b.visibility &&
        a.occluderLayers==b.occluderLayers && x.includeLayers==y.includeLayers &&
        x.excludeLayers==y.excludeLayers && x.requiredTags==y.requiredTags &&
        x.excludedTags==y.excludedTags && x.includeSensors==y.includeSensors && x.planar==y.planar;
}
void Require(bool value,const char* message) {if(!value)throw std::invalid_argument(message);}
}

struct AISensing::Impl {
    AISensingSettings settings;
    AISensingRequest request;
    AISensingResult previous;
    const RuntimeWorld* world = nullptr;
    SceneObjectId owner = 0;
    BodyHandle ownBody{};
    unsigned entityVersion = 0;
    std::uint64_t originRevision = 0;
    bool valid = false;
    explicit Impl(AISensingSettings s):settings(s) {}
};
AISensing::AISensing(AISensingSettings settings):m(std::make_unique<Impl>(settings)) {
    Require(std::isfinite(settings.interval)&&settings.interval>=0&&settings.interval<=60,"AISenses interval must be in [0,60]");
    Require(settings.maxResults>=1&&settings.maxResults<=128,"AISenses maxResults must be 1..128");
    Require(settings.maxRaycasts<=128,"AISenses maxRaycasts must be 0..128");
    Require(settings.maxCandidates>=settings.maxResults&&settings.maxCandidates<=4096,
            "AISenses maxCandidates must be maxResults..4096");
}
AISensing::~AISensing() = default;
void AISensing::Invalidate() {m->valid=false;}
const AISensingSettings& AISensing::Settings() const {return m->settings;}
AISensingResult AISensing::Sample(RuntimeWorld& world,SceneObjectId owner,const AISensingRequest& request) {
    Require(world.RuntimeDefinition(owner)!=nullptr,"AISenses owner is not a live entity");
    Require(Finite(request.origin)&&std::isfinite(request.radius)&&request.radius>=0&&request.radius<=1000000,
            "AISenses origin and radius must be finite local coordinates/radius [0,1000000]");
    Require(request.targets.ignoredBodies.empty(),"AISenses ignores its own body; custom ignored bodies are unsupported");
    const auto ownBody=world.RuntimeBody(owner);
    const auto now=world.SimulationTimeSeconds();
    Require(std::isfinite(now),"AISenses simulation clock must be finite");
    bool reuse=m->valid&&m->world==&world&&m->owner==owner&&SameOwner(m->ownBody,ownBody)&&
        m->entityVersion==world.EntityVersion()&&m->originRevision==world.OriginRevision()&&
        SameRequest(request,m->request)&&now>=m->previous.sampledAt&&
        now-m->previous.sampledAt<m->settings.interval;
    // Only retained, bounded results are checked; no all-entity scan on cache hits.
    if(reuse)for(const auto& target:m->previous.targets) {
        if(!world.RuntimeDefinition(target.entityId)||!SameOwner(world.RuntimeBody(target.entityId),target.body)) {reuse=false;break;}
        const auto pose=world.Physics().GetTransform(target.body);
        unsigned layer=0;CategoryMask mask=0;
        if(pose.position!=target.position||!world.Physics().GetCollisionFilter(target.body,layer,mask)||
            !(request.targets.includeLayers&CategoryBit(layer))||(request.targets.excludeLayers&CategoryBit(layer))||
            (world.TagsOf(target.entityId)&request.targets.requiredTags)!=request.targets.requiredTags||
            (world.TagsOf(target.entityId)&request.targets.excludedTags)) {reuse=false;break;}
    }
    if(reuse) {
        auto result=m->previous;result.cached=true;result.age=now-result.sampledAt;
        // Reuse does not prove same-tick world geometry remained unchanged.
        result.stale=true;
        result.raycasts=0;result.candidatesConsidered=0;result.queryCandidates=0;
        return result;
    }
    AISensingResult result;result.sampledAt=now;
    auto filter=request.targets;
    if(ownBody.IsValid())filter.ignoredBodies.push_back(ownBody);
    const auto extent=glm::vec3(request.radius);
    const auto min=request.origin-extent,max=request.origin+extent;
    Require(Finite(min)&&Finite(max),"AISenses query bounds overflow");
    const auto matches=world.Physics().QueryBodiesInAabb(min,max,filter);
    result.queryCandidates=matches.size();
    result.truncated=matches.size()>m->settings.maxCandidates;
    const auto count=std::min(matches.size(),std::size_t(m->settings.maxCandidates));
    std::set<SceneObjectId> seen;
    for(std::size_t i=0;i<count;++i) {
        ++result.candidatesConsidered;
        const auto body=matches[i];
        const auto id=world.EntityIdOfBody(body);
        // An articulation's auxiliary body is not silently represented as its
        // root. This first sensor covers normal published entity-owned bodies.
        if(!id||id==owner||!world.RuntimeDefinition(id)||!SameOwner(world.RuntimeBody(id),body)||!seen.insert(id).second)continue;
        const auto p=world.Physics().GetTransform(body).position;
        const auto distance=glm::distance(glm::dvec3(p),glm::dvec3(request.origin));
        if(!Finite(p)||!std::isfinite(distance)||distance>request.radius)continue;
        result.targets.push_back({id,body,p,distance,{}});
    }
    std::sort(result.targets.begin(),result.targets.end(),[](const auto& a,const auto& b) {
        return a.distance!=b.distance ? a.distance<b.distance : a.entityId<b.entityId;
    });
    if(result.targets.size()>m->settings.maxResults) {result.truncated=true;result.targets.resize(m->settings.maxResults);}
    if(request.visibility) {
        PhysicsQueryFilter occluders;occluders.includeLayers=request.occluderLayers;
        occluders.planar=request.targets.planar;
        if(ownBody.IsValid())occluders.ignoredBodies.push_back(ownBody);
        const auto point=std::make_shared<const PlanarCollider>(PlanarCollider{PlanarKind::Circle,{{0,0}},0,false});
        for(auto& target:result.targets) {
            if(result.raycasts>=m->settings.maxRaycasts)break;
            // Coincident origins need no invented direction or visibility ray.
            if(target.distance==0)continue;
            ++result.raycasts;
            const auto direction=target.position-request.origin;
            PhysicsCastHit hit;
            if(request.targets.planar)hit=world.Physics().CastPlanar(Shape::Planar(point),{request.origin,ownBody.IsValid()?world.Physics().GetTransform(ownBody).rotation:glm::quat(1,0,0,0)},direction,float(target.distance),occluders);
            else hit=world.Physics().Raycast(request.origin,direction,float(target.distance),occluders);
            target.visible=!hit.hit||SameOwner(hit.body,target.body);
        }
    }
    m->request=request;m->previous=result;m->world=&world;m->owner=owner;m->ownBody=ownBody;
    m->entityVersion=world.EntityVersion();m->originRevision=world.OriginRevision();m->valid=true;
    return result;
}
std::string AISensingResult::Json() const {
    using J=nlohmann::json;
    J result={{"targets",J::array()},{"cached",cached},{"stale",stale},{"age",age},{"sampledAt",sampledAt},
        {"truncated",truncated},{"candidatesConsidered",candidatesConsidered},{"raycasts",raycasts},{"queryCandidates",queryCandidates}};
    for(const auto& t:targets)result["targets"].push_back({{"entityId",std::to_string(t.entityId)},
        {"position",{{"x",t.position.x},{"y",t.position.y},{"z",t.position.z}}},
        {"distance",t.distance},{"visible",t.visible?J(*t.visible):J(nullptr)}});
    return result.dump();
}
