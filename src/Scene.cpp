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
    std::vector<SceneObjectId> ids{id};
    for(size_t i=0;i<ids.size();++i)for(const auto& o:m_objects)if(o.parent==ids[i])ids.push_back(o.id);
    m_objects.erase(std::remove_if(m_objects.begin(),m_objects.end(),[&](const auto& o){return std::find(ids.begin(),ids.end(),o.id)!=ids.end();}),m_objects.end());
    for(auto& object:m_objects)if(object.joint&&(std::find(ids.begin(),ids.end(),object.joint->bodyA)!=ids.end()||std::find(ids.begin(),ids.end(),object.joint->bodyB)!=ids.end()))object.joint.reset();
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
    if(a.authoringFolder!=b.authoringFolder)return false;
    if(DeformableProperties(a)!=DeformableProperties(b))return false;
    if(NavigationProperties(a)!=NavigationProperties(b))return false;
    if(a.scripts!=b.scripts)return false;
    if(!OptEq(a.characterMotor,b.characterMotor,[](const auto& x,const auto& y){return x.enabled==y.enabled&&x.radius==y.radius&&x.halfHeight==y.halfHeight&&x.offset==y.offset&&x.stepHeight==y.stepHeight&&x.supportDistance==y.supportDistance&&x.skin==y.skin&&x.maxSlopeDegrees==y.maxSlopeDegrees&&x.gravityScale==y.gravityScale&&x.reorientationDegreesPerSecond==y.reorientationDegreesPerSecond&&x.interactionMass==y.interactionMass&&x.maxPushImpulse==y.maxPushImpulse&&x.collisionLayer==y.collisionLayer&&x.collisionMask==y.collisionMask&&x.requiredTags==y.requiredTags&&x.excludedTags==y.excludedTags;}))return false;
    if(!OptEq(a.ragdoll,b.ragdoll,[](const auto& x,const auto& y){return RagdollDefinitionsEqual(x,y);}))return false;
    if(!OptEq(a.animation,b.animation,[](const auto& x,const auto& y){return x.enabled==y.enabled&&x.playOnStart==y.playOnStart&&x.loop==y.loop&&x.clip==y.clip&&x.speed==y.speed&&x.time==y.time&&x.layers==y.layers&&x.fullBodyIK.has_value()==y.fullBodyIK.has_value()&&(!x.fullBodyIK||SerializeFullBodyIKSettings(*x.fullBodyIK)==SerializeFullBodyIKSettings(*y.fullBodyIK));}))return false;
    if(!OptEq(a.joint,b.joint,[](const auto& x,const auto& y){return x.bodyA==y.bodyA&&x.bodyB==y.bodyB&&JointSettingsEqual(x.settings,y.settings);}))return false;
    if(!OptEq(a.ui,b.ui,[](const auto& x,const auto& y){return x.asset==y.asset&&x.name==y.name&&x.enabled==y.enabled;}))return false;
    if (a.tags != b.tags || a.renderLayer != b.renderLayer) return false;
    if (a.id != b.id || a.name != b.name || a.parent != b.parent ||
        a.prefabAsset != b.prefabAsset || a.prefabRoot != b.prefabRoot ||
        a.prefabSource != b.prefabSource || a.prefabIds != b.prefabIds ||
        a.prefabOverrides != b.prefabOverrides) return false;
    if(!OptEq(a.particleEmitter,b.particleEmitter,ParticleSettingsEqual))return false;
    if (!Eq(a.transform.position, b.transform.position) ||
        !Eq(a.transform.rotation, b.transform.rotation) ||
        !Eq(a.transform.scale, b.transform.scale)) {
        return false;
    }
    if (!OptEq(a.render, b.render, [](const SceneRenderComponent& x, const SceneRenderComponent& y) {
            return x.shape == y.shape && Eq(x.halfExtents, y.halfExtents) && x.radius == y.radius &&
                   Eq(x.color, y.color) && x.alpha == y.alpha &&
                   Eq(x.secondaryColor, y.secondaryColor) && x.secondaryAlpha == y.secondaryAlpha &&
                   x.meshAsset == y.meshAsset && x.textureAsset == y.textureAsset && x.textureCamera == y.textureCamera && EncodeMaterialSlots(x.materials)==EncodeMaterialSlots(y.materials);
        })) {
        return false;
    }
    if (!OptEq(a.audioEmitter,b.audioEmitter,[](const auto& x,const auto& y){
        return x.asset==y.asset&&x.enabled==y.enabled&&x.playOnStart==y.playOnStart&&x.loop==y.loop&&x.spatial==y.spatial&&x.volume==y.volume&&x.pitch==y.pitch&&x.referenceDistance==y.referenceDistance&&x.maximumDistance==y.maximumDistance&&x.rolloff==y.rolloff&&x.attenuation==y.attenuation;
    }) || !OptEq(a.audioListener,b.audioListener,[](const auto& x,const auto& y){return x.enabled==y.enabled&&x.followActiveView==y.followActiveView;})) return false;
    if (!OptEq(a.body, b.body, [](const SceneBodyComponent& x, const SceneBodyComponent& y) {
            if (x.sensor!=y.sensor || x.enabled!=y.enabled || x.collisionLayer != y.collisionLayer || x.collisionMask != y.collisionMask) return false;
            if (x.compoundBoxes.size() != y.compoundBoxes.size()) return false;
            for (std::size_t i = 0; i < x.compoundBoxes.size(); ++i) {
                if (!Eq(x.compoundBoxes[i].localCenter, y.compoundBoxes[i].localCenter) ||
                    !Eq(x.compoundBoxes[i].halfExtents, y.compoundBoxes[i].halfExtents)) {
                    return false;
                }
            }
            if (x.fluidCavities.size() != y.fluidCavities.size()) return false;
            for (std::size_t i = 0; i < x.fluidCavities.size(); ++i) {
                if (!Eq(x.fluidCavities[i].localCenter, y.fluidCavities[i].localCenter) ||
                    !Eq(x.fluidCavities[i].halfExtents, y.fluidCavities[i].halfExtents)) return false;
            }
            return x.motion == y.motion && x.shape == y.shape && Eq(x.halfExtents, y.halfExtents) &&
                   x.radius == y.radius && x.terrainSurface == y.terrainSurface &&
                   x.mass == y.mass && x.friction == y.friction &&
                   x.restitution == y.restitution &&
                   Eq(x.initialLinearVelocity, y.initialLinearVelocity) &&
                   Eq(x.initialAngularVelocity, y.initialAngularVelocity) && x.pickable == y.pickable &&
                   x.managed == y.managed;
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
            return Eq(x.localHingeAxis, y.localHingeAxis) && x.collisionLayer == y.collisionLayer && x.collisionMask == y.collisionMask && x.openAngleDegrees == y.openAngleDegrees &&
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
                   return x.yawDegrees == y.yawDegrees && x.view == y.view &&
                          x.density == y.density && x.fluidDrag == y.fluidDrag &&
                          x.swimAcceleration == y.swimAcceleration && x.collisionLayer == y.collisionLayer && x.collisionMask == y.collisionMask;
               })) {
        return false;
    }
    if (!OptEq(a.renderCamera, b.renderCamera, [](const SceneRenderCameraComponent& x, const SceneRenderCameraComponent& y) {
        return x.enabled == y.enabled && x.width == y.width && x.height == y.height &&
            x.updateEveryFrames == y.updateEveryFrames && x.verticalFovDegrees == y.verticalFovDegrees &&
            x.nearPlane == y.nearPlane && x.farPlane == y.farPlane && x.renderMask == y.renderMask;
    })) return false;
    return true;
}

