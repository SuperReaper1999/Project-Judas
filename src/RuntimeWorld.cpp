#include "SceneSession.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include "CollisionAuthoring.h"
#include <fstream>
#include <sstream>
#include "Prefab.h"
#include <set>
#include <sstream>
#include "SceneSerialization.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "FaithfulGravity.h"
#include "RadialTerrain.h"
#include "ProductionFluidCoupling.h"
#include "RadicalGravity.h"
#include "ResourceManager.h"
#include "SceneFingerprint.h"
#include "SphericalVolume.h"
#include "TerrainLibrary.h"
#include "UniformGravity.h"

namespace {
constexpr float kFaithfulMagnitude = 9.81f;
// Below these an entity leaving Full simulation is treated as resting on
// whatever supported it (CoarseMotion::Settled) rather than in free flight.
constexpr float kSettledLinearSpeed = 0.05f;
constexpr float kSettledAngularSpeed = 0.05f;

EntityPhysicalState StateFromDefinition(const SceneObject& o) {
    EntityPhysicalState state;
    state.position = o.transform.position;
    state.rotation = glm::normalize(o.transform.rotation);
    if (o.body) state.linearVelocity = o.body->initialLinearVelocity;
    return state;
}
}  // namespace

RuntimeWorld::RuntimeWorld() = default;

RuntimeWorld::~RuntimeWorld() {
    Destroy();
}

bool RuntimeWorld::EntityRequiresFull(const SceneObject& o) {
    if (o.vehicle || o.combustible || o.characterMotor || o.deformable || o.liquidContainer || o.liquidInteraction || !o.scripts.empty()) return true;
    if (o.body && o.body->shape == SceneShape::Compound) return true;
    return false;
}

bool RuntimeWorld::ValidateVisualAssets(const SceneObject& o, std::string& error) const {
    if(o.deformable){const auto* db=m_assets?m_assets->Assets():nullptr;auto* record=db?db->Find(o.deformable->asset):nullptr;if(!record||record->missing||record->type!=AssetType::Deformable){error="missing/wrong-type deformable asset";return false;}}
    if(o.liquidBasin||o.liquidContainer){const auto* db=m_assets?m_assets->Assets():nullptr;for(auto id:o.liquidBasin?std::vector<std::string>{o.liquidBasin->geometry,o.liquidBasin->asset}:std::vector<std::string>{o.liquidContainer->geometry}){const auto* a=db?db->Find(id):nullptr;LiquidResource resource;if(!a||a->missing||a->type!=AssetType::Liquid){error="missing/wrong-type liquid asset";return false;}std::ifstream file(a->path,std::ios::binary);std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),{}};if(!DecodeLiquidResource(bytes,resource,error))return false;}}

    if(o.liquidContainer){const auto* db=m_assets?m_assets->Assets():nullptr;const auto* a=db?db->Find(o.liquidContainer->geometry):nullptr;LiquidGeometry geometry;if(!a||!LoadLiquidGeometry(a->path,geometry,error))return false;double capacity=0;for(auto& t:geometry.cells)capacity+=LiquidClip(t,{0,0,0,0},1).volume;if(o.liquidContainer->initialVolume>capacity){error="initial container volume exceeds physical capacity";return false;}}
    if (!o.render || o.render->shape != SceneShape::Mesh || !m_assets) return true;
    const AssetDatabase* db = m_assets->Assets();
    const auto check = [&](const AssetId& id, AssetType type) {
        if (id.empty()) return true;
        const AssetRecord* record = db ? db->Find(id) : nullptr;
        if (!record) {
            error = "unknown asset id " + id;
            return false;
        }
        if (record->type != type) {
            error = "asset " + record->relativePath + " is a " + AssetTypeName(record->type) + ", not a " + AssetTypeName(type);
            return false;
        }
        return true;
    };
    if (o.render->meshAsset.empty()) {
        error = "mesh render has no mesh asset";
        return false;
    }
    if (!check(o.render->meshAsset, AssetType::Mesh) || !check(o.render->textureAsset, AssetType::Texture)) return false;
    return true;
}

bool RuntimeWorld::RequestVisualAssets(const SceneObject& o, std::string* outError) {
    std::string error;
    if (!ValidateVisualAssets(o, error)) { if (outError) *outError = error; return false; }
    if(o.render&&m_assets)for(const auto& slot:o.render->materials)if(!slot.asset.empty()){m_assets->AddRef(slot.asset);m_referencedAssets.push_back(slot.asset);m_assets->RequestMaterial(slot.asset,JobPriority::High);}
    if (!o.render || o.render->shape != SceneShape::Mesh || !m_assets) return true;
    // Demand: referenced for the life of this world; the load runs in the
    // background and presentation picks it up when Ready.
    m_assets->AddRef(o.render->meshAsset);
    m_referencedAssets.push_back(o.render->meshAsset);
    m_assets->RequestMesh(o.render->meshAsset, JobPriority::High);
    if (!o.render->textureAsset.empty()) {
        m_assets->AddRef(o.render->textureAsset);
        m_referencedAssets.push_back(o.render->textureAsset);
        m_assets->RequestTexture(o.render->textureAsset, JobPriority::High);
    }
    return true;
}

bool RuntimeWorld::InstantiateEntityBody(EntityRecord& record, const EntityPhysicalState& state,
                                         std::string* outError) {
    const SceneObject& o = record.definition;
    const SceneBodyComponent& b = *o.body;
    BodyHandle handle;
    try {
    switch (b.shape) {
        case SceneShape::Box:
            handle = m_physics.CreateDynamicBox(state.position, b.halfExtents, b.mass, b.friction, b.restitution);
            break;
        case SceneShape::Sphere:
            handle = m_physics.CreateDynamicSphere(state.position, b.radius, b.mass, b.friction, b.restitution);
            break;
        case SceneShape::ConvexHull: {
            Shape shape;std::string error;if(!ResolveBodyCollision(b,m_assets,shape,error)){if(outError)*outError=error;return false;}
            handle=m_physics.CreateShape(shape,{state.position,state.rotation},true,b.mass,b.friction,b.restitution);break;
        }
        case SceneShape::TriangleMesh:break;
        case SceneShape::Compound:
            if(ExtendedCompound(b)){Shape shape;std::string error;if(!ResolveBodyCollision(b,m_assets,shape,error)){if(outError)*outError=error;return false;}handle=m_physics.CreateShape(shape,{state.position,state.rotation},true,b.mass,b.friction,b.restitution);break;}
            handle = m_physics.CreateDynamicCompoundBoxes(state.position, b.compoundBoxes, b.mass, b.friction,
                                                          b.restitution);
            break;
        case SceneShape::Terrain:
        case SceneShape::Mesh:
            break;
    }
    }catch(const std::exception& e){if(outError)*outError=e.what();return false;}
    if (!handle.IsValid()) {
        if (outError) *outError = "the body could not be created";
        return false;
    }
    // Reconstruction hands the body its retained pose AND velocities: no
    // reset to rest, no impulse.
    if(!b.physicalMaterial.empty()){if(!m_assets){m_physics.DestroyBody(handle);if(outError)*outError="physical material requires project resources";return false;}std::string error;auto mat=m_assets->RequirePhysicalMaterial(b.physicalMaterial,error);if(!mat){m_physics.DestroyBody(handle);if(outError)*outError=error;return false;}m_physics.SetPhysicalMaterial(handle,b.physicalMaterial,b.physicalMaterialOverride?b.friction:mat->friction,b.physicalMaterialOverride?b.restitution:mat->restitution);}
    m_physics.SetCollisionFilter(handle,b.collisionLayer,b.collisionMask);
    m_physics.SetBodySensor(handle,b.sensor);m_physics.SetBodyEnabled(handle,b.enabled);
    m_physics.SetBodyTags(handle,TagsOf(record.id));
    m_entityCategories[record.id].body=handle;
    m_physics.ResetBody(handle, state.position, state.rotation);
    m_physics.SetLinearVelocity(handle, state.linearVelocity);
    m_physics.SetAngularVelocity(handle, state.angularVelocity);
    DynamicBody& slot = m_dynamicBodies[record.slot];
    slot.Rebind(handle);
    slot.SetPoseFromState(state.position, state.rotation);
    slot.SnapPresentation();
    return true;
}

void RuntimeWorld::ReleaseEntityBody(EntityRecord& record) {
    DynamicBody& slot = m_dynamicBodies[record.slot];
    if (slot.IsLive()) {
        m_physics.DestroyBody(slot.Handle());
        slot.Rebind(BodyHandle{});
    }
}

bool RuntimeWorld::AppendEntitySlot(const SceneObject& o, bool authored, const EntityPhysicalState& state,
                                    SimulationFidelity fidelity, std::string* outError) {
    const SceneBodyComponent& b = *o.body;
    DynamicBody::Visual visual;
    visual.shape = b.shape == SceneShape::Sphere ? DynamicBody::Shape::Sphere : DynamicBody::Shape::Box;
    visual.halfExtents = b.halfExtents;
    visual.radius = b.radius;
    if (o.render) visual.color = o.render->color;

    DynamicVisual dv;
    dv.id = o.id;
    dv.name = o.name;
    dv.hasRender = o.render.has_value();
    if (o.render) dv.render = *o.render;
    dv.compoundBoxes = b.compoundBoxes;
    dv.scale = o.transform.scale;
    dv.initialLinearVelocity = b.initialLinearVelocity;
    dv.pickable = b.pickable;
    if (!RequestVisualAssets(o, outError)) return false;

    EntityRecord record;
    record.id = o.id;
    record.name = o.name;
    record.definition = o;
    record.authored = authored;
    record.managed = b.managed;
    record.requiresFull = EntityRequiresFull(o)||m_jointParticipants.count(o.id);
    record.state = state;
    record.slot = m_dynamicBodies.size();
    if (record.requiresFull) fidelity = SimulationFidelity::Full;
    record.fidelity = fidelity;
    record.lifecycle = fidelity == SimulationFidelity::Dormant ? EntityLifecycle::Unloaded : EntityLifecycle::Active;
    if (fidelity == SimulationFidelity::Dormant) record.dormantSinceSeconds = m_simulationTime;
    record.coarseMotion = glm::length(state.linearVelocity) < kSettledLinearSpeed &&
                                  glm::length(state.angularVelocity) < kSettledAngularSpeed
                              ? CoarseMotion::Settled
                              : CoarseMotion::Inertial;

    m_dynamicBodies.emplace_back(BodyHandle{}, visual, o.transform.position, glm::normalize(o.transform.rotation));
    m_dynamicBodies.back().SetPoseFromState(state.position, state.rotation);
    m_dynamicBodies.back().SnapPresentation();
    m_dynamicVisuals.push_back(dv);
    m_entities.push_back(record);
    if (fidelity == SimulationFidelity::Full) {
        if (!InstantiateEntityBody(m_entities.back(), state, outError)) {
            m_dynamicBodies.pop_back();
            m_dynamicVisuals.pop_back();
            m_entities.pop_back();
            return false;
        }
    }
    ++m_entityVersion;
    return true;
}

