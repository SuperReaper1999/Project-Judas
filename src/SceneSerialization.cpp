#include <set>
#include <charconv>
#include "SceneSerialization.h"
#include "NamedAuthoring.h"
#include "AuthoringNumeric.h"
#include "../third_party/nlohmann/json.hpp"
#include "Prefab.h"
#include "AssetDatabase.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "Scene.h"

namespace {
const char* B(bool value) { return value ? "true" : "false"; }

// --- Writing ---------------------------------------------------------------

// The shortest decimal that parses back to exactly the same float, so a
// file reads "9.81" rather than "9.81000042" while still round-tripping
// bit-exactly (9 significant digits always suffice for a float, 17 for a
// double).
std::string F(float v) {
    AuthoringNumericLocale numericLocale;
    char buffer[64];
    for (int precision = 1; precision <= 9; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(v));
        // Prefer plain notation ("20", not "2e+01") while a short form exists.
        if (std::strtof(buffer, nullptr) == v && (precision == 9 || std::strchr(buffer, 'e') == nullptr)) break;
    }
    return buffer;
}
std::string D(double v) {
    AuthoringNumericLocale numericLocale;
    char buffer[64];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, v);
        if (std::strtod(buffer, nullptr) == v && (precision == 17 || std::strchr(buffer, 'e') == nullptr)) break;
    }
    return buffer;
}
std::string V(const glm::vec3& v) { return F(v.x) + " " + F(v.y) + " " + F(v.z); }
std::string DV(const glm::dvec3& v) { return D(v.x) + " " + D(v.y) + " " + D(v.z); }
std::string Q(const glm::quat& q) { return F(q.w) + " " + F(q.x) + " " + F(q.y) + " " + F(q.z); }
std::string Quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

const char* ShapeName(SceneShape s) {
    switch (s) {
        case SceneShape::Box: return "box";
        case SceneShape::Sphere: return "sphere";
        case SceneShape::Compound: return "compound";
        case SceneShape::Mesh: return "mesh";
        case SceneShape::Terrain: return "terrain";
        case SceneShape::ConvexHull: return "hull";
        case SceneShape::TriangleMesh: return "triangle-mesh";
    }
    return "box";
}

class Writer {
public:
    void Line(const std::string& key, const std::string& value) {
        m_out += "  " + key + " " + value + "\n";
    }
    void Raw(const std::string& s) { m_out += s; }
    std::string Take() { return std::move(m_out); }

private:
    std::string m_out;
};

