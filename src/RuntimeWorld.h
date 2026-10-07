#include "NavigationSystem.h"
#include "LiquidSystem.h"
#pragma once

#include <memory>
#include <set>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AssetDatabase.h"
#include "AtmosphereField.h"
#include "CelestialGravity.h"
#include "CombustionWorld.h"
#include "Door.h"
#include "DynamicBody.h"
#include "EntityLifecycle.h"
#include "FidelityPolicy.h"
#include "FluidWorld.h"
#include "GravityContextMap.h"
#include "GravityField.h"
#include "GravityVolume.h"
#include "LightSwitch.h"
#include "PhysicsWorld.h"
#include "ReferenceFrame.h"
#include "Renderer.h"
#include "Scene.h"
#include "AudioSystem.h"
#include "ScriptSystem.h"
#include "SkeletalAnimation.h"
#include "RuntimeUI.h"

class SceneSession;
class RadialTerrain;
class ResourceManager;
struct PhysicalMaterial;
class ProductionFluidCoupling;

// Milestone 28: the RUNTIME instance of a Scene.
//
// Build() walks the authored objects once and creates the engine-side
// state each component asks for — rigid bodies in PhysicsWorld, gravity
// fields and their regions in a GravityContextMap, fluid particles,
// the atmosphere field, combustion state, doors and switches, GPU meshes
// through the ResourceManager (M30) — and remembers, per object, only what the
// simulation and presentation need to reach that state again (handles,
// indices, the authored pose a static body was placed at). Everything here
// is transient: RestoreAuthoredState() puts every body back where the
// Scene said, and Destroy() (or destruction) releases it all. The Scene
// itself is read once during Build and never written — this is the
// authored/runtime boundary the editor's Play/Stop relies on.
//
// This class owns no gameplay: no player, no pilot control, no pick-up,
// no input. Those live in GameSession (src/GameSession.h) and read this
// world through the accessors below. It also decides no step ordering —
// see src/Simulation.h — and issues no draw calls — see
// src/WorldPresentation.h. It is the inventory of a running scene, not
// the loop that runs it.
class SaveArchive;
class RuntimeWorld {
public:
    // Project execution policy, retained across Build/Destroy; not simulation state.
    bool legacyGameplay = true;
    bool pointerCapture = false; // Script intent; modal UI temporarily overrides capture.
    void SetSceneControl(std::shared_ptr<SceneSession> control) { m_sceneControl=std::move(control); }
    std::shared_ptr<SceneSession> SceneControl() const { return m_sceneControl; }
    struct StaticBody {
        SceneObjectId id = kInvalidSceneObjectId;
        BodyHandle handle;
        SceneShape shape = SceneShape::Box;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 halfExtents{0.5f};
        float radius = 0.5f;
    };
    struct Terrain {
        SceneObjectId id = kInvalidSceneObjectId;
        BodyHandle handle;
        std::shared_ptr<const RadialTerrain> surface;
        std::string identifier;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        MeshHandle mesh;
        glm::vec3 color{0.5f};
    };
    struct ParticleEmitter {
        SceneObjectId id=0;SceneTransform transform;VisualParticlePool pool;
    };
    void UpdateVisualParticles(float dt);
    bool EmitParticleBurst(SceneObjectId id,unsigned count);
    std::vector<ParticleEmitter>& VisualEmitters()const{return m_particleEmitters;}
    struct AudioEmitter {
        SceneObjectId id=0;SceneTransform transform;BodyHandle staticBody;
        SceneAudioEmitterComponent settings;AudioVoiceHandle voice;
        bool wantPlay=false;std::string error;
        bool motionValid=false;glm::vec3 lastPosition{0},lastVelocity{0};
        std::optional<glm::vec3> explicitVelocity;
        float obstruction=0;

    };
    struct AudioListener {
        SceneObjectId id=0;SceneTransform transform;BodyHandle staticBody;
        SceneAudioListenerComponent settings;
    };
    struct AudioZone {SceneObjectId id=0;SceneTransform transform;SceneAudioZoneComponent settings;};
    ProjectAudioSettings audioGroups;
    std::vector<AudioZone>& AudioZones(){return m_audioZones;}
    void BeginAudio();void EndAudio();
    bool PlayAudioOneShot(SceneObjectId,std::string& error);
    void ReleaseEntityAudio(SceneObjectId);
    bool SeekAudio(SceneObjectId,double seconds);
    bool SetAudioSettings(SceneObjectId,const AudioSettings&);
    bool SetAudioVelocity(SceneObjectId,std::optional<glm::vec3>);
    void ResetAudioMotion();
    std::string AudioStableIdentity(EntityId id)const{auto it=m_audioIdentities.find(id);return it==m_audioIdentities.end()?"root:"+std::to_string(id):it->second;}
    std::uint64_t AudioQueryCount()const{return m_audioQueries;}

