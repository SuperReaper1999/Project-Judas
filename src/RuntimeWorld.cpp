#include "RuntimeWorld.h"

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

bool IsWorldDown(const glm::quat& rotation) {
    const glm::vec3 down = rotation * glm::vec3(0.0f, -1.0f, 0.0f);
    return std::abs(down.x) < 1.0e-6f && std::abs(down.z) < 1.0e-6f && down.y < 0.0f;
}
}  // namespace

RuntimeWorld::RuntimeWorld() = default;

RuntimeWorld::~RuntimeWorld() {
    Destroy();
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

    // Fluid solver settings are scene-wide; length scales follow the
    // authored fluidScale (M25's lake ran the M24 solver at 10x spacing).
    if (!(m_settings.fluidScale > 0.0f)) {
        outError = "settings: fluid-scale must be positive";
        Destroy();
        return false;
    }
    m_fluidSettings.particleRadius *= m_settings.fluidScale;
    m_fluidSettings.smoothingRadius *= m_settings.fluidScale;
    m_fluidSettings.maxDensityCorrection *= m_settings.fluidScale;
    m_fluid = std::make_unique<FluidWorld>(m_fluidSettings);

    std::vector<BodyHandle> dynamicCelestials;
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
                        bodyHandle = m_physics.CreateStaticBox(position, rotation, b.halfExtents,
                                                               b.friction, b.restitution);
                        break;
                    case SceneShape::Sphere:
                        bodyHandle = m_physics.CreateStaticSphere(position, b.radius, b.friction,
                                                                  b.restitution);
                        break;
                    case SceneShape::Terrain: {
                        std::shared_ptr<const RadialTerrain> surface = CreateTerrainSurface(b.terrainSurface);
                        if (!surface) return fail(o, "unknown terrain surface '" + b.terrainSurface + "'");
                        bodyHandle = m_physics.CreateStaticTerrain(position, rotation, surface,
                                                                   b.friction, b.restitution);
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
                isDynamic = true;
                switch (b.shape) {
                    case SceneShape::Box:
                        bodyHandle = m_physics.CreateDynamicBox(position, b.halfExtents, b.mass,
                                                                b.friction, b.restitution);
                        break;
                    case SceneShape::Sphere:
                        bodyHandle = m_physics.CreateDynamicSphere(position, b.radius, b.mass,
                                                                   b.friction, b.restitution);
                        break;
                    case SceneShape::Compound:
                        bodyHandle = m_physics.CreateDynamicCompoundBoxes(position, b.compoundBoxes,
                                                                          b.mass, b.friction,
                                                                          b.restitution);
                        break;
                    case SceneShape::Terrain:
                        return fail(o, "terrain bodies must be static");
                    case SceneShape::Mesh:
                        return fail(o, "a body cannot use the mesh shape");
                }
                m_physics.ResetBody(bodyHandle, position, rotation);
                m_physics.SetLinearVelocity(bodyHandle, b.initialLinearVelocity);

                DynamicBody::Visual visual;
                visual.shape = b.shape == SceneShape::Sphere ? DynamicBody::Shape::Sphere
                                                             : DynamicBody::Shape::Box;
                visual.halfExtents = b.halfExtents;
                visual.radius = b.radius;
                if (o.render) visual.color = o.render->color;
                m_dynamicBodies.emplace_back(bodyHandle, visual, position, rotation);

                DynamicVisual dv;
                dv.id = o.id;
                dv.name = o.name;
                dv.hasRender = o.render.has_value();
                if (o.render) dv.render = *o.render;
                dv.compoundBoxes = b.compoundBoxes;
                dv.scale = o.transform.scale;
                dv.initialLinearVelocity = b.initialLinearVelocity;
                dv.pickable = b.pickable;
                if (o.render && o.render->shape == SceneShape::Mesh && m_assets) {
                    std::string assetError;
                    dv.mesh = m_assets->GetMesh(o.render->meshPath, assetError);
                    if (!dv.mesh.IsValid()) return fail(o, assetError);
                    if (!o.render->texturePath.empty()) {
                        dv.texture = m_assets->GetTexture(o.render->texturePath, assetError);
                        if (!dv.texture.IsValid()) return fail(o, assetError);
                    }
                }
                m_dynamicVisuals.push_back(dv);
                if (b.pickable) m_pickableBodies.push_back(bodyHandle);
            }
        } else if (o.render && (o.render->shape == SceneShape::Compound ||
                                o.render->shape == SceneShape::Terrain)) {
            return fail(o, "compound/terrain rendering needs a body");
        }

        // --- Renderable without a dynamic body ---
        if (o.render && !isDynamic && o.render->shape != SceneShape::Terrain && !o.door &&
            !o.lightSwitch) {
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
                field = std::make_unique<UniformGravity>(
                    rotation * glm::vec3(0.0f, -g.magnitude, 0.0f));
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
            m_doors.emplace_back(m_physics, position, rotation, o.render->halfExtents,
                                 o.door->localHingeAxis, glm::radians(o.door->openAngleDegrees),
                                 glm::radians(o.door->angularSpeedDegreesPerSecond), o.render->color);
        }
        if (o.lightSwitch) {
            if (!o.render) return fail(o, "light switch needs a render component");
            const SceneLightSwitchComponent& s = *o.lightSwitch;
            m_lightSwitches.emplace_back(position, rotation, o.render->halfExtents, s.localHingeAxis,
                                         glm::radians(s.toggleAngleDegrees),
                                         glm::radians(s.angularSpeedDegreesPerSecond), o.render->color,
                                         position + rotation * s.lampLocalOffset, s.lampColor,
                                         s.lampRange);
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
                dynamicCelestials.push_back(bodyHandle);
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

    // Combustion registration needs the atmosphere (initial temperature is
    // the local gas temperature, else 300 K), so it runs after every
    // object has been visited.
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
            const AtmosphereSample gas = m_atmosphere->field.Sample(
                m_physics.GetTransform(c.handle).position, m_atmosphere->frame);
            if (gas.temperatureKelvin > 0.0f) initialTemperature = gas.temperatureKelvin;
        }
        m_combustion.AddBody(c.handle, fuel, initialTemperature);
    }

    // Pairwise Newtonian set: every dynamic celestial body, plus a vehicle
    // that samples local gravity (the classic scene's spacecraft).
    m_celestialParticipants = dynamicCelestials;
    if (m_vehicle && m_vehicle->component.gravity == SceneVehicleGravity::Local &&
        !dynamicCelestials.empty()) {
        m_celestialParticipants.push_back(m_vehicle->handle);
    }
    m_celestial = std::make_unique<CelestialGravity>(m_celestialParticipants);

    if (m_assets && !m_fluidVolumes.empty() && m_assets->GetRenderer()) {
        m_fluidMesh = m_assets->GetRenderer()->CreateMesh(MeshData{});
    }
    PopulateFluid();
    return true;
}