void WriteObject(Writer& w, const SceneObject& o) {
    w.Raw("object " + std::to_string(o.id) + " " + Quote(o.name) + "\n");
    if (!o.authoringFolder.empty()) w.Line("authoring-folder",Quote(o.authoringFolder));
    if (o.parent) w.Line("parent", std::to_string(o.parent));
    if (o.prefabRoot) {
        w.Line("prefab.asset", Quote(o.prefabAsset));
        w.Line("prefab.root", std::to_string(o.prefabRoot));
        w.Line("prefab.source", std::to_string(o.prefabSource));
        PrefabProperties ids;
        for (const auto& pair : o.prefabIds) ids[std::to_string(pair.first)] = std::to_string(pair.second);
        w.Line("prefab.ids", Quote(EncodePrefabOverrides(ids)));
        w.Line("prefab.overrides", Quote(EncodePrefabOverrides(o.prefabOverrides)));
    }
    for(const auto& [key,value]:DeformableProperties(o))w.Line(key,Quote(value));
    for(const auto& [key,value]:LiquidProperties(o))w.Line(key,Quote(value));
    for(const auto& [key,value]:NavigationProperties(o))w.Line(key,Quote(value));
    if(o.characterMotor){const auto& m=*o.characterMotor;
        w.Line("motor.enabled",B(m.enabled));
        w.Line("motor.radius",F(m.radius));
        w.Line("motor.halfHeight",F(m.halfHeight));
        w.Line("motor.offset",V(m.offset));
        w.Line("motor.stepHeight",F(m.stepHeight));
        w.Line("motor.supportDistance",F(m.supportDistance));
        w.Line("motor.skin",F(m.skin));
        w.Line("motor.maxSlopeDegrees",F(m.maxSlopeDegrees));
        w.Line("motor.gravityScale",F(m.gravityScale));
        w.Line("motor.reorientationDegreesPerSecond",F(m.reorientationDegreesPerSecond));
        w.Line("motor.interactionMass",F(m.interactionMass));
        w.Line("motor.maxPushImpulse",F(m.maxPushImpulse));
        w.Line("motor.collisionLayer",std::to_string(m.collisionLayer));
        w.Line("motor.collisionMask",std::to_string(m.collisionMask));
        w.Line("motor.requiredTags",std::to_string(m.requiredTags));
        w.Line("motor.excludedTags",std::to_string(m.excludedTags));
    }
    if(o.animation){const auto& a=*o.animation;w.Line("animation.enabled",B(a.enabled));w.Line("animation.play-on-start",B(a.playOnStart));w.Line("animation.loop",B(a.loop));w.Line("animation.clip",Quote(a.clip));w.Line("animation.speed",F(a.speed));w.Line("animation.time",F(a.time));
        if(!a.layers.empty()){w.Line("animation.layers",std::to_string(a.layers.size()));for(size_t i=0;i<a.layers.size();++i){const auto& l=a.layers[i];auto key="animation.layer."+std::to_string(i)+".";
            w.Line(key+"id",Quote(l.id));w.Line(key+"clip",Quote(l.clip));w.Line(key+"enabled",B(l.enabled));w.Line(key+"additive",B(l.additive));w.Line(key+"weight",F(l.weight));w.Line(key+"speed",F(l.speed));w.Line(key+"time",F(l.time));w.Line(key+"reference-clip",Quote(l.referenceClip));w.Line(key+"reference-time",F(l.referenceTime));w.Line(key+"mask-count",std::to_string(l.mask.size()));for(size_t n=0;n<l.mask.size();++n)w.Line(key+"mask."+std::to_string(n),Quote(l.mask[n]));}}
    }
    if(o.body&&!o.body->physicalMaterial.empty())w.Line("body.physical-material",Quote(o.body->physicalMaterial));
    if(o.body&&o.body->physicalMaterialOverride)w.Line("body.physical-material-override","true");
    if(o.socket){const auto& k=*o.socket;w.Line("socket.target",std::to_string(k.target));w.Line("socket.joint",Quote(k.joint));w.Line("socket.enabled",B(k.enabled));w.Line("socket.position",V(k.offset.position));w.Line("socket.rotation",Q(k.offset.rotation));w.Line("socket.scale",V(k.offset.scale));}
    if(o.animation&&!o.animation->limbs.empty()){w.Line("animation.limbs",std::to_string(o.animation->limbs.size()));for(size_t i=0;i<o.animation->limbs.size();++i){const auto& k=o.animation->limbs[i];auto key="animation.limb."+std::to_string(i)+".";w.Line(key+"id",Quote(k.id));w.Line(key+"root",Quote(k.root));w.Line(key+"middle",Quote(k.middle));w.Line(key+"end",Quote(k.end));w.Line(key+"target",V(k.target));w.Line(key+"pole",V(k.pole));w.Line(key+"weight",F(k.weight));w.Line(key+"enabled",B(k.enabled));w.Line(key+"order",std::to_string(k.order));}}
    if(o.animation&&o.animation->fullBodyIK)w.Line("animation.full-body",Quote(SerializeFullBodyIKSettings(*o.animation->fullBodyIK)));
    if(o.ragdoll&&o.ragdoll->physicalAnimation)w.Line("ragdoll.physical-animation",Quote(SerializePhysicalAnimationSettings(*o.ragdoll->physicalAnimation)));
    if(o.ragdoll&&o.ragdoll->receiveContactEvents)w.Line("ragdoll.receive-contact-events","true");
    if(o.ragdoll){const auto& r=*o.ragdoll;w.Line("ragdoll.enabled",B(r.enabled));w.Line("ragdoll.play-on-start",B(r.playOnStart));w.Line("ragdoll.self-collision",B(r.selfCollision));w.Line("ragdoll.bones",std::to_string(r.bones.size()));
        for(size_t i=0;i<r.bones.size();++i){const auto& b=r.bones[i];const auto& c=b.constraint;auto k="ragdoll.bone."+std::to_string(i)+".";
            w.Line(k+"joint",Quote(b.joint));w.Line(k+"parent",Quote(b.parent));w.Line(k+"shape",std::to_string(int(b.shape)));w.Line(k+"offset",V(b.offset));w.Line(k+"orientation",Q(b.orientation));w.Line(k+"half-extents",V(b.halfExtents));w.Line(k+"radius",F(b.radius));w.Line(k+"mass",F(b.mass));w.Line(k+"friction",F(b.friction));w.Line(k+"restitution",F(b.restitution));w.Line(k+"layer",std::to_string(b.collisionLayer));w.Line(k+"mask",std::to_string(b.collisionMask));w.Line(k+"suppress-parent",B(b.suppressParentCollision));w.Line(k+"auto-anchors",B(b.autoAnchors));
            w.Line(k+"constraint",std::to_string(int(c.type)));w.Line(k+"enabled",B(c.enabled));w.Line(k+"anchor-a",V(c.anchorA));w.Line(k+"anchor-b",V(c.anchorB));w.Line(k+"frame-a",Q(c.frameA));w.Line(k+"frame-b",Q(c.frameB));w.Line(k+"limits",B(c.limits));w.Line(k+"lower",F(c.lower));w.Line(k+"upper",F(c.upper));if(c.rotationalResistance)w.Line(k+"rotational-resistance",F(c.rotationalResistance));
        }
    }
    if(o.joint){const auto& j=*o.joint;const auto& s=j.settings;
        w.Line("joint",std::to_string(int(s.type)));w.Line("joint.body-a",std::to_string(j.bodyA));w.Line("joint.body-b",std::to_string(j.bodyB));
        w.Line("joint.anchor-a",V(s.anchorA));w.Line("joint.anchor-b",V(s.anchorB));w.Line("joint.frame-a",Q(s.frameA));w.Line("joint.frame-b",Q(s.frameB));
        w.Line("joint.enabled",B(s.enabled));w.Line("joint.limits",B(s.limits));w.Line("joint.motor",B(s.motor));w.Line("joint.spring",B(s.spring));
        w.Line("joint.lower",F(s.lower));w.Line("joint.upper",F(s.upper));w.Line("joint.speed",F(s.speed));w.Line("joint.max-force",F(s.maxForce));
        w.Line("joint.rest",F(s.rest));w.Line("joint.stiffness",F(s.stiffness));w.Line("joint.damping",F(s.damping));if(s.rotationalResistance)w.Line("joint.rotational-resistance",F(s.rotationalResistance));
    }
    if(o.ui){w.Line("ui.asset",Quote(o.ui->asset));w.Line("ui.name",Quote(o.ui->name));w.Line("ui.enabled",B(o.ui->enabled));}
    if(!o.scripts.empty()) {
        w.Line("scripts",std::to_string(o.scripts.size()));
        for(size_t i=0;i<o.scripts.size();++i){const auto& slot=o.scripts[i];auto key="script."+std::to_string(i)+".";
            w.Line(key+"id",std::to_string(slot.id));w.Line(key+"asset",Quote(slot.asset));
            w.Line(key+"enabled",B(slot.enabled));w.Line(key+"properties",Quote(slot.properties));
        }
    }
    if(o.tags) w.Line("tags", std::to_string(o.tags));
    if(o.renderLayer) w.Line("render-layer", std::to_string(o.renderLayer));
    w.Line("position", V(o.transform.position));
    w.Line("rotation", Q(o.transform.rotation));
    w.Line("scale", V(o.transform.scale));
    if (o.render) {
        const SceneRenderComponent& r = *o.render;
        w.Line("render", ShapeName(r.shape));
        w.Line("render.half-extents", V(r.halfExtents));
        w.Line("render.radius", F(r.radius));
        w.Line("render.color", V(r.color));
        w.Line("render.alpha", F(r.alpha));
        w.Line("render.secondary-color", V(r.secondaryColor));
        w.Line("render.secondary-alpha", F(r.secondaryAlpha));
        w.Line("render.mesh-asset", Quote(r.meshAsset));
        w.Line("render.texture-asset", Quote(r.textureAsset));
        if(!r.hiddenParts.empty()){w.Line("render.hidden-part-count",std::to_string(r.hiddenParts.size()));for(size_t i=0;i<r.hiddenParts.size();++i)w.Line("render.hidden-part-"+std::to_string(i),Quote(r.hiddenParts[i]));}
        if(!r.materials.empty())w.Line("render.materials",Quote(EncodeMaterialSlots(r.materials)));
        if (r.textureCamera) w.Line("render.texture-camera", std::to_string(r.textureCamera));
    }
    if(o.particleEmitter){const auto& e=*o.particleEmitter;w.Line("particle-emitter", "");
        w.Line("particle.enabled",B(e.enabled));
        w.Line("particle.loop",B(e.loop));
        w.Line("particle.local-space",B(e.localSpace));
        w.Line("particle.gravity",B(e.useGravity));
        w.Line("particle.rate",F(e.rate));
        w.Line("particle.lifetime",F(e.lifetime));
        w.Line("particle.size",F(e.size));
        w.Line("particle.end-size",F(e.endSize));
        w.Line("particle.burst",std::to_string(e.burst));
        w.Line("particle.capacity",std::to_string(e.maxParticles));
        w.Line("particle.spread",V(e.spread));
        w.Line("particle.velocity",V(e.velocity));
        w.Line("particle.variation",V(e.velocityVariation));
        w.Line("particle.acceleration",V(e.acceleration));
        w.Line("particle.texture",Quote(e.textureAsset));
        w.Line("particle.color",V(glm::vec3(e.color)));w.Line("particle.alpha",F(e.color.a));
        w.Line("particle.end-color",V(glm::vec3(e.endColor)));w.Line("particle.end-alpha",F(e.endColor.a));
        w.Line("particle.seed",std::to_string(e.seed));
    }
    if (o.audioEmitter) {
        const auto& a=*o.audioEmitter;w.Line("audio-emitter", "");
        w.Line("audio.asset",Quote(a.asset));w.Line("audio.enabled",B(a.enabled));w.Line("audio.play-on-start",B(a.playOnStart));
        w.Line("audio.loop",B(a.loop));w.Line("audio.spatial",B(a.spatial));w.Line("audio.volume",F(a.volume));w.Line("audio.pitch",F(a.pitch));
        w.Line("audio.reference-distance",F(a.referenceDistance));w.Line("audio.maximum-distance",F(a.maximumDistance));w.Line("audio.rolloff",F(a.rolloff));
        w.Line("audio.attenuation",a.attenuation==AudioAttenuation::Inverse?"inverse":a.attenuation==AudioAttenuation::Linear?"linear":"none");
        if(a.loading!=AudioLoading::Buffered)w.Line("audio.loading","streamed");
        if(a.streamPageFrames!=4096)w.Line("audio.page-frames",std::to_string(a.streamPageFrames));
        if(!a.group.empty())w.Line("audio.group",Quote(a.group));
        if(a.doppler!=0)w.Line("audio.doppler",F(a.doppler));
        if(a.send!=0)w.Line("audio.send",F(a.send));
        if(a.occlusion)w.Line("audio.occlusion",B(a.occlusion));
        if(a.bypass)w.Line("audio.bypass",B(a.bypass));
        if(a.occlusionMask!=kAllCategories)w.Line("audio.occlusion-mask",std::to_string(a.occlusionMask));
        if(a.occludedGain!=.25f)w.Line("audio.occluded-gain",F(a.occludedGain));
        if(a.occludedCutoff!=1200)w.Line("audio.occluded-cutoff",F(a.occludedCutoff));
    }
    if(o.audioZone){const auto& z=*o.audioZone;w.Line("audio-zone","");w.Line("zone.asset",Quote(z.asset));w.Line("zone.enabled",B(z.enabled));w.Line("zone.shape",z.shape==SceneRegionShape::Box?"box":"sphere");w.Line("zone.half-extents",V(z.halfExtents));w.Line("zone.radius",F(z.radius));w.Line("zone.blend",F(z.blendDistance));w.Line("zone.amount",F(z.amount));w.Line("zone.priority",std::to_string(z.priority));}
    if(o.audioListener){w.Line("audio-listener", "");w.Line("listener.enabled",B(o.audioListener->enabled));w.Line("listener.follow-view",B(o.audioListener->followActiveView));}
    if (o.renderCamera) {
        const auto& c = *o.renderCamera;
        w.Line("render-camera", "");
        if(c.renderMask!=kAllCategories) w.Line("camera.render-mask",std::to_string(c.renderMask));
        w.Line("camera.enabled", c.enabled ? "true" : "false");
        w.Line("camera.width", std::to_string(c.width));
        w.Line("camera.height", std::to_string(c.height));
        w.Line("camera.cadence", std::to_string(c.updateEveryFrames));
        w.Line("camera.fov", F(c.verticalFovDegrees));
        w.Line("camera.near", F(c.nearPlane));
        w.Line("camera.far", F(c.farPlane));
    }
    if (o.body) {
        const SceneBodyComponent& b = *o.body;
        w.Line("body", std::string(b.motion == SceneBodyMotion::Static ? "static" : b.motion == SceneBodyMotion::Kinematic ? "kinematic" : "dynamic") +
                           " " + ShapeName(b.shape));
        if(b.sensor) w.Line("body.sensor","true");
        if(!b.enabled) w.Line("body.enabled","false");
        if(b.collisionLayer) w.Line("body.collision-layer",std::to_string(b.collisionLayer));
        if(b.collisionMask!=kAllCategories) w.Line("body.collision-mask",std::to_string(b.collisionMask));
        w.Line("body.half-extents", V(b.halfExtents));
        w.Line("body.radius", F(b.radius));
        w.Line("body.terrain", Quote(b.terrainSurface));
        if(!b.collisionAsset.empty())w.Line("body.collision-asset",Quote(b.collisionAsset));
        w.Line("body.mass", F(b.mass));
        w.Line("body.friction", F(b.friction));
        w.Line("body.restitution", F(b.restitution));
        w.Line("body.initial-velocity", V(b.initialLinearVelocity));
        if(b.initialAngularVelocity!=glm::vec3(0))w.Line("body.initial-angular-velocity",V(b.initialAngularVelocity));
        w.Line("body.pickable", b.pickable ? "true" : "false");
        w.Line("body.managed", b.managed ? "true" : "false");
        w.Line("body.compound-count", std::to_string(b.compoundBoxes.size()));
        for (const CompoundBox& box : b.compoundBoxes) {
            std::string value=V(box.localCenter)+" "+V(box.halfExtents);
            if(box.rotation!=glm::quat(1,0,0,0)||box.type!=ShapeType::Box||box.key||!box.assetId.empty())value+=" "+Q(box.rotation)+" "+std::to_string(int(box.type))+" "+F(box.radius)+" "+Quote(box.assetId)+" "+std::to_string(box.key?box.key:uint32_t(&box-b.compoundBoxes.data()+1));
            w.Line("body.compound-box",value);
        }
        w.Line("body.fluid-cavity-count", std::to_string(b.fluidCavities.size()));
        for (const SceneFluidCavity& cavity : b.fluidCavities) {
            w.Line("body.fluid-cavity", V(cavity.localCenter) + " " + V(cavity.halfExtents));
        }
    }
    if (o.gravity) {
        const SceneGravityComponent& g = *o.gravity;
        w.Line("gravity", std::string(g.kind == SceneGravityKind::Radial ? "radial" : "uniform") +
                              " " + F(g.magnitude));
        w.Line("gravity.region", g.regionShape == SceneRegionShape::Sphere
                                     ? "sphere " + F(g.regionRadius)
                                     : "box " + V(g.regionHalfExtents));
    }
    if (o.light) {
        const SceneLightComponent& l = *o.light;
        w.Line("light", l.kind == SceneLightKind::Point ? "point" : "spot");
        w.Line("light.color", V(l.color));
        w.Line("light.range", F(l.range));
        w.Line("light.cone", F(l.innerConeDegrees) + " " + F(l.outerConeDegrees));
    }
    if (o.door) {
        const SceneDoorComponent& d = *o.door;
        if(d.collisionLayer)w.Line("door.collision-layer",std::to_string(d.collisionLayer));
        if(d.collisionMask!=kAllCategories)w.Line("door.collision-mask",std::to_string(d.collisionMask));
        w.Line("door", "");
        w.Line("door.hinge-axis", V(d.localHingeAxis));
        w.Line("door.open-angle", F(d.openAngleDegrees));
        w.Line("door.angular-speed", F(d.angularSpeedDegreesPerSecond));
    }
    if (o.lightSwitch) {
        const SceneLightSwitchComponent& s = *o.lightSwitch;
        w.Line("light-switch", "");
        w.Line("light-switch.hinge-axis", V(s.localHingeAxis));
        w.Line("light-switch.toggle-angle", F(s.toggleAngleDegrees));
        w.Line("light-switch.angular-speed", F(s.angularSpeedDegreesPerSecond));
        w.Line("light-switch.lamp-offset", V(s.lampLocalOffset));
        w.Line("light-switch.lamp-color", V(s.lampColor));
        w.Line("light-switch.lamp-range", F(s.lampRange));
    }
    if (o.vehicle) {
        const SceneVehicleComponent& v = *o.vehicle;
        w.Line("vehicle", v.gravity == SceneVehicleGravity::Local ? "local" : "celestial");
        w.Line("vehicle.headlight", v.headlight ? "true" : "false");
        w.Line("vehicle.navigation-lights", v.navigationLights ? "true" : "false");
        w.Line("vehicle.drag-coefficient", F(v.dragCoefficient));
        w.Line("vehicle.initial-pilot-attached", v.initialPilotAttached ? "true" : "false");
    }
    if (o.celestial) {
        const SceneCelestialComponent& c = *o.celestial;
        w.Line("celestial", "");
        w.Line("celestial.gravitational-parameter", F(c.gravitationalParameter));
        w.Line("celestial.operator-thrust", F(c.operatorThrustForce));
    }
    if (o.atmosphere) {
        const SceneAtmosphereComponent& a = *o.atmosphere;
        w.Line("atmosphere", "");
        w.Line("atmosphere.reference-radius", F(a.referenceRadius));
        w.Line("atmosphere.top-radius", F(a.topRadius));
        w.Line("atmosphere.reference-density", F(a.referenceDensity));
        w.Line("atmosphere.polytropic-exponent", F(a.polytropicExponent));
        w.Line("atmosphere.oxidizer-fraction", F(a.oxidizerMassFraction));
        w.Line("atmosphere.reference-temperature", F(a.referenceTemperatureKelvin));
    }
    if (o.combustible) {
        const SceneCombustibleComponent& c = *o.combustible;
        w.Line("combustible", "");
        w.Line("combustible.heat-capacity", F(c.heatCapacityJPerK));
        w.Line("combustible.fuel-mass", F(c.initialFuelMassKg));
        w.Line("combustible.ignition-temperature", F(c.ignitionTemperatureK));
        w.Line("combustible.max-fuel-rate", F(c.maximumFuelRateKgPerSecond));
        w.Line("combustible.radiative-area", F(c.radiativeAreaSquareMeters));
        w.Line("combustible.retained-heat", F(c.retainedCombustionHeatFraction));
    }
    if (o.fluidVolume) {
        const SceneFluidVolumeComponent& f = *o.fluidVolume;
        w.Line("fluid-volume", "");
        w.Line("fluid-volume.spacing", F(f.spacing));
        w.Line("fluid-volume.count", std::to_string(f.countX) + " " + std::to_string(f.countY) +
                                         " " + std::to_string(f.countZ));
        w.Line("fluid-volume.emitter", f.emitter ? "true" : "false");
        w.Line("fluid-volume.emitter-offset", V(f.emitterLocalOffset));
        w.Line("fluid-volume.max-particles", std::to_string(f.maxParticles));
    }
    if (o.playerStart) {
        const ScenePlayerStartComponent& p = *o.playerStart;
        if(p.collisionLayer) w.Line("player-start.collision-layer",std::to_string(p.collisionLayer));
        if(p.collisionMask!=kAllCategories) w.Line("player-start.collision-mask",std::to_string(p.collisionMask));
        w.Line("player-start", p.view == ScenePlayerView::ThirdPerson ? "third-person" : "first-person");
        w.Line("player-start.yaw", F(p.yawDegrees));
        w.Line("player-start.density", F(p.density));
        w.Line("player-start.fluid-drag", F(p.fluidDrag));
        w.Line("player-start.swim-acceleration", F(p.swimAcceleration));
    }
    w.Raw("end\n");
}