    void UpdateAudio(const glm::mat4& activeView,float alpha=1.0f,float frameSeconds=1.f/60);
    bool PlayAudio(SceneObjectId id);bool StopAudio(SceneObjectId id);
    bool PauseAudio(SceneObjectId id);bool ResumeAudio(SceneObjectId id);
    bool SetAudioEnabled(SceneObjectId id,bool enabled);
    const std::vector<AudioEmitter>& AudioEmitters() const {return m_audioEmitters;}
    // M33: authored camera pose/configuration plus transient presentation resources.
    struct RenderCamera {
        SceneObjectId id = 0;
        SceneTransform transform;
        BodyHandle staticBody;
        SceneRenderCameraComponent settings;
        RenderTargetHandle target;
        std::string error;
        bool allocationAttempted = false,attemptedLinear=false;
        int attemptedWidth = 0, attemptedHeight = 0;
        std::uint64_t updates = 0;
    };
    // Presentation-only resources: lazy, discarded on Stop/world destruction.
    std::vector<RenderCamera>& PresentationCameras() const { return m_renderCameras; }
    std::uint64_t NextCameraFrame() const { return m_cameraFrame++; }
    void BindCameraRenderer(Renderer& renderer) const { m_cameraRenderer = &renderer; }
    TextureHandle CameraTexture(SceneObjectId id) const;

    // One authored renderable (mesh, box or sphere) that has no dynamic
    // body — drawn at its authored pose every frame.
    // Mesh/texture handles are NOT stored here (Milestone 31): presentation
    // resolves the render component's asset ids through Resources() every
    // frame, so an asset still loading draws as a placeholder and switches
    // to the real resource the frame it becomes Ready, and an evicted one
    // never leaves a stale handle behind.
    struct StaticRenderable {
        SceneObjectId id = kInvalidSceneObjectId;
        SceneRenderComponent render;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
    };
    // Presentation data for a dynamic body, parallel to DynamicBodies().
    struct DynamicVisual {
        SceneObjectId id = kInvalidSceneObjectId;
        std::string name;
        SceneRenderComponent render;
        bool hasRender = false;
        std::vector<CompoundBox> compoundBoxes;  // Compound render only
        glm::vec3 scale{1.0f};
        glm::vec3 initialLinearVelocity{0.0f};
        bool pickable = false;
    };
    struct StaticLight {
        SceneObjectId id = kInvalidSceneObjectId;
        SceneLightComponent light;
        glm::vec3 position{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
    };
    struct Vehicle {
        SceneObjectId id = kInvalidSceneObjectId;
        BodyHandle handle;
        std::size_t dynamicIndex = 0;
        SceneVehicleComponent component;
        glm::vec3 halfExtents{1.0f};
    };
    // Static Newtonian source acting on Celestial-gravity vehicles.
    struct PointMassSource {
        SceneObjectId id = kInvalidSceneObjectId;
        std::string name;
        glm::vec3 position{0.0f};
        float gravitationalParameter = 0.0f;
    };
    struct OperatorThrust {
        BodyHandle handle;
        float force = 0.0f;
    };
    struct Atmosphere {
        SceneObjectId id = kInvalidSceneObjectId;
        std::string name;
        AtmosphereField field;
        ReferenceFrame frame;
        float gravitationalParameter = 0.0f;
    };
    struct Combustible {
        SceneObjectId id = kInvalidSceneObjectId;
        std::string name;
        BodyHandle handle;
        std::size_t dynamicIndex = 0;
        float sourceRadius = 0.25f;
    };
    struct FluidVolume {
        SceneObjectId id = kInvalidSceneObjectId;
        SceneFluidVolumeComponent component;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        float particleMass = 0.0f;
    };
    struct PlayerStart {
        glm::vec3 position{0.0f};
        float yawDegrees = 0.0f;
        ScenePlayerView view = ScenePlayerView::ThirdPerson;
        float density = 950.0f, fluidDrag = 2.0f, swimAcceleration = 4.0f;
    };
    // Milestone 30: the authored gravity regions as instantiated, kept only
    // so the debug view can draw them (GravityContextMap holds abstract
    // volumes with no geometry accessors, by design).
    struct GravityRegion {
        SceneObjectId id = kInvalidSceneObjectId;
        SceneGravityComponent component;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };

    RuntimeWorld();
    ~RuntimeWorld();
    RuntimeWorld(const RuntimeWorld&) = delete;
    RuntimeWorld& operator=(const RuntimeWorld&) = delete;

    // Instantiates `scene`. `resources` may be null for headless use (no GPU
    // resources are created). Mesh/texture assets are REQUESTED (and
    // reference-counted) here, not waited for: Build fails only when an id
    // is unknown to the asset database or of the wrong type; a file that is
    // missing or undecodable shows as a placeholder and reads Failed in the
    // resource manager. On failure nothing is left allocated and `outError`
    // says which object/component could not be realised.
    bool Build(const Scene& scene, ResourceManager* resources, std::string& outError, const ProjectClassification* categories=nullptr,const ProjectNavigation* navigation=nullptr,bool runtimeSnapshot=false);
    ResourceManager* Resources() const { return m_assets; }
    void Destroy();
    bool IsBuilt() const { return m_built; }

    // Puts every dynamic body back at its authored pose with its authored
    // initial velocity, re-creates the authored fluid particles, resets
    // thermal state and the emitter count. Static bodies never moved.
    void RestoreAuthoredState();

    PhysicsWorld& Physics() { return m_physics; }
    const PhysicsWorld& Physics() const { return m_physics; }
    const GravityField& Gravity() const { return m_gravityMap; }

    std::vector<DynamicBody>& DynamicBodies() { return m_dynamicBodies; }
    const std::vector<DynamicBody>& DynamicBodies() const { return m_dynamicBodies; }
    const std::vector<DynamicVisual>& DynamicVisuals() const { return m_dynamicVisuals; }
    const std::vector<StaticBody>& StaticBodies() const { return m_staticBodies; }
    const std::vector<StaticRenderable>& StaticRenderables() const { return m_staticRenderables; }
    const std::vector<Terrain>& Terrains() const { return m_terrains; }
    const std::vector<StaticLight>& StaticLights() const { return m_staticLights; }
    const std::vector<GravityRegion>& GravityRegions() const { return m_gravityRegions; }
    std::vector<Door>& Doors() { return m_doors; }
    const std::vector<Door>& Doors() const { return m_doors; }
    std::vector<LightSwitch>& LightSwitches() { return m_lightSwitches; }
    const std::vector<LightSwitch>& LightSwitches() const { return m_lightSwitches; }

    const std::optional<Vehicle>& GetVehicle() const { return m_vehicle; }
    const CelestialGravity& Celestial() const { return *m_celestial; }
    const std::vector<BodyHandle>& CelestialParticipants() const { return m_celestialParticipants; }
    const std::vector<PointMassSource>& PointMassSources() const { return m_pointMassSources; }
    const std::vector<OperatorThrust>& OperatorThrusts() const { return m_operatorThrusts; }
    const std::optional<Atmosphere>& GetAtmosphere() const { return m_atmosphere; }
    CombustionWorld& Combustion() { return m_combustion; }
    const CombustionWorld& Combustion() const { return m_combustion; }
    const std::vector<Combustible>& Combustibles() const { return m_combustibles; }