bool RuntimeWorld::Build(const Scene& authored, ResourceManager* resources, std::string& outError, const ProjectClassification* categories,const ProjectNavigation* navigation,bool runtimeSnapshot) {
 JUDAS_PROFILE_SCOPE("World build"); PerformanceProfiler::Get().Boundary("World build");
    Scene resolved, scene;
    if(runtimeSnapshot){resolved=scene=authored;} // already resolved world-space definitions, source content validated by save service
    else if (!ResolvePrefabs(authored, resources ? resources->Assets() : nullptr, resolved, outError) ||
        !FlattenHierarchy(resolved, scene, outError)) return false;
    const ProjectClassification selected=categories?*categories:ProjectClassification{};
    if(!ValidateSceneClassification(scene,selected,outError))return false;
    const auto navConfig=navigation?*navigation:ProjectNavigation{};if(!navConfig.Validate(outError))return false;
    for(const auto& o:scene.Objects()){if(!ValidateNavigationComponents(o,outError))return false;if((o.navigationSurface&&!navConfig.profiles.count(o.navigationSurface->profile))||(o.navigationAgent&&!navConfig.profiles.count(o.navigationAgent->profile))||(o.navigationModifier&&!navConfig.areas.names.count(o.navigationModifier->area))||(o.navigationLink&&!navConfig.areas.names.count(o.navigationLink->area))){outError="unknown navigation profile/area";return false;}if(o.navigationAgent)for(auto [id,c]:o.navigationAgent->costs)if(!navConfig.areas.names.count(id)){outError="unknown navigation area cost";return false;}}
    int activeAudioListeners=0;
    for(const auto& o:scene.Objects())if(o.audioListener&&o.audioListener->enabled)++activeAudioListeners;
    if(activeAudioListeners>1){outError="more than one enabled audio listener";return false;}
    for (const auto& o : scene.Objects()) {
        if (o.renderCamera) {
            const auto& c = *o.renderCamera;
            if (c.width < 1 || c.width > 4096 || c.height < 1 || c.height > 4096 ||
                c.updateEveryFrames < 1 || !(c.verticalFovDegrees > 0 && c.verticalFovDegrees < 179) ||
                !(c.nearPlane > 0 && c.farPlane > c.nearPlane) || o.door || o.lightSwitch) {
                outError = "invalid render-camera configuration"; return false;
            }
        }
        if (o.render && o.render->textureCamera) {
            const auto* c = scene.Find(o.render->textureCamera);
            if (!c || !c->renderCamera || !o.render->textureAsset.empty() ||
                (o.render->shape != SceneShape::Box && o.render->shape != SceneShape::Sphere && o.render->shape != SceneShape::Mesh)) {
                outError = "invalid render-camera texture reference"; return false;
            }
        }
    }
    std::string fingerprint;
    if (!ComputeSceneFingerprint(scene, fingerprint, outError)) return false;
    if(resources&&resources->Assets()){
        std::set<std::string> collisionIds;for(const auto& o:scene.Objects())if(o.body){if(!o.body->collisionAsset.empty())collisionIds.insert(o.body->collisionAsset);for(auto& c:o.body->compoundBoxes)if(!c.assetId.empty())collisionIds.insert(c.assetId);}
        if(!collisionIds.empty()){std::string content="Judas.CollisionSources.1:"+fingerprint;for(auto& id:collisionIds){auto* a=resources->Assets()->Find(id);std::string hash;if(!a||a->missing||a->type!=AssetType::Collision||!SceneFingerprintSha256File(a->path,hash,outError)){outError="missing collision asset "+id+": "+outError;return false;}content+=id+hash;}fingerprint=SceneFingerprintSha256(content);}
    }
    bool appearanceDependencies=scene.Settings().linearRendering||!scene.Settings().environmentAsset.empty();
    for(const auto& o:scene.Objects())appearanceDependencies|=o.render&&!o.render->materials.empty();
    if(resources&&resources->Assets()){
        // Registered M57 resources may be assigned later by project scripts.
        // Projects without optional M57 content retain their previous baseline.
        for(const auto& [id,record]:resources->Assets()->Records())
            appearanceDependencies|=record.type==AssetType::Material||record.type==AssetType::Environment;
        if(appearanceDependencies){
            std::string content="Judas.MaterialSources.1";
            for(const auto& [id,record]:resources->Assets()->Records()){
                if(record.type!=AssetType::Material&&record.type!=AssetType::Environment&&record.type!=AssetType::Texture&&record.type!=AssetType::Mesh)continue;
                if(record.missing){outError="missing appearance asset "+record.relativePath;return false;}
                std::ifstream file(record.path,std::ios::binary);
                if(!file){outError="cannot read appearance asset "+record.relativePath;return false;}
                std::string bytes{std::istreambuf_iterator<char>(file),{}};
                if(file.bad()){outError="failed reading appearance asset "+record.relativePath;return false;}
                content+=id+SceneFingerprintSha256(bytes);
            }
            fingerprint=SceneFingerprintSha256(fingerprint+content);
        }
    }

    if(std::any_of(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){return !o.scripts.empty();})){
        if(!resources||!resources->Assets()){outError="scripted scene requires a project asset database";return false;}
        std::string scripts;if(!ScriptSystem::SourceFingerprint(*resources->Assets(),scene,scripts,outError,false))return false;
        fingerprint=SceneFingerprintSha256(fingerprint+scripts);
    }
    if(std::any_of(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){return o.ui.has_value()||!o.scripts.empty();})&&resources&&resources->Assets()&&std::any_of(resources->Assets()->Records().begin(),resources->Assets()->Records().end(),[](const auto& p){return p.second.type==AssetType::UI;})){
        if(!resources||!resources->Assets()){outError="UI scene requires project assets";return false;}
        std::string bytes="Judas.UISources.1";
        for(const auto& p:resources->Assets()->Records())if(p.second.type==AssetType::UI){std::ifstream file(p.second.path);std::ostringstream contents;contents<<file.rdbuf();bytes+=p.first+SceneFingerprintSha256(contents.str());}
        fingerprint=SceneFingerprintSha256(fingerprint+SceneFingerprintSha256(bytes));
    }
    if(resources&&resources->Assets()){std::string navBytes="Judas.NavSources.1";bool has=false;for(const auto& o:scene.Objects())if(o.navigationSurface){has=true;auto* asset=resources->Assets()->Find(o.navigationSurface->asset);if(asset){std::ifstream file(asset->path,std::ios::binary);std::ostringstream contents;contents<<file.rdbuf();navBytes+=asset->id+SceneFingerprintSha256(contents.str());}}if(has)fingerprint=SceneFingerprintSha256(fingerprint+navBytes);}
    if(resources&&resources->Assets()){bool localized=std::any_of(resources->Assets()->Records().begin(),resources->Assets()->Records().end(),[](const auto& p){return p.second.type==AssetType::Catalog;});if(auto control=SceneControl())localized|=!control->Localization(resources).Configuration().fonts.empty();if(localized){std::string bytes="Judas.LocalizationSources.1";for(auto& p:resources->Assets()->Records())if(p.second.type==AssetType::Catalog||p.second.type==AssetType::Font){std::ifstream f(p.second.path,std::ios::binary);if(!f){outError="cannot fingerprint localization dependency "+p.first;return false;}std::string content(std::istreambuf_iterator<char>(f),{});bytes+=p.first+SceneFingerprintSha256(content);}if(auto control=SceneControl())bytes+=control->Localization(resources).Configuration().Encode();fingerprint=SceneFingerprintSha256(fingerprint+bytes);}}
    if(resources&&resources->Assets()){std::string liquidBytes="Judas.LiquidSources.1";bool has=false;for(const auto& o:scene.Objects())if(o.liquidBasin||o.liquidContainer){has=true;std::vector<std::string> ids;if(o.liquidBasin)ids={o.liquidBasin->geometry,o.liquidBasin->asset};else ids={o.liquidContainer->geometry};for(auto id:ids){auto* asset=resources->Assets()->Find(id);if(!asset||asset->missing||asset->type!=AssetType::Liquid){outError="missing/wrong-type liquid asset";return false;}std::ifstream file(asset->path,std::ios::binary);std::ostringstream contents;contents<<file.rdbuf();liquidBytes+=id+SceneFingerprintSha256(contents.str());}if(o.liquidBasin&&!runtimeSnapshot){LiquidBasinData data;auto* asset=resources->Assets()->Find(o.liquidBasin->asset);if(!LoadLiquidBasin(asset->path,data,outError)||LiquidSourceFingerprint(scene,o,*resources->Assets(),outError)!=data.fingerprint){outError="stale/invalid liquid bake: "+outError;return false;}}}if(has)fingerprint=SceneFingerprintSha256(fingerprint+liquidBytes);}
    for(const auto& o:scene.Objects())if(!ValidateLiquidComponents(o,outError))return false;
    Destroy();
    m_restoreConstruction=runtimeSnapshot;
    m_categories=selected;
    m_navigation=std::make_unique<NavigationSystem>(navigation?*navigation:ProjectNavigation{});
    m_assets = resources;
    m_audioSystem=resources?resources->GetAudioSystem():nullptr;
    m_settings = scene.Settings();
    if(m_assets&&!m_settings.environmentAsset.empty()){m_assets->AddRef(m_settings.environmentAsset);m_referencedAssets.push_back(m_settings.environmentAsset);m_assets->RequestEnvironment(m_settings.environmentAsset,JobPriority::High);}
    if(std::any_of(resolved.Objects().begin(),resolved.Objects().end(),[](const auto& o){return o.parent!=0;})){
        m_hierarchy=resolved;
        if(runtimeSnapshot)for(auto& local:m_hierarchy.Objects())if(local.parent){
            const auto* parent=resolved.Find(local.parent);
            if(!parent){outError="snapshot hierarchy parent unavailable";return false;}
            auto inverse=glm::inverse(parent->transform.rotation);
            if(glm::any(glm::lessThan(glm::abs(parent->transform.scale),glm::vec3(1e-6f)))){outError="snapshot hierarchy singular scale";return false;}
            local.transform.position=(inverse*(local.transform.position-parent->transform.position))/parent->transform.scale;
            local.transform.rotation=glm::normalize(inverse*local.transform.rotation);
            local.transform.scale/=parent->transform.scale;
        }
    }
    if (!m_physics.Init()) {
        outError = "physics initialization failed";
        return false;
    }
    m_built = true;
    for(const auto& o:scene.Objects())if(o.joint){m_jointParticipants.insert(o.joint->bodyA);if(o.joint->bodyB)m_jointParticipants.insert(o.joint->bodyB);}

    if (!(m_settings.fluidScale > 0.0f)) {
        outError = "settings: fluid-scale must be positive";
        Destroy();
        return false;
    }
    m_fluidSettings.updateRateHz = m_settings.fluidUpdateRateHz;
    m_fluidSettings.hydrostaticDragRate = m_settings.fluidHydrostaticDragRate;
    m_fluidSettings.particleRadius *= m_settings.fluidScale;
    m_fluidSettings.smoothingRadius *= m_settings.fluidScale;
    m_fluidSettings.maxDensityCorrection *= m_settings.fluidScale;
    m_fluid = std::make_unique<FluidWorld>(m_fluidSettings);

    if (m_settings.fidelityPolicy == SceneFidelityPolicy::Distance) {
        m_policy = std::make_unique<DistanceFidelityPolicy>(m_settings.fidelityFullRadius,
                                                            m_settings.fidelityCoarseRadius);
    }
    // The policy's focus at load: the player start. A managed entity that
    // the policy would not simulate fully is never given a live body at
    // all — a large world does not wake everything up just to put most of
    // it back to sleep.
    FidelityPolicyContext loadContext;
    for (const SceneObject& o : scene.Objects()) {
        if (o.playerStart) loadContext.focus = o.transform.position;
    }

    auto restoredPolicy=runtimeSnapshot?std::move(m_policy):std::unique_ptr<FidelityPolicy>{};
    if (!AppendSceneObjects(scene, true, loadContext, outError)) { Destroy(); return false; }
    if(runtimeSnapshot)m_policy=std::move(restoredPolicy);
    SynchronizeJoints();
    for(const auto& o:scene.Objects())if((runtimeSnapshot||m_hasScripts||o.animation||o.ragdoll||o.characterMotor||o.deformable||!NavigationProperties(o).empty()||!LiquidProperties(o).empty())&&!FindEntity(o.id)){
        std::string unsupported;
        if(o.scripts.empty()&&!ValidateEntityDefinition(o,unsupported))continue;
        EntityRecord e;e.id=o.id;e.name=o.name;e.definition=o;e.authored=true;e.requiresFull=true;
        e.state=StateFromDefinition(o);e.slot=std::numeric_limits<std::size_t>::max();m_extraEntities.push_back(e);
    }
    m_composed=m_sceneControl&&m_sceneControl->ComposedProject();
    m_baselineFingerprint = legacyGameplay ? std::move(fingerprint)
        : SceneFingerprintSha256("Judas.ScriptedRuntime.1:" + fingerprint);
    return true;
}

