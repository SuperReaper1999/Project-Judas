#include "RuntimeWorld.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "FaithfulGravity.h"
#include "RadialTerrain.h"
#include "RadicalGravity.h"
#include "RenderAssetCache.h"
#include "SphericalVolume.h"
#include "TerrainLibrary.h"
#include "UniformGravity.h"

namespace {
constexpr float kFaithfulMagnitude = 9.81f;
// Below these an entity leaving Full simulation is treated as resting on
// whatever supported it (CoarseMotion::Settled) rather than in free flight.
constexpr float kSettledLinearSpeed = 0.05f;
constexpr float kSettledAngularSpeed = 0.05f;

bool IsWorldDown(const glm::quat& rotation) {
    const glm::vec3 down = rotation * glm::vec3(0.0f, -1.0f, 0.0f);
    return std::abs(down.x) < 1.0e-6f && std::abs(down.z) < 1.0e-6f && down.y < 0.0f;
}

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
    if (o.vehicle || o.combustible) return true;
    if (o.body && o.body->shape == SceneShape::Compound) return true;
    return false;
}

bool RuntimeWorld::LoadVisualAssets(const SceneObject& o, DynamicVisual& visual, std::string* outError) {
    if (o.render && o.render->shape == SceneShape::Mesh && m_assets) {
        std::string assetError;
        visual.mesh = m_assets->GetMesh(o.render->meshPath, assetError);
        if (!visual.mesh.IsValid()) {
            if (outError) *outError = assetError;
            return false;
        }
        if (!o.render->texturePath.empty()) {
            visual.texture = m_assets->GetTexture(o.render->texturePath, assetError);
            if (!visual.texture.IsValid()) {
                if (outError) *outError = assetError;
                return false;
            }
        }
    }
    return true;
}