    FluidWorld& Fluid() { return *m_fluid; }
    const FluidWorld& Fluid() const { return *m_fluid; }
    const FluidSettings& FluidSettingsUsed() const { return m_fluidSettings; }
    const std::vector<FluidVolume>& FluidVolumes() const { return m_fluidVolumes; }
    MeshHandle FluidMesh() const { return m_fluidMesh; }
    bool FluidSurfaceDirty() const { return m_fluidSurfaceRevision != m_fluid->Revision(); }
    void MarkFluidSurfaceUploaded() const { m_fluidSurfaceRevision = m_fluid->Revision(); }
    ProductionFluidCoupling& FluidCoupling() { return *m_fluidCoupling; }
    const ProductionFluidCoupling& FluidCoupling() const { return *m_fluidCoupling; }
    bool HasFluid() const { return !m_fluidVolumes.empty(); }
    // The M25 held-key water source: adds one particle at the next emitter
    // slot if any emitter has capacity. Returns false when none does.
    void UpdateScripts(const InputSystem* input,float dt);
    void PresentationScripts(const InputSystem* input,float dt,float alpha);
    void DispatchPhysicsEvents(const InputSystem* input,float dt);
    bool SetColliderEnabled(EntityId id,bool enabled);
    void FixedScripts(const InputSystem* input,float dt);
    bool RestoreScriptState(const std::vector<ScriptStateRecord>& records,std::string& error,bool resume=false);
    void EndScripts(){m_scripts.reset();m_ui.reset();pointerCapture=false;}
    LocalizationSession& Localization();
    RuntimeUI& UI(){if(!m_ui)m_ui=std::make_unique<RuntimeUI>(m_assets);m_ui->SetLocalization(&Localization());return *m_ui;}
    const RuntimeUI* UIIfLoaded()const{return m_ui.get();}
    void UpdateUIScripts(InputSystem* input,float dt);
    void DispatchUIEvents(const InputSystem* input,float dt);
    ScriptSystem* Scripts() const {return m_scripts.get();}
    struct MetadataWork {uint64_t lookups=0,indexRebuilds=0,definitionsCopied=0,scriptSlotsCopied=0,touchBodyResolutions=0;};
    MetadataWork MetadataStats()const{return m_metadataWork;}
    std::vector<SceneObject> ScriptObjects(bool scriptedOnly=false) const;
    // Internal callback snapshots contain only slot metadata; full definitions
    // remain available to explicit world enumeration. Keys freeze authored order.
    std::vector<SceneObject> ScriptSlots() const;
    std::vector<std::pair<EntityId,uint64_t>> ScriptKeys() const;
    std::vector<EntityId> DefinitionIds() const;
    const SceneObject* RuntimeDefinition(EntityId id) const;
    bool SetRuntimeTransform(EntityId id,const SceneTransform& transform);
    bool SetMaterialSlot(EntityId,unsigned,const MaterialSlot&);
    bool SetModelPartVisible(EntityId,const std::string&,bool);
    bool SetAppearance(const SceneSettings&);
    BodyHandle RuntimeBody(EntityId id) const;
    JointHandle RuntimeJoint(EntityId owner);
    bool SetRuntimeJoint(EntityId owner,const SceneJointComponent&,bool remove,std::string& error);
    bool SetBodyMaterial(EntityId,const std::string&,const PhysicalMaterial*,std::string& error);
    CharacterMotor* RuntimeCharacter(EntityId id);
    void UpdateCharacters(float dt);
    LiquidSystem& Liquids(){return *m_liquid;}
    const LiquidSystem& Liquids()const{return *m_liquid;}
    void UpdateLiquids(double dt){if(m_liquid&&m_hasLiquid)m_liquid->Update(*this,dt);}
    NavigationSystem& Navigation(){return *m_navigation;}
    const NavigationSystem& Navigation()const{return *m_navigation;}
    bool SetNavigationEnabled(EntityId id,const std::string& kind,bool enabled);
    bool SetNavigationAgentSettings(EntityId id,const NavigationAgentSettings& settings);
    void UpdateNavigation(float dt){if(m_navigation&&m_hasNavigation)m_navigation->Update(*this,dt);}
    bool SetCharacterSettings(EntityId,const CharacterMotorSettings&);
    void ClearCharacters();
    struct CharacterInstance {CharacterMotor motor;SceneTransform previous;};
    struct RuntimeView {SceneTransform pose;float fov=70,nearPlane=.1f,farPlane=500;};
    int viewportWidth=0,viewportHeight=0;
    std::optional<RuntimeView> view;
    std::map<std::pair<EntityId,uint64_t>,std::string> spawnStates; // consumed once by script construction; never restore defaults
    bool SetRuntimeView(const SceneTransform& pose,float fov,float nearPlane=.1f,float farPlane=500);
    struct AnimationLayer {AnimationLayerSettings settings;AnimationPlayback playback;std::vector<int> mask;SkeletalPose reference;};
    struct AnimationInstance {EntityId owner=0;std::shared_ptr<const SkeletalAsset> asset;AnimationPlayback playback;PoseMixer mixer;SkeletalPose sourcePose,finalPose;std::vector<glm::mat4> skin;std::vector<AnimationLayer> layers;std::map<std::string,PoseContribution> external;std::string error;std::vector<glm::mat4> previousWorld,recentWorld;float motionDt=0;};
    AnimationInstance* RuntimeAnimation(EntityId);
    bool JointPose(EntityId,const std::string& key,const std::string& space,float alpha,SceneTransform&);
    bool SetLimbIK(EntityId,const LimbIKSettings&,bool remove,std::string&);
    void UpdateSockets();
    bool SetSocket(EntityId,const SceneSocketComponent&,bool remove,std::string&);
    const std::vector<glm::mat4>* AnimationSkin(EntityId) const;
    void UpdateAnimations(float dt);
    struct FractureRigidState {std::vector<EntityId> parts;std::vector<JointHandle> bonds,supports;bool initialized=false;};
    struct DeformableRecord {FractureRigidState rigid;mutable uint64_t meshTopology=0;DeformableInstance simulation;std::vector<DeformableTarget> targets;mutable MeshData presentation;mutable MeshHandle mesh;uint64_t revision=0;mutable uint64_t mappedRevision=~uint64_t(0);mutable float mappedAlpha=-1;};
    DeformableInstance* RuntimeDeformable(EntityId,std::string& error,bool staged=false);
    void UpdateDeformables(double dt);
    bool PrepareRigidFracture(EntityId,std::string&);
    void UpdateRigidFracture(EntityId,double);
    void PersistRigidFracture(EntityId,SaveArchive&);
    void DispatchFractureEvents();
    bool LoadFracturePart(EntityId,unsigned,glm::vec3,bool impulse);
    EntityId FracturePartEntity(EntityId,unsigned,bool includeDisabled=false)const;
    void DrawDeformables(Renderer&,float alpha)const;
    void RemoveDeformable(EntityId);
    void ClearDeformables();
    bool RestoreDeformable(EntityId,const std::string&,std::string&,bool staged=false);
    bool CaptureDeformable(EntityId,std::string&,std::string&);
    void SetDeformableEnabled(EntityId,bool);
    bool ResetDeformable(EntityId,std::string&);
    void InvalidateDeformableTargets(EntityId);
    bool SetFinalPose(EntityId,const SkeletalPose&,std::string& error);
    bool SetPoseContribution(EntityId,const std::string&,const PoseContribution&,std::string& error);
    void RemovePoseContribution(EntityId,const std::string&);
    bool SetAnimationLayer(EntityId,const AnimationLayerSettings&,bool remove,std::string& error);
    void ResolveAnimationPose(AnimationInstance&,float dt);