bool RuntimeWorld::AppendSceneObjects(const Scene& scene, bool authored,
    const FidelityPolicyContext& loadContext, std::string& outError) {
    for(const auto& o:scene.Objects())if(o.deformable)m_deformableOwners.insert(o.id);
    for(const auto& o:scene.Objects())if(o.animation)m_animationOwners.insert(o.id);
    for(const auto& o:scene.Objects())if(o.joint){m_jointOwners.insert(o.id);m_jointParticipants.insert(o.joint->bodyA);if(o.joint->bodyB)m_jointParticipants.insert(o.joint->bodyB);}
    const auto fail = [&](const SceneObject& o, const std::string& what) {
        outError = "object " + std::to_string(o.id) + " \"" + o.name + "\": " + what;
        return false;
    };

    const bool particleWater=!m_fluidVolumes.empty()||std::any_of(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){return bool(o.fluidVolume);});
    for (const SceneObject& o : scene.Objects()) {
        if(!ValidateCollisionFluid(o,particleWater,outError))return fail(o,outError);
        if(o.ui&&o.ui->enabled){std::string uiError;if(!UI().Load(o.ui->asset,o.ui->name,o.id,uiError))return fail(o,uiError);}
        m_hasScripts|=!o.scripts.empty();
        if(o.deformable){if(o.body||o.animation||o.ragdoll||o.characterMotor||o.transform.scale!=glm::vec3(1))return fail(o,"deformable owns node motion; owner cannot have another motion producer or nonunit scale");std::string error;if(!ValidDeformableSettings(*o.deformable,error)||!ValidateVisualAssets(o,error))return fail(o,error);if(m_assets){m_assets->AddRef(o.deformable->asset);m_referencedAssets.push_back(o.deformable->asset);m_assets->RequestDeformable(o.deformable->asset);}}
        m_hasLiquid|=!LiquidProperties(o).empty();
        if(o.body&&m_assets){std::vector<std::string> ids;if(!o.body->collisionAsset.empty())ids.push_back(o.body->collisionAsset);for(auto& c:o.body->compoundBoxes)if(!c.assetId.empty())ids.push_back(c.assetId);for(auto& id:ids){m_assets->AddRef(id);m_referencedAssets.push_back(id);m_assets->RequestCollision(id,JobPriority::High);}}
        if((o.liquidBasin||o.liquidContainer)&&m_assets){std::vector<std::string> ids;if(o.liquidBasin)ids={o.liquidBasin->geometry,o.liquidBasin->asset};else ids={o.liquidContainer->geometry};for(auto id:ids){m_assets->AddRef(id);m_referencedAssets.push_back(id);m_assets->RequestLiquid(id);}}
        m_hasNavigation|=o.navigationSurface.has_value()||o.navigationAgent.has_value()||o.navigationObstacle.has_value()||o.navigationLink.has_value()||o.navigationModifier.has_value();
        m_scriptDefinitions[o.id]=o;
        if(!o.scripts.empty())m_scriptOwners.insert(o.id);
        if(o.characterMotor)m_characterOwners.insert(o.id);
        m_entityCategories[o.id]={o.tags,o.tags,o.renderLayer,{}};
        const glm::vec3 position = o.transform.position;
        const glm::quat rotation = glm::normalize(o.transform.rotation);

        // --- Body ---
        BodyHandle bodyHandle;
        std::size_t dynamicIndex = m_dynamicBodies.size();
        bool isDynamic = false;
        if (o.body) {
            const SceneBodyComponent& b = *o.body;
            if (b.motion == SceneBodyMotion::Static) {
                switch (b.shape) {
                    case SceneShape::Box:
                        bodyHandle = m_physics.CreateStaticBox(position, rotation, b.halfExtents, b.friction, b.restitution);
                        break;
                    case SceneShape::Sphere:
                        bodyHandle = m_physics.CreateStaticSphere(position, b.radius, b.friction, b.restitution);
                        break;
                    case SceneShape::Terrain: {
                        std::shared_ptr<const RadialTerrain> surface = CreateTerrainSurface(b.terrainSurface);
                        if (!surface) return fail(o, "unknown terrain surface '" + b.terrainSurface + "'");
                        bodyHandle = m_physics.CreateStaticTerrain(position, rotation, surface, b.friction, b.restitution);
                        Terrain terrain;
                        terrain.id = o.id;
                        terrain.handle = bodyHandle;
                        terrain.surface = surface;
                        terrain.identifier = b.terrainSurface;
                        terrain.position = position;
                        terrain.rotation = rotation;
                        if (o.render) {
                            terrain.color = o.render->color;
                            if (m_assets) terrain.mesh = m_assets->GetTerrainMesh(b.terrainSurface, *surface);
                        }
                        m_terrains.push_back(terrain);
                        break;
                    }
                    case SceneShape::Compound:
                    case SceneShape::ConvexHull:
                    case SceneShape::TriangleMesh:{
                        Shape shape;std::string error;if(!ResolveBodyCollision(b,m_assets,shape,error))return fail(o,error);
                        try{bodyHandle=m_physics.CreateShape(shape,{position,rotation},false,0,b.friction,b.restitution);}catch(const std::exception& e){return fail(o,e.what());}break;
                    }
                    case SceneShape::Mesh:
                        return fail(o, "a body cannot use the mesh shape");
                }
                if (b.shape != SceneShape::Terrain) {
                    StaticBody sb;
                    sb.id = o.id;
                    sb.handle = bodyHandle;
                    sb.shape = b.shape;
                    sb.position = position;
                    sb.rotation = rotation;
                    sb.halfExtents = b.halfExtents;
                    sb.radius = b.radius;
                    m_staticBodies.push_back(sb);
                }
            } else {
                if (b.shape == SceneShape::Terrain) return fail(o, "terrain bodies must be static");
                if (b.shape == SceneShape::Mesh) return fail(o, "a body cannot use the mesh shape");
                isDynamic = true;
                const EntityPhysicalState state = StateFromDefinition(o);
                SimulationFidelity fidelity = SimulationFidelity::Full;
                if (m_policy && b.managed && !EntityRequiresFull(o)) {
                    FidelityPolicyEntity view;
                    view.id = o.id;
                    view.current = SimulationFidelity::Dormant;  // nothing exists yet
                    view.position = state.position;
                    view.linearVelocity = state.linearVelocity;
                    fidelity = m_policy->Desired(view, loadContext);
                }
                std::string entityError;
                if (!AppendEntitySlot(o, /*authored=*/authored, state, fidelity, &entityError)) return fail(o, entityError);
                bodyHandle = m_dynamicBodies[dynamicIndex].Handle();
            }
        } else if (o.render && (o.render->shape == SceneShape::Compound || o.render->shape == SceneShape::Terrain)) {
            return fail(o, "compound/terrain rendering needs a body");
        }

        if(bodyHandle.IsValid()&&o.body){
            if(!o.body->physicalMaterial.empty()){std::string error;auto mat=m_assets?m_assets->RequirePhysicalMaterial(o.body->physicalMaterial,error):nullptr;if(!mat)return fail(o,"physical material: "+error);m_assets->AddRef(o.body->physicalMaterial);m_referencedAssets.push_back(o.body->physicalMaterial);m_physics.SetPhysicalMaterial(bodyHandle,o.body->physicalMaterial,o.body->physicalMaterialOverride?o.body->friction:mat->friction,o.body->physicalMaterialOverride?o.body->restitution:mat->restitution);}
            m_physics.SetCollisionFilter(bodyHandle,o.body->collisionLayer,o.body->collisionMask);
            m_physics.SetBodySensor(bodyHandle,o.body->sensor);m_physics.SetBodyEnabled(bodyHandle,o.body->enabled);
            m_physics.SetBodyTags(bodyHandle,o.tags);m_entityCategories[o.id].body=bodyHandle;
        }
        if(o.navigationSurface && o.navigationSurface->enabled){
            if(!m_assets || !m_assets->Assets()){return fail(o,"navigation surface needs project assets");}
            auto* asset=m_assets->Assets()->Find(o.navigationSurface->asset);
            if(!asset||asset->missing||asset->type!=AssetType::Navigation)return fail(o,"missing/wrong-type navigation asset");
            m_assets->AddRef(asset->id);m_referencedAssets.push_back(asset->id);m_assets->RequestNavigation(asset->id);
        }
        // --- Renderable without a dynamic body ---
        if (o.render && !isDynamic && o.render->shape != SceneShape::Terrain && !o.door && !o.lightSwitch) {
            StaticRenderable sr;
            sr.id = o.id;
            sr.render = *o.render;
            sr.position = position;
            sr.rotation = rotation;
            sr.scale = o.transform.scale;
            std::string assetError;
            if (!RequestVisualAssets(o, &assetError)) return fail(o, assetError);
            if (o.render->shape == SceneShape::Compound) return fail(o, "compound render needs a dynamic body");
            m_staticRenderables.push_back(sr);
        }

        // --- Gravity region ---
        if (o.gravity) {
            const SceneGravityComponent& g = *o.gravity;
            std::unique_ptr<GravityField> field;
            if (g.kind == SceneGravityKind::Radial) {
                field = std::make_unique<RadicalGravity>(position, g.magnitude);
            } else {
                const glm::vec3 acceleration = rotation * glm::vec3(0.0f, -g.magnitude, 0.0f);
                // Constructor selection must preserve the complete authored
                // vector. An angular tolerance would erase small rotations.
                if (acceleration == glm::vec3(0.0f, -kFaithfulMagnitude, 0.0f)) {
                    field = std::make_unique<FaithfulGravity>();
                } else {
                    field = std::make_unique<UniformGravity>(acceleration);
                }
            }
            std::unique_ptr<GravityVolume> volume;
            if (g.regionShape == SceneRegionShape::Sphere) {
                volume = std::make_unique<SphericalVolume>(position, g.regionRadius);
            } else {
                volume = std::make_unique<BoxVolume>(position, g.regionHalfExtents);
            }
            m_gravityMap.AddRegion(*field, *volume);
            m_gravityFields.push_back(std::move(field));
            m_gravityVolumes.push_back(std::move(volume));
            m_gravityRegions.push_back(GravityRegion{o.id, g, position, rotation});
        }

        if(o.particleEmitter){
            ParticleEmitter e;e.id=o.id;e.transform=o.transform;e.pool=VisualParticlePool(*o.particleEmitter);
            m_particleEmitters.push_back(std::move(e));
            if(m_assets&&!o.particleEmitter->textureAsset.empty()){
                m_assets->AddRef(o.particleEmitter->textureAsset);m_referencedAssets.push_back(o.particleEmitter->textureAsset);
                m_assets->RequestTexture(o.particleEmitter->textureAsset,JobPriority::High);
            }
        }
        if(o.audioZone){m_audioZones.push_back({o.id,o.transform,*o.audioZone});if(m_assets&&!o.audioZone->asset.empty()){m_assets->AddRef(o.audioZone->asset);m_referencedAssets.push_back(o.audioZone->asset);std::string error;m_assets->GetAudioEnvironment(o.audioZone->asset,error);}}
        if(o.audioEmitter){
            AudioEmitter emitter;emitter.id=o.id;emitter.transform=o.transform;emitter.settings=*o.audioEmitter;
            emitter.wantPlay=emitter.settings.playOnStart;
            if(!isDynamic)emitter.staticBody=bodyHandle;
            m_audioEmitters.push_back(emitter);
            if(m_assets&&!emitter.settings.asset.empty()){
                m_assets->AddRef(emitter.settings.asset);m_referencedAssets.push_back(emitter.settings.asset);
                if(emitter.settings.enabled&&emitter.settings.loading==AudioLoading::Buffered)m_assets->RequestAudio(emitter.settings.asset);
            }
        }
        if(o.audioListener&&o.audioListener->enabled){
            AudioListener listener;listener.id=o.id;listener.transform=o.transform;listener.settings=*o.audioListener;
            if(!isDynamic)listener.staticBody=bodyHandle;
            m_audioListener=listener;
        }
        if (o.renderCamera) {
            RenderCamera camera; camera.id = o.id; camera.transform = o.transform; camera.settings = *o.renderCamera;
            if (!isDynamic) camera.staticBody = bodyHandle;
            m_renderCameras.push_back(camera);
        }
        // --- Standalone light ---
        if (o.light) {
            StaticLight light;
            light.id = o.id;
            light.light = *o.light;
            light.position = position;
            light.direction = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            m_staticLights.push_back(light);
        }

        // --- Door / switch ---
        if (o.door) {
            if (!o.render) return fail(o, "door needs a render component");
            m_doors.emplace_back(m_physics, position, rotation, o.render->halfExtents, o.door->localHingeAxis,
                                 glm::radians(o.door->openAngleDegrees),
                                 glm::radians(o.door->angularSpeedDegreesPerSecond), o.render->color);
            const auto handle=m_doors.back().Handle();
            m_physics.SetCollisionFilter(handle,o.door->collisionLayer,o.door->collisionMask);
            m_physics.SetBodyTags(handle,o.tags);m_entityCategories[o.id].body=handle;
            m_doorIds.push_back(o.id);
        }
        if (o.lightSwitch) {
            if (!o.render) return fail(o, "light switch needs a render component");
            const SceneLightSwitchComponent& s = *o.lightSwitch;
            m_lightSwitches.emplace_back(position, rotation, o.render->halfExtents, s.localHingeAxis,
                                         glm::radians(s.toggleAngleDegrees),
                                         glm::radians(s.angularSpeedDegreesPerSecond), o.render->color,
                                         position + rotation * s.lampLocalOffset, s.lampColor, s.lampRange);
            m_lightSwitchIds.push_back(o.id);
        }

        // --- Vehicle ---
        if (o.vehicle) {
            if (!isDynamic || o.body->shape != SceneShape::Box) return fail(o, "vehicle needs a dynamic box body");
            if (m_vehicle) return fail(o, "M28 supports one vehicle per scene");
            Vehicle v;
            v.id = o.id;
            v.handle = bodyHandle;
            v.dynamicIndex = dynamicIndex;
            v.component = *o.vehicle;
            v.halfExtents = o.body->halfExtents;
            m_vehicle = v;
        }

        // --- Celestial ---
        if (o.celestial) {
            if (!o.body) return fail(o, "celestial needs a body");
            if (!isDynamic) {
                if (!(o.celestial->gravitationalParameter > 0.0f)) {
                    return fail(o, "a static celestial source needs a positive gravitational parameter");
                }
                m_pointMassSources.push_back({o.id, o.name, position, o.celestial->gravitationalParameter});
            }
        }

        // --- Atmosphere ---
        if (o.atmosphere) {
            if (m_atmosphere) return fail(o, "M28 supports one atmosphere per scene");
            if (!o.celestial || !(o.celestial->gravitationalParameter > 0.0f)) {
                return fail(o, "atmosphere needs a static celestial gravitational parameter");
            }
            const SceneAtmosphereComponent& a = *o.atmosphere;
            AtmosphereParameters parameters;
            parameters.referenceRadius = a.referenceRadius;
            parameters.topRadius = a.topRadius;
            parameters.gravitationalParameter = o.celestial->gravitationalParameter;
            parameters.polytropicExponent = a.polytropicExponent;
            parameters.referenceDensity = a.referenceDensity;
            parameters.oxidizerMassFraction = a.oxidizerMassFraction;
            parameters.referenceTemperatureKelvin = a.referenceTemperatureKelvin;
            const RadialTerrain* solid = nullptr;
            if (!m_terrains.empty() && m_terrains.back().id == o.id) {
                m_atmosphereTerrain = m_terrains.back().surface;
                solid = m_atmosphereTerrain.get();
            }
            m_atmosphere.emplace(Atmosphere{o.id, o.name, AtmosphereField(parameters, solid),
                                            ReferenceFrame{position, rotation, glm::vec3(0.0f), glm::vec3(0.0f)},
                                            o.celestial->gravitationalParameter});
        }

        // --- Combustible ---
        if (o.combustible) {
            if (!isDynamic) return fail(o, "combustible needs a dynamic body");
            Combustible c;
            c.id = o.id;
            c.name = o.name;
            c.handle = bodyHandle;
            c.dynamicIndex = dynamicIndex;
            c.sourceRadius = o.body->shape == SceneShape::Sphere ? o.body->radius : o.body->halfExtents.x;
            m_combustibles.push_back(c);
        }

        // --- Fluid volume ---
        if (o.fluidVolume) {
            FluidVolume fv;
            fv.id = o.id;
            fv.component = *o.fluidVolume;
            fv.position = position;
            fv.rotation = rotation;
            const float s = o.fluidVolume->spacing;
            fv.particleMass = m_fluidSettings.restDensity * s * s * s;
            m_fluidVolumes.push_back(fv);
        }

        // --- Player start ---
        if (o.playerStart) {
            m_physics.SetPlayerCollisionFilter(o.playerStart->collisionLayer,o.playerStart->collisionMask);
            if (m_playerStart) return fail(o, "more than one player-start");
            m_playerStart = PlayerStart{position, o.playerStart->yawDegrees, o.playerStart->view,
                o.playerStart->density, o.playerStart->fluidDrag, o.playerStart->swimAcceleration};
        }
    }

    for (const Combustible& c : m_combustibles) {
        if (!scene.Find(c.id)) continue;
        const SceneObject* o = scene.Find(c.id);
        const SceneCombustibleComponent& sc = *o->combustible;
        CombustibleMaterial fuel;
        fuel.heatCapacityJPerK = sc.heatCapacityJPerK;
        fuel.initialFuelMassKg = sc.initialFuelMassKg;
        fuel.ignitionTemperatureK = sc.ignitionTemperatureK;
        fuel.maximumFuelRateKgPerSecond = sc.maximumFuelRateKgPerSecond;
        fuel.radiativeAreaSquareMeters = sc.radiativeAreaSquareMeters;
        fuel.retainedCombustionHeatFraction = sc.retainedCombustionHeatFraction;
        float initialTemperature = 300.0f;
        if (m_atmosphere) {
            const AtmosphereSample gas = m_atmosphere->field.Sample(m_physics.GetTransform(c.handle).position,
                                                                    m_atmosphere->frame);
            if (gas.temperatureKelvin > 0.0f) initialTemperature = gas.temperatureKelvin;
        }
        m_combustion.AddBody(c.handle, fuel, initialTemperature);
    }

    RebuildCelestialParticipants();
    if (m_assets && !m_fluidVolumes.empty() && m_assets->GetRenderer() && !m_fluidMesh.IsValid()) {
        m_fluidMesh = m_assets->GetRenderer()->CreateMesh(MeshData{});
    }
    if (authored) PopulateFluid();
    return true;
}