// --- Reading ---------------------------------------------------------------

struct Token {
    std::string text;
    bool quoted = false;
};

bool Tokenize(const std::string& line, std::vector<Token>& out, std::string& error) {
    out.clear();
    std::size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        if (c == '#') break;
        if (c == '"') {
            Token t;
            t.quoted = true;
            ++i;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) { t.text += line[i + 1]; i += 2; continue; }
                if (line[i] == '"') { closed = true; ++i; break; }
                t.text += line[i++];
            }
            if (!closed) { error = "unterminated string"; return false; }
            out.push_back(t);
            continue;
        }
        Token t;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '#') {
            t.text += line[i++];
        }
        out.push_back(t);
    }
    return true;
}

class Reader {
public:
    Reader(const std::string& text, std::string& error) : m_error(error) {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) m_lines.push_back(line);
    }

    bool Fail(const std::string& message) {
        m_error = "scene line " + std::to_string(m_lineNumber) + ": " + message;
        return false;
    }

    // Reads the next non-empty, non-comment line's tokens. Returns false at
    // end of input (with no error set).
    bool Next(std::vector<Token>& tokens) {
        while (m_index < m_lines.size()) {
            m_lineNumber = m_index + 1;
            const std::string& line = m_lines[m_index++];
            std::string tokenError;
            if (!Tokenize(line, tokens, tokenError)) { Fail(tokenError); m_failed = true; return false; }
            if (!tokens.empty()) return true;
        }
        return false;
    }
    bool Failed() const { return m_failed; }
    void MarkFailed() { m_failed = true; }

private:
    std::vector<std::string> m_lines;
    std::size_t m_index = 0;
    std::size_t m_lineNumber = 0;
    bool m_failed = false;
    std::string& m_error;
};

bool ParseFloat(const Token& t, float& out) {
    AuthoringNumericLocale numericLocale;
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    const double value = std::strtod(t.text.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(value)) return false;
    out = static_cast<float>(value);
    return std::isfinite(out);
}
bool ParseDouble(const Token& t, double& out) {
    AuthoringNumericLocale numericLocale;
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    out = std::strtod(t.text.c_str(), &end);
    return end != nullptr && *end == '\0' && std::isfinite(out);
}
bool ParseInt(const Token& t, long long& out) {
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    out = std::strtoll(t.text.c_str(), &end, 10);
    return end != nullptr && *end == '\0';
}
bool ParseBool(const Token& t, bool& out) {
    if (t.quoted) return false;
    if (t.text == "true") { out = true; return true; }
    if (t.text == "false") { out = false; return true; }
    return false;
}
bool ParseShape(const Token& t, SceneShape& out) {
    if (t.quoted) return false;
    if (t.text == "box") out = SceneShape::Box;
    else if (t.text == "sphere") out = SceneShape::Sphere;
    else if (t.text == "compound") out = SceneShape::Compound;
    else if (t.text == "mesh") out = SceneShape::Mesh;
    else if (t.text == "terrain") out = SceneShape::Terrain;
    else if(t.text=="hull")out=SceneShape::ConvexHull;
    else if(t.text=="triangle-mesh")out=SceneShape::TriangleMesh;
    else return false;
    return true;
}

// One object block being assembled: collects `key -> tokens` and tracks
// which keys were seen so required fields can be checked after `end`.
struct Block {
    std::map<std::string, std::vector<Token>> values;
    std::vector<std::vector<Token>> compoundBoxes;
    std::vector<std::vector<Token>> fluidCavities;
    std::set<std::string> seen;
};

class ObjectParser {
public:
    ObjectParser(Reader& reader, const Block& block) : m_reader(reader), m_block(block) {}

    bool Has(const std::string& key) const { return m_block.values.count(key) != 0; }

    bool Vec3(const std::string& key, glm::vec3& out) {
        const std::vector<Token>* t = Require(key, 3);
        if (!t) return false;
        return ParseFloat((*t)[0], out.x) && ParseFloat((*t)[1], out.y) && ParseFloat((*t)[2], out.z)
                   ? Consume(key)
                   : m_reader.Fail("key '" + key + "' expects three finite numbers");
    }
    bool DVec3(const std::string& key, glm::dvec3& out) {
        const std::vector<Token>* t = Require(key, 3);
        if (!t) return false;
        return ParseDouble((*t)[0], out.x) && ParseDouble((*t)[1], out.y) && ParseDouble((*t)[2], out.z)
                   ? Consume(key)
                   : m_reader.Fail("key '" + key + "' expects three finite numbers");
    }
    bool Quat(const std::string& key, glm::quat& out) {
        const std::vector<Token>* t = Require(key, 4);
        if (!t) return false;
        float w, x, y, z;
        if (!ParseFloat((*t)[0], w) || !ParseFloat((*t)[1], x) || !ParseFloat((*t)[2], y) ||
            !ParseFloat((*t)[3], z)) {
            return m_reader.Fail("key '" + key + "' expects four finite numbers (w x y z)");
        }
        out = glm::quat(w, x, y, z);
        return Consume(key);
    }
    bool Float(const std::string& key, float& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        return ParseFloat((*t)[0], out) ? Consume(key)
                                        : m_reader.Fail("key '" + key + "' expects a finite number");
    }
    bool Id(const std::string& key, SceneObjectId& out) {
        const auto* t = Require(key, 1);
        long long value = 0;
        if (!t || !ParseInt((*t)[0], value) || value < 0) return m_reader.Fail("invalid object reference " + key);
        out = static_cast<SceneObjectId>(value); return Consume(key);
    }