    struct RagdollMappedBody {int node=-1,parent=-1;EntityId entity=0;BodyHandle body;glm::vec3 offset{0},scale{1};glm::quat orientation{1,0,0,0};};
    struct RagdollInstance {std::shared_ptr<const SkeletalAsset> asset;std::vector<RagdollMappedBody> bodies;std::vector<JointHandle> joints;SceneTransform reference;glm::vec3 rootLocalPosition{0};};
    bool EnterRagdoll(EntityId,std::string& error);
    bool LeaveRagdoll(EntityId,float fadeSeconds,std::string& error);
    bool SetRagdollEnabled(EntityId,bool enabled,std::string& error);
    bool RagdollActive(EntityId)const;
    EntityId RagdollBody(EntityId,const std::string& key)const;
    void UpdateRagdolls(float dt);
    bool SetTransientEntity(EntityId id,bool transient,std::string& error);
    bool IsTransientEntity(EntityId id)const {const auto* e=FindEntity(id);return e&&e->transient;}

    void SynchronizeJoints();
    bool EmitFluidParticle();
    std::size_t EmittedFluidParticles() const { return m_emittedParticles; }

    const std::optional<PlayerStart>& GetPlayerStart() const { return m_playerStart; }
    // Handles of the live, pickable entities (recomputed: handles change
    // across reconstruction — refresh whenever EntityVersion() changes).
    std::vector<BodyHandle> PickableBodies() const;
    const SceneSettings& Settings() const { return m_settings; }

    // Name of the scene object a body belongs to, or "" if unknown.
    std::string NameOfBody(BodyHandle handle) const;

