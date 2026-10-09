#include "SceneSession.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "PhysicalMaterial.h"
#include "InputSystem.h"
#include <algorithm>
#include <cmath>
#include <limits>

const SceneObject* RuntimeWorld::RuntimeDefinition(EntityId id) const {
    if(!IsPublished(id))return nullptr;
    if(const auto* e=FindEntity(id))if(e->lifecycle==EntityLifecycle::Destroyed)return nullptr;
    auto it=m_scriptDefinitions.find(id);return it==m_scriptDefinitions.end()?nullptr:&it->second;
}
BodyHandle RuntimeWorld::RuntimeBody(EntityId id) const {
    if(!RuntimeDefinition(id))return {};
    if(auto it=m_characters.find(id);it!=m_characters.end())return it->second.motor.observationBody;
    if(const auto* e=FindEntity(id))if(e->slot!=std::numeric_limits<size_t>::max())return m_dynamicBodies[e->slot].Handle();
    auto it=m_entityCategories.find(id);return it==m_entityCategories.end()?BodyHandle{}:it->second.body;
}
std::vector<SceneObject> RuntimeWorld::ScriptObjects(bool scriptedOnly) const {
    std::vector<SceneObject> result;
    if(scriptedOnly){result.reserve(m_scriptOwners.size());for(auto id:m_scriptOwners)if(RuntimeDefinition(id)){result.push_back(m_scriptDefinitions.at(id));++m_metadataWork.definitionsCopied;}}
    else {result.reserve(m_scriptDefinitions.size());for(const auto& [id,definition]:m_scriptDefinitions)if(RuntimeDefinition(id)){result.push_back(definition);++m_metadataWork.definitionsCopied;}}
    return result;
}
std::vector<SceneObject> RuntimeWorld::ScriptSlots() const {
    std::vector<SceneObject> result;result.reserve(m_scriptOwners.size());
    for(auto id:m_scriptOwners)if(const auto* d=RuntimeDefinition(id)){
        SceneObject slots;slots.id=id;slots.scripts=d->scripts;
        m_metadataWork.scriptSlotsCopied+=slots.scripts.size();result.push_back(std::move(slots));
    }
    return result;
}
std::vector<std::pair<EntityId,uint64_t>> RuntimeWorld::ScriptKeys() const {
    std::vector<std::pair<EntityId,uint64_t>> keys;
    for(auto id:m_scriptOwners)if(const auto* d=RuntimeDefinition(id))
        for(const auto& slot:d->scripts)keys.emplace_back(id,slot.id);
    return keys;
}
std::vector<EntityId> RuntimeWorld::DefinitionIds() const {
    std::vector<EntityId> ids;ids.reserve(m_scriptDefinitions.size());
    for(const auto& [id,_]:m_scriptDefinitions)if(RuntimeDefinition(id))ids.push_back(id);
    return ids;
}
bool RuntimeWorld::SetRuntimeTransform(EntityId id,const SceneTransform& t){
    const auto* authored=RuntimeDefinition(id);if(!authored)return false;
    // Baked static capacity data cannot silently follow a runtime teleport.
    if(authored->liquidBasin&&(t.position!=authored->transform.position||t.rotation!=authored->transform.rotation||t.scale!=authored->transform.scale))return false;
    if((authored->liquidContainer||authored->deformable)&&t.scale!=glm::vec3(1))return false;
    if(!std::isfinite(glm::dot(t.position,t.position))||!std::isfinite(glm::dot(t.scale,t.scale))||
       !std::isfinite(glm::dot(t.rotation,t.rotation))||glm::dot(t.rotation,t.rotation)<1e-12f)return false;
    if(auto it=m_characters.find(id);it!=m_characters.end()){
        it->second.motor.Reset(t.position,t.rotation);it->second.previous=t;
    }
    auto transform=t;transform.rotation=glm::normalize(t.rotation);
    auto& definition=m_scriptDefinitions.at(id);definition.transform=transform;
    // Explicit transform writes are teleports, not continuous kinematic motion.
    auto body=RuntimeBody(id);if(body.IsValid())m_physics.ResetBody(body,transform.position,transform.rotation);
    if(auto* e=FindEntity(id)){e->state.position=transform.position;e->state.rotation=transform.rotation;
        if(e->slot!=std::numeric_limits<size_t>::max()){m_dynamicBodies[e->slot].SetPoseFromState(transform.position,transform.rotation);m_dynamicBodies[e->slot].SnapPresentation();m_dynamicVisuals[e->slot].scale=transform.scale;}}
    if(auto* o=m_hierarchy.Find(id)){
        auto local=transform;if(o->parent){const auto* p=RuntimeDefinition(o->parent);if(p){auto parent=PresentedTransform(p->id,p->transform,1);
            local.position=glm::inverse(parent.rotation)*(transform.position-parent.position)/parent.scale;
            local.rotation=glm::inverse(parent.rotation)*transform.rotation;local.scale=transform.scale/parent.scale;}}
        o->transform=local;
    }
    for(auto& r:m_staticRenderables)if(r.id==id){r.position=transform.position;r.rotation=transform.rotation;r.scale=transform.scale;}
    for(auto& l:m_staticLights)if(l.id==id){l.position=transform.position;l.direction=transform.rotation*glm::vec3(0,0,-1);}
    for(auto& e:m_audioEmitters)if(e.id==id)e.transform=transform;
    for(auto& e:m_particleEmitters)if(e.id==id)e.transform=transform;
    for(auto& c:m_renderCameras)if(c.id==id)c.transform=transform;
    return true;
}
bool RuntimeWorld::SetRuntimeMotionType(EntityId id,SceneBodyMotion motion,bool preserveVelocity,std::string& error){
    const auto* definition=RuntimeDefinition(id);auto* record=FindEntity(id);auto body=RuntimeBody(id);
    if(!definition||!definition->body||!body.IsValid()){
        error="authority change requires a live ordinary rigid body";return false;
    }
    if(motion!=SceneBodyMotion::Static&&motion!=SceneBodyMotion::Dynamic&&motion!=SceneBodyMotion::Kinematic){error="invalid body motion type";return false;}
    if((record&&record->fidelity!=SimulationFidelity::Full)||definition->body->managed||definition->characterMotor||definition->animation||definition->ragdoll||
       definition->vehicle||definition->combustible||definition->celestial||definition->door||definition->lightSwitch||definition->gravity||
       definition->atmosphere||definition->fluidVolume||definition->liquidBasin||definition->liquidContainer||definition->deformable||
       definition->navigationSurface||m_jointParticipants.count(id)){
        error="this body's component ownership or fidelity does not support authority changes";return false;
    }
    const auto native=motion==SceneBodyMotion::Static?BodyMotionType::Static:motion==SceneBodyMotion::Dynamic?BodyMotionType::Dynamic:BodyMotionType::Kinematic;
    const auto previous=m_physics.GetMotionType(body);
    if(!m_physics.SetMotionType(body,native,definition->body->mass,preserveVelocity)){
        error="unsupported shape, mass or body authority transition";return false;
    }
    if(!record){
        // Native consumers may change an otherwise ordinary static actor in a
        // scene with no scripts. Give it normal identity bookkeeping lazily.
        EntityRecord retained;retained.id=id;retained.name=definition->name;retained.definition=*definition;
        retained.state.position=definition->transform.position;retained.state.rotation=definition->transform.rotation;
        retained.slot=std::numeric_limits<size_t>::max();retained.requiresFull=true;
        m_extraEntities.push_back(std::move(retained));record=FindEntity(id);
    }
    // A static actor acquires the existing moving presentation slot, preserving
    // its physical handle, tags and touch subscriptions. Slot ownership then
    // remains stable even when the body is made static again.
    if(record->slot==std::numeric_limits<size_t>::max()&&native!=BodyMotionType::Static){
        auto retained=*record;const auto pose=m_physics.GetTransform(body);const auto& settings=*definition->body;
        DynamicBody::Visual visual;visual.shape=settings.shape==SceneShape::Sphere?DynamicBody::Shape::Sphere:DynamicBody::Shape::Box;
        visual.halfExtents=settings.halfExtents;visual.radius=settings.radius;if(definition->render)visual.color=definition->render->color;
        DynamicVisual dv;dv.id=id;dv.name=definition->name;dv.hasRender=bool(definition->render);if(definition->render)dv.render=*definition->render;
        dv.compoundBoxes=settings.compoundBoxes;dv.scale=definition->transform.scale;dv.initialLinearVelocity=settings.initialLinearVelocity;dv.pickable=settings.pickable;
        retained.slot=m_dynamicBodies.size();retained.requiresFull=true;
        m_dynamicBodies.emplace_back(body,visual,pose.position,pose.rotation);m_dynamicVisuals.push_back(std::move(dv));
        m_extraEntities.erase(std::remove_if(m_extraEntities.begin(),m_extraEntities.end(),[id](const auto& e){return e.id==id;}),m_extraEntities.end());
        m_entities.push_back(std::move(retained));m_entityIndexVersion=~0u;
        m_staticBodies.erase(std::remove_if(m_staticBodies.begin(),m_staticBodies.end(),[id](const auto& b){return b.id==id;}),m_staticBodies.end());
        m_staticRenderables.erase(std::remove_if(m_staticRenderables.begin(),m_staticRenderables.end(),[id](const auto& r){return r.id==id;}),m_staticRenderables.end());
        record=FindEntity(id);
    }
    m_scriptDefinitions.at(id).body->motion=motion;
    // Baseline authored definitions remain the Stop/reset source. Authority
    // changes pin their ordinary slot to Full so reconstruction cannot reapply
    // baseline authority while a command is active.
    record->requiresFull=true;
    if(previous!=native&&record->slot!=std::numeric_limits<size_t>::max()){
        auto& presentation=m_dynamicBodies[record->slot];presentation.SyncFromPhysics(m_physics);presentation.SnapPresentation();
    }
    ++m_entityVersion;return true;
}
void RuntimeWorld::UpdateScripts(const InputSystem* input,float dt){
    if(!m_scripts){if(!m_hasScripts)return;
        m_scripts=std::make_unique<ScriptSystem>(this,m_assets?m_assets->Assets():nullptr);}
    m_scripts->Synchronize(ScriptSlots());m_scripts->Frame(input,dt);
}
void RuntimeWorld::FixedScripts(const InputSystem* input,float dt){
    // Register authored motor geometry before this step, including its first one.
    for(auto id:m_characterOwners)RuntimeCharacter(id);
    SynchronizeJoints();
    if(!m_scripts){if(!m_hasScripts)return;
        m_scripts=std::make_unique<ScriptSystem>(this,m_assets?m_assets->Assets():nullptr);}
    if(m_scripts){m_scripts->Synchronize(ScriptSlots());m_scripts->Fixed(input,dt);}
}

