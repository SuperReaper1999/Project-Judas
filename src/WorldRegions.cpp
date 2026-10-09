#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "RadicalGravity.h"
#include "UniformGravity.h"
#include "FaithfulGravity.h"
#include "SphericalVolume.h"
#include "SaveArchive.h"
#include <algorithm>
#include <limits>
namespace {
// Region boxes must rotate with their physical/content frame, including oblique gravity.
class OrientedRegionVolume final:public GravityVolume {
    glm::vec3 p,h;glm::quat inverse;
public:OrientedRegionVolume(glm::vec3 p_,glm::quat q,glm::vec3 h_):p(p_),h(h_),inverse(glm::inverse(q)){}
    bool Contains(const glm::vec3& x)const override {auto a=glm::abs(inverse*(x-p));return a.x<=h.x&&a.y<=h.y&&a.z<=h.z;}
};
}
bool RuntimeWorld::StageRegionObject(const SceneObject& definition,const SceneObject& local,std::string& error,const std::string& stableIdentity) {
    if(FindEntity(definition.id)){error="duplicate staged identity";return false;}
    m_regionPending.insert(definition.id);
    if(definition.audioEmitter||definition.audioZone)m_audioIdentities[definition.id]=stableIdentity;
    auto stage=definition;stage.gravity.reset(); // globally routed only at publication
    // Kinematic initialization uses the public body's normal validation while
    // this actor is private. Capture intent before the staging disable retires
    // it; no callback, step or publication runs in this owner-thread scope.
    if(stage.body)stage.body->enabled=stage.body->motion==SceneBodyMotion::Kinematic;
    Scene batch;batch.Settings()=m_settings;batch.Objects().push_back(stage);
    const auto before=m_referencedAssets.size();FidelityPolicyContext context;
    bool ok=AppendSceneObjects(batch,false,context,error);
    if(ok&&stage.body&&stage.body->motion==SceneBodyMotion::Kinematic){
        const auto body=m_entityCategories.at(definition.id).body;
        if(definition.body->enabled){SaveArchive command;m_physics.PersistKinematic(command,body);m_regionKinematicCommands[definition.id]=std::move(command.bytes);}
        m_physics.SetBodyEnabled(body,false);
    }
    m_regionAssets[definition.id]={m_referencedAssets.begin()+before,m_referencedAssets.end()};
    if(!stage.body||stage.body->motion==SceneBodyMotion::Static){
        EntityRecord e;e.id=definition.id;e.name=definition.name;e.definition=definition;e.authored=false;e.requiresFull=true;
        e.state.position=definition.transform.position;e.state.rotation=definition.transform.rotation;e.slot=std::numeric_limits<size_t>::max();m_extraEntities.push_back(e);
    }
    m_scriptDefinitions[definition.id]=definition;
    if(auto* e=FindEntity(definition.id))e->definition=definition;
    m_hierarchy.InsertObject(local);
    return ok;
}
void RuntimeWorld::BindRegionCameraReference(EntityId id,EntityId target) {
    if(auto it=m_scriptDefinitions.find(id);it!=m_scriptDefinitions.end()&&it->second.render)it->second.render->textureCamera=target;
    for(auto& r:m_staticRenderables)if(r.id==id)r.render.textureCamera=target;
    for(auto& r:m_dynamicVisuals)if(r.id==id)r.render.textureCamera=target;
}
void RuntimeWorld::HideRegion(const std::vector<EntityId>& ids) {
    for(auto id:ids){auto body=RuntimeBody(id);m_physics.SetBodyEnabled(body,false);m_regionPending.insert(id);}
}
void RuntimeWorld::PublishRegion(const std::vector<EntityId>& ids) {
    for(auto id:ids){
        m_regionPending.erase(id);
        if(auto* e=FindEntity(id))e->requiresFull|=EntityRequiresFull(e->definition)||m_jointParticipants.count(id);
        if(auto* d=RuntimeDefinition(id))if(d->body){
            const auto body=RuntimeBody(id);m_physics.SetBodyEnabled(body,d->body->enabled);
            if(auto pending=m_regionKinematicCommands.find(id);pending!=m_regionKinematicCommands.end()){
                if(d->body->enabled){SaveArchive command(pending->second);m_physics.PersistKinematic(command,body);command.Finish();}
                m_regionKinematicCommands.erase(pending);
            }
        }
    }
    for(auto& e:m_audioEmitters)if(std::find(ids.begin(),ids.end(),e.id)!=ids.end())e.wantPlay=e.settings.playOnStart;
    ++m_entityVersion;
}
bool RuntimeWorld::RestoreRegionObject(EntityId id,const SceneTransform& transform,const EntityPhysicalState& state,const std::vector<ScriptStateRecord>& scripts,std::string& error,bool resume){
    if(!m_regionPending.count(id)){error="restoration requires a private staged entity";return false;}
    // Owner-thread scope only: no callback or step can observe temporary lookup
    // access. Bodies stay disabled, including a newly constructed motor capsule.
    m_regionPending.erase(id);
    const bool ok=SetRuntimeTransform(id,transform)&&SetEntityState(id,state);
    if(auto* motor=RuntimeCharacter(id))motor->velocity=state.linearVelocity;
    m_physics.SetBodyEnabled(RuntimeBody(id),false);m_regionPending.insert(id);
    if(!ok){error="invalid retained entity state";return false;}
    return RestoreScriptState(scripts,error,resume);
}
bool RuntimeWorld::RegionVisualReady(const std::vector<EntityId>& ids)const {
    if(!m_assets)return true;
    for(auto id:ids)if(auto it=m_regionAssets.find(id);it!=m_regionAssets.end())for(auto& asset:it->second){auto* r=m_assets->Assets()->Find(asset);if(r&&(r->type==AssetType::Audio||r->type==AssetType::AudioEffect))continue;if(m_assets->StateOf(asset)!=ResourceState::Ready)return false;}
    for(auto id:ids)if(auto it=m_renderAssetDemand.find(id);it!=m_renderAssetDemand.end())for(const auto& asset:it->second)if(m_assets->StateOf(asset)!=ResourceState::Ready)return false;
    return true;
}
void RuntimeWorld::EndRegionScripts(const std::vector<EntityId>& ids){if(m_scripts)m_scripts->RemoveEntities(ids);}
void RuntimeWorld::RemoveRegionObject(EntityId id) {
    // Suspension is preflighted by the residency coordinator. In particular,
    // conserved liquids are pinned; destruction's parked-parcel path is not used.
    ReleaseRenderResources(id);
    m_regionPending.insert(id);m_regionKinematicCommands.erase(id);if(m_scripts)m_scripts->RemoveEntities({id});
    if(m_navigation)m_navigation->RemoveSurface(id);
    if(auto it=m_runtimeJoints.find(id);it!=m_runtimeJoints.end()){m_physics.DestroyJoint(it->second);m_runtimeJoints.erase(it);}
    BodyHandle handle;
    if(auto* e=FindEntity(id)) {
        if(e->slot!=std::numeric_limits<size_t>::max()) {
            const auto slot=e->slot;handle=m_dynamicBodies[slot].Handle();if(handle.IsValid())m_physics.DestroyBody(handle);
            m_dynamicBodies.erase(m_dynamicBodies.begin()+slot);m_dynamicVisuals.erase(m_dynamicVisuals.begin()+slot);
            for(auto& other:m_entities)if(other.slot>slot)--other.slot;
            if(m_vehicle&&m_vehicle->dynamicIndex>slot)--m_vehicle->dynamicIndex;
            for(auto& c:m_combustibles)if(c.dynamicIndex>slot)--c.dynamicIndex;
        }else if(auto it=m_entityCategories.find(id);it!=m_entityCategories.end()){handle=it->second.body;if(handle.IsValid())m_physics.DestroyBody(handle);}
    }
    ReleaseEntityAudio(id);m_audioIdentities.erase(id);
    if(m_cameraRenderer)for(auto& c:m_renderCameras)if(c.id==id)m_cameraRenderer->DestroyRenderTarget(c.target);
    if(m_ui)m_ui->RemoveOwner(id);
    auto erase=[&](auto& v){v.erase(std::remove_if(v.begin(),v.end(),[&](const auto& e){return e.id==id;}),v.end());};
    erase(m_entities);erase(m_extraEntities);m_entityIndexVersion=~0u;erase(m_staticBodies);erase(m_staticRenderables);erase(m_staticLights);
    erase(m_audioZones);erase(m_audioEmitters);erase(m_particleEmitters);erase(m_renderCameras);
    RemoveDeformable(id);m_deformableOwners.erase(id);m_characters.erase(id);m_animationInstances.erase(id);m_physicalAnimations.erase(id);m_animationOwners.erase(id);m_jointOwners.erase(id);m_jointParticipants.erase(id);
    m_scriptDefinitions.erase(id);m_scriptOwners.erase(id);m_characterOwners.erase(id);m_entityCategories.erase(id);m_hierarchy.DestroyObject(id);
    if(auto it=m_regionAssets.find(id);it!=m_regionAssets.end()){
        if(m_assets)for(auto& a:it->second){m_assets->ReleaseRef(a);auto ref=std::find(m_referencedAssets.begin(),m_referencedAssets.end(),a);if(ref!=m_referencedAssets.end())m_referencedAssets.erase(ref);}
        m_regionAssets.erase(it);
    }
    m_regionPending.erase(id);++m_entityVersion;
}
void RuntimeWorld::RebuildRegionGravity(const std::map<EntityId,std::string>& order) {
    m_gravityMap.Clear();m_gravityFields.clear();m_gravityVolumes.clear();m_gravityRegions.clear();
    std::vector<SceneObject> objects;for(const auto& [id,d]:m_scriptDefinitions)if(d.gravity&&RuntimeDefinition(id))objects.push_back(d);std::stable_sort(objects.begin(),objects.end(),[&](const auto& a,const auto& b){
        auto key=[&](EntityId id){auto it=order.find(id);return it==order.end()?std::string("0:")+std::to_string(id):it->second;};return key(a.id)<key(b.id);
    });
    for(auto& o:objects)if(o.gravity){auto g=*o.gravity;std::unique_ptr<GravityField> field;
        if(g.kind==SceneGravityKind::Radial)field=std::make_unique<RadicalGravity>(o.transform.position,g.magnitude);
        else field=std::make_unique<UniformGravity>(o.transform.rotation*glm::vec3(0,-g.magnitude,0));
        std::unique_ptr<GravityVolume> volume;
        if(g.regionShape==SceneRegionShape::Sphere)volume=std::make_unique<SphericalVolume>(o.transform.position,g.regionRadius);
        else volume=std::make_unique<OrientedRegionVolume>(o.transform.position,o.transform.rotation,g.regionHalfExtents);
        m_gravityMap.AddRegion(*field,*volume);m_gravityFields.push_back(std::move(field));m_gravityVolumes.push_back(std::move(volume));m_gravityRegions.push_back({o.id,g,o.transform.position,o.transform.rotation});
    }
}