void RuntimeWorld::RebuildCelestialParticipants() {
    // Pairwise Newtonian set: every LIVE dynamic celestial body, plus a
    // vehicle that samples local gravity (the classic scene's spacecraft).
    // Coarse celestial entities contribute through CoarseSimulation and
    // Simulation's coarse-to-live pass instead.
    m_celestialParticipants.clear();
    m_operatorThrusts.clear();
    bool anyCelestial = false;
    for (const EntityRecord& e : m_entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed || !e.definition.celestial) continue;
        anyCelestial = true;
        if (e.fidelity == SimulationFidelity::Full) {
            const BodyHandle handle = m_dynamicBodies[e.slot].Handle();
            m_celestialParticipants.push_back(handle);
            // Operator forces belong to the entity definition, but their
            // physics handle belongs to this live incarnation. Rebuild both
            // inventories after creation, destruction or fidelity transitions.
            if (e.definition.celestial->operatorThrustForce > 0.0f)
                m_operatorThrusts.push_back({handle, e.definition.celestial->operatorThrustForce});
        }
    }
    if (m_vehicle && m_vehicle->component.gravity == SceneVehicleGravity::Local && anyCelestial) {
        // A vehicle may also explicitly carry the celestial component. It is
        // still one physical participant, not a second copy of every pair.
        const auto present = std::find_if(m_celestialParticipants.begin(), m_celestialParticipants.end(),
            [&](BodyHandle handle) { return handle.id == m_vehicle->handle.id; });
        if (present == m_celestialParticipants.end()) m_celestialParticipants.push_back(m_vehicle->handle);
    }
    m_celestial = std::make_unique<CelestialGravity>(m_celestialParticipants);
}

std::vector<BodyHandle> RuntimeWorld::PickableBodies() const {
    std::vector<BodyHandle> handles;
    for (const EntityRecord& e : m_entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed || e.fidelity != SimulationFidelity::Full) continue;
        if (e.definition.body && e.definition.body->pickable) handles.push_back(m_dynamicBodies[e.slot].Handle());
    }
    return handles;
}

void RuntimeWorld::PopulateFluid() {
    if (!m_fluidVolumes.empty()) m_fluidCoupling = std::make_unique<ProductionFluidCoupling>();
    else m_fluidCoupling.reset();
    m_fluid->Clear();
    m_emittedParticles = 0;
    for (const FluidVolume& fv : m_fluidVolumes) {
        const SceneFluidVolumeComponent& c = fv.component;
        const float s = c.spacing;
        const float x0 = -0.5f * static_cast<float>(c.countX - 1);
        const float z0 = -0.5f * static_cast<float>(c.countZ - 1);
        for (int y = 0; y < c.countY; ++y) {
            for (int z = 0; z < c.countZ; ++z) {
                for (int x = 0; x < c.countX; ++x) {
                    const glm::vec3 local((x0 + static_cast<float>(x)) * s, static_cast<float>(y) * s,
                                          (z0 + static_cast<float>(z)) * s);
                    m_fluid->AddParticle(fv.position + fv.rotation * local, glm::vec3(0.0f), fv.particleMass);
                }
            }
        }
    }
}

bool RuntimeWorld::EmitFluidParticle() {
    for (const FluidVolume& fv : m_fluidVolumes) {
        const SceneFluidVolumeComponent& c = fv.component;
        if (!c.emitter) continue;
        if (static_cast<int>(m_fluid->Particles().size()) >= c.maxParticles) continue;
        const int column = static_cast<int>(m_emittedParticles % 9);
        const glm::vec3 spread(static_cast<float>(column % 3 - 1) * 0.3f, 0.0f,
                               static_cast<float>(column / 3 - 1) * 0.3f);
        m_fluid->AddParticle(fv.position + fv.rotation * (c.emitterLocalOffset + spread), glm::vec3(0.0f),
                             fv.particleMass);
        ++m_emittedParticles;
        return true;
    }
    return false;
}