bool RuntimeWorld::RestoreScriptState(const std::vector<ScriptStateRecord>& records,std::string& error,bool resume){
    if(records.empty())return true;
    if(!m_scripts)m_scripts=std::make_unique<ScriptSystem>(this,m_assets?m_assets->Assets():nullptr);
    return m_scripts->Restore(records,error,resume);
}

void RuntimeWorld::UpdateUIScripts(InputSystem* input,float dt){
    if(!m_scripts){if(!m_hasScripts)return;m_scripts=std::make_unique<ScriptSystem>(this,m_assets?m_assets->Assets():nullptr);}
    bool wasPaused=m_ui&&m_ui->Paused();m_scripts->Synchronize(ScriptSlots());m_scripts->UIFrame(input,dt);
    if(input&&!wasPaused&&m_ui&&m_ui->Paused())input->ConsumeBindings({"pause"});
}
void RuntimeWorld::DispatchUIEvents(const InputSystem* input,float dt){if(m_scripts)m_scripts->UIEvents(input,dt);}

void RuntimeWorld::DispatchPhysicsEvents(const InputSystem* input,float dt){
    if(!m_scripts)return; // No script consumers: do not scan/copy entity definitions.
    // Freeze pair->entity identities before any callback can destroy/spawn bodies.
    // Only bodies in actual observations need identity resolution. Exit events
    // can use the preceding generation's frozen identity after body destruction.
    auto resolve=[&](BodyHandle h){auto found=m_touchEntityHistory.find(h.id);
        if(found!=m_touchEntityHistory.end())return found->second;
        ++m_metadataWork.touchBodyResolutions;return EntityIdOfBody(h);};
    struct Delivery {EntityId a,b;PhysicsWorld::TouchEvent event;std::pair<EntityId,std::string> aa,bb;};
    std::vector<Delivery> deliveries;
    std::map<unsigned,EntityId> next;
    std::map<unsigned,std::pair<EntityId,std::string>> nextArticulation;
    auto articulation=[&](BodyHandle h,EntityId entity){auto found=m_touchArticulationHistory.find(h.id);return found==m_touchArticulationHistory.end()?ArticulatedIdentity(entity):found->second;};
    for(const auto& event:m_physics.LastStepTouchEvents()){
        auto a=resolve(event.a),b=resolve(event.b);
        if(!a||!b)continue;
        auto aa=articulation(event.a,a),bb=articulation(event.b,b);deliveries.push_back({a,b,event,aa,bb});
        if(event.phase!=PhysicsWorld::TouchPhase::Exit){if(aa.first)nextArticulation[event.a.id]=aa;if(bb.first)nextArticulation[event.b.id]=bb;}
        if(event.phase!=PhysicsWorld::TouchPhase::Exit){next[event.a.id]=a;next[event.b.id]=b;}
    }
    if(m_scripts){m_scripts->Synchronize(ScriptSlots());
        for(const auto& d:deliveries){m_scripts->PhysicsEvent(d.a,d.b,d.event,false,d.a,d.aa.second,d.bb.first,d.bb.second);m_scripts->PhysicsEvent(d.b,d.a,d.event,true,d.b,d.bb.second,d.aa.first,d.aa.second);
            // Forward the existing per-body-pair observation only on explicit
            // owner subscription. Internal self contacts do not call the same
            // owner twice or flood it with articulation constraint contacts;
            // scripts placed on individual bodies retain ordinary M42 delivery.
            auto subscribed=[&](EntityId owner){const auto* definition=RuntimeDefinition(owner);
                return definition&&definition->ragdoll&&definition->ragdoll->receiveContactEvents;};
            if(d.aa.first!=d.bb.first){
                if(d.aa.first&&d.aa.first!=d.a&&subscribed(d.aa.first))m_scripts->PhysicsEvent(d.aa.first,d.b,d.event,false,d.a,d.aa.second,d.bb.first,d.bb.second);
                if(d.bb.first&&d.bb.first!=d.b&&subscribed(d.bb.first))m_scripts->PhysicsEvent(d.bb.first,d.a,d.event,true,d.b,d.bb.second,d.aa.first,d.aa.second);
            }
        }}
    m_touchEntityHistory=std::move(next);m_touchArticulationHistory=std::move(nextArticulation);(void)input;(void)dt;
}
bool RuntimeWorld::SetColliderEnabled(EntityId id,bool enabled){
    if(const auto* d=RuntimeDefinition(id);d&&d->characterMotor){auto settings=*d->characterMotor;settings.enabled=enabled;SetCharacterSettings(id,settings);return true;}
    auto h=RuntimeBody(id);if(!h.IsValid()||!m_physics.SetBodyEnabled(h,enabled))return false;
    auto& d=m_scriptDefinitions.at(id);if(d.body)d.body->enabled=enabled;return true;
}

