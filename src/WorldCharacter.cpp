#include "CameraProjection.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
CharacterMotor* RuntimeWorld::RuntimeCharacter(EntityId id){
    auto* d=RuntimeDefinition(id);if(!d||!d->characterMotor)return nullptr;
    auto [it,added]=m_characters.try_emplace(id);
    auto& c=it->second;if(added){c.motor.Reset(d->transform.position,d->transform.rotation);c.previous=d->transform;}
    c.motor.settings=*d->characterMotor;
    if(!c.motor.observationBody.IsValid()){
        c.motor.observationBody=m_physics.CreateQueryCapsule(c.motor.settings.radius,c.motor.settings.halfHeight,{c.motor.position+c.motor.orientation*c.motor.settings.offset,c.motor.orientation});
    }
    return &c.motor;
}
void RuntimeWorld::ClearCharacters(){for(auto& [_,c]:m_characters)m_physics.DestroyBody(c.motor.observationBody);m_characters.clear();view.reset();}
void RuntimeWorld::UpdateCharacters(float dt,bool onlyM70,bool skipM70){
    JUDAS_PROFILE_SCOPE("Character motors");
 JUDAS_PROFILE_COUNTER("Character motor instances",double(m_characters.size()),ProfileCounterMode::Latest);
    if(onlyM70)m_physics.BeginPreStepQueryContacts();
    // Stable entity ordering, independent state; no input/camera/clip ownership.
    for(auto id:m_characterOwners)RuntimeCharacter(id);
    for(auto it=m_characters.begin();it!=m_characters.end();){auto id=it->first;auto* d=RuntimeDefinition(id);
        if(!d||!d->characterMotor){m_physics.DestroyBody(it->second.motor.observationBody);it=m_characters.erase(it);continue;}
        const bool m70=(d->animation&&d->animation->fullBodyIK&&d->animation->fullBodyIK->enabled)||(d->ragdoll&&d->ragdoll->physicalAnimation&&d->ragdoll->physicalAnimation->enabled);
        if((onlyM70&&!m70)||(skipM70&&m70)){++it;continue;}
        auto& c=it->second;auto& motor=c.motor;c.previous=d->transform;
        motor.settings=*d->characterMotor;
        m_physics.SetBodyEnabled(motor.observationBody,motor.settings.enabled);
        m_physics.SetCollisionFilter(motor.observationBody,motor.settings.collisionLayer,motor.settings.collisionMask);
        m_physics.SetBodyTags(motor.observationBody,d->tags);
        motor.filter.ignoredBodies.erase(std::remove_if(motor.filter.ignoredBodies.begin(),motor.filter.ignoredBodies.end(),[&](auto h){return !m_physics.IsBodyEnabled(h);}),motor.filter.ignoredBodies.end());
        if(std::none_of(motor.filter.ignoredBodies.begin(),motor.filter.ignoredBodies.end(),[&](auto h){return h.id==motor.observationBody.id;}))motor.filter.ignoredBodies.push_back(motor.observationBody);
        motor.Step(m_physics,GravityForEntity(id),dt);
        m_physics.ResetBody(motor.observationBody,motor.position+motor.orientation*motor.settings.offset,motor.orientation);
        // Internal resolved motion writes bypass the explicit-teleport/reset API.
        auto& definition=m_scriptDefinitions.at(id);definition.transform.position=motor.position;definition.transform.rotation=motor.orientation;
        if(auto* e=FindEntity(id)){e->state.position=motor.position;e->state.rotation=motor.orientation;e->state.linearVelocity=motor.velocity;}
        if(auto* h=m_hierarchy.Find(id)){auto t=definition.transform;if(h->parent){auto* p=RuntimeDefinition(h->parent);if(p){auto parent=PresentedTransform(p->id,p->transform,1);t.position=glm::inverse(parent.rotation)*(t.position-parent.position)/parent.scale;t.rotation=glm::inverse(parent.rotation)*t.rotation;}}h->transform=t;}
        ++it;
    }
    m_physics.FinishQueryTouches();
}
bool RuntimeWorld::SetRuntimeView(const SceneTransform& pose,float fov,float nearPlane,float farPlane){
    if(!ValidCameraRange(nearPlane,farPlane)||!std::isfinite(glm::dot(pose.position,pose.position))||!std::isfinite(glm::dot(pose.rotation,pose.rotation))||glm::dot(pose.rotation,pose.rotation)<1e-12f||!std::isfinite(fov)||fov<=1||fov>=179)return false;
    view=RuntimeView{pose,fov,nearPlane,farPlane};view->pose.rotation=glm::normalize(pose.rotation);return true;
}

bool RuntimeWorld::SetCharacterSettings(EntityId id,const CharacterMotorSettings& settings){
    auto* m=RuntimeCharacter(id);std::string error;if(!m||!ValidCharacterMotor(settings,error))return false;
    const auto old=m->settings;
    if(old.radius!=settings.radius||old.halfHeight!=settings.halfHeight){m_physics.DestroyBody(m->observationBody);m->observationBody=m_physics.CreateQueryCapsule(settings.radius,settings.halfHeight,{m->position+m->orientation*settings.offset,m->orientation});}
    m_scriptDefinitions.at(id).characterMotor=settings;m->settings=settings;
    m_physics.SetBodyEnabled(m->observationBody,settings.enabled);
    if(!settings.enabled){m->result={};m->acceleration={0,0,0};}return true;
}