void RuntimeWorld::RestoreAuthoredState() {
    if(m_navigation)m_navigation=std::make_unique<NavigationSystem>(m_navigation->Configuration());
    std::vector<EntityId> articulations;for(const auto& entry:m_ragdolls)articulations.push_back(entry.first);
    for(auto id:articulations){std::string error;LeaveRagdoll(id,0,error);}
    // Destroy callbacks may still inspect lazy runtime components. Retire
    // scripts before clearing them so callbacks cannot recreate reset state.
    m_scripts.reset();spawnStates.clear();
    m_ragdollReturns.clear();m_ragdollAutostarted.clear();ClearDeformables();ClearCharacters();m_animationInstances.clear();
    m_ui.reset();m_localization.reset();pointerCapture=false;m_touchEntityHistory.clear();m_physics.ClearTouchHistory();
    if (!m_built) return;
    for(const auto& o:ScriptObjects())if(o.ui&&o.ui->enabled){std::string error;UI().Load(o.ui->asset,o.ui->name,o.id,error);}
    for(auto& [id,info]:m_entityCategories){(void)id;info.tags=info.authoredTags;m_physics.SetBodyTags(info.body,info.tags);}
    // Every surviving entity returns to its definition's state at Full
    // fidelity (the policy re-decides on the next step). Destroyed entities
    // stay destroyed: destruction is permanent within a run.
    for (EntityRecord& e : m_entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed) continue;
        const EntityPhysicalState authored = StateFromDefinition(e.definition);
        e.state = authored;
        e.coarseMotion = CoarseMotion::Settled;
        e.forcedFidelity.reset();
        if (e.fidelity == SimulationFidelity::Full) {
            const BodyHandle handle = m_dynamicBodies[e.slot].Handle();
            m_physics.ResetBody(handle, authored.position, authored.rotation);
            m_physics.SetLinearVelocity(handle, authored.linearVelocity);
            m_physics.SetAngularVelocity(handle, authored.angularVelocity);
            m_dynamicBodies[e.slot].SetPoseFromState(authored.position, authored.rotation);
            m_dynamicBodies[e.slot].SnapPresentation();
        } else {
            std::string error;
            InstantiateEntityBody(e, authored, &error);
            e.fidelity = SimulationFidelity::Full;
            e.lifecycle = EntityLifecycle::Active;
            e.dormantSinceSeconds = -1.0;
            ++e.reconstructions;
        }
    }
    if(m_hasScripts||std::any_of(m_extraEntities.begin(),m_extraEntities.end(),[](const auto& e){return bool(e.definition.characterMotor);})){
        for(auto& e:m_extraEntities)if(e.authored&&e.lifecycle!=EntityLifecycle::Destroyed){
            SetEntityState(e.id,StateFromDefinition(e.definition));SetRuntimeTransform(e.id,e.definition.transform);
        }
        // Restore parent-local data independently of entity ordering. Physical
        // records hold the flattened authored baseline; no game rule is involved.
        for(auto& o:m_hierarchy.Objects())if(const auto* e=FindEntity(o.id))if(e->authored){
            auto local=e->definition.transform;
            if(o.parent){const auto* parent=RuntimeDefinition(o.parent);const auto* record=FindEntity(o.parent);
                if(parent){const auto p=record&&record->authored?record->definition.transform:parent->transform;
                    local.position=glm::inverse(p.rotation)*(local.position-p.position)/p.scale;
                    local.rotation=glm::inverse(p.rotation)*local.rotation;local.scale/=p.scale;}}
            o.transform=local;
        }
    }
    for(auto& [id,d]:m_scriptDefinitions)if(const auto* record=FindEntity(id))if(record->authored&&record->definition.body&&d.body){
        d.body->enabled=record->definition.body->enabled;
        m_physics.SetBodyEnabled(RuntimeBody(id),d.body->enabled);
    }
    ++m_entityVersion;
    RebuildCelestialParticipants();
    m_combustion.Reset();
    m_liquid=std::make_unique<LiquidSystem>();
    PopulateFluid();
}

std::string RuntimeWorld::NameOfBody(BodyHandle handle) const {
    for (std::size_t i = 0; i < m_dynamicBodies.size(); ++i) {
        if (m_dynamicBodies[i].IsLive() && m_dynamicBodies[i].Handle().id == handle.id) return m_dynamicVisuals[i].name;
    }
    return std::string();
}

// --- Milestone 29 -------------------------------------------------------

void RuntimeWorld::RefreshEntityIndex() const {
    const bool reset=m_entityIndexVersion==~0u||m_entityIndexMainSize>m_entities.size()||m_entityIndexExtraSize>m_extraEntities.size();
    if(reset){++m_metadataWork.indexRebuilds;m_entityIndex.clear();m_entityIndexMainSize=m_entityIndexExtraSize=0;}
    // Appends keep prior indices valid. Version changes for tags/motion do not
    // invalidate identity storage; removals and mutable access explicitly do.
    for(size_t i=m_entityIndexMainSize;i<m_entities.size();++i)m_entityIndex.emplace(m_entities[i].id,std::make_pair(false,i));
    for(size_t i=m_entityIndexExtraSize;i<m_extraEntities.size();++i)m_entityIndex.emplace(m_extraEntities[i].id,std::make_pair(true,i));
    m_entityIndexVersion=m_entityVersion;m_entityIndexMainSize=m_entities.size();m_entityIndexExtraSize=m_extraEntities.size();
}
const EntityRecord* RuntimeWorld::FindEntity(EntityId id) const {
    ++m_metadataWork.lookups;RefreshEntityIndex();auto it=m_entityIndex.find(id);if(it==m_entityIndex.end())return nullptr;
    return it->second.first?&m_extraEntities[it->second.second]:&m_entities[it->second.second];
}
EntityRecord* RuntimeWorld::FindEntity(EntityId id) {
    return const_cast<EntityRecord*>(static_cast<const RuntimeWorld&>(*this).FindEntity(id));
}

EntityId RuntimeWorld::EntityIdOfBody(BodyHandle handle) const {
    for(const auto& [id,c]:m_characters)if(c.motor.observationBody.id==handle.id&&RuntimeDefinition(id))return id;
    unsigned layer=0;CategoryMask mask=0;
    if (!handle.IsValid() || !m_physics.GetCollisionFilter(handle,layer,mask)) return kInvalidSceneObjectId;
    for (const EntityRecord& e : m_entities) {
        if (e.lifecycle != EntityLifecycle::Destroyed && m_dynamicBodies[e.slot].IsLive() &&
            m_dynamicBodies[e.slot].Handle().id == handle.id) {
            return e.id;
        }
    }
    for(const auto& entry:m_entityCategories){
        // Dynamic entries retain authored category data while unloaded. Their
        // cached handle is not a live incarnation; only the loop above may
        // resolve them. This fallback supplies ordinary static bodies.
        const auto* entity=FindEntity(entry.first);
        if(entity && entity->slot!=std::numeric_limits<std::size_t>::max())continue;
        if(entry.second.body.id==handle.id && RuntimeDefinition(entry.first))return entry.first;
    }
    return kInvalidSceneObjectId;
}

bool RuntimeWorld::GetEntityState(EntityId id, EntityPhysicalState& outState) const {
    const EntityRecord* e = FindEntity(id);
    if (!e || e->lifecycle == EntityLifecycle::Destroyed) return false;
    if (e->slot == std::numeric_limits<std::size_t>::max()) { outState=e->state; return true; }
    if (e->fidelity == SimulationFidelity::Full) {
        const BodyHandle handle = m_dynamicBodies[e->slot].Handle();
        const BodyTransform transform = m_physics.GetTransform(handle);
        outState.position = transform.position;
        outState.rotation = transform.rotation;
        outState.linearVelocity = m_physics.GetLinearVelocity(handle);
        outState.angularVelocity = m_physics.GetAngularVelocity(handle);
    } else {
        outState = e->state;
    }
    return true;
}

bool RuntimeWorld::SetEntityState(EntityId id, const EntityPhysicalState& state) {
    EntityRecord* e = FindEntity(id);
    if (!e || e->lifecycle == EntityLifecycle::Destroyed) return false;
    e->state = state;
    if(e->definition.characterMotor){auto t=RuntimeDefinition(id)->transform;t.position=state.position;t.rotation=state.rotation;
        SetRuntimeTransform(id,t);if(auto* motor=RuntimeCharacter(id))motor->velocity=state.linearVelocity;
    }
    if (e->slot == std::numeric_limits<std::size_t>::max()) {
        for(auto& r:m_staticRenderables)if(r.id==id){r.position=state.position;r.rotation=state.rotation;}
        for(auto& b:m_staticBodies)if(b.id==id){m_physics.ResetBody(b.handle,state.position,state.rotation);b.position=state.position;b.rotation=state.rotation;}
        for(auto& a:m_audioEmitters)if(a.id==id){a.transform.position=state.position;a.transform.rotation=state.rotation;}
        for(auto& c:m_renderCameras)if(c.id==id){c.transform.position=state.position;c.transform.rotation=state.rotation;}
        for(auto& l:m_staticLights)if(l.id==id){l.position=state.position;l.direction=state.rotation*glm::vec3(0,0,-1);}
        return true;
    }
    e->coarseMotion = glm::length(state.linearVelocity) < kSettledLinearSpeed &&
                              glm::length(state.angularVelocity) < kSettledAngularSpeed
                          ? CoarseMotion::Settled
                          : CoarseMotion::Inertial;
    if (e->fidelity == SimulationFidelity::Full) {
        const BodyHandle handle = m_dynamicBodies[e->slot].Handle();
        m_physics.ResetBody(handle, state.position, state.rotation);
        m_physics.SetLinearVelocity(handle, state.linearVelocity);
        m_physics.SetAngularVelocity(handle, state.angularVelocity);
    }
    m_dynamicBodies[e->slot].SetPoseFromState(state.position, state.rotation);
    m_dynamicBodies[e->slot].SnapPresentation();
    return true;
}

bool RuntimeWorld::SetEntityFidelity(EntityId id, SimulationFidelity fidelity, std::string* outError) {
    EntityRecord* e = FindEntity(id);
    if (!e) {
        if (outError) *outError = "unknown entity id " + std::to_string(id);
        return false;
    }
    if (e->lifecycle == EntityLifecycle::Destroyed) {
        if (outError) *outError = "entity " + std::to_string(id) + " is destroyed";
        return false;
    }
    if (fidelity != SimulationFidelity::Full && e->requiresFull) {
        if (outError) *outError = "entity " + std::to_string(id) + " (" + e->name + ") has no reduced representation";
        return false;
    }
    if (fidelity == e->fidelity) return true;

    if (e->fidelity == SimulationFidelity::Full) {
        // Leaving Full: capture the live state, then release the body.
        GetEntityState(id, e->state);
        e->coarseMotion = glm::length(e->state.linearVelocity) < kSettledLinearSpeed &&
                                  glm::length(e->state.angularVelocity) < kSettledAngularSpeed
                              ? CoarseMotion::Settled
                              : CoarseMotion::Inertial;
        ReleaseEntityBody(*e);
        m_dynamicBodies[e->slot].SetPoseFromState(e->state.position, e->state.rotation);
        m_dynamicBodies[e->slot].SnapPresentation();
    } else if (fidelity == SimulationFidelity::Full) {
        // Reconstruction from the retained state.
        if (!InstantiateEntityBody(*e, e->state, outError)) return false;
        ++e->reconstructions;
    }
    e->fidelity = fidelity;
    e->lifecycle = fidelity == SimulationFidelity::Dormant ? EntityLifecycle::Unloaded : EntityLifecycle::Active;
    e->dormantSinceSeconds = fidelity == SimulationFidelity::Dormant ? m_simulationTime : -1.0;
    ++m_transitionsThisStep;
    ++m_entityVersion;
    if (e->definition.celestial) RebuildCelestialParticipants();
    return true;
}