    bool Mask(const std::string& key, CategoryMask& out) {
        const auto* t=Require(key,1); if(!t)return false;
        const auto& text=(*t)[0].text; auto result=std::from_chars(text.data(),text.data()+text.size(),out);
        if((*t)[0].quoted||result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return m_reader.Fail("invalid mask "+key);
        return Consume(key);
    }
    bool Layer(const std::string& key,unsigned& out) {
        int value=0;if(!Int(key,value))return false;
        if(value<0||value>=64)return m_reader.Fail("layer must be in [0,63]");
        out=unsigned(value);return true;
    }
    bool Int(const std::string& key, int& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        long long v = 0;
        if (!ParseInt((*t)[0], v) || v < -2147483647LL || v > 2147483647LL) {
            return m_reader.Fail("key '" + key + "' expects an integer");
        }
        out = static_cast<int>(v);
        return Consume(key);
    }
    bool Bool(const std::string& key, bool& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        return ParseBool((*t)[0], out) ? Consume(key)
                                       : m_reader.Fail("key '" + key + "' expects true or false");
    }
    bool String(const std::string& key, std::string& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        if (!(*t)[0].quoted) return m_reader.Fail("key '" + key + "' expects a quoted string");
        out = (*t)[0].text;
        return Consume(key);
    }
    // Header keys with a small enumerated word list; returns the tokens.
    const std::vector<Token>* Header(const std::string& key, std::size_t minCount, std::size_t maxCount) {
        const auto it = m_block.values.find(key);
        if (it == m_block.values.end()) { m_reader.Fail("missing '" + key + "'"); return nullptr; }
        if (it->second.size() < minCount || it->second.size() > maxCount) {
            m_reader.Fail("key '" + key + "' has the wrong number of values");
            return nullptr;
        }
        Consume(key);
        return &it->second;
    }

    // After parsing: every key present must have been consumed.
    bool CheckNoUnknown() {
        for (const auto& [key, tokens] : m_block.values) {
            if (!m_consumed.count(key)) return m_reader.Fail("unknown or misplaced key '" + key + "'");
        }
        return true;
    }
    const std::vector<std::vector<Token>>& CompoundBoxes() const { return m_block.compoundBoxes; }
    const std::vector<std::vector<Token>>& FluidCavities() const { return m_block.fluidCavities; }

private:
    const std::vector<Token>* Require(const std::string& key, std::size_t count) {
        const auto it = m_block.values.find(key);
        if (it == m_block.values.end()) { m_reader.Fail("missing required key '" + key + "'"); return nullptr; }
        if (it->second.size() != count) {
            m_reader.Fail("key '" + key + "' has the wrong number of values");
            return nullptr;
        }
        return &it->second;
    }
    bool Consume(const std::string& key) { m_consumed.insert(key); return true; }

    Reader& m_reader;
    const Block& m_block;
    std::set<std::string> m_consumed;
};

bool ReadBlock(Reader& reader, Block& block) {
    std::vector<Token> tokens;
    while (reader.Next(tokens)) {
        if (tokens[0].text == "end" && !tokens[0].quoted) {
            if (tokens.size() != 1) return reader.Fail("end expects no values");
            return true;
        }
        if (tokens[0].quoted) return reader.Fail("expected a key");
        const std::string key = tokens[0].text;
        std::vector<Token> values(tokens.begin() + 1, tokens.end());
        if (key == "body.compound-box") {
            block.compoundBoxes.push_back(values);
            block.seen.insert(key);
            continue;
        }
        if (key == "body.fluid-cavity") {
            block.fluidCavities.push_back(values);
            block.seen.insert(key);
            continue;
        }
        if (block.values.count(key)) return reader.Fail("duplicate key '" + key + "'");
        block.values[key] = values;
        block.seen.insert(key);
    }
    if (reader.Failed()) return false;
    return reader.Fail("unexpected end of file inside a block (missing 'end')");
}

bool ParseSettings(Reader& reader, const Block& block, Scene& scene) {
    ObjectParser p(reader, block);
    SceneSettings& s = scene.Settings();
    if (!p.String("name", s.name)) return false;
    if (!p.DVec3("world-origin", s.worldOrigin)) return false;
    if (!p.Vec3("sun-direction", s.sunDirection)) return false;
    if (!p.Vec3("sun-color", s.sunColor)) return false;
    if (!p.Vec3("ambient", s.ambientColor)) return false;
    if(p.Has("background-color")&&(!p.Vec3("background-color",s.backgroundColor)||glm::any(glm::lessThan(s.backgroundColor,glm::vec3(0)))||glm::any(glm::greaterThan(s.backgroundColor,glm::vec3(10000)))))return reader.Fail("background colour must be finite 0..10000");
    if((p.Has("linear-rendering")&&!p.Bool("linear-rendering",s.linearRendering))||(p.Has("exposure")&&!p.Float("exposure",s.exposure))||(p.Has("environment")&&!p.String("environment",s.environmentAsset))||(p.Has("environment-intensity")&&!p.Float("environment-intensity",s.environmentIntensity))||(p.Has("environment-rotation")&&!p.Quat("environment-rotation",s.environmentRotation))||(p.Has("environment-background")&&!p.Bool("environment-background",s.environmentBackground)))return false;
    if(!(s.exposure>0)||s.exposure>10000||s.environmentIntensity<0||s.environmentIntensity>10000||(!s.environmentAsset.empty()&&!IsValidAssetId(s.environmentAsset)))return reader.Fail("invalid display/environment settings");
    if (!p.Float("fluid-scale", s.fluidScale)) return false;
    // New authored fields are optional when reading existing version-3 scenes; their
    // declared defaults are always serialized and fingerprinted on output.
    if (p.Has("fluid-update-rate-hz") && !p.Float("fluid-update-rate-hz", s.fluidUpdateRateHz)) return false;
    if (p.Has("fluid-hydrostatic-drag-rate") &&
        !p.Float("fluid-hydrostatic-drag-rate", s.fluidHydrostaticDragRate)) return false;
    if (!(s.fluidUpdateRateHz > 0.0f) || s.fluidHydrostaticDragRate < 0.0f) {
        return reader.Fail("fluid update rate must be positive and hydrostatic drag non-negative");
    }
    if (!p.FluidCavities().empty()) return reader.Fail("body.fluid-cavity is misplaced in settings");
    const std::vector<Token>* policy = p.Header("fidelity-policy", 1, 3);
    if (!policy) return false;
    if ((*policy)[0].text == "none" && policy->size() == 1) {
        s.fidelityPolicy = SceneFidelityPolicy::None;
    } else if ((*policy)[0].text == "distance" && policy->size() == 3) {
        s.fidelityPolicy = SceneFidelityPolicy::Distance;
        if (!ParseFloat((*policy)[1], s.fidelityFullRadius) || !ParseFloat((*policy)[2], s.fidelityCoarseRadius) ||
            s.fidelityFullRadius < 0.0f || s.fidelityCoarseRadius < s.fidelityFullRadius) {
            return reader.Fail("fidelity-policy distance expects 0 <= full radius <= coarse radius");
        }
    } else {
        return reader.Fail("fidelity-policy must be 'none' or 'distance <fullRadius> <coarseRadius>'");
    }
    if(p.Has("authoring-recipes")){std::string text;if(!p.String("authoring-recipes",text))return false;if(text.size()>4*1024*1024)return reader.Fail("authoring recipes exceed 4 MiB");try{auto recipes=nlohmann::ordered_json::parse(text);if(!recipes.is_object()||recipes.size()>128)return reader.Fail("invalid authoring recipe map");for(auto it=recipes.begin();it!=recipes.end();++it)s.authoringRecipes[it.key()]=it.value().dump();}catch(const std::exception&){return reader.Fail("invalid authoring recipe JSON");}}
    if(p.Has("main-camera.render-mask")&&!p.Mask("main-camera.render-mask",s.mainCameraRenderMask))return false;
    SceneObjectId nextId = 0;
    if (!p.Id("next-id", nextId)) return false;
    if (nextId < 1) return reader.Fail("next-id must be at least 1");
    scene.SetNextId(static_cast<SceneObjectId>(nextId));
    return p.CheckNoUnknown();
}

bool ParseObject(Reader& reader, const std::vector<Token>& header, const Block& block,
                 SceneObject& o) {
    if (header.size() != 3 || header[1].quoted || !header[2].quoted) {
        return reader.Fail("object header must be: object <id> \"<name>\"");
    }
    long long id = 0;
    if (!ParseInt(header[1], id) || id <= 0) return reader.Fail("object id must be a positive integer");
    o.id = static_cast<SceneObjectId>(id);
    o.name = header[2].text;

    ObjectParser p(reader, block);
    if(p.Has("authoring-folder")&&!p.String("authoring-folder",o.authoringFolder))return false;
    if(p.Has("tags")&&!p.Mask("tags",o.tags))return false;
    if(p.Has("render-layer")&&!p.Layer("render-layer",o.renderLayer))return false;
    if (p.Has("parent") && !p.Id("parent", o.parent)) return false;
    if (p.Has("prefab.root")) {
        std::string ids, overrides;
        if (!p.Id("prefab.root", o.prefabRoot) || !p.Id("prefab.source", o.prefabSource) ||
            !p.String("prefab.asset", o.prefabAsset) || !p.String("prefab.ids", ids) ||
            !p.String("prefab.overrides", overrides)) return false;
        PrefabProperties mapping;
        std::string error;
        if (!DecodePrefabOverrides(ids, mapping, error) ||
            !DecodePrefabOverrides(overrides, o.prefabOverrides, error)) return reader.Fail(error);
        for (const auto& pair : mapping) {
            try { size_t a, b; auto src=std::stoull(pair.first,&a), dst=std::stoull(pair.second,&b);
                if (!src || !dst || a!=pair.first.size() || b!=pair.second.size()) return reader.Fail("invalid prefab ID mapping");
                o.prefabIds[src]=dst;
            } catch (...) { return reader.Fail("invalid prefab ID mapping"); }
        }
    }
    if (!p.Vec3("position", o.transform.position)) return false;
    if (!p.Quat("rotation", o.transform.rotation)) return false;
    if (!p.Vec3("scale", o.transform.scale)) return false;

    std::map<std::string,std::string> deformFields;
    for(const auto& [key,tokens]:block.values)if(key.rfind("deformable.",0)==0){std::string value;if(!p.String(key,value))return false;deformFields[key]=value;}
    std::string deformError;if(!ApplyDeformableProperties(deformFields,o,deformError))return reader.Fail(deformError);
    std::map<std::string,std::string> liquidFields;
    for(const auto& [key,tokens]:block.values)if(key.rfind("liquid.",0)==0){std::string value;if(!p.String(key,value))return false;liquidFields[key]=value;}
    // Decode after all collider components: liquid validation depends on them.
    std::map<std::string,std::string> navFields;
    for(const auto& [key,tokens]:block.values)if(key.rfind("nav.",0)==0){std::string value;if(!p.String(key,value))return false;navFields[key]=value;}
    std::string navError;if(!ApplyNavigationProperties(navFields,o,navError))return reader.Fail(navError);
    if(p.Has("motor.enabled")){CharacterMotorSettings m;
        if(!p.Bool("motor.enabled",m.enabled)||!p.Float("motor.radius",m.radius)||!p.Float("motor.halfHeight",m.halfHeight)||!p.Vec3("motor.offset",m.offset)||!p.Float("motor.stepHeight",m.stepHeight)||!p.Float("motor.supportDistance",m.supportDistance)||!p.Float("motor.skin",m.skin)||!p.Float("motor.maxSlopeDegrees",m.maxSlopeDegrees)||!p.Float("motor.gravityScale",m.gravityScale)||!p.Float("motor.reorientationDegreesPerSecond",m.reorientationDegreesPerSecond)||!p.Float("motor.interactionMass",m.interactionMass)||!p.Float("motor.maxPushImpulse",m.maxPushImpulse))return false;
        if(!p.Layer("motor.collisionLayer",m.collisionLayer)||!p.Mask("motor.collisionMask",m.collisionMask)||!p.Mask("motor.requiredTags",m.requiredTags)||!p.Mask("motor.excludedTags",m.excludedTags))return false;
        std::string error;if(!ValidCharacterMotor(m,error))return reader.Fail(error);o.characterMotor=m;
    }
    if(p.Has("animation.enabled")){SceneAnimationComponent a;
        if(!p.Bool("animation.enabled",a.enabled)||!p.Bool("animation.play-on-start",a.playOnStart)||!p.Bool("animation.loop",a.loop)||!p.String("animation.clip",a.clip)||!p.Float("animation.speed",a.speed)||!p.Float("animation.time",a.time)||a.time<0)return false;
        if(p.Has("animation.layers")){int count=0;if(!p.Int("animation.layers",count)||count<0||count>16)return reader.Fail("invalid layer count");for(int i=0;i<count;++i){AnimationLayerSettings l;auto key="animation.layer."+std::to_string(i)+".";int masks=0;
            if(!p.String(key+"id",l.id)||!p.String(key+"clip",l.clip)||!p.Bool(key+"enabled",l.enabled)||!p.Bool(key+"additive",l.additive)||!p.Float(key+"weight",l.weight)||!p.Float(key+"speed",l.speed)||!p.Float(key+"time",l.time)||!p.String(key+"reference-clip",l.referenceClip)||!p.Float(key+"reference-time",l.referenceTime)||!p.Int(key+"mask-count",masks)||masks<0||masks>int(kModelNodeLimit))return false;
            for(int n=0;n<masks;++n){std::string joint;if(!p.String(key+"mask."+std::to_string(n),joint))return false;l.mask.push_back(joint);}a.layers.push_back(std::move(l));}
        }std::string layerError;if(!ValidAnimationLayers(a.layers,layerError))return reader.Fail(layerError);
        if(p.Has("animation.limbs")){int count=0;if(!p.Int("animation.limbs",count)||count<0||count>16)return reader.Fail("16 IK contributor limit");std::set<std::string> ids;for(int i=0;i<count;++i){LimbIKSettings k;auto key="animation.limb."+std::to_string(i)+".";if(!p.String(key+"id",k.id)||!p.String(key+"root",k.root)||!p.String(key+"middle",k.middle)||!p.String(key+"end",k.end)||!p.Vec3(key+"target",k.target)||!p.Vec3(key+"pole",k.pole)||!p.Float(key+"weight",k.weight)||!p.Bool(key+"enabled",k.enabled)||!p.Int(key+"order",k.order))return false;std::string error;if(!ids.insert(k.id).second||!ValidLimbIK(k,error))return reader.Fail("invalid/duplicate IK: "+error);a.limbs.push_back(k);}}
        if(p.Has("animation.full-body")){std::string json,error;FullBodyIKSettings value;if(!p.String("animation.full-body",json)||!ParseFullBodyIKSettings(json,value,error))return reader.Fail("full-body IK: "+error);a.fullBodyIK=std::move(value);}
        o.animation=a;
    }

    if(p.Has("socket.target")){SceneSocketComponent k;if(!p.Id("socket.target",k.target)||!p.String("socket.joint",k.joint)||!p.Bool("socket.enabled",k.enabled)||!p.Vec3("socket.position",k.offset.position)||!p.Quat("socket.rotation",k.offset.rotation)||!p.Vec3("socket.scale",k.offset.scale)||!k.target||k.joint.empty())return reader.Fail("invalid visual socket");o.socket=k;}
    if(p.Has("ragdoll.enabled")){RagdollDefinition r;int count=0;
        if(!p.Bool("ragdoll.enabled",r.enabled)||!p.Bool("ragdoll.play-on-start",r.playOnStart)||!p.Bool("ragdoll.self-collision",r.selfCollision)||!p.Int("ragdoll.bones",count)||count<1||count>32)return reader.Fail("invalid ragdoll definition");
        if(p.Has("ragdoll.receive-contact-events")&&!p.Bool("ragdoll.receive-contact-events",r.receiveContactEvents))return false;
        for(int i=0;i<count;++i){RagdollBone b;auto& c=b.constraint;auto k="ragdoll.bone."+std::to_string(i)+".";int shape=0,type=0;
            if(!p.String(k+"joint",b.joint)||!p.String(k+"parent",b.parent)||!p.Int(k+"shape",shape)||!p.Vec3(k+"offset",b.offset)||!p.Quat(k+"orientation",b.orientation)||!p.Vec3(k+"half-extents",b.halfExtents)||!p.Float(k+"radius",b.radius)||!p.Float(k+"mass",b.mass)||!p.Float(k+"friction",b.friction)||!p.Float(k+"restitution",b.restitution)||!p.Layer(k+"layer",b.collisionLayer)||!p.Mask(k+"mask",b.collisionMask)||!p.Bool(k+"suppress-parent",b.suppressParentCollision)||!p.Bool(k+"auto-anchors",b.autoAnchors)||!p.Int(k+"constraint",type)||!p.Bool(k+"enabled",c.enabled)||!p.Vec3(k+"anchor-a",c.anchorA)||!p.Vec3(k+"anchor-b",c.anchorB)||!p.Quat(k+"frame-a",c.frameA)||!p.Quat(k+"frame-b",c.frameB)||!p.Bool(k+"limits",c.limits)||!p.Float(k+"lower",c.lower)||!p.Float(k+"upper",c.upper)||(p.Has(k+"rotational-resistance")&&!p.Float(k+"rotational-resistance",c.rotationalResistance)))return false;
            b.shape=RagdollShape(shape);c.type=JointType(type);r.bones.push_back(std::move(b));
        }if(p.Has("ragdoll.physical-animation")){std::string json,error;PhysicalAnimationSettings value;if(!p.String("ragdoll.physical-animation",json)||!ParsePhysicalAnimationSettings(json,value,error))return reader.Fail("physical animation: "+error);r.physicalAnimation=std::move(value);}
        std::string why;if(!ValidRagdollDefinition(r,why))return reader.Fail(why);o.ragdoll=std::move(r);
    }
    if(p.Has("joint")){SceneJointComponent j;auto& s=j.settings;int type=0;
        if(!p.Int("joint",type)||type<0||type>3||!p.Id("joint.body-a",j.bodyA)||!p.Id("joint.body-b",j.bodyB)||
           !p.Vec3("joint.anchor-a",s.anchorA)||!p.Vec3("joint.anchor-b",s.anchorB)||!p.Quat("joint.frame-a",s.frameA)||!p.Quat("joint.frame-b",s.frameB)||
           !p.Bool("joint.enabled",s.enabled)||!p.Bool("joint.limits",s.limits)||!p.Bool("joint.motor",s.motor)||!p.Bool("joint.spring",s.spring)||
           !p.Float("joint.lower",s.lower)||!p.Float("joint.upper",s.upper)||!p.Float("joint.speed",s.speed)||!p.Float("joint.max-force",s.maxForce)||
           !p.Float("joint.rest",s.rest)||!p.Float("joint.stiffness",s.stiffness)||!p.Float("joint.damping",s.damping)||(p.Has("joint.rotational-resistance")&&!p.Float("joint.rotational-resistance",s.rotationalResistance)))return false;
        s.type=JointType(type);if(!j.bodyA||j.bodyA==j.bodyB||!ValidJointSettings(s))return reader.Fail("invalid joint settings");o.joint=j;
    }
    if(p.Has("ui.asset")){SceneUIComponent u;if(!p.String("ui.asset",u.asset)||!p.String("ui.name",u.name)||!p.Bool("ui.enabled",u.enabled))return false;if(!IsValidAssetId(u.asset)||u.name.empty())return reader.Fail("invalid UI component");o.ui=u;}
    if(p.Has("scripts")) {
        int count=0;if(!p.Int("scripts",count)||count<0||count>64)return reader.Fail("invalid script slot count");
        std::set<SceneObjectId> ids;
        for(int i=0;i<count;++i){SceneScriptSlot slot;auto key="script."+std::to_string(i)+".";
            if(!p.Id(key+"id",slot.id)||!p.String(key+"asset",slot.asset)||!p.Bool(key+"enabled",slot.enabled)||!p.String(key+"properties",slot.properties))return false;
            if(!slot.id||!ids.insert(slot.id).second||!IsValidAssetId(slot.asset)||slot.properties.size()>65536)return reader.Fail("invalid script slot");
            o.scripts.push_back(std::move(slot));
        }
    }
    if (p.Has("render")) {
        SceneRenderComponent r;
        const std::vector<Token>* h = p.Header("render", 1, 1);
        if (!h) return false;
        if (!ParseShape((*h)[0], r.shape)) return reader.Fail("render shape must be box, sphere, compound, mesh or terrain");
        if (!p.Vec3("render.half-extents", r.halfExtents)) return false;
        if (!p.Float("render.radius", r.radius)) return false;
        if (!p.Vec3("render.color", r.color)) return false;
        if (!p.Float("render.alpha", r.alpha)) return false;
        if (!p.Vec3("render.secondary-color", r.secondaryColor)) return false;
        if (!p.Float("render.secondary-alpha", r.secondaryAlpha)) return false;
        if (!p.String("render.mesh-asset", r.meshAsset)) return false;
        if (!p.String("render.texture-asset", r.textureAsset)) return false;
        if(p.Has("render.hidden-part-count")){int count=0;if(!p.Int("render.hidden-part-count",count)||count<0||count>16384)return false;for(int i=0;i<count;++i){std::string key;if(!p.String("render.hidden-part-"+std::to_string(i),key)||key.empty())return false;r.hiddenParts.push_back(key);}}
        if(p.Has("render.materials")){std::string text;if(!p.String("render.materials",text))return false;std::string error;if(!DecodeMaterialSlots(text,r.materials,error))return reader.Fail(error);}
        if (p.Has("render.texture-camera") && !p.Id("render.texture-camera", r.textureCamera)) return false;
        o.render = r;
    }
    if(p.Has("particle-emitter")){ParticleEmitterSettings e;
        if(!p.Header("particle-emitter",0,0))return false;
        if(!p.Bool("particle.enabled",e.enabled))return false;
        if(!p.Bool("particle.loop",e.loop))return false;
        if(!p.Bool("particle.local-space",e.localSpace))return false;
        if(!p.Bool("particle.gravity",e.useGravity))return false;
        if(!p.Float("particle.rate",e.rate))return false;
        if(!p.Float("particle.lifetime",e.lifetime))return false;
        if(!p.Float("particle.size",e.size))return false;
        if(!p.Float("particle.end-size",e.endSize))return false;
        if(!p.Int("particle.burst",e.burst))return false;
        if(!p.Int("particle.capacity",e.maxParticles))return false;
        if(!p.Vec3("particle.spread",e.spread))return false;
        if(!p.Vec3("particle.velocity",e.velocity))return false;
        if(!p.Vec3("particle.variation",e.velocityVariation))return false;
        if(!p.Vec3("particle.acceleration",e.acceleration))return false;
        if(!p.String("particle.texture",e.textureAsset))return false;
        glm::vec3 color,endColor;SceneObjectId seed;
        if(!p.Vec3("particle.color",color)||!p.Float("particle.alpha",e.color.a)||!p.Vec3("particle.end-color",endColor)||!p.Float("particle.end-alpha",e.endColor.a)||!p.Id("particle.seed",seed))return false;
        e.color=glm::vec4(color,e.color.a);e.endColor=glm::vec4(endColor,e.endColor.a);
        if(seed>0xffffffffu)return reader.Fail("particle seed exceeds uint32");
        e.seed=static_cast<std::uint32_t>(seed);
        if(!ValidParticleSettings(e)||(!e.textureAsset.empty()&&!IsValidAssetId(e.textureAsset)))return reader.Fail("invalid particle emitter settings");
        o.particleEmitter=e;
    }
    if(p.Has("audio-emitter")) {
        SceneAudioEmitterComponent a;
        if(!p.Header("audio-emitter",0,0)||!p.String("audio.asset",a.asset)||!p.Bool("audio.enabled",a.enabled)||
           !p.Bool("audio.play-on-start",a.playOnStart)||!p.Bool("audio.loop",a.loop)||!p.Bool("audio.spatial",a.spatial)||
           !p.Float("audio.volume",a.volume)||!p.Float("audio.pitch",a.pitch)||!p.Float("audio.reference-distance",a.referenceDistance)||
           !p.Float("audio.maximum-distance",a.maximumDistance)||!p.Float("audio.rolloff",a.rolloff))return false;
        const auto* model=p.Header("audio.attenuation",1,1);if(!model)return false;
        if((*model)[0].text=="inverse")a.attenuation=AudioAttenuation::Inverse;
        else if((*model)[0].text=="linear")a.attenuation=AudioAttenuation::Linear;
        else if((*model)[0].text=="none")a.attenuation=AudioAttenuation::None;
        else return reader.Fail("unknown audio attenuation");

        if(p.Has("audio.loading")){auto* v=p.Header("audio.loading",1,1);if(!v)return false;if((*v)[0].text=="streamed")a.loading=AudioLoading::Streamed;else if((*v)[0].text!="buffered")return reader.Fail("invalid audio loading policy");}
        int pages=int(a.streamPageFrames);
        if((p.Has("audio.page-frames")&&!p.Int("audio.page-frames",pages))||(p.Has("audio.group")&&!p.String("audio.group",a.group))||(p.Has("audio.doppler")&&!p.Float("audio.doppler",a.doppler))||(p.Has("audio.send")&&!p.Float("audio.send",a.send))||(p.Has("audio.occlusion")&&!p.Bool("audio.occlusion",a.occlusion))||(p.Has("audio.bypass")&&!p.Bool("audio.bypass",a.bypass))||(p.Has("audio.occlusion-mask")&&!p.Mask("audio.occlusion-mask",a.occlusionMask))||(p.Has("audio.occluded-gain")&&!p.Float("audio.occluded-gain",a.occludedGain))||(p.Has("audio.occluded-cutoff")&&!p.Float("audio.occluded-cutoff",a.occludedCutoff)))return false;
        a.streamPageFrames=unsigned(pages);
        if(!ValidAudioSettings(a)||(!a.asset.empty()&&!IsValidAssetId(a.asset)))return reader.Fail("invalid audio settings or asset id");
        o.audioEmitter=a;
    }
    if(p.Has("audio-zone")){
        SceneAudioZoneComponent z;
        if(!p.Header("audio-zone",0,0)||!p.String("zone.asset",z.asset)||!p.Bool("zone.enabled",z.enabled)||!p.Vec3("zone.half-extents",z.halfExtents)||!p.Float("zone.radius",z.radius)||!p.Float("zone.blend",z.blendDistance)||!p.Float("zone.amount",z.amount)||!p.Int("zone.priority",z.priority))return false;
        const auto* shape=p.Header("zone.shape",1,1);if(!shape)return false;
        if((*shape)[0].text=="box")z.shape=SceneRegionShape::Box;else if((*shape)[0].text=="sphere")z.shape=SceneRegionShape::Sphere;else return reader.Fail("invalid audio zone shape");
        if(!IsValidAssetId(z.asset)||glm::any(glm::lessThanEqual(z.halfExtents,glm::vec3(0)))||z.radius<=0||z.blendDistance<0||z.amount<0||z.amount>1)return reader.Fail("invalid audio zone");
        o.audioZone=z;
    }
    if(p.Has("audio-listener")){
        SceneAudioListenerComponent l;if(!p.Header("audio-listener",0,0)||!p.Bool("listener.enabled",l.enabled)||!p.Bool("listener.follow-view",l.followActiveView))return false;
        o.audioListener=l;
    }
    if (p.Has("render-camera")) {
        SceneRenderCameraComponent c;
        if(p.Has("camera.render-mask")&&!p.Mask("camera.render-mask",c.renderMask))return false;
        if (!p.Header("render-camera", 0, 0) || !p.Bool("camera.enabled", c.enabled) ||
            !p.Int("camera.width", c.width) || !p.Int("camera.height", c.height) ||
            !p.Int("camera.cadence", c.updateEveryFrames) || !p.Float("camera.fov", c.verticalFovDegrees) ||
            !p.Float("camera.near", c.nearPlane) || !p.Float("camera.far", c.farPlane)) return false;
        if (c.width < 1 || c.height < 1 || c.width > 4096 || c.height > 4096 ||
            c.updateEveryFrames < 1 || !(c.verticalFovDegrees > 0 && c.verticalFovDegrees < 179) ||
            !(c.nearPlane > 0 && c.farPlane > c.nearPlane)) return reader.Fail("invalid render-camera settings");
        o.renderCamera = c;
    }
    if (p.Has("body")) {
        SceneBodyComponent b;
        if(p.Has("body.sensor")&&!p.Bool("body.sensor",b.sensor))return false;
        if(p.Has("body.enabled")&&!p.Bool("body.enabled",b.enabled))return false;
        if(p.Has("body.collision-layer")&&!p.Layer("body.collision-layer",b.collisionLayer))return false;
        if(p.Has("body.collision-mask")&&!p.Mask("body.collision-mask",b.collisionMask))return false;
        const std::vector<Token>* h = p.Header("body", 2, 2);
        if (!h) return false;
        if ((*h)[0].text == "static") b.motion = SceneBodyMotion::Static;
        else if ((*h)[0].text == "dynamic") b.motion = SceneBodyMotion::Dynamic;
        else if ((*h)[0].text == "kinematic") b.motion = SceneBodyMotion::Kinematic;
        else return reader.Fail("body motion must be static, dynamic or kinematic");
        if (!ParseShape((*h)[1], b.shape) || b.shape == SceneShape::Mesh) {
            return reader.Fail("body shape must be box, sphere, compound or terrain");
        }
        if (!p.Vec3("body.half-extents", b.halfExtents)) return false;
        if (!p.Float("body.radius", b.radius)) return false;
        if (!p.String("body.terrain", b.terrainSurface)) return false;
        if(p.Has("body.collision-asset")&&!p.String("body.collision-asset",b.collisionAsset))return false;
        if (!p.Float("body.mass", b.mass)) return false;
        if (!p.Float("body.friction", b.friction)) return false;
        if (!p.Float("body.restitution", b.restitution)) return false;
        if (!p.Vec3("body.initial-velocity", b.initialLinearVelocity)) return false;
        if(p.Has("body.initial-angular-velocity")&&!p.Vec3("body.initial-angular-velocity",b.initialAngularVelocity))return false;
        if (!p.Bool("body.pickable", b.pickable)) return false;
        if (!p.Bool("body.managed", b.managed)) return false;
        int compoundCount = 0;
        if (!p.Int("body.compound-count", compoundCount)) return false;
        if (compoundCount < 0 || static_cast<std::size_t>(compoundCount) != p.CompoundBoxes().size()) {
            return reader.Fail("body.compound-count does not match the body.compound-box lines");
        }
        for (const std::vector<Token>& t : p.CompoundBoxes()) {
            CompoundBox box;
            if ((t.size() != 6&&t.size()!=14) || !ParseFloat(t[0], box.localCenter.x) || !ParseFloat(t[1], box.localCenter.y) ||
                !ParseFloat(t[2], box.localCenter.z) || !ParseFloat(t[3], box.halfExtents.x) ||
                !ParseFloat(t[4], box.halfExtents.y) || !ParseFloat(t[5], box.halfExtents.z)) {
                return reader.Fail("body.compound-box expects six finite numbers");
            }
            box.key=uint32_t(b.compoundBoxes.size()+1);
            if(t.size()==14){long long type=0,key=0;if(!ParseFloat(t[6],box.rotation.w)||!ParseFloat(t[7],box.rotation.x)||!ParseFloat(t[8],box.rotation.y)||!ParseFloat(t[9],box.rotation.z)||!ParseInt(t[10],type)||!ParseFloat(t[11],box.radius)||!ParseInt(t[13],key)||key<=0||key>UINT32_MAX||glm::dot(box.rotation,box.rotation)<1e-12f||(type!=int(ShapeType::Box)&&type!=int(ShapeType::Sphere)&&type!=int(ShapeType::ConvexHull)))return reader.Fail("invalid compound child rotation/type/key");box.rotation=glm::normalize(box.rotation);box.type=ShapeType(type);box.assetId=t[12].text;box.key=uint32_t(key);}
            else box.key=0; // legacy vector-position identity is preserved
            b.compoundBoxes.push_back(box);
        }
        int cavityCount = 0;
        if (p.Has("body.fluid-cavity-count") && !p.Int("body.fluid-cavity-count", cavityCount)) return false;
        if (cavityCount < 0 || static_cast<std::size_t>(cavityCount) != p.FluidCavities().size()) {
            return reader.Fail("body.fluid-cavity-count does not match the body.fluid-cavity lines");
        }
        for (const std::vector<Token>& t : p.FluidCavities()) {
            SceneFluidCavity cavity;
            if (t.size() != 6 || !ParseFloat(t[0], cavity.localCenter.x) ||
                !ParseFloat(t[1], cavity.localCenter.y) || !ParseFloat(t[2], cavity.localCenter.z) ||
                !ParseFloat(t[3], cavity.halfExtents.x) || !ParseFloat(t[4], cavity.halfExtents.y) ||
                !ParseFloat(t[5], cavity.halfExtents.z) || !(cavity.halfExtents.x > 0.0f) ||
                !(cavity.halfExtents.y > 0.0f) || !(cavity.halfExtents.z > 0.0f)) {
                return reader.Fail("body.fluid-cavity expects a finite center and three finite positive half extents");
            }
            b.fluidCavities.push_back(cavity);
        }
        if(b.compoundBoxes.size()>64)return reader.Fail("compound limit is 64 children");
        std::set<uint32_t> childKeys;for(size_t i=0;i<b.compoundBoxes.size();++i)if(!childKeys.insert(b.compoundBoxes[i].key?b.compoundBoxes[i].key:uint32_t(i+1)).second)return reader.Fail("duplicate compound child key");
        if (b.shape == SceneShape::Compound && b.compoundBoxes.empty()) {
            return reader.Fail("a compound body needs at least one body.compound-box");
        }
        if (b.shape == SceneShape::Terrain && b.terrainSurface.empty()) {
            return reader.Fail("a terrain body needs a body.terrain surface identifier");
        }
        if (b.motion != SceneBodyMotion::Static && !(b.mass > 0.0f)) {
            return reader.Fail("a moving body needs a positive body.mass for authority changes");
        }
        if (b.motion != SceneBodyMotion::Static && b.shape == SceneShape::Terrain) {
            return reader.Fail("terrain bodies must be static");
        }
        if((b.shape==SceneShape::ConvexHull||b.shape==SceneShape::TriangleMesh)&&b.collisionAsset.empty())return reader.Fail("cooked body requires body.collision-asset");
        if(b.shape==SceneShape::TriangleMesh&&(b.motion!=SceneBodyMotion::Static||b.sensor))return reader.Fail("triangle mesh is static surface, not moving body or volume sensor");
        o.body = b;
    } else if (p.Has("body.compound-count") || !p.CompoundBoxes().empty() ||
               p.Has("body.fluid-cavity-count") || !p.FluidCavities().empty()) {
        return reader.Fail("body.* keys require a 'body' header");
    }
    if (p.Has("gravity")) {
        SceneGravityComponent g;
        const std::vector<Token>* h = p.Header("gravity", 2, 2);
        if (!h) return false;
        if ((*h)[0].text == "radial") g.kind = SceneGravityKind::Radial;
        else if ((*h)[0].text == "uniform") g.kind = SceneGravityKind::Uniform;
        else return reader.Fail("gravity kind must be radial or uniform");
        if (!ParseFloat((*h)[1], g.magnitude)) return reader.Fail("gravity magnitude must be a finite number");
        const std::vector<Token>* region = p.Header("gravity.region", 2, 4);
        if (!region) return false;
        if ((*region)[0].text == "sphere" && region->size() == 2) {
            g.regionShape = SceneRegionShape::Sphere;
            if (!ParseFloat((*region)[1], g.regionRadius)) return reader.Fail("gravity.region sphere radius must be a finite number");
        } else if ((*region)[0].text == "box" && region->size() == 4) {
            g.regionShape = SceneRegionShape::Box;
            if (!ParseFloat((*region)[1], g.regionHalfExtents.x) || !ParseFloat((*region)[2], g.regionHalfExtents.y) ||
                !ParseFloat((*region)[3], g.regionHalfExtents.z)) {
                return reader.Fail("gravity.region box expects three finite numbers");
            }
        } else {
            return reader.Fail("gravity.region must be 'sphere <r>' or 'box <x> <y> <z>'");
        }
        o.gravity = g;
    }
    if (p.Has("light")) {
        SceneLightComponent l;
        const std::vector<Token>* h = p.Header("light", 1, 1);
        if (!h) return false;
        if ((*h)[0].text == "point") l.kind = SceneLightKind::Point;
        else if ((*h)[0].text == "spot") l.kind = SceneLightKind::Spot;
        else return reader.Fail("light kind must be point or spot");
        if (!p.Vec3("light.color", l.color)) return false;
        if (!p.Float("light.range", l.range)) return false;
        const std::vector<Token>* cone = p.Header("light.cone", 2, 2);
        if (!cone) return false;
        if (!ParseFloat((*cone)[0], l.innerConeDegrees) || !ParseFloat((*cone)[1], l.outerConeDegrees)) {
            return reader.Fail("light.cone expects two finite numbers");
        }
        o.light = l;
    }
    if (p.Has("door")) {
        SceneDoorComponent d;
        if(p.Has("door.collision-layer")&&!p.Layer("door.collision-layer",d.collisionLayer))return false;
        if(p.Has("door.collision-mask")&&!p.Mask("door.collision-mask",d.collisionMask))return false;
        if (!p.Header("door", 0, 0)) return false;
        if (!p.Vec3("door.hinge-axis", d.localHingeAxis)) return false;
        if (!p.Float("door.open-angle", d.openAngleDegrees)) return false;
        if (!p.Float("door.angular-speed", d.angularSpeedDegreesPerSecond)) return false;
        if (!o.render || o.render->shape != SceneShape::Box) {
            return reader.Fail("a door needs a box render component for its panel");
        }
        o.door = d;
    }
    if (p.Has("light-switch")) {
        SceneLightSwitchComponent s;
        if (!p.Header("light-switch", 0, 0)) return false;
        if (!p.Vec3("light-switch.hinge-axis", s.localHingeAxis)) return false;
        if (!p.Float("light-switch.toggle-angle", s.toggleAngleDegrees)) return false;
        if (!p.Float("light-switch.angular-speed", s.angularSpeedDegreesPerSecond)) return false;
        if (!p.Vec3("light-switch.lamp-offset", s.lampLocalOffset)) return false;
        if (!p.Vec3("light-switch.lamp-color", s.lampColor)) return false;
        if (!p.Float("light-switch.lamp-range", s.lampRange)) return false;
        if (!o.render || o.render->shape != SceneShape::Box) {
            return reader.Fail("a light switch needs a box render component for its lever");
        }
        o.lightSwitch = s;
    }
    if (p.Has("vehicle")) {
        SceneVehicleComponent v;
        const std::vector<Token>* h = p.Header("vehicle", 1, 1);
        if (!h) return false;
        if ((*h)[0].text == "local") v.gravity = SceneVehicleGravity::Local;
        else if ((*h)[0].text == "celestial") v.gravity = SceneVehicleGravity::Celestial;
        else return reader.Fail("vehicle gravity must be local or celestial");
        if (!p.Bool("vehicle.headlight", v.headlight)) return false;
        if (!p.Bool("vehicle.navigation-lights", v.navigationLights)) return false;
        if (!p.Float("vehicle.drag-coefficient", v.dragCoefficient)) return false;
        if (!p.Bool("vehicle.initial-pilot-attached", v.initialPilotAttached)) return false;
        if (!o.body || o.body->motion != SceneBodyMotion::Dynamic || o.body->shape != SceneShape::Box) {
            return reader.Fail("a vehicle needs a dynamic box body");
        }
        o.vehicle = v;
    }
    if (p.Has("celestial")) {
        SceneCelestialComponent c;
        if (!p.Header("celestial", 0, 0)) return false;
        if (!p.Float("celestial.gravitational-parameter", c.gravitationalParameter)) return false;
        if (!p.Float("celestial.operator-thrust", c.operatorThrustForce)) return false;
        if (!o.body) return reader.Fail("a celestial component needs a body");
        o.celestial = c;
    }
    if (p.Has("atmosphere")) {
        SceneAtmosphereComponent a;
        if (!p.Header("atmosphere", 0, 0)) return false;
        if (!p.Float("atmosphere.reference-radius", a.referenceRadius)) return false;
        if (!p.Float("atmosphere.top-radius", a.topRadius)) return false;
        if (!p.Float("atmosphere.reference-density", a.referenceDensity)) return false;
        if (!p.Float("atmosphere.polytropic-exponent", a.polytropicExponent)) return false;
        if (!p.Float("atmosphere.oxidizer-fraction", a.oxidizerMassFraction)) return false;
        if (!p.Float("atmosphere.reference-temperature", a.referenceTemperatureKelvin)) return false;
        if (!o.celestial || !(o.celestial->gravitationalParameter > 0.0f)) {
            return reader.Fail("an atmosphere needs a celestial component with a positive gravitational parameter");
        }
        o.atmosphere = a;
    }
    if (p.Has("combustible")) {
        SceneCombustibleComponent c;
        if (!p.Header("combustible", 0, 0)) return false;
        if (!p.Float("combustible.heat-capacity", c.heatCapacityJPerK)) return false;
        if (!p.Float("combustible.fuel-mass", c.initialFuelMassKg)) return false;
        if (!p.Float("combustible.ignition-temperature", c.ignitionTemperatureK)) return false;
        if (!p.Float("combustible.max-fuel-rate", c.maximumFuelRateKgPerSecond)) return false;
        if (!p.Float("combustible.radiative-area", c.radiativeAreaSquareMeters)) return false;
        if (!p.Float("combustible.retained-heat", c.retainedCombustionHeatFraction)) return false;
        if (!o.body || o.body->motion != SceneBodyMotion::Dynamic) {
            return reader.Fail("a combustible component needs a dynamic body");
        }
        o.combustible = c;
    }
    if (p.Has("fluid-volume")) {
        SceneFluidVolumeComponent f;
        if (!p.Header("fluid-volume", 0, 0)) return false;
        if (!p.Float("fluid-volume.spacing", f.spacing)) return false;
        const std::vector<Token>* count = p.Header("fluid-volume.count", 3, 3);
        if (!count) return false;
        long long cx = 0, cy = 0, cz = 0;
        if (!ParseInt((*count)[0], cx) || !ParseInt((*count)[1], cy) || !ParseInt((*count)[2], cz) ||
            cx < 0 || cy < 0 || cz < 0 || cx > 1000 || cy > 1000 || cz > 1000) {
            return reader.Fail("fluid-volume.count expects three non-negative integers");
        }
        f.countX = static_cast<int>(cx);
        f.countY = static_cast<int>(cy);
        f.countZ = static_cast<int>(cz);
        if (!p.Bool("fluid-volume.emitter", f.emitter)) return false;
        if (!p.Vec3("fluid-volume.emitter-offset", f.emitterLocalOffset)) return false;
        if (!p.Int("fluid-volume.max-particles", f.maxParticles)) return false;
        if (!(f.spacing > 0.0f)) return reader.Fail("fluid-volume.spacing must be positive");
        o.fluidVolume = f;
    }
    if (p.Has("player-start")) {
        ScenePlayerStartComponent ps;
        if(p.Has("player-start.collision-layer")&&!p.Layer("player-start.collision-layer",ps.collisionLayer))return false;
        if(p.Has("player-start.collision-mask")&&!p.Mask("player-start.collision-mask",ps.collisionMask))return false;
        const std::vector<Token>* h = p.Header("player-start", 1, 1);
        if (!h) return false;
        if ((*h)[0].text == "third-person") ps.view = ScenePlayerView::ThirdPerson;
        else if ((*h)[0].text == "first-person") ps.view = ScenePlayerView::FirstPerson;
        else return reader.Fail("player-start view must be third-person or first-person");
        if (!p.Float("player-start.yaw", ps.yawDegrees)) return false;
        if (p.Has("player-start.density") && !p.Float("player-start.density", ps.density)) return false;
        if (p.Has("player-start.fluid-drag") && !p.Float("player-start.fluid-drag", ps.fluidDrag)) return false;
        if (p.Has("player-start.swim-acceleration") &&
            !p.Float("player-start.swim-acceleration", ps.swimAcceleration)) return false;
        if (!(ps.density > 0.0f) || ps.fluidDrag < 0.0f || ps.swimAcceleration < 0.0f) {
            return reader.Fail("player density must be positive; fluid drag and swim acceleration must be non-negative");
        }
        o.playerStart = ps;
    }
    if(o.animation&&(!o.render||o.render->shape!=SceneShape::Mesh))return reader.Fail("animation requires a mesh render component");
    if (o.render && (o.render->shape == SceneShape::Compound || o.render->shape == SceneShape::Terrain)) {
        if (!o.body || o.body->shape != o.render->shape) {
            return reader.Fail("a compound/terrain render component needs a body of the same shape");
        }
    }
    if(p.Has("body.physical-material-override")&&(!o.body||!p.Bool("body.physical-material-override",o.body->physicalMaterialOverride)))return reader.Fail("invalid physical material override");
    if(p.Has("body.physical-material")){std::string asset;if(!o.body||!p.String("body.physical-material",asset)||(!asset.empty()&&!IsValidAssetId(asset)))return reader.Fail("invalid physical material identity");o.body->physicalMaterial=asset;}
    std::string liquidError;if(!ApplyLiquidProperties(liquidFields,o,liquidError))return reader.Fail(liquidError);
    return p.CheckNoUnknown();
}

}  // namespace

