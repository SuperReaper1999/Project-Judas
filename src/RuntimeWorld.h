#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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

class RadialTerrain;
class ResourceManager;

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
class RuntimeWorld {
public:
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
    // One authored renderable (mesh, box or sphere) that has no dynamic
    // body — drawn at its authored pose every frame.
    struct StaticRenderable {
        SceneObjectId id = kInvalidSceneObjectId;
        SceneRenderComponent render;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        MeshHandle mesh;
        TextureHandle texture;
    };
    // Presentation data for a dynamic body, parallel to DynamicBodies().
    struct DynamicVisual {
        SceneObjectId id = kInvalidSceneObjectId;
        std::string name;
        SceneRenderComponent render;
        bool hasRender = false;
        MeshHandle mesh;
        TextureHandle texture;
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

    // Instantiates `scene`. `assets` may be null for headless use (no GPU
    // resources are created; every mesh handle stays invalid). On failure
    // nothing is left allocated and `outError` says which object/component
    // could not be realised.
    bool Build(const Scene& scene, ResourceManager* resources, std::string& outError);
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
    bool HasFluid() const { return !m_fluidVolumes.empty(); }
    // The M25 held-key water source: adds one particle at the next emitter
    // slot if any emitter has capacity. Returns false when none does.
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
    const std::vector<EntityRecord>& Entities() const { return m_entities; }
    std::vector<EntityRecord>& MutableEntities() { return m_entities; }
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
    // Creates a persistent entity at runtime from a scene-object definition
    // (its id may be preset from a delta, else one is allocated from the
    // runtime range). `state` overrides the definition's transform/initial
    // velocity when given. Returns the invalid id on failure.
    EntityId CreateEntity(const SceneObject& definition, const EntityPhysicalState* state,
                          std::string* outError = nullptr);
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

private:
    struct FluidVolumeSetup;
    void PopulateFluid();
    // Creates the PhysicsWorld body for a record's definition at `state`,
    // binds it to the record's slot, and refreshes the slot's visual.
    bool InstantiateEntityBody(EntityRecord& record, const EntityPhysicalState& state, std::string* outError);
    void ReleaseEntityBody(EntityRecord& record);
    bool AppendEntitySlot(const SceneObject& definition, bool authored, const EntityPhysicalState& state,
                          SimulationFidelity fidelity, std::string* outError);
    bool LoadVisualAssets(const SceneObject& o, DynamicVisual& visual, std::string* outError);
    void RebuildCelestialParticipants();

    bool m_built = false;
    PhysicsWorld m_physics;
    SceneSettings m_settings;
    ResourceManager* m_assets = nullptr;

    std::vector<std::unique_ptr<GravityField>> m_gravityFields;
    std::vector<std::unique_ptr<GravityVolume>> m_gravityVolumes;
    GravityContextMap m_gravityMap;
    std::vector<GravityRegion> m_gravityRegions;

    std::vector<StaticBody> m_staticBodies;
    std::vector<Terrain> m_terrains;
    std::vector<StaticRenderable> m_staticRenderables;
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
    std::vector<FluidVolume> m_fluidVolumes;
    MeshHandle m_fluidMesh;
    std::size_t m_emittedParticles = 0;

    std::optional<PlayerStart> m_playerStart;
    std::vector<BodyHandle> m_pickableBodies;

    std::vector<EntityRecord> m_entities;
    std::unique_ptr<FidelityPolicy> m_policy;
    EntityId m_nextRuntimeId = kRuntimeEntityIdBase;
    unsigned int m_entityVersion = 0;
    unsigned int m_transitionsThisStep = 0;
    double m_simulationTime = 0.0;
    std::vector<SceneObjectId> m_doorIds;
    std::vector<SceneObjectId> m_lightSwitchIds;
};