bool RuntimeWorld::ForceEntityFidelity(EntityId id, std::optional<SimulationFidelity> fidelity, std::string* outError) {
    EntityRecord* e = FindEntity(id);
    if (!e) {
        if (outError) *outError = "unknown entity id";
        return false;
    }
    e->forcedFidelity = fidelity;
    if (fidelity) return SetEntityFidelity(id, *fidelity, outError);
    return true;
}

bool RuntimeWorld::ValidateEntityDestruction(EntityId id, std::string& error) const {
    const EntityRecord* e = FindEntity(id);
    if (!e) {
        error = "unknown entity id " + std::to_string(id);
        return false;
    }
    if (e->lifecycle == EntityLifecycle::Destroyed) return true;
    const bool ownedComponent = e->slot == std::numeric_limits<std::size_t>::max() &&
        (e->definition.door || e->definition.lightSwitch || e->definition.gravity || e->definition.atmosphere ||
         e->definition.fluidVolume || e->definition.playerStart || e->definition.audioListener);
    if (e->definition.vehicle || e->definition.combustible || ownedComponent) {
        error = "entity " + std::to_string(id) + " (" + e->name + ") cannot be destroyed at runtime";
        return false;
    }
    return true;
}

bool RuntimeWorld::DestroyEntity(EntityId id, std::string* outError) {
    std::string error;
    if (!ValidateEntityDestruction(id, error)) { if (outError) *outError = error; return false; }
    for(auto it=spawnStates.begin();it!=spawnStates.end();)if(it->first.first==id)it=spawnStates.erase(it);else++it;
    RemoveDeformable(id);m_deformableOwners.erase(id);
    m_liquid->Remove(m_liquid->Handle(id));
    LeaveRagdoll(id,0,error);
    EntityRecord* e = FindEntity(id);
    if (e->lifecycle == EntityLifecycle::Destroyed) return true;
    const bool extra=e->slot==std::numeric_limits<std::size_t>::max();
    if(!extra)ReleaseEntityBody(*e);
    else {
        for(auto& b:m_staticBodies)if(b.id==id)m_physics.DestroyBody(b.handle);
        const auto eraseId=[id](auto& values){values.erase(std::remove_if(values.begin(),values.end(),[id](const auto& v){return v.id==id;}),values.end());};
        eraseId(m_staticBodies);eraseId(m_staticRenderables);eraseId(m_staticLights);
    }
    m_particleEmitters.erase(std::remove_if(m_particleEmitters.begin(),m_particleEmitters.end(),[&](const auto& e){return e.id==id;}),m_particleEmitters.end());
    ReleaseEntityAudio(id);
    if(m_ui)m_ui->RemoveOwner(id);
    e->lifecycle = EntityLifecycle::Destroyed;
    if (m_cameraRenderer) for (auto& camera : m_renderCameras) {
        if (camera.id == id) { m_cameraRenderer->DestroyRenderTarget(camera.target); camera.target = {}; }
    }
    m_staticLights.erase(std::remove_if(m_staticLights.begin(),m_staticLights.end(),[id](const auto& light){return light.id==id;}),m_staticLights.end());
    e->fidelity = SimulationFidelity::Dormant;
    if(!extra)m_dynamicVisuals[e->slot].hasRender = false;
    ++m_entityVersion;
    if (e->definition.celestial) RebuildCelestialParticipants();
    return true;
}

EntityId RuntimeWorld::AllocateRuntimeEntityId() {
    return m_nextRuntimeId++;
}

void RuntimeWorld::SetNextRuntimeEntityId(EntityId next) {
    m_nextRuntimeId = std::max(next, kRuntimeEntityIdBase);
    for (const EntityRecord& e : m_entities) {
        if (!e.authored && e.id >= m_nextRuntimeId) m_nextRuntimeId = e.id + 1;
    }
}

bool RuntimeWorld::ValidateEntityDefinition(const SceneObject& definition, std::string& error) {
    const auto singleLine = [](const std::string& value) {
        return value.find_first_of("\r\n") == std::string::npos && value.find('\0') == std::string::npos;
    };
    if (!singleLine(definition.name) || (definition.body && !singleLine(definition.body->terrainSurface)) ||
        (definition.render && (!singleLine(definition.render->meshAsset) || !singleLine(definition.render->textureAsset)))) {
        error = "runtime entity strings must be single-line and contain no NUL bytes";
        return false;
    }
    // Finite fields / enum validation follows the canonical authored schema.
    // Check the smaller subset runtime creation can actually instantiate below.
    Scene validation;
    if(definition.joint&&(!definition.joint->bodyA||definition.joint->bodyA==definition.joint->bodyB||!ValidJointSettings(definition.joint->settings))){error="invalid runtime joint settings";return false;}
    SceneObject copy = definition;
    // Cross-body references are checked at scene/batch/save preflight, not in
    // this one-object component validator.
    copy.joint.reset();
    copy.id = 1;
    validation.InsertObject(copy);
    std::string fingerprint;
    if (!ComputeSceneFingerprint(validation, fingerprint, error)) return false;
    if (definition.vehicle || definition.combustible || definition.atmosphere || definition.fluidVolume ||
        definition.audioListener || definition.playerStart || definition.door || definition.lightSwitch || definition.gravity ||
        (definition.body && (definition.body->shape==SceneShape::Terrain || definition.body->shape==SceneShape::Mesh))) {
        error="runtime creation supports prop bodies/render/light/audio-emitter/render-camera/celestial components; scene-global and gameplay ownership components remain scene-authored";
        return false;
    }
    std::string block;WriteSceneObjectBlock(copy,block);std::vector<std::string> lines;
    std::istringstream in(block);std::string line;while(std::getline(in,line))lines.push_back(line);
    size_t index=0;SceneObject parsed;if(!ParseSceneObjectBlock(lines,index,parsed,error))return false;
    if (!definition.body) return true;
    const auto positive = [](const glm::vec3& v) { return v.x > 0 && v.y > 0 && v.z > 0; };
    const auto& b = *definition.body;
    const float norm = glm::dot(definition.transform.rotation, definition.transform.rotation);
    if (!(b.mass > 0) || !std::isfinite(1.0f / b.mass) || !(norm > 0) || !std::isfinite(norm) ||
        b.friction < 0 || b.restitution < 0 || b.restitution > 1 ||
        (b.shape == SceneShape::Box && !positive(b.halfExtents)) ||
        (b.shape == SceneShape::Sphere && !(b.radius > 0)) ||
        (b.shape == SceneShape::Compound && b.compoundBoxes.empty())) {
        error = "invalid runtime body mass, geometry, material or rotation";
        return false;
    }
    for (const auto& box : b.compoundBoxes) {
        if (box.type==ShapeType::Box&&!positive(box.halfExtents)) { error = "compound half-extents must be positive"; return false; }
    }
    if (definition.render && (definition.render->shape == SceneShape::Terrain ||
        (definition.render->shape == SceneShape::Compound && b.shape != SceneShape::Compound))) {
        error = "render geometry is incompatible with the runtime body";
        return false;
    }
    return true;
}

bool RuntimeWorld::ValidateEntityCreation(const SceneObject& definition, std::string& error) const {
    if (!m_built) { error = "no world"; return false; }
    if(!ValidateCollisionFluid(definition,!m_fluidVolumes.empty(),error))return false;
    if(definition.ui&&definition.ui->enabled){
        const auto* db=m_assets?m_assets->Assets():nullptr;const auto* asset=db?db->Find(definition.ui->asset):nullptr;UIDocument doc;
        if(!asset||asset->missing||asset->type!=AssetType::UI){error="missing UI document asset";return false;}
        if(!LoadUIDocument(asset->path,doc,error)||!ValidateUIAssets(doc,*db,error))return false;
        if(m_ui&&m_ui->Find(definition.ui->name)){error="duplicate runtime UI document name";return false;}
    }
    Scene classified;auto classifiedDefinition=definition;
    if(!classifiedDefinition.id)classifiedDefinition.id=1;
    classified.InsertObject(classifiedDefinition);
    if(!ValidateSceneClassification(classified,m_categories,error))return false;
    if (!ValidateEntityDefinition(definition, error) || !ValidateVisualAssets(definition, error)) return false;
    if(definition.navigationSurface&&definition.navigationSurface->enabled){auto* asset=m_assets&&m_assets->Assets()?m_assets->Assets()->Find(definition.navigationSurface->asset):nullptr;if(!asset||asset->missing||asset->type!=AssetType::Navigation){error="missing/wrong-type navigation asset";return false;}}
    if((definition.navigationAgent&&!m_navigation->Configuration().profiles.count(definition.navigationAgent->profile))||(definition.navigationLink&&!m_navigation->Configuration().areas.names.count(definition.navigationLink->area))){error="unknown navigation profile/area";return false;}

    if (definition.render && definition.render->textureCamera) {
        const SceneObjectId reference = definition.render->textureCamera;
        const bool present = std::any_of(m_renderCameras.begin(), m_renderCameras.end(), [reference](const RenderCamera& camera) { return camera.id == reference; });
        // References identify authored camera definitions, not live GPU images.
        // Destroyed cameras resolve to the ordinary white fallback, including
        // when a validated delta destroys a camera before creating its consumer.
        if (!present || !definition.render->textureAsset.empty()) {
            error = "runtime texture references no authored camera or conflicts with an asset texture"; return false;
        }
    }
    const EntityId id = definition.id == kInvalidSceneObjectId ? m_nextRuntimeId : definition.id;
    if (id < kRuntimeEntityIdBase || id >= static_cast<EntityId>(std::numeric_limits<std::int64_t>::max())) {
        error = "runtime-created entity id is outside the allocatable runtime range";
        return false;
    }
    if (FindEntity(id)) { error = "entity id " + std::to_string(id) + " already exists"; return false; }
    return true;
}

EntityId RuntimeWorld::CreateEntity(const SceneObject& definitionIn, const EntityPhysicalState* state,
                                    std::string* outError) {
    std::string error;
    if (!ValidateEntityCreation(definitionIn, error)) { if (outError) *outError = error; return kInvalidSceneObjectId; }
    SceneObject definition = definitionIn;
    if (definition.id == kInvalidSceneObjectId) definition.id = m_nextRuntimeId;
    auto local=definition;
    if(!state&&definition.parent){
        const auto* parent=m_hierarchy.Find(definition.parent);
        SceneTransform parentPose;
        if(parent)parentPose=PresentedTransform(parent->id,parent->transform,1.0f);
        else {EntityPhysicalState p;if(!GetEntityState(definition.parent,p)){if(outError)*outError="unknown runtime parent";return 0;}parentPose.position=p.position;parentPose.rotation=p.rotation;}
        definition.transform.position=parentPose.position+parentPose.rotation*(parentPose.scale*local.transform.position);
        definition.transform.rotation=glm::normalize(parentPose.rotation*local.transform.rotation);definition.transform.scale*=parentPose.scale;
    }
    if(state){definition.transform.position=state->position;definition.transform.rotation=state->rotation;
        if(definition.body)definition.body->initialLinearVelocity=state->linearVelocity;}
    Scene batch;batch.Settings()=m_settings;batch.InsertObject(definition);
    FidelityPolicyContext context;
    if(!AppendSceneObjects(batch,false,context,error)){if(outError)*outError=error;return 0;}
    if(!definition.body||definition.body->motion==SceneBodyMotion::Static){
        EntityRecord e;e.id=definition.id;e.name=definition.name;e.definition=definition;e.authored=false;
        e.requiresFull=true;e.state=StateFromDefinition(definition);e.slot=std::numeric_limits<std::size_t>::max();
        m_extraEntities.push_back(e);
    }
    FindEntity(definition.id)->definition=local;
    m_hierarchy.InsertObject(local);
    if(state)SetEntityState(definition.id,*state);
    m_nextRuntimeId=std::max(m_nextRuntimeId,definition.id+1);
    return definition.id;
}

