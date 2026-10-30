#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "PerformanceProfiler.h"
const SpriteAnimationStep* RuntimeWorld::SpriteAnimationObservation(EntityId id) const {
    auto it=m_spriteSteps.find(id);
    return it==m_spriteSteps.end()?nullptr:&it->second;
}
bool RuntimeWorld::SetSpritePlayback(EntityId id,const SpritePlayback& value,std::string& error) {
    const auto* d=RuntimeDefinition(id);
    if(!d||!d->render||d->render->shape!=SceneShape::Sprite||!ValidSpritePlayback(value)){
        error="sprite animation requires a live sprite and valid playback";
        return false;
    }
    if(!m_assets){
        error="sprite sheet resource manager is unavailable";
        return false;
    }
    auto sheet=m_assets->GetSpriteSheet(value.sheet,error);
    if(!sheet)return false;
    auto sample=d->render->sprite;
    if(!SampleSpriteFrame(*sheet,value,sample,error))return false;
    const auto* texture=m_assets->Assets()->Find(sheet->texture);
    if(!texture||texture->missing||texture->type!=AssetType::Texture){
        error="sprite sheet texture is missing or wrong type";
        return false;
    }
    m_scriptDefinitions.at(id).render->spritePlayback=value;
    m_scriptDefinitions.at(id).render->sprite=sample;
    m_spriteAnimationOwners.insert(id);
    m_spriteSteps.erase(id);
    PublishSpriteRender(id);
    ReconcileRenderResources(id);
    error.clear();
    return true;
}
void RuntimeWorld::PublishSpriteRender(EntityId id) {
    const auto* d=RuntimeDefinition(id);
    if(!d||!d->render)return;
    if(auto* e=FindEntity(id))e->definition.render=d->render;
    for(auto& v:m_staticRenderables)if(v.id==id)v.render=*d->render;
    for(auto& v:m_dynamicVisuals)if(v.id==id)v.render=*d->render;
}
void RuntimeWorld::UpdateSpriteAnimations(double dt) {
    if(m_spriteAnimationOwners.empty())return;
    JUDAS_PROFILE_SCOPE("Sprite animation");
    for(auto id:m_spriteAnimationOwners){
        auto* d=RuntimeDefinition(id);
        if(!d||!d->render||!d->render->spritePlayback)continue;
        auto& step=m_spriteSteps[id];
        step={
        };
        if(m_regionPending.count(id)||!m_assets)continue;
        auto sheet=m_assets->GetSpriteSheet(d->render->spritePlayback->sheet,step.error);
        if(!sheet)continue;
        if(!m_renderAssetDemand[id].count(sheet->texture))ReconcileRenderResources(id);
        auto candidate=*d->render->spritePlayback;
        if(StepSpriteAnimation(*sheet,candidate,dt,step)){
            auto sample=d->render->sprite;
            if(SampleSpriteFrame(*sheet,candidate,sample,step.error)){
                m_scriptDefinitions.at(id).render->spritePlayback=candidate;
                m_scriptDefinitions.at(id).render->sprite=sample;
            }
        }
    }
    // One linear presentation publication, not one world scan per animated sprite.
    for(auto id:m_spriteAnimationOwners){
        auto* d=RuntimeDefinition(id);
        if(d)if(auto* e=FindEntity(id))e->definition.render=d->render;
    }
    for(auto& v:m_staticRenderables)if(m_spriteAnimationOwners.count(v.id))if(auto* d=RuntimeDefinition(v.id);d&&d->render)v.render=*d->render;
    for(auto& v:m_dynamicVisuals)if(m_spriteAnimationOwners.count(v.id))if(auto* d=RuntimeDefinition(v.id);d&&d->render)v.render=*d->render;
}
