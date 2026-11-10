#include "SleepContacts.h"
#include <algorithm>

namespace JudasDeformables {
namespace {
bool Same(const Shape& a,const Shape& b){
    if(a.type!=b.type||a.radius!=b.radius||a.halfHeight!=b.halfHeight||a.halfExtents!=b.halfExtents||a.pivotOffset!=b.pivotOffset||a.asset!=b.asset||a.terrain!=b.terrain||a.planar!=b.planar||a.planarSegment!=b.planarSegment||a.boxes.size()!=b.boxes.size())return false;
    for(size_t i=0;i<a.boxes.size();++i){const auto& x=a.boxes[i];const auto& y=b.boxes[i];if(x.localCenter!=y.localCenter||x.halfExtents!=y.halfExtents||x.rotation!=y.rotation||x.type!=y.type||x.radius!=y.radius||x.asset!=y.asset||x.key!=y.key)return false;}
    return true;
}
}
bool SleepContacts::Refresh(PhysicsWorld& world,glm::dvec3 low,glm::dvec3 high,unsigned layer,CategoryMask mask,bool& moving){
    PhysicsQueryFilter filter;filter.includeLayers=mask;
    world.QueryBodiesInAabbInto(glm::vec3(low),glm::vec3(high),candidates,filter);
    scratch.clear();moving=false;
    for(auto body:candidates){
        unsigned otherLayer;CategoryMask otherMask;
        if(!world.GetCollisionFilter(body,otherLayer,otherMask)||!(otherMask&CategoryBit(layer))||world.IsBodySensor(body))continue;
        Entry entry;entry.body=body;
        if(!world.GetBodyShape(body,entry.shape,entry.pose,false))continue;
        moving|=glm::length(world.GetLinearVelocity(body))+glm::length(world.GetAngularVelocity(body))>1e-5f;
        scratch.push_back(std::move(entry));
    }
    std::sort(scratch.begin(),scratch.end(),[](const auto& a,const auto& b){return a.body.id<b.body.id;});
    bool changed=!valid||scratch.size()!=retained.size();
    for(size_t i=0;!changed&&i<scratch.size();++i){const auto& a=scratch[i];const auto& b=retained[i];changed=a.body.id!=b.body.id||a.body.world!=b.body.world||a.pose.position!=b.pose.position||a.pose.rotation!=b.pose.rotation||!Same(a.shape,b.shape);}
    retained.swap(scratch);valid=true;return changed;
}
}
