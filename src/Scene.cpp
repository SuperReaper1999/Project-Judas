#include "Scene.h"

#include <algorithm>

SceneObject& Scene::CreateObject(const std::string& name) {
    SceneObject object;
    object.id = m_nextId++;
    object.name = name;
    m_objects.push_back(object);
    return m_objects.back();
}

bool Scene::InsertObject(const SceneObject& object) {
    if (object.id == kInvalidSceneObjectId || Find(object.id) != nullptr) return false;
    m_objects.push_back(object);
    if (object.id >= m_nextId) m_nextId = object.id + 1;
    return true;
}

bool Scene::DestroyObject(SceneObjectId id) {
    const auto it = std::find_if(m_objects.begin(), m_objects.end(),
                                 [id](const SceneObject& o) { return o.id == id; });
    if (it == m_objects.end()) return false;
    m_objects.erase(it);
    return true;
}

SceneObject* Scene::Find(SceneObjectId id) {
    for (SceneObject& object : m_objects) {
        if (object.id == id) return &object;
    }
    return nullptr;
}

const SceneObject* Scene::Find(SceneObjectId id) const {
    for (const SceneObject& object : m_objects) {
        if (object.id == id) return &object;
    }
    return nullptr;
}

bool Scene::MoveObject(SceneObjectId id, int delta) {
    const auto it = std::find_if(m_objects.begin(), m_objects.end(),
                                 [id](const SceneObject& o) { return o.id == id; });
    if (it == m_objects.end()) return false;
    const std::ptrdiff_t index = it - m_objects.begin();
    const std::ptrdiff_t target = index + delta;
    if (target < 0 || target >= static_cast<std::ptrdiff_t>(m_objects.size()) || delta == 0) {
        return false;
    }
    std::swap(m_objects[static_cast<std::size_t>(index)],
              m_objects[static_cast<std::size_t>(target)]);
    return true;
}

void Scene::SetNextId(SceneObjectId nextId) {
    m_nextId = std::max<SceneObjectId>(nextId, 1);
    for (const SceneObject& object : m_objects) {
        if (object.id >= m_nextId) m_nextId = object.id + 1;
    }
}

void Scene::Clear() {
    m_settings = SceneSettings{};
    m_objects.clear();
    m_nextId = 1;
}