void WriteSceneObjectBlock(const SceneObject& object, std::string& outText) {
    Writer w;
    WriteObject(w, object);
    outText += w.Take();
}

bool ParseSceneObjectBlock(const std::vector<std::string>& lines, std::size_t& index, SceneObject& outObject,
                           std::string& outError) {
    // Re-use the scene reader on just this block's lines.
    std::string text;
    std::size_t consumed = 0;
    bool closed = false;
    for (std::size_t i = index; i < lines.size(); ++i) {
        text += lines[i] + "\n";
        ++consumed;
        std::vector<Token> tokens;
        std::string tokenError;
        if (!Tokenize(lines[i], tokens, tokenError)) { outError = tokenError; return false; }
        if (!tokens.empty() && !tokens[0].quoted && tokens[0].text == "end") { closed = true; break; }
    }
    if (!closed) {
        outError = "object block is missing its 'end'";
        return false;
    }
    Reader reader(text, outError);
    std::vector<Token> header;
    if (!reader.Next(header) || header[0].text != "object") {
        if (outError.empty()) outError = "expected an object block";
        return false;
    }
    Block block;
    if (!ReadBlock(reader, block)) return false;
    SceneObject object;
    if (!ParseObject(reader, header, block, object)) return false;
    outObject = object;
    index += consumed;
    return true;
}