bool RuntimeWorld::InstantiateEntityBody(EntityRecord& record, const EntityPhysicalState& state,
                                         std::string* outError) {
    const SceneObject& o = record.definition;
    const SceneBodyComponent& b = *o.body;
    BodyHandle handle;
    switch (b.shape) {
        case SceneShape::Box:
            handle = m_physics.CreateDynamicBox(state.position, b.halfExtents, b.mass, b.friction, b.restitution);
            break;
        case SceneShape::Sphere:
            handle = m_physics.CreateDynamicSphere(state.position, b.radius, b.mass, b.friction, b.restitution);
            break;
        case SceneShape::Compound:
            handle = m_physics.CreateDynamicCompoundBoxes(state.position, b.compoundBoxes, b.mass, b.friction,
                                                          b.restitution);
            break;
        case SceneShape::Terrain:
        case SceneShape::Mesh:
            break;
    }
    if (!handle.IsValid()) {
        if (outError) *outError = "the body could not be created";
        return false;
    }
    // Reconstruction hands the body its retained pose AND velocities: no
    // reset to rest, no impulse.
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
    if (!LoadVisualAssets(o, dv, outError)) return false;

    EntityRecord record;
    record.id = o.id;
    record.name = o.name;
    record.definition = o;
    record.authored = authored;
    record.managed = b.managed;
    record.requiresFull = EntityRequiresFull(o);
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

bool RuntimeWorld::Build(const Scene& scene, RenderAssetCache* assets, std::string& outError) {
    Destroy();
    m_assets = assets;
    m_settings = scene.Settings();
    if (!m_physics.Init()) {
        outError = "physics initialization failed";
        return false;
    }
    m_built = true;

    if (!(m_settings.fluidScale > 0.0f)) {
        outError = "settings: fluid-scale must be positive";
        Destroy();
        return false;
    }
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

    const auto fail = [&](const SceneObject& o, const std::string& what) {
        outError = "object " + std::to_string(o.id) + " \"" + o.name + "\": " + what;
        Destroy();
        return false;
    };

    for (const SceneObject& o : scene.Objects()) {
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
                        return fail(o, "static compound bodies are not supported");
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
                if (!AppendEntitySlot(o, /*authored=*/true, state, fidelity, &entityError)) return fail(o, entityError);
                bodyHandle = m_dynamicBodies[dynamicIndex].Handle();
            }
        } else if (o.render && (o.render->shape == SceneShape::Compound || o.render->shape == SceneShape::Terrain)) {
            return fail(o, "compound/terrain rendering needs a body");
        }

        // --- Renderable without a dynamic body ---
        if (o.render && !isDynamic && o.render->shape != SceneShape::Terrain && !o.door && !o.lightSwitch) {
            StaticRenderable sr;
            sr.id = o.id;
            sr.render = *o.render;
            sr.position = position;
            sr.rotation = rotation;
            sr.scale = o.transform.scale;
            if (o.render->shape == SceneShape::Mesh && m_assets) {
                std::string assetError;
                sr.mesh = m_assets->GetMesh(o.render->meshPath, assetError);
                if (!sr.mesh.IsValid()) return fail(o, assetError);
                if (!o.render->texturePath.empty()) {
                    sr.texture = m_assets->GetTexture(o.render->texturePath, assetError);
                    if (!sr.texture.IsValid()) return fail(o, assetError);
                }
            }
            if (o.render->shape == SceneShape::Compound) return fail(o, "compound render needs a dynamic body");
            m_staticRenderables.push_back(sr);
        }

        // --- Gravity region ---
        if (o.gravity) {
            const SceneGravityComponent& g = *o.gravity;
            std::unique_ptr<GravityField> field;
            if (g.kind == SceneGravityKind::Radial) {
                field = std::make_unique<RadicalGravity>(position, g.magnitude);
            } else if (IsWorldDown(rotation) && g.magnitude == kFaithfulMagnitude) {
                field = std::make_unique<FaithfulGravity>();
            } else {
                field = std::make_unique<UniformGravity>(rotation * glm::vec3(0.0f, -g.magnitude, 0.0f));
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
            if (isDynamic) {
                if (o.celestial->operatorThrustForce > 0.0f) {
                    m_operatorThrusts.push_back({bodyHandle, o.celestial->operatorThrustForce});
                }
            } else {
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
            if (m_playerStart) return fail(o, "more than one player-start");
            m_playerStart = PlayerStart{position, o.playerStart->yawDegrees, o.playerStart->view};
        }
    }

    for (const Combustible& c : m_combustibles) {
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
    if (m_assets && !m_fluidVolumes.empty() && m_assets->GetRenderer()) {
        m_fluidMesh = m_assets->GetRenderer()->CreateMesh(MeshData{});
    }
    PopulateFluid();
    return true;
}

void RuntimeWorld::RebuildCelestialParticipants() {
    // Pairwise Newtonian set: every LIVE dynamic celestial body, plus a
    // vehicle that samples local gravity (the classic scene's spacecraft).
    // Coarse celestial entities contribute through CoarseSimulation and
    // Simulation's coarse-to-live pass instead.
    m_celestialParticipants.clear();
    bool anyCelestial = false;
    for (const EntityRecord& e : m_entities) {
        if (e.lifecycle == EntityLifecycle::Destroyed || !e.definition.celestial) continue;
        anyCelestial = true;
        if (e.fidelity == SimulationFidelity::Full) m_celestialParticipants.push_back(m_dynamicBodies[e.slot].Handle());
    }
    if (m_vehicle && m_vehicle->component.gravity == SceneVehicleGravity::Local && anyCelestial) {
        m_celestialParticipants.push_back(m_vehicle->handle);
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
    if (!m_built) return;
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
    ++m_entityVersion;
    RebuildCelestialParticipants();
    m_combustion.Reset();
    PopulateFluid();
}

std::string RuntimeWorld::NameOfBody(BodyHandle handle) const {
    for (std::size_t i = 0; i < m_dynamicBodies.size(); ++i) {
        if (m_dynamicBodies[i].IsLive() && m_dynamicBodies[i].Handle().id == handle.id) return m_dynamicVisuals[i].name;
    }
    return std::string();
}

// --- Milestone 29 -------------------------------------------------------

const EntityRecord* RuntimeWorld::FindEntity(EntityId id) const {
    for (const EntityRecord& e : m_entities) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

EntityRecord* RuntimeWorld::FindEntity(EntityId id) {
    for (EntityRecord& e : m_entities) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

EntityId RuntimeWorld::EntityIdOfBody(BodyHandle handle) const {
    if (!handle.IsValid()) return kInvalidSceneObjectId;
    for (const EntityRecord& e : m_entities) {
        if (e.lifecycle != EntityLifecycle::Destroyed && m_dynamicBodies[e.slot].IsLive() &&
            m_dynamicBodies[e.slot].Handle().id == handle.id) {
            return e.id;
        }
    }
    return kInvalidSceneObjectId;
}

bool RuntimeWorld::GetEntityState(EntityId id, EntityPhysicalState& outState) const {
    const EntityRecord* e = FindEntity(id);
    if (!e || e->lifecycle == EntityLifecycle::Destroyed) return false;
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

bool RuntimeWorld::DestroyEntity(EntityId id, std::string* outError) {
    EntityRecord* e = FindEntity(id);
    if (!e) {
        if (outError) *outError = "unknown entity id " + std::to_string(id);
        return false;
    }
    if (e->lifecycle == EntityLifecycle::Destroyed) return true;
    if (e->definition.vehicle || e->definition.combustible) {
        if (outError) *outError = "entity " + std::to_string(id) + " (" + e->name + ") cannot be destroyed at runtime";
        return false;
    }
    ReleaseEntityBody(*e);
    e->lifecycle = EntityLifecycle::Destroyed;
    e->fidelity = SimulationFidelity::Dormant;
    m_dynamicVisuals[e->slot].hasRender = false;
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

EntityId RuntimeWorld::CreateEntity(const SceneObject& definitionIn, const EntityPhysicalState* state,
                                    std::string* outError) {
    if (!m_built) {
        if (outError) *outError = "no world";
        return kInvalidSceneObjectId;
    }
    SceneObject definition = definitionIn;
    if (!definition.body || definition.body->motion != SceneBodyMotion::Dynamic ||
        definition.body->shape == SceneShape::Terrain || definition.body->shape == SceneShape::Mesh) {
        if (outError) *outError = "a runtime-created entity needs a dynamic box, sphere or compound body";
        return kInvalidSceneObjectId;
    }
    if (definition.vehicle || definition.combustible || definition.atmosphere || definition.fluidVolume ||
        definition.playerStart || definition.door || definition.lightSwitch || definition.gravity) {
        if (outError) *outError = "runtime-created entities carry only body/render/celestial components";
        return kInvalidSceneObjectId;
    }
    if (definition.id == kInvalidSceneObjectId) {
        definition.id = AllocateRuntimeEntityId();
    } else if (definition.id < kRuntimeEntityIdBase) {
        if (outError) *outError = "runtime-created entity ids must be in the runtime range";
        return kInvalidSceneObjectId;
    } else if (FindEntity(definition.id)) {
        if (outError) *outError = "entity id " + std::to_string(definition.id) + " already exists";
        return kInvalidSceneObjectId;
    } else if (definition.id >= m_nextRuntimeId) {
        m_nextRuntimeId = definition.id + 1;
    }
    const EntityPhysicalState initial = state ? *state : StateFromDefinition(definition);
    if (!AppendEntitySlot(definition, /*authored=*/false, initial, SimulationFidelity::Full, outError)) {
        return kInvalidSceneObjectId;
    }
    if (definition.celestial) RebuildCelestialParticipants();
    return definition.id;
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
    if (!m_built) return;
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
    m_physics.Shutdown();

    m_doors.clear();
    m_doorIds.clear();
    m_lightSwitches.clear();
    m_lightSwitchIds.clear();
    m_dynamicBodies.clear();
    m_dynamicVisuals.clear();
    m_entities.clear();
    m_policy.reset();
    m_nextRuntimeId = kRuntimeEntityIdBase;
    m_entityVersion = 0;
    m_transitionsThisStep = 0;
    m_simulationTime = 0.0;
    m_staticBodies.clear();
    m_terrains.clear();
    m_staticRenderables.clear();
    m_staticLights.clear();
    m_gravityMap = GravityContextMap();
    m_gravityFields.clear();
    m_gravityVolumes.clear();
    m_vehicle.reset();
    m_celestial.reset();
    m_celestialParticipants.clear();
    m_pointMassSources.clear();
    m_operatorThrusts.clear();
    m_atmosphere.reset();
    m_atmosphereTerrain.reset();
    m_combustion.Clear();
    m_combustibles.clear();
    m_fluid.reset();
    m_fluidSettings = FluidSettings{};
    m_fluidVolumes.clear();
    m_emittedParticles = 0;
    m_playerStart.reset();
    m_pickableBodies.clear();
    m_assets = nullptr;
    m_built = false;
}
