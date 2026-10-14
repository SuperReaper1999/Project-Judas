#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include <algorithm>

EntityId RuntimeWorld::WheelChassisEntity(const WheelSettings& settings)const{
 if(settings.part.empty())return settings.chassis;
 auto found=m_deformables.find(settings.chassis);if(found==m_deformables.end())return 0;
 const auto& asset=found->second.simulation.asset;if(!asset||!asset->fracture||!asset->fracture->rigid)return 0;
 for(unsigned p=0;p<asset->fracture->parts.size();++p)if(asset->fracture->parts[p].key==settings.part)return FracturePartEntity(settings.chassis,p);
 return 0;
}
WheelInstance* RuntimeWorld::RuntimeWheel(EntityId id){
    const auto* definition=RuntimeDefinition(id);
    if(!definition||!definition->wheel)return nullptr;
    auto [it,added]=m_wheels.try_emplace(id);
    auto& wheel=it->second;
    wheel.settings=*definition->wheel;
    if(added){
        wheel.state.length=wheel.settings.restLength;
        const auto chassis=RuntimeBody(WheelChassisEntity(wheel.settings));wheel.state.available=m_physics.IsDynamicBody(chassis)&&m_physics.IsBodyEnabled(chassis);
        wheel.Publish(m_physics,RuntimeBody(WheelChassisEntity(wheel.settings)));
        wheel.previous=wheel.state;
    }
    return &wheel;
}
const WheelInstance* RuntimeWorld::RuntimeWheel(EntityId id)const{
    const auto* definition=RuntimeDefinition(id);
    if(!definition||!definition->wheel)return nullptr;
    const auto found=m_wheels.find(id);
    return found==m_wheels.end()?nullptr:&found->second;
}
bool RuntimeWorld::SetWheelSettings(EntityId id,const WheelSettings& settings){
    auto* wheel=RuntimeWheel(id);std::string error;
    if(!wheel||!ValidateWheelSettings(settings,error))return false;
    const auto* chassis=RuntimeDefinition(WheelChassisEntity(settings));
    if(!chassis||!chassis->body||IsTransientEntity(WheelChassisEntity(settings))||!RuntimeBody(WheelChassisEntity(settings)).IsValid())return false;
    m_scriptDefinitions.at(id).wheel=settings;wheel->settings=settings;
    RefreshWheelParticipants();
    wheel->state.length=std::clamp(wheel->state.length,settings.minLength,settings.maxLength);
    if(!settings.enabled){wheel->state.contact=false;wheel->state.body={};wheel->state.normalForce=0;wheel->state.longitudinalImpulse=wheel->state.lateralImpulse=0;}
    wheel->Publish(m_physics,RuntimeBody(WheelChassisEntity(settings)));wheel->previous=wheel->state;
    return true;
}
void RuntimeWorld::UpdateWheels(float dt){
    JUDAS_PROFILE_SCOPE("Wheel suspension and tyres");
    JUDAS_PROFILE_COUNTER("Wheel instances",double(m_wheelOwners.size()),ProfileCounterMode::Latest);
    for(auto id:m_wheelOwners){
        auto* wheel=RuntimeWheel(id);if(!wheel)continue;
        // Entity identity is resolved every step. A destroyed/reused native slot
        // never substitutes for the authored chassis reference.
        wheel->Step(m_physics,RuntimeBody(WheelChassisEntity(wheel->settings)),dt);
    }
}
void RuntimeWorld::PublishWheels(){
    for(auto id:m_wheelOwners){
        auto* wheel=RuntimeWheel(id);if(!wheel)continue;
        wheel->Publish(m_physics,RuntimeBody(WheelChassisEntity(wheel->settings)));
        if(!wheel->state.available)continue;
        auto& definition=m_scriptDefinitions.at(id);
        definition.transform.position=wheel->state.center;definition.transform.rotation=wheel->state.rotation;
        if(auto* record=FindEntity(id)){record->state.position=wheel->state.center;record->state.rotation=wheel->state.rotation;}
        if(auto* local=m_hierarchy.Find(id)){
            auto pose=definition.transform;
            if(local->parent)if(const auto* parent=RuntimeDefinition(local->parent)){
                const auto frame=PresentedTransform(parent->id,parent->transform,1);
                pose.position=glm::inverse(frame.rotation)*(pose.position-frame.position)/frame.scale;
                pose.rotation=glm::normalize(glm::inverse(frame.rotation)*pose.rotation);pose.scale/=frame.scale;
            }
            local->transform=pose;
        }
    }
}

void RuntimeWorld::RefreshWheelParticipants(){
    // Wheel fidelity is a derived requirement, not permanent ownership of the
    // chassis record. Other systems retain their existing requiresFull flags.
    m_wheelParticipants.clear();
    for(auto id:m_wheelOwners){const auto definition=m_scriptDefinitions.find(id);
        if(definition!=m_scriptDefinitions.end()&&definition->second.wheel)m_wheelParticipants.insert(definition->second.wheel->chassis);
    }
}