void RuntimeWorld::SynchronizeJoints(){
    for(auto owner:m_jointOwners){const auto* definition=RuntimeDefinition(owner);
        auto it=m_runtimeJoints.find(owner);JointState state;
        if(!definition||!definition->joint){if(it!=m_runtimeJoints.end()){m_physics.DestroyJoint(it->second);m_runtimeJoints.erase(it);}continue;}
        const auto& j=*definition->joint;auto a=RuntimeBody(j.bodyA),b=RuntimeBody(j.bodyB);
        if(it!=m_runtimeJoints.end()&&m_physics.GetJoint(it->second,state)&&state.settings.bodyA.id==a.id&&state.settings.bodyB.id==b.id)continue;
        if(it!=m_runtimeJoints.end()){m_physics.DestroyJoint(it->second);m_runtimeJoints.erase(it);}
        if(!a.IsValid()||(j.bodyB&&!b.IsValid()))continue;
        auto settings=j.settings;settings.bodyA=a;settings.bodyB=b;
        if(!j.bodyB){settings.anchorB=definition->transform.position+definition->transform.rotation*settings.anchorB;settings.frameB=definition->transform.rotation*settings.frameB;}
        auto handle=m_physics.CreateJoint(settings);if(handle.IsValid())m_runtimeJoints[owner]=handle;
    }
}
JointHandle RuntimeWorld::RuntimeJoint(EntityId owner){SynchronizeJoints();auto it=m_runtimeJoints.find(owner);return it==m_runtimeJoints.end()?JointHandle{}:it->second;}