    // --- Milestone 29: entity lifecycle and fidelity -------------------
    //
    // Every dynamic body is a persistent entity with a record here, in
    // slot order (parallel to DynamicBodies()). Static content is not an
    // entity in this sense: it never changes and needs no lifecycle.
    const ProjectClassification& Categories() const { return m_categories; }
    CategoryMask TagsOf(EntityId id) const;
    bool HasTag(EntityId id,unsigned tag) const {return (TagsOf(id)&CategoryBit(tag))!=0;}
    bool AddTag(EntityId id,unsigned tag);
    bool RemoveTag(EntityId id,unsigned tag);
    unsigned RenderLayerOf(EntityId id) const;
    std::vector<EntityId> QueryEntities(CategoryMask requiredTags,CategoryMask excludedTags=0,
                                      const std::vector<EntityId>* candidates=nullptr) const;
    const std::vector<EntityRecord>& Entities() const { return m_entities; }
    std::vector<EntityRecord>& MutableEntities() { m_entityIndexVersion=~0u; return m_entities; }
    const EntityRecord* FindEntity(EntityId id) const;
    EntityRecord* FindEntity(EntityId id);
    EntityId EntityIdOfBody(BodyHandle handle) const;
    // Increments whenever the set of entities/slots changes (create,
    // destroy, reconstruct) so gameplay lists keyed on handles can refresh.
    unsigned int EntityVersion() const { return m_entityVersion; }

    // Capability: a component set with no reduced representation (vehicle,
    // combustible, compound body) must stay Full while active.
    static bool EntityRequiresFull(const SceneObject& definition);

    // The lifecycle operations. Each returns false with a message when the
    // request is impossible (unknown id, destroyed entity, a Full-only
    // entity asked to reduce). Transitions preserve identity, pose and
    // both velocities; reconstruction applies no impulse and reuses the
    // entity's slot, so nothing is duplicated.
    bool SetEntityFidelity(EntityId id, SimulationFidelity fidelity, std::string* outError = nullptr);
    bool DestroyEntity(EntityId id, std::string* outError = nullptr);
    // Read-only preflight shared by persistence and the mutation entry points.
    bool ValidateEntityDestruction(EntityId id, std::string& outError) const;
    static bool ValidateEntityDefinition(const SceneObject& definition, std::string& outError);
    bool ValidateEntityCreation(const SceneObject& definition, std::string& outError) const;
    // Identity captured from authored Scene once by a successful Build.
    const std::string& BaselineFingerprint() const { return m_baselineFingerprint; }
    // Creates a persistent entity at runtime from a scene-object definition
    // (its id may be preset from a delta, else one is allocated from the
    // runtime range). `state` overrides the definition's transform/initial
    // velocity when given. Returns the invalid id on failure.
    EntityId CreateEntity(const SceneObject& definition, const EntityPhysicalState* state,
                          std::string* outError = nullptr);
    SceneTransform PresentedTransform(SceneObjectId id, const SceneTransform& fallback, float alpha) const;
    struct PrefabScriptInit {SceneObjectId source=0;uint64_t slot=0;std::string properties,state="null";};
    struct PrefabSpawnOptions {std::optional<glm::vec3> velocity,angularVelocity;std::vector<PrefabScriptInit> scripts;};
    EntityId SpawnPrefab(const AssetId& asset, const SceneTransform& placement, std::string& error,const PrefabSpawnOptions& options={});
    std::string TakeSpawnState(EntityId id,uint64_t slot);
    bool DestroyHierarchy(EntityId root, std::string& error);
    const std::vector<EntityRecord>& AdditionalEntities() const { return m_extraEntities; }
    EntityId AllocateRuntimeEntityId();
    void SetNextRuntimeEntityId(EntityId next);
    EntityId NextRuntimeEntityId() const { return m_nextRuntimeId; }
    // Current physical state of any non-destroyed entity, from the live
    // body when Full, from the record otherwise.
    bool GetEntityState(EntityId id, EntityPhysicalState& outState) const;
    bool SetEntityState(EntityId id, const EntityPhysicalState& state);

    // Policy: the scene's authored policy is installed by Build; a game can
    // replace it (or install none). Evaluated by EvaluateFidelityPolicy for
    // managed, unforced, unpinned entities. `pinned` are entities gameplay
    // needs Full this step (held, supporting the player).
    void SetFidelityPolicy(std::unique_ptr<FidelityPolicy> policy);
    const FidelityPolicy* GetFidelityPolicy() const { return m_policy.get(); }
    void EvaluateFidelityPolicy(const FidelityPolicyContext& context, const std::vector<EntityId>& pinned);
    // Debug override (editor): forces one entity to a fidelity until cleared.
    bool ForceEntityFidelity(EntityId id, std::optional<SimulationFidelity> fidelity, std::string* outError = nullptr);