EntityId RuntimeWorld::SpawnPrefab(const AssetId& asset,const SceneTransform& placement,std::string& error,const PrefabSpawnOptions& options) {
    if(!m_assets||!m_assets->Assets()){error="no project asset database";return 0;}
    Scene source;if(!LoadPrefab(*m_assets->Assets(),asset,source,error))return 0;
    Scene instance;instance.SetNextId(m_nextRuntimeId);SceneObjectId root=0;
    if(!InstantiatePrefab(instance,source,asset,placement,root,error))return 0;
    auto* rootDefinition=instance.Find(root);if(!rootDefinition){error="prefab root missing";return 0;}
    if(options.scripts.size()>256){error="prefab initialization exceeds 256 script slots";return 0;}
    auto finite=[](glm::vec3 v){return std::isfinite(glm::dot(v,v));};if((options.velocity&&!finite(*options.velocity))||(options.angularVelocity&&!finite(*options.angularVelocity))){error="finite initial motion required";return 0;}
    if((options.velocity||options.angularVelocity)&&(!rootDefinition->body||rootDefinition->body->motion!=SceneBodyMotion::Dynamic)){error="initial motion requires a dynamic prefab root";return 0;}
    if(options.velocity)rootDefinition->body->initialLinearVelocity=*options.velocity;
    std::map<std::pair<EntityId,uint64_t>,std::string> states;std::set<std::pair<EntityId,uint64_t>> initialized;
    for(const auto& init:options.scripts){auto target=root;if(init.source){auto found=rootDefinition->prefabIds.find(init.source);if(found==rootDefinition->prefabIds.end()){error="unknown prefab source entity";return 0;}target=found->second;}auto* object=instance.Find(target);auto slot=std::find_if(object->scripts.begin(),object->scripts.end(),[&](const auto& v){return v.id==init.slot;});if(slot==object->scripts.end()||!slot->enabled||!initialized.emplace(target,init.slot).second){error="unknown/disabled/duplicate prefab script slot";return 0;}
        if(!init.properties.empty())slot->properties=init.properties;
        std::string schema;if(!ScriptSystem::Inspect(*m_assets->Assets(),slot->asset,schema,error))return 0;std::vector<ScriptProperty> fields;if(!ScriptSystem::ReadProperties(schema,slot->properties,fields,error))return 0;
        for(auto id:ScriptSystem::PropertyEntities(slot->properties))if(id&&!instance.Find(id)&&(!RuntimeDefinition(id)||!IsPublished(id))){error="initial properties contain stale/unpublished entity reference";return 0;}
        if(init.state.size()>65536||!ScriptSystem::ValidateJson(init.state,error,false))return 0;
        states[{target,init.slot}]=init.state;
    }
    Scene flat;if(!FlattenHierarchy(instance,flat,error))return 0;
    if(!ValidateSceneClassification(flat,m_categories,error))return 0;
    // Complete preflight before creating any member. The resolved hierarchy is
    // ordinary data; runtime state and saved creations never depend on a live source.
    for(auto& o:flat.Objects()){
        o.prefabAsset.clear();o.prefabRoot=o.prefabSource=0;o.prefabIds.clear();o.prefabOverrides.clear();
        if(!ValidateCollisionFluid(o,!m_fluidVolumes.empty(),error)||!ValidateEntityDefinition(o,error)||!ValidateVisualAssets(o,error))return 0;
    }
    for(const auto& o:instance.Objects())if(o.joint){m_jointParticipants.insert(o.joint->bodyA);if(o.joint->bodyB)m_jointParticipants.insert(o.joint->bodyB);}
    std::vector<EntityId> created;
    std::set<SceneObjectId> pending;
    for(const auto& o:instance.Objects())pending.insert(o.id);
    while(!pending.empty()){
        bool progress=false;
        for(const auto& original:instance.Objects())if(pending.count(original.id)&&(!original.parent||!pending.count(original.parent))){
            auto o=original;o.prefabAsset.clear();o.prefabRoot=o.prefabSource=0;o.prefabIds.clear();o.prefabOverrides.clear();
            if(o.render&&o.render->textureCamera&&pending.count(o.render->textureCamera))continue;
            auto id=CreateEntity(o,nullptr,&error);
            if(!id){for(auto prior:created)DestroyEntity(prior);return 0;}
            pending.erase(id);created.push_back(id);progress=true;
        }
        if(!progress){error="cyclic runtime camera/parent creation dependencies";for(auto prior:created)DestroyEntity(prior);return 0;}
    }
    if(options.angularVelocity){EntityPhysicalState state;if(GetEntityState(root,state)){state.angularVelocity=*options.angularVelocity;SetEntityState(root,state);}}
    spawnStates.insert(states.begin(),states.end());
    SynchronizeJoints();
    return root;
}

SceneTransform RuntimeWorld::PresentedTransform(SceneObjectId id,const SceneTransform& fallback,float alpha) const {
    if(const auto* d=RuntimeDefinition(id);d&&d->socket&&d->socket->enabled){
        const auto& socket=*d->socket;auto it=m_animationInstances.find(socket.target);
        if(it!=m_animationInstances.end()&&it->second.asset){const auto& s=it->second.asset->skeleton;int n=FindSkeletonJoint(s,socket.joint);
            if(n>=0&&size_t(n)<it->second.finalPose.local.size())if(const auto* target=RuntimeDefinition(socket.target)){
                auto root=PresentedTransform(socket.target,target->transform,alpha);auto model=glm::translate(glm::mat4(1),root.position)*glm::mat4_cast(root.rotation)*glm::scale(glm::mat4(1),root.scale);auto matrix=model*PoseGlobalMatrices(s,it->second.finalPose)[n];JointTransform joint;std::string error;
                if(DecomposeRigidPose(matrix,joint,error)){SceneTransform t;t.position=joint.translation+joint.rotation*(joint.scale*socket.offset.position);t.rotation=glm::normalize(joint.rotation*socket.offset.rotation);t.scale=joint.scale*socket.offset.scale;return t;}
            }
        }
    }
    auto character=m_characters.find(id);
    if(character!=m_characters.end()){
        auto t=fallback;const auto& c=character->second;
        t.position=glm::mix(c.previous.position,c.motor.position,alpha);
        t.rotation=glm::normalize(glm::slerp(c.previous.rotation,c.motor.orientation,alpha));return t;
    }

    if(const auto* e=FindEntity(id))if(e->slot!=std::numeric_limits<std::size_t>::max()){
        const auto& body=m_dynamicBodies[e->slot];auto t=fallback;
        t.position=body.GetPresentedPosition(alpha);t.rotation=body.GetPresentedOrientation(alpha);return t;
    }
    // Physical children are independent ordinary bodies; hierarchy is not a
    // hidden constraint. Body-free visual/audio/light/camera children follow.
    for(const auto& b:m_staticBodies)if(b.id==id){auto t=fallback;const auto pose=m_physics.GetTransform(b.handle);t.position=pose.position;t.rotation=pose.rotation;return t;}
    const auto* local=m_hierarchy.Find(id);
    if(local&&local->parent){
        const auto* parent=m_hierarchy.Find(local->parent);
        if(parent){auto p=PresentedTransform(parent->id,parent->transform,alpha);auto t=local->transform;
            t.position=p.position+p.rotation*(p.scale*t.position);t.rotation=glm::normalize(p.rotation*t.rotation);t.scale=p.scale*t.scale;return t;}
    }
    if(const auto* e=FindEntity(id)){auto t=fallback;t.position=e->state.position;t.rotation=e->state.rotation;return t;}
    return local?local->transform:fallback;
}

bool RuntimeWorld::DestroyHierarchy(EntityId root,std::string& error) {
    std::vector<EntityId> ids{root};
    for(size_t i=0;i<ids.size();++i){
        for(const auto& o:ScriptObjects())if(o.parent==ids[i]&&std::find(ids.begin(),ids.end(),o.id)==ids.end())ids.push_back(o.id);
    }
    for(auto id:ids)if(!ValidateEntityDestruction(id,error))return false;
    for(auto it=ids.rbegin();it!=ids.rend();++it)if(!DestroyEntity(*it,&error))return false;
    return true;
}

void RuntimeWorld::SetFidelityPolicy(std::unique_ptr<FidelityPolicy> policy) {
    m_policy = std::move(policy);
}

void RuntimeWorld::EvaluateFidelityPolicy(const FidelityPolicyContext& context, const std::vector<EntityId>& pinned) {
    m_transitionsThisStep = 0;
    if (!m_policy) return;
    for (EntityRecord& e : m_entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed || !e.managed || e.requiresFull || e.forcedFidelity) continue;
        if (std::find(pinned.begin(), pinned.end(), e.id) != pinned.end()) {
            if (e.fidelity != SimulationFidelity::Full) SetEntityFidelity(e.id, SimulationFidelity::Full);
            continue;
        }
        FidelityPolicyEntity view;
        view.id = e.id;
        view.current = e.fidelity;
        EntityPhysicalState state;
        GetEntityState(e.id, state);
        view.position = state.position;
        view.linearVelocity = state.linearVelocity;
        const SimulationFidelity desired = m_policy->Desired(view, context);
        if (desired != e.fidelity) SetEntityFidelity(e.id, desired);
    }
}

RuntimeWorld::LifecycleCounts RuntimeWorld::CountLifecycle() const {
    LifecycleCounts counts;
    for (const EntityRecord& e : m_entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed) ++counts.destroyed;
        else if (e.fidelity == SimulationFidelity::Full) ++counts.full;
        else if (e.fidelity == SimulationFidelity::Coarse) ++counts.coarse;
        else ++counts.dormant;
    }
    counts.physicsBodies = m_physics.AliveBodyCount();
    counts.dynamicPhysicsBodies = m_physics.DynamicBodyCount();
    counts.transitionsThisStep = m_transitionsThisStep;
    return counts;
}

Door* RuntimeWorld::FindDoor(SceneObjectId id) {
    for (std::size_t i = 0; i < m_doorIds.size(); ++i) {
        if (m_doorIds[i] == id) return &m_doors[i];
    }
    return nullptr;
}

LightSwitch* RuntimeWorld::FindLightSwitch(SceneObjectId id) {
    for (std::size_t i = 0; i < m_lightSwitchIds.size(); ++i) {
        if (m_lightSwitchIds[i] == id) return &m_lightSwitches[i];
    }
    return nullptr;
}