void RuntimeWorld::PresentationScripts(const InputSystem* input,float dt,float alpha){
    // Downstream of fixed simulation; reuse the renderer's existing pose history.
    if(m_scripts)m_scripts->Presentation(input,dt,alpha);
}

LocalizationSession& RuntimeWorld::Localization(){if(auto scenes=SceneControl())return scenes->Localization(m_assets);if(!m_localization)m_localization=std::make_unique<LocalizationSession>();m_localization->Bind(m_assets);return *m_localization;}

bool RuntimeWorld::SetRuntimeJoint(EntityId owner,const SceneJointComponent& authored,bool remove,std::string& error){
 auto* def=RuntimeDefinition(owner);if(!def){error="stale joint owner";return false;}
 auto old=m_runtimeJoints.find(owner);
 if(remove){if(old!=m_runtimeJoints.end()){m_physics.DestroyJoint(old->second);m_runtimeJoints.erase(old);}m_scriptDefinitions.at(owner).joint.reset();if(auto* e=FindEntity(owner))e->definition.joint.reset();if(auto* h=m_hierarchy.Find(owner))h->joint.reset();m_jointOwners.erase(owner);return true;}
 auto copy=authored;auto& s=copy.settings;s.bodyA=RuntimeBody(copy.bodyA);s.bodyB=RuntimeBody(copy.bodyB);
 auto* a=RuntimeDefinition(copy.bodyA);auto* b=RuntimeDefinition(copy.bodyB);
 if(!a||!a->body||(copy.bodyB&&(!b||!b->body))||!ValidJointSettings(s)||!s.bodyA.IsValid()||(copy.bodyB&&!s.bodyB.IsValid())||copy.bodyA==copy.bodyB||(!m_physics.IsDynamicBody(s.bodyA)&&(!copy.bodyB||!m_physics.IsDynamicBody(s.bodyB)))){error="joint requires valid body references/settings and a dynamic participant";return false;}
 if(!copy.bodyB){s.anchorB=def->transform.position+def->transform.rotation*s.anchorB;s.frameB=glm::normalize(def->transform.rotation*s.frameB);}
 // Scripts run outside the physics solver; validate and allocate before replacing.
 JointHandle replacement;JointState previous;bool same=old!=m_runtimeJoints.end()&&m_physics.GetJoint(old->second,previous)&&previous.settings.bodyA.id==s.bodyA.id&&previous.settings.bodyB.id==s.bodyB.id;
 if(same){replacement=old->second;if(!m_physics.SetJoint(replacement,s)){error="invalid joint configuration";return false;}}else replacement=m_physics.CreateJoint(s);if(!replacement.IsValid()){error="joint allocation failed";return false;}
 if(!same&&old!=m_runtimeJoints.end())m_physics.DestroyJoint(old->second);
 copy.settings.bodyA={};copy.settings.bodyB={};m_scriptDefinitions.at(owner).joint=copy;if(auto* e=FindEntity(owner))e->definition.joint=copy;if(auto* h=m_hierarchy.Find(owner))h->joint=copy;
 m_runtimeJoints[owner]=replacement;m_jointOwners.insert(owner);m_jointParticipants.insert(copy.bodyA);if(copy.bodyB)m_jointParticipants.insert(copy.bodyB);return true;
}
bool RuntimeWorld::SetBodyMaterial(EntityId id,const std::string& asset,const PhysicalMaterial* factors,std::string& error){
 auto* d=RuntimeDefinition(id);auto body=RuntimeBody(id);if(!d||!d->body||!body.IsValid()){error="stale entity or unavailable physical body";return false;}
 PhysicalMaterial material{d->body->friction,d->body->restitution};
 if(!asset.empty()){if(!m_assets){error="physical material needs project resources";return false;}auto loaded=m_assets->RequirePhysicalMaterial(asset,error);if(!loaded)return false;material=*loaded;}
 if(factors)material=*factors;
 if(!ValidPhysicalMaterial(material,error))return false;
 if(!m_physics.SetPhysicalMaterial(body,asset,material.friction,material.restitution)){error="physical material mutation failed";return false;}
 auto settings=*d->body;settings.physicalMaterial=asset;settings.physicalMaterialOverride=factors!=nullptr;settings.friction=material.friction;settings.restitution=material.restitution;m_scriptDefinitions.at(id).body=settings;if(auto* e=FindEntity(id))e->definition.body=settings;
 if(!asset.empty()){m_assets->AddRef(asset);m_referencedAssets.push_back(asset);}return true;
}