    struct LifecycleCounts {
        std::size_t full = 0, coarse = 0, dormant = 0, destroyed = 0;
        std::size_t physicsBodies = 0;       // every live PhysicsWorld body
        std::size_t dynamicPhysicsBodies = 0;
        unsigned int transitionsThisStep = 0;
    };
    LifecycleCounts CountLifecycle() const;
    double SimulationTimeSeconds() const { return m_simulationTime; }
    void AdvanceSimulationTime(double seconds) { m_simulationTime += seconds; }

    // Doors/switches by scene object id, for persisted interactable state.
    Door* FindDoor(SceneObjectId id);
    LightSwitch* FindLightSwitch(SceneObjectId id);
    const std::vector<SceneObjectId>& DoorIds() const { return m_doorIds; }
    const std::vector<SceneObjectId>& LightSwitchIds() const { return m_lightSwitchIds; }

// M59 registration seam uses the same component constructor as Build.
    bool StageRegionObject(const SceneObject& worldObject,const SceneObject& localObject,std::string&,const std::string& stableIdentity="");
    void PublishRegion(const std::vector<EntityId>& ids);
    bool RestoreRegionObject(EntityId,const SceneTransform&,const EntityPhysicalState&,const std::vector<ScriptStateRecord>&,std::string&,bool);
    bool RegionVisualReady(const std::vector<EntityId>& ids) const;
    void HideRegion(const std::vector<EntityId>& ids);
    void BindRegionCameraReference(EntityId id,EntityId target);
    void RemoveRegionObject(EntityId id);
    void EndRegionScripts(const std::vector<EntityId>& ids);
    bool IsPublished(EntityId id) const { return !m_regionPending.count(id); }
    void RebuildRegionGravity(const std::map<EntityId,std::string>& order);
    void SetCompositionFingerprint(const std::string& hash) { m_baselineFingerprint=hash;m_composed=true; }
    bool IsComposed() const { return m_composed; }
private:
    friend class WorldPersistence;
    bool m_restoreConstruction=false;
    bool m_composed=false;
    std::set<EntityId> m_regionPending;
    std::map<EntityId,std::vector<AssetId>> m_regionAssets;
    std::shared_ptr<SceneSession> m_sceneControl;
    std::map<unsigned,EntityId> m_touchEntityHistory;
    struct FluidVolumeSetup;
    void PopulateFluid();
    // Creates the PhysicsWorld body for a record's definition at `state`,
    // binds it to the record's slot, and refreshes the slot's visual.
    bool InstantiateEntityBody(EntityRecord& record, const EntityPhysicalState& state, std::string* outError);
    void ReleaseEntityBody(EntityRecord& record);
    bool AppendEntitySlot(const SceneObject& definition, bool authored, const EntityPhysicalState& state,
                          SimulationFidelity fidelity, std::string* outError);
    // Requests and references the render component's assets; false with a
    // message when an id cannot be resolved by the asset database.
    bool RequestVisualAssets(const SceneObject& o, std::string* outError);
    bool ValidateVisualAssets(const SceneObject& o, std::string& outError) const;
    void RebuildCelestialParticipants();

    struct EntityCategories {CategoryMask tags=0, authoredTags=0;unsigned renderLayer=0;BodyHandle body;};
    ProjectClassification m_categories;
    std::unique_ptr<NavigationSystem> m_navigation;
    std::unique_ptr<LiquidSystem> m_liquid=std::make_unique<LiquidSystem>();
    bool m_hasLiquid=false;
    std::map<EntityId,EntityCategories> m_entityCategories;
    bool m_built = false;
    PhysicsWorld m_physics;
    bool AppendSceneObjects(const Scene& scene, bool authored, const FidelityPolicyContext& context, std::string& error);
    Scene m_hierarchy; // local authored transforms, allocated only for parented scenes/spawns
    bool m_hasScripts=false,m_hasNavigation=false;
    std::set<EntityId> m_animationOwners,m_deformableOwners;
    std::map<EntityId,DeformableRecord> m_deformables;
    std::map<EntityId,CharacterInstance> m_characters;
    std::map<EntityId,AnimationInstance> m_animationInstances;
    std::map<EntityId,RagdollInstance> m_ragdolls;
    struct RagdollReturn {float elapsed=0,duration=0;};
    std::map<EntityId,RagdollReturn> m_ragdollReturns;
    std::set<EntityId> m_ragdollAutostarted;
    std::set<EntityId> m_jointOwners,m_jointParticipants;
    std::map<EntityId,JointHandle> m_runtimeJoints;
    std::unique_ptr<ScriptSystem> m_scripts;
    std::unique_ptr<RuntimeUI> m_ui;
    std::unique_ptr<LocalizationSession> m_localization;
    std::map<EntityId,SceneObject> m_scriptDefinitions;
    std::set<EntityId> m_characterOwners;
    std::set<EntityId> m_scriptOwners; // Ordered membership; inert scenery is not copied into callback phases.
    std::vector<EntityRecord> m_extraEntities; // normal non-dynamic runtime components
    // Store positions rather than pointers: vector growth never aliases stale records.
    mutable MetadataWork m_metadataWork;
    void RefreshEntityIndex() const;
    mutable std::map<EntityId,std::pair<bool,size_t>> m_entityIndex;
    mutable unsigned m_entityIndexVersion=~0u;
    mutable size_t m_entityIndexMainSize=~size_t(0),m_entityIndexExtraSize=~size_t(0);
    SceneSettings m_settings;
    std::string m_baselineFingerprint;
    ResourceManager* m_assets = nullptr;