void RuntimeWorld::PopulateFluid() {
    m_fluid->Clear();
    m_emittedParticles = 0;
    for (const FluidVolume& fv : m_fluidVolumes) {
        const SceneFluidVolumeComponent& c = fv.component;
        const float s = c.spacing;
        // Lattice centred in local X/Z, growing along local +Y from the
        // object's position — the arrangement both the M24 cup and the M25
        // lake authored.
        const float x0 = -0.5f * static_cast<float>(c.countX - 1);
        const float z0 = -0.5f * static_cast<float>(c.countZ - 1);
        for (int y = 0; y < c.countY; ++y) {
            for (int z = 0; z < c.countZ; ++z) {
                for (int x = 0; x < c.countX; ++x) {
                    const glm::vec3 local((x0 + static_cast<float>(x)) * s,
                                          static_cast<float>(y) * s,
                                          (z0 + static_cast<float>(z)) * s);
                    m_fluid->AddParticle(fv.position + fv.rotation * local, glm::vec3(0.0f),
                                         fv.particleMass);
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
        // The M25 nine-column drip pattern, in the volume's local frame.
        const int column = static_cast<int>(m_emittedParticles % 9);
        const glm::vec3 spread(static_cast<float>(column % 3 - 1) * 0.3f, 0.0f,
                               static_cast<float>(column / 3 - 1) * 0.3f);
        m_fluid->AddParticle(fv.position + fv.rotation * (c.emitterLocalOffset + spread),
                             glm::vec3(0.0f), fv.particleMass);
        ++m_emittedParticles;
        return true;
    }
    return false;
}

void RuntimeWorld::RestoreAuthoredState() {
    if (!m_built) return;
    for (std::size_t i = 0; i < m_dynamicBodies.size(); ++i) {
        m_dynamicBodies[i].ResetToSpawn(m_physics);
        m_physics.SetLinearVelocity(m_dynamicBodies[i].Handle(), m_dynamicVisuals[i].initialLinearVelocity);
    }
    m_combustion.Reset();
    PopulateFluid();
}

std::string RuntimeWorld::NameOfBody(BodyHandle handle) const {
    for (const DynamicVisual& v : m_dynamicVisuals) {
        if (m_dynamicBodies[&v - m_dynamicVisuals.data()].Handle().id == handle.id) return v.name;
    }
    return std::string();
}

void RuntimeWorld::Destroy() {
    if (!m_built) return;
    for (Door& door : m_doors) door.Destroy(m_physics);
    for (const DynamicBody& body : m_dynamicBodies) m_physics.DestroyBody(body.Handle());
    for (const StaticBody& body : m_staticBodies) m_physics.DestroyBody(body.handle);
    for (const Terrain& terrain : m_terrains) m_physics.DestroyBody(terrain.handle);
    if (m_assets && m_assets->GetRenderer() && m_fluidMesh.IsValid()) {
        m_assets->GetRenderer()->DestroyMesh(m_fluidMesh);
    }
    m_fluidMesh = MeshHandle{};
    m_physics.Shutdown();

    m_doors.clear();
    m_lightSwitches.clear();
    m_dynamicBodies.clear();
    m_dynamicVisuals.clear();
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