void RuntimeWorld::Destroy() {
    m_regionPending.clear();m_regionAssets.clear();m_composed=false;
 JUDAS_PROFILE_SCOPE("World destroy"); PerformanceProfiler::Get().Boundary("World destroy");
    m_scripts.reset();spawnStates.clear();
    m_ragdolls.clear();m_ragdollReturns.clear();m_ragdollAutostarted.clear();
    ClearDeformables();m_deformableOwners.clear();
    ClearCharacters();
    m_animationInstances.clear();m_animationOwners.clear();
    m_jointOwners.clear();m_jointParticipants.clear();m_runtimeJoints.clear();
    m_liquid=std::make_unique<LiquidSystem>();m_hasLiquid=false;m_navigation.reset();m_ui.reset();m_localization.reset();pointerCapture=false;m_scriptDefinitions.clear();m_scriptOwners.clear();m_characterOwners.clear();m_touchEntityHistory.clear();m_hasScripts=false;m_hasNavigation=false;
    EndAudio();
    m_particleEmitters.clear();
    m_audioEmitters.clear();m_audioZones.clear();m_audioIdentities.clear();m_audioListener.reset();m_audioSystem=nullptr;
    if (!m_built) return;
    if (m_cameraRenderer) for (const auto& camera : m_renderCameras) m_cameraRenderer->DestroyRenderTarget(camera.target);
    m_renderCameras.clear(); m_cameraRenderer = nullptr; m_cameraFrame = 0;
    for (Door& door : m_doors) door.Destroy(m_physics);
    for (const DynamicBody& body : m_dynamicBodies) {
        if (body.IsLive()) m_physics.DestroyBody(body.Handle());
    }
    for (const StaticBody& body : m_staticBodies) m_physics.DestroyBody(body.handle);
    for (const Terrain& terrain : m_terrains) m_physics.DestroyBody(terrain.handle);
    if (m_assets && m_assets->GetRenderer() && m_fluidMesh.IsValid()) {
        m_assets->GetRenderer()->DestroyMesh(m_fluidMesh);
    }
    m_fluidMesh = MeshHandle{};
    m_fluidSurfaceRevision = ~std::uint64_t{0};
    m_physics.Shutdown();

    m_doors.clear();
    m_doorIds.clear();
    m_lightSwitches.clear();
    m_lightSwitchIds.clear();
    m_dynamicBodies.clear();
    m_dynamicVisuals.clear();
    m_entities.clear();
    m_entityCategories.clear();
    m_policy.reset();
    m_nextRuntimeId = kRuntimeEntityIdBase;
    m_entityVersion = 0;m_entityIndex.clear();m_entityIndexVersion=~0u;
    m_transitionsThisStep = 0;
    m_simulationTime = 0.0;
    m_staticBodies.clear();
    m_terrains.clear();
    m_staticRenderables.clear();
    m_staticLights.clear();
    m_gravityMap = GravityContextMap();
    m_gravityFields.clear();
    m_gravityVolumes.clear();
    m_gravityRegions.clear();
    m_vehicle.reset();
    m_celestial.reset();
    m_celestialParticipants.clear();
    m_pointMassSources.clear();
    m_operatorThrusts.clear();
    m_atmosphere.reset();
    m_atmosphereTerrain.reset();
    m_combustion.Clear();
    m_combustibles.clear();
    m_fluidCoupling.reset();
    m_fluid.reset();
    m_fluidSettings = FluidSettings{};
    m_fluidVolumes.clear();
    m_emittedParticles = 0;
    m_playerStart.reset();
    m_pickableBodies.clear();
    if (m_assets) {
        for (const AssetId& id : m_referencedAssets) m_assets->ReleaseRef(id);
    }
    m_referencedAssets.clear();
    m_assets = nullptr;
    m_hierarchy.Clear();
    m_extraEntities.clear();
    m_baselineFingerprint.clear();
    m_built = false;
}

TextureHandle RuntimeWorld::CameraTexture(SceneObjectId id) const {
    if (!id || !m_cameraRenderer) return {};
    if (const auto* entity = FindEntity(id)) if (entity->lifecycle == EntityLifecycle::Destroyed) return {};
    for (const auto& c : m_renderCameras) if (c.id == id) return m_cameraRenderer->RenderTargetTexture(c.target);
    return {};
}


void RuntimeWorld::UpdateVisualParticles(float dt){
 JUDAS_PROFILE_SCOPE("Visual particle simulation");
    for(auto& e:m_particleEmitters){
        if(!IsPublished(e.id))continue;
        if(const auto* entity=FindEntity(e.id))if(entity->lifecycle==EntityLifecycle::Destroyed)continue;
        const auto t=PresentedTransform(e.id,e.transform,1);
        e.pool.Update(dt,t.position,t.rotation,t.scale,Gravity());
    }
}
bool RuntimeWorld::EmitParticleBurst(SceneObjectId id,unsigned count){
    for(auto& e:m_particleEmitters)if(e.id==id){const auto t=PresentedTransform(e.id,e.transform,1);e.pool.Burst(count,t.position,t.rotation,t.scale);return true;}
    return false;
}

CategoryMask RuntimeWorld::TagsOf(EntityId id)const{
    const auto it=m_entityCategories.find(id);const auto* entity=FindEntity(id);
    return it==m_entityCategories.end()||(entity&&entity->lifecycle==EntityLifecycle::Destroyed)?0:it->second.tags;
}
unsigned RuntimeWorld::RenderLayerOf(EntityId id)const{const auto it=m_entityCategories.find(id);return it==m_entityCategories.end()?0:it->second.renderLayer;}
bool RuntimeWorld::AddTag(EntityId id,unsigned tag){
    auto it=m_entityCategories.find(id);const auto* entity=FindEntity(id);
    if(it==m_entityCategories.end()||!m_categories.tags.names.count(tag)||(entity&&entity->lifecycle==EntityLifecycle::Destroyed))return false;
    it->second.tags|=CategoryBit(tag);m_physics.SetBodyTags(it->second.body,it->second.tags);return true;
}
bool RuntimeWorld::RemoveTag(EntityId id,unsigned tag){
    auto it=m_entityCategories.find(id);if(it==m_entityCategories.end()||tag>=64)return false;
    it->second.tags&=~CategoryBit(tag);m_physics.SetBodyTags(it->second.body,it->second.tags);return true;
}
std::vector<EntityId> RuntimeWorld::QueryEntities(CategoryMask required,CategoryMask excluded,const std::vector<EntityId>* candidates)const{
    std::vector<EntityId> result;
    for(const auto& [id,info]:m_entityCategories){
        if(candidates&&std::find(candidates->begin(),candidates->end(),id)==candidates->end())continue;
        if(!IsPublished(id))continue;
        const auto* e=FindEntity(id);if(e&&e->lifecycle==EntityLifecycle::Destroyed)continue;
        if((info.tags&required)==required&&!(info.tags&excluded))result.push_back(id);
    }
    return result;
}

bool RuntimeWorld::SetNavigationAgentSettings(EntityId id,const NavigationAgentSettings& settings){auto it=m_scriptDefinitions.find(id);if(it==m_scriptDefinitions.end()||!it->second.navigationAgent)return false;SceneObject check;check.navigationAgent=settings;std::string error;if(!ValidateNavigationComponents(check,error)||!m_navigation->Configuration().profiles.count(settings.profile))return false;it->second.navigationAgent=settings;if(auto agent=m_navigation->Agent(id))agent->elapsed=1e10f;return true;}

bool RuntimeWorld::SetNavigationEnabled(EntityId id,const std::string& kind,bool enabled){auto it=m_scriptDefinitions.find(id);if(it==m_scriptDefinitions.end())return false;auto& o=it->second;if(kind=="agent"&&o.navigationAgent)o.navigationAgent->enabled=enabled;else if(kind=="obstacle"&&o.navigationObstacle)o.navigationObstacle->enabled=enabled;else if(kind=="link"&&o.navigationLink)o.navigationLink->enabled=enabled;else return false;return true;}

bool RuntimeWorld::SetMaterialSlot(EntityId id,unsigned slot,const MaterialSlot& value){auto* entity=FindEntity(id);if(!entity||entity->lifecycle==EntityLifecycle::Destroyed||!entity->definition.render||slot>=64)return false;std::string error;if(!ValidateMaterial(ApplyMaterialOverride(MaterialDefinition{},value.overrides),error))return false;if(!value.asset.empty()){auto* record=m_assets&&m_assets->Assets()?m_assets->Assets()->Find(value.asset):nullptr;if(!record||record->missing||record->type!=AssetType::Material)return false;if(std::find(m_referencedAssets.begin(),m_referencedAssets.end(),value.asset)==m_referencedAssets.end()){m_assets->AddRef(value.asset);m_referencedAssets.push_back(value.asset);}m_assets->RequestMaterial(value.asset);}auto& slots=entity->definition.render->materials;if(slots.size()<=slot)slots.resize(slot+1);slots[slot]=value;if(auto it=m_scriptDefinitions.find(id);it!=m_scriptDefinitions.end()&&it->second.render)it->second.render->materials=slots;for(auto& render:m_staticRenderables)if(render.id==id)render.render.materials=slots;for(auto& render:m_dynamicVisuals)if(render.id==id)render.render.materials=slots;return true;}
bool RuntimeWorld::SetAppearance(const SceneSettings& s){for(int k=0;k<3;++k)if(!std::isfinite(s.backgroundColor[k])||s.backgroundColor[k]<0||s.backgroundColor[k]>10000)return false;if(!std::isfinite(s.exposure)||s.exposure<=0||s.exposure>10000||!std::isfinite(s.environmentIntensity)||s.environmentIntensity<0||s.environmentIntensity>10000||!std::isfinite(s.environmentRotation.w)||!std::isfinite(s.environmentRotation.x)||!std::isfinite(s.environmentRotation.y)||!std::isfinite(s.environmentRotation.z)||glm::length(s.environmentRotation)<1e-6f)return false;if(!s.environmentAsset.empty()){auto* r=m_assets&&m_assets->Assets()?m_assets->Assets()->Find(s.environmentAsset):nullptr;if(!r||r->missing||r->type!=AssetType::Environment)return false;if(std::find(m_referencedAssets.begin(),m_referencedAssets.end(),s.environmentAsset)==m_referencedAssets.end()){m_assets->AddRef(s.environmentAsset);m_referencedAssets.push_back(s.environmentAsset);}m_assets->RequestEnvironment(s.environmentAsset);}m_settings.backgroundColor=s.backgroundColor;m_settings.linearRendering=s.linearRendering;m_settings.exposure=s.exposure;m_settings.environmentAsset=s.environmentAsset;m_settings.environmentIntensity=s.environmentIntensity;m_settings.environmentRotation=glm::normalize(s.environmentRotation);m_settings.environmentBackground=s.environmentBackground;return true;}

bool RuntimeWorld::SetModelPartVisible(EntityId id,const std::string& key,bool visible){
 auto* e=FindEntity(id);if(!e||e->lifecycle==EntityLifecycle::Destroyed||!e->definition.render||!m_assets)return false;
 auto* parts=m_assets->TryGetModelParts(e->definition.render->meshAsset);if(!parts||std::none_of(parts->begin(),parts->end(),[&](auto& p){return p.part==key;}))return false;
 auto& hidden=e->definition.render->hiddenParts;hidden.erase(std::remove(hidden.begin(),hidden.end(),key),hidden.end());if(!visible)hidden.push_back(key);
 if(auto it=m_scriptDefinitions.find(id);it!=m_scriptDefinitions.end()&&it->second.render)it->second.render->hiddenParts=hidden;
 for(auto& r:m_staticRenderables)if(r.id==id)r.render.hiddenParts=hidden;
 for(auto& r:m_dynamicVisuals)if(r.id==id)r.render.hiddenParts=hidden;
 return true;
}

std::string RuntimeWorld::TakeSpawnState(EntityId id,uint64_t slot){auto it=spawnStates.find({id,slot});if(it==spawnStates.end())return "null";auto state=std::move(it->second);spawnStates.erase(it);return state;}