    std::vector<std::unique_ptr<GravityField>> m_gravityFields;
    std::vector<std::unique_ptr<GravityVolume>> m_gravityVolumes;
    GravityContextMap m_gravityMap;
    std::vector<GravityRegion> m_gravityRegions;

    std::vector<StaticBody> m_staticBodies;
    std::vector<Terrain> m_terrains;
    std::vector<StaticRenderable> m_staticRenderables;
    AudioSystem* m_audioSystem=nullptr;
    std::map<EntityId,std::string> m_audioIdentities;
    struct AudioResume {double time=0;AudioPlaybackState state=AudioPlaybackState::Stopped;bool requested=false,seekSubmitted=false;float groupGain=1;uint64_t fadeFrames=0;};
    std::map<EntityId,AudioResume> m_audioResume;
    std::vector<std::pair<EntityId,AudioVoiceHandle>> m_audioOneShots;
    bool m_audioRunning=false;
    mutable std::vector<ParticleEmitter> m_particleEmitters;
    std::vector<AudioEmitter> m_audioEmitters;
    std::optional<AudioListener> m_audioListener;
    std::vector<AudioZone> m_audioZones;
    bool m_listenerMotionValid=false;glm::vec3 m_lastListenerPosition{0};
    float m_audioQueryClock=0;std::size_t m_audioQueryCursor=0;
    std::uint64_t m_audioQueries=0;

    mutable std::vector<RenderCamera> m_renderCameras;
    mutable Renderer* m_cameraRenderer = nullptr;
    mutable std::uint64_t m_cameraFrame = 0;
    std::vector<DynamicBody> m_dynamicBodies;
    std::vector<DynamicVisual> m_dynamicVisuals;
    std::vector<StaticLight> m_staticLights;
    std::vector<Door> m_doors;
    std::vector<LightSwitch> m_lightSwitches;

    std::optional<Vehicle> m_vehicle;
    std::unique_ptr<CelestialGravity> m_celestial;
    std::vector<BodyHandle> m_celestialParticipants;
    std::vector<PointMassSource> m_pointMassSources;
    std::vector<OperatorThrust> m_operatorThrusts;
    std::optional<Atmosphere> m_atmosphere;
    std::shared_ptr<const RadialTerrain> m_atmosphereTerrain;  // keeps the masked surface alive
    CombustionWorld m_combustion;
    std::vector<Combustible> m_combustibles;

    FluidSettings m_fluidSettings;
    std::unique_ptr<FluidWorld> m_fluid;
    std::unique_ptr<ProductionFluidCoupling> m_fluidCoupling;
    std::vector<FluidVolume> m_fluidVolumes;
    MeshHandle m_fluidMesh;
    mutable std::uint64_t m_fluidSurfaceRevision = ~std::uint64_t{0};
    std::size_t m_emittedParticles = 0;

    std::optional<PlayerStart> m_playerStart;
    std::vector<BodyHandle> m_pickableBodies;
    std::vector<AssetId> m_referencedAssets;  // released on Destroy

    std::vector<EntityRecord> m_entities;
    std::unique_ptr<FidelityPolicy> m_policy;
    EntityId m_nextRuntimeId = kRuntimeEntityIdBase;
    unsigned int m_entityVersion = 0;
    unsigned int m_transitionsThisStep = 0;
    double m_simulationTime = 0.0;
    std::vector<SceneObjectId> m_doorIds;
    std::vector<SceneObjectId> m_lightSwitchIds;
};