bool SaveSceneToString(const Scene& scene, std::string& outText) {
    Writer w;
    w.Raw("JudasScene " + std::to_string(kSceneFormatVersion) + "\n");
    w.Raw("settings\n");
    const SceneSettings& s = scene.Settings();
    if(!s.authoringRecipes.empty()){nlohmann::ordered_json recipes=nlohmann::ordered_json::object();for(auto& [id,value]:s.authoringRecipes)recipes[id]=nlohmann::ordered_json::parse(value);w.Line("authoring-recipes",Quote(recipes.dump()));}
    w.Line("name", Quote(s.name));
    w.Line("world-origin", DV(s.worldOrigin));
    w.Line("sun-direction", V(s.sunDirection));
    w.Line("sun-color", V(s.sunColor));
    w.Line("ambient", V(s.ambientColor));
    if(s.backgroundColor!=glm::vec3(.08f,.09f,.11f))w.Line("background-color",V(s.backgroundColor));
    if(s.linearRendering||!s.environmentAsset.empty()){w.Line("linear-rendering",B(s.linearRendering));w.Line("exposure",F(s.exposure));w.Line("environment",Quote(s.environmentAsset));w.Line("environment-intensity",F(s.environmentIntensity));w.Line("environment-rotation",Q(s.environmentRotation));w.Line("environment-background",B(s.environmentBackground));}
    w.Line("fluid-scale", F(s.fluidScale));
    w.Line("fluid-update-rate-hz", F(s.fluidUpdateRateHz));
    w.Line("fluid-hydrostatic-drag-rate", F(s.fluidHydrostaticDragRate));
    w.Line("fidelity-policy", s.fidelityPolicy == SceneFidelityPolicy::None
                                  ? std::string("none")
                                  : "distance " + F(s.fidelityFullRadius) + " " + F(s.fidelityCoarseRadius));
    if(s.mainCameraRenderMask!=kAllCategories)w.Line("main-camera.render-mask",std::to_string(s.mainCameraRenderMask));
    w.Line("next-id", std::to_string(scene.NextId()));
    w.Raw("end\n");
    for (const SceneObject& o : scene.Objects()) {
        w.Raw("\n");
        WriteObject(w, o);
    }
    outText = w.Take();
    return true;
}