bool ScenesEqual(const Scene& a, const Scene& b) {
    const SceneSettings& sa = a.Settings();
    const SceneSettings& sb = b.Settings();
    if(sa.mainCameraRenderMask!=sb.mainCameraRenderMask)return false;
    if(sa.authoringRecipes!=sb.authoringRecipes)return false;
    if (sa.name != sb.name || !Eq(sa.worldOrigin, sb.worldOrigin) ||
        !Eq(sa.sunDirection, sb.sunDirection) || !Eq(sa.sunColor, sb.sunColor) ||
        !Eq(sa.ambientColor, sb.ambientColor) || sa.fluidScale != sb.fluidScale ||
        sa.fluidUpdateRateHz != sb.fluidUpdateRateHz ||
        sa.fluidHydrostaticDragRate != sb.fluidHydrostaticDragRate ||
        sa.fidelityPolicy != sb.fidelityPolicy || sa.fidelityFullRadius != sb.fidelityFullRadius ||
        sa.fidelityCoarseRadius != sb.fidelityCoarseRadius) {
        return false;
    }
    if(sa.linearRendering!=sb.linearRendering||sa.exposure!=sb.exposure||sa.environmentAsset!=sb.environmentAsset||sa.environmentIntensity!=sb.environmentIntensity||sa.environmentRotation!=sb.environmentRotation||sa.environmentBackground!=sb.environmentBackground)return false;
    if (a.Objects().size() != b.Objects().size()) return false;
    for (std::size_t i = 0; i < a.Objects().size(); ++i) {
        if (!SceneObjectsEqual(a.Objects()[i], b.Objects()[i])) return false;
    }
    return true;
}