namespace {
bool Eq(const glm::vec3& a, const glm::vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool Eq(const glm::dvec3& a, const glm::dvec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool Eq(const glm::quat& a, const glm::quat& b) {
    return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}

template <typename T, typename F>
bool OptEq(const std::optional<T>& a, const std::optional<T>& b, F&& equal) {
    if (a.has_value() != b.has_value()) return false;
    return !a.has_value() || equal(*a, *b);
}
}  // namespace

bool SceneObjectsEqual(const SceneObject& a, const SceneObject& b) {
    if (a.id != b.id || a.name != b.name) return false;
    if (!Eq(a.transform.position, b.transform.position) ||
        !Eq(a.transform.rotation, b.transform.rotation) ||
        !Eq(a.transform.scale, b.transform.scale)) {
        return false;
    }
    if (!OptEq(a.render, b.render, [](const SceneRenderComponent& x, const SceneRenderComponent& y) {
            return x.shape == y.shape && Eq(x.halfExtents, y.halfExtents) && x.radius == y.radius &&
                   Eq(x.color, y.color) && x.alpha == y.alpha &&
                   Eq(x.secondaryColor, y.secondaryColor) && x.secondaryAlpha == y.secondaryAlpha &&
                   x.meshPath == y.meshPath && x.texturePath == y.texturePath;
        })) {
        return false;
    }
    if (!OptEq(a.body, b.body, [](const SceneBodyComponent& x, const SceneBodyComponent& y) {
            if (x.compoundBoxes.size() != y.compoundBoxes.size()) return false;
            for (std::size_t i = 0; i < x.compoundBoxes.size(); ++i) {
                if (!Eq(x.compoundBoxes[i].localCenter, y.compoundBoxes[i].localCenter) ||
                    !Eq(x.compoundBoxes[i].halfExtents, y.compoundBoxes[i].halfExtents)) {
                    return false;
                }
            }
            return x.motion == y.motion && x.shape == y.shape && Eq(x.halfExtents, y.halfExtents) &&
                   x.radius == y.radius && x.terrainSurface == y.terrainSurface &&
                   x.mass == y.mass && x.friction == y.friction &&
                   x.restitution == y.restitution &&
                   Eq(x.initialLinearVelocity, y.initialLinearVelocity) && x.pickable == y.pickable;
        })) {
        return false;
    }
    if (!OptEq(a.gravity, b.gravity, [](const SceneGravityComponent& x, const SceneGravityComponent& y) {
            return x.kind == y.kind && x.magnitude == y.magnitude &&
                   x.regionShape == y.regionShape && x.regionRadius == y.regionRadius &&
                   Eq(x.regionHalfExtents, y.regionHalfExtents);
        })) {
        return false;
    }
    if (!OptEq(a.light, b.light, [](const SceneLightComponent& x, const SceneLightComponent& y) {
            return x.kind == y.kind && Eq(x.color, y.color) && x.range == y.range &&
                   x.innerConeDegrees == y.innerConeDegrees &&
                   x.outerConeDegrees == y.outerConeDegrees;
        })) {
        return false;
    }
    if (!OptEq(a.door, b.door, [](const SceneDoorComponent& x, const SceneDoorComponent& y) {
            return Eq(x.localHingeAxis, y.localHingeAxis) && x.openAngleDegrees == y.openAngleDegrees &&
                   x.angularSpeedDegreesPerSecond == y.angularSpeedDegreesPerSecond;
        })) {
        return false;
    }
    if (!OptEq(a.lightSwitch, b.lightSwitch,
               [](const SceneLightSwitchComponent& x, const SceneLightSwitchComponent& y) {
                   return Eq(x.localHingeAxis, y.localHingeAxis) &&
                          x.toggleAngleDegrees == y.toggleAngleDegrees &&
                          x.angularSpeedDegreesPerSecond == y.angularSpeedDegreesPerSecond &&
                          Eq(x.lampLocalOffset, y.lampLocalOffset) && Eq(x.lampColor, y.lampColor) &&
                          x.lampRange == y.lampRange;
               })) {
        return false;
    }
    if (!OptEq(a.vehicle, b.vehicle, [](const SceneVehicleComponent& x, const SceneVehicleComponent& y) {
            return x.gravity == y.gravity && x.headlight == y.headlight &&
                   x.navigationLights == y.navigationLights &&
                   x.dragCoefficient == y.dragCoefficient &&
                   x.initialPilotAttached == y.initialPilotAttached;
        })) {
        return false;
    }
    if (!OptEq(a.celestial, b.celestial,
               [](const SceneCelestialComponent& x, const SceneCelestialComponent& y) {
                   return x.gravitationalParameter == y.gravitationalParameter &&
                          x.operatorThrustForce == y.operatorThrustForce;
               })) {
        return false;
    }
    if (!OptEq(a.atmosphere, b.atmosphere,
               [](const SceneAtmosphereComponent& x, const SceneAtmosphereComponent& y) {
                   return x.referenceRadius == y.referenceRadius && x.topRadius == y.topRadius &&
                          x.referenceDensity == y.referenceDensity &&
                          x.polytropicExponent == y.polytropicExponent &&
                          x.oxidizerMassFraction == y.oxidizerMassFraction &&
                          x.referenceTemperatureKelvin == y.referenceTemperatureKelvin;
               })) {
        return false;
    }
    if (!OptEq(a.combustible, b.combustible,
               [](const SceneCombustibleComponent& x, const SceneCombustibleComponent& y) {
                   return x.heatCapacityJPerK == y.heatCapacityJPerK &&
                          x.initialFuelMassKg == y.initialFuelMassKg &&
                          x.ignitionTemperatureK == y.ignitionTemperatureK &&
                          x.maximumFuelRateKgPerSecond == y.maximumFuelRateKgPerSecond &&
                          x.radiativeAreaSquareMeters == y.radiativeAreaSquareMeters &&
                          x.retainedCombustionHeatFraction == y.retainedCombustionHeatFraction;
               })) {
        return false;
    }
    if (!OptEq(a.fluidVolume, b.fluidVolume,
               [](const SceneFluidVolumeComponent& x, const SceneFluidVolumeComponent& y) {
                   return x.spacing == y.spacing && x.countX == y.countX && x.countY == y.countY &&
                          x.countZ == y.countZ && x.emitter == y.emitter &&
                          Eq(x.emitterLocalOffset, y.emitterLocalOffset) &&
                          x.maxParticles == y.maxParticles;
               })) {
        return false;
    }
    if (!OptEq(a.playerStart, b.playerStart,
               [](const ScenePlayerStartComponent& x, const ScenePlayerStartComponent& y) {
                   return x.yawDegrees == y.yawDegrees && x.view == y.view;
               })) {
        return false;
    }
    return true;
}

bool ScenesEqual(const Scene& a, const Scene& b) {
    const SceneSettings& sa = a.Settings();
    const SceneSettings& sb = b.Settings();
    if (sa.name != sb.name || !Eq(sa.worldOrigin, sb.worldOrigin) ||
        !Eq(sa.sunDirection, sb.sunDirection) || !Eq(sa.sunColor, sb.sunColor) ||
        !Eq(sa.ambientColor, sb.ambientColor) || sa.fluidScale != sb.fluidScale) {
        return false;
    }
    if (a.Objects().size() != b.Objects().size()) return false;
    for (std::size_t i = 0; i < a.Objects().size(); ++i) {
        if (!SceneObjectsEqual(a.Objects()[i], b.Objects()[i])) return false;
    }
    return true;
}