bool SaveSceneToFile(const Scene& scene, const std::string& path, std::string& outError) {
    std::string text;
    SaveSceneToString(scene, text);
    return WriteAuthoredDocument(path,text,std::filesystem::path(path).extension()==".judasprefab"?"prefab":"scene",outError);
}

bool LoadSceneFromString(const std::string& text, Scene& outScene, std::string& outError) {
    outError.clear();
    if(IsNamedDocument(text)){std::string legacy;if(!NamedToLegacy(text,"scene",legacy,outError,false))return false;return LoadSceneFromString(legacy,outScene,outError);}
    Reader reader(text, outError);
    Scene scene;
    std::vector<Token> tokens;

    if (!reader.Next(tokens)) {
        if (!reader.Failed()) outError = "scene file is empty";
        return false;
    }
    if (tokens.size() != 2 || tokens[0].text != "JudasScene" || tokens[0].quoted) {
        return reader.Fail("expected 'JudasScene <version>' on the first line");
    }
    long long version = 0;
    if (!ParseInt(tokens[1], version)) return reader.Fail("scene version must be an integer");
    if (version != kSceneFormatVersion) {
        return reader.Fail("unsupported scene format version " + std::to_string(version) +
                           " (this build reads version " + std::to_string(kSceneFormatVersion) + ")");
    }

    bool settingsSeen = false;
    bool playerStartSeen = false;
    while (reader.Next(tokens)) {
        if (tokens[0].quoted) return reader.Fail("expected 'settings' or 'object'");
        if (tokens[0].text == "settings") {
            if (settingsSeen) return reader.Fail("duplicate settings block");
            if (tokens.size() != 1) return reader.Fail("settings takes no values");
            Block block;
            if (!ReadBlock(reader, block)) return false;
            if (!ParseSettings(reader, block, scene)) return false;
            settingsSeen = true;
            continue;
        }
        if (tokens[0].text == "object") {
            if (!settingsSeen) return reader.Fail("settings block must precede objects");
            const std::vector<Token> header = tokens;
            Block block;
            if (!ReadBlock(reader, block)) return false;
            SceneObject object;
            if (!ParseObject(reader, header, block, object)) return false;
            if (object.playerStart) {
                if (playerStartSeen) return reader.Fail("more than one object has a player-start");
                playerStartSeen = true;
            }
            if (!scene.InsertObject(object)) {
                return reader.Fail("duplicate object id " + std::to_string(object.id));
            }
            continue;
        }
        return reader.Fail("unknown top-level directive '" + tokens[0].text + "'");
    }
    if (reader.Failed()) return false;
    if (!settingsSeen) {
        outError = "scene file has no settings block";
        return false;
    }
    int activeListeners=0;
    for(const auto& object:scene.Objects()) if(object.audioListener&&object.audioListener->enabled)++activeListeners;
    if(activeListeners>1)return reader.Fail("scene has more than one enabled audio listener");
    for (const auto& o : scene.Objects()) {
        if (!o.render || !o.render->textureCamera) continue;
        const auto* camera = scene.Find(o.render->textureCamera);
        if (!camera || !camera->renderCamera) return reader.Fail("render.texture-camera references no camera");
        if (!o.render->textureAsset.empty() || (o.render->shape != SceneShape::Box &&
            o.render->shape != SceneShape::Sphere && o.render->shape != SceneShape::Mesh))
            return reader.Fail("camera texture requires a box, sphere or mesh and no disk texture");
    }
    for(const auto& object:scene.Objects())if(object.joint){const auto& j=*object.joint;const auto* a=scene.Find(j.bodyA);const auto* b=scene.Find(j.bodyB);
        if(!a||!a->body||(j.bodyB&&(!b||!b->body))||(a->body->motion!=SceneBodyMotion::Dynamic&&(!b||b->body->motion!=SceneBodyMotion::Dynamic)))return reader.Fail("joint requires existing bodies and at least one dynamic body");}
    if (!ValidateHierarchy(scene, outError)) return false;
    outScene = std::move(scene);
    return true;
}

bool LoadSceneFromFile(const std::string& path, Scene& outScene, std::string& outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "could not open scene file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (!LoadSceneFromString(buffer.str(), outScene, outError)) {
        outError = path + ": " + outError;
        return false;
    }
    return true;
}
