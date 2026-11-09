#include "../../../third_party/nlohmann/json.hpp"
#include "../../AssetDatabase.h"
#include "../../ResourceManager.h"
#include "../../RuntimeWorld.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"
#include "../BindingValues.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "../../PowertrainAuthoring.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleMechanics(BindingContext &context, const std::string &op, int argc,
                        JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };

    if (op.rfind("fracture", 0) == 0) {
        uint64_t id = 0;
        try {
            id = std::stoull(String(c, arg(1)));
        } catch (...) {
            return JS_ThrowReferenceError(c, "invalid fracture owner");
        }
        std::string error;
        auto *d = world.RuntimeDeformable(id, error);
        bool exists = d && d->asset->fracture;
        bool valid = exists && String(c, arg(2)) == std::to_string(d->generation);
        if (op == "fractureExists")
            return exists ? JS_NewString(c, std::to_string(d->generation).c_str()) : JS_NULL;
        if (op == "fractureValid")
            return JS_NewBool(c, valid);
        if (!valid)
            return JS_ThrowReferenceError(c, "stale/not ready fracture handle");
        const auto &cook = *d->asset->fracture;
        auto &state = d->fracture;
        if (op == "fractureState") {
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "revision", JS_NewFloat64(c, double(state.revision)));
            JS_SetPropertyStr(c, o, "rigid", JS_NewBool(c, cook.rigid));
            JS_SetPropertyStr(c, o, "error", JS_NewString(c, state.error.c_str()));
            auto parts = JS_NewArray(c);
            for (unsigned p = 0; p < cook.parts.size(); ++p) {
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(c, v, "key", JS_NewString(c, cook.parts[p].key.c_str()));
                JS_SetPropertyStr(c, v, "index", JS_NewUint32(c, p));
                JS_SetPropertyStr(c, v, "component",
                                  state.removed[p] ? JS_NULL : JS_NewUint32(c, state.component[p]));
                JS_SetPropertyStr(c, v, "removed", JS_NewBool(c, state.removed[p]));
                auto entity = world.FracturePartEntity(id, p);
                JS_SetPropertyStr(c, v, "entity",
                                  entity ? JS_NewString(c, std::to_string(entity).c_str())
                                         : JS_NULL);
                double mass = 0;
                for (auto n : cook.parts[p].nodes)
                    mass += d->NodeMass(n);
                JS_SetPropertyStr(c, v, "mass", JS_NewFloat64(c, mass));
                JS_SetPropertyUint32(c, parts, p, v);
            }
            JS_SetPropertyStr(c, o, "parts", parts);
            auto bonds = JS_NewArray(c);
            for (unsigned b = 0; b < cook.bonds.size(); ++b) {
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(c, v, "key", JS_NewString(c, cook.bonds[b].key.c_str()));
                JS_SetPropertyStr(c, v, "a", JS_NewUint32(c, cook.bonds[b].a));
                JS_SetPropertyStr(c, v, "b", JS_NewUint32(c, cook.bonds[b].b));
                JS_SetPropertyStr(c, v, "broken", JS_NewBool(c, state.broken[b] != 0));
                JS_SetPropertyStr(
                    c, v, "cause",
                    state.broken[b]
                        ? JS_NewString(c, state.broken[b] == 1 ? "physical" : "explicit")
                        : JS_NULL);
                JS_SetPropertyStr(c, v, "material", JS_NewBool(c, cook.bonds[b].material.enabled));
                JS_SetPropertyStr(c, v, "plasticTranslation",
                                  Vec(c, state.plasticTranslation.empty()
                                             ? glm::vec3(0)
                                             : state.plasticTranslation[b]));
                JS_SetPropertyStr(c, v, "plasticRotation",
                                  Vec(c, state.plasticRotation.empty() ? glm::vec3(0)
                                                                       : state.plasticRotation[b]));
                JS_SetPropertyStr(
                    c, v, "plasticWork",
                    JS_NewFloat64(c, state.plasticWork.empty() ? 0 : state.plasticWork[b]));
                JS_SetPropertyStr(c, v, "tension", JS_NewFloat64(c, state.tensionDemand[b]));
                JS_SetPropertyStr(c, v, "shear", JS_NewFloat64(c, state.shearDemand[b]));
                JS_SetPropertyUint32(c, bonds, b, v);
            }
            JS_SetPropertyStr(c, o, "interfaces", bonds);
            JS_SetPropertyStr(c, o, "tensileStrength", JS_NewFloat64(c, cook.tension));
            JS_SetPropertyStr(c, o, "shearStrength", JS_NewFloat64(c, cook.shear));
            return o;
        }
        if (!s->fixed)
            return JS_ThrowTypeError(c, "fracture mutations belong in fixedUpdate");
        if (op == "fractureRemove") {
            uint32_t part;
            double revision;
            if (JS_ToUint32(c, &part, arg(3)) || JS_ToFloat64(c, &revision, arg(4)) ||
                !std::isfinite(revision) || revision != double(state.revision))
                return JS_FALSE;
            return JS_NewBool(c, state.RequestRemoval(cook, part, state.revision));
        }
        if (op == "fractureRelease") {
            double revision;
            if (JS_ToFloat64(c, &revision, arg(4)) || !std::isfinite(revision) || revision < 1 ||
                revision != double(state.revision))
                return JS_FALSE;
            auto key = String(c, arg(3));
            for (unsigned b = 0; b < cook.bonds.size(); ++b)
                if (cook.bonds[b].key == key)
                    return JS_NewBool(c, state.Request(cook, b, state.revision));
            return JS_FALSE;
        }
        if (op == "fractureForce" || op == "fractureImpulse") {
            uint32_t part;
            glm::vec3 value;
            if (JS_ToUint32(c, &part, arg(3)) || !ReadVec(c, arg(4), value))
                return JS_ThrowTypeError(c, "part index and finite vector required");
            return JS_NewBool(c, world.LoadFracturePart(id, part, value, op == "fractureImpulse"));
        }
        return JS_ThrowTypeError(c, "unknown fracture operation");
    }

    if (op.rfind("deformable", 0) == 0) {
        uint64_t id = 0;
        try {
            id = std::stoull(String(c, arg(1)));
        } catch (...) {
            return JS_ThrowReferenceError(c, "invalid deformable entity");
        }
        std::string error;
        auto *d = world.RuntimeDeformable(id, error);
        if (op == "deformableExists") {
            if (!d && world.RuntimeDefinition(id) && world.RuntimeDefinition(id)->deformable &&
                error != "loading")
                return JS_ThrowTypeError(c, "deformable: %s", error.c_str());
            return d ? JS_NewString(c, std::to_string(d->generation).c_str()) : JS_NULL;
        }
        bool valid = d && String(c, arg(2)) == std::to_string(d->generation);
        if (op == "deformableValid")
            return JS_NewBool(c, valid);
        if (!valid)
            return JS_ThrowReferenceError(c, "stale/not ready deformable handle");
        if (op == "deformableState") {
            auto v = JS_NewObject(c);
            JS_SetPropertyStr(c, v, "enabled", JS_NewBool(c, d->settings.enabled));
            JS_SetPropertyStr(c, v, "sleeping", JS_NewBool(c, d->sleeping));
            JS_SetPropertyStr(c, v, "error", JS_NewString(c, d->error.c_str()));
            JS_SetPropertyStr(c, v, "mass", JS_NewFloat64(c, d->Mass()));
            JS_SetPropertyStr(c, v, "minimum", Vec(c, glm::vec3(d->Minimum())));
            JS_SetPropertyStr(c, v, "maximum", Vec(c, glm::vec3(d->Maximum())));
            JS_SetPropertyStr(c, v, "nodes", JS_NewUint32(c, unsigned(d->positions.size())));
            JS_SetPropertyStr(c, v, "contacts", JS_NewUint32(c, unsigned(d->stats.contacts)));
            JS_SetPropertyStr(c, v, "minimumJacobian", JS_NewFloat64(c, d->stats.minimumJacobian));
            JS_SetPropertyStr(c, v, "maximumStrain", JS_NewFloat64(c, d->stats.maximumStrain));
            auto groups = JS_NewArray(c);
            uint32_t n = 0;
            for (auto &[name, _] : d->asset->groups)
                JS_SetPropertyUint32(c, groups, n++, JS_NewString(c, name.c_str()));
            JS_SetPropertyStr(c, v, "groups", groups);
            auto attachments = JS_NewArray(c);
            for (size_t i = 0; i < d->settings.attachments.size(); ++i) {
                const auto &a = d->settings.attachments[i];
                const auto observed = i < d->attachmentState.size() ? d->attachmentState[i]
                                                                    : DeformableAttachmentState{};
                auto item = JS_NewObject(c);
                JS_SetPropertyStr(c, item, "group", JS_NewString(c, a.group.c_str()));
                JS_SetPropertyStr(c, item, "enabled", JS_NewBool(c, a.enabled));
                const bool released = i < d->released.size() && d->released[i] != 0;
                JS_SetPropertyStr(c, item, "released", JS_NewBool(c, released));
                JS_SetPropertyStr(c, item, "active",
                                  JS_NewBool(c, d->settings.enabled && a.enabled && !released));
                JS_SetPropertyStr(c, item, "forceLimit", JS_NewFloat64(c, a.forceLimit));
                JS_SetPropertyStr(c, item, "torqueLimit", JS_NewFloat64(c, a.torqueLimit));
                JS_SetPropertyStr(c, item, "force", Vec(c, glm::vec3(observed.force)));
                JS_SetPropertyStr(c, item, "moment", Vec(c, glm::vec3(observed.moment)));
                JS_SetPropertyStr(c, item, "peakForce", JS_NewFloat64(c, observed.peakForce));
                JS_SetPropertyStr(c, item, "peakTorque", JS_NewFloat64(c, observed.peakTorque));
                JS_SetPropertyUint32(c, attachments, uint32_t(i), item);
            }
            JS_SetPropertyStr(c, v, "attachments", attachments);
            return v;
        }
        if (op == "deformableRaycast") {
            glm::vec3 origin, direction;
            double maximum;
            if (!ReadVec(c, arg(3), origin) || !ReadVec(c, arg(4), direction) ||
                JS_ToFloat64(c, &maximum, arg(5)) || !std::isfinite(maximum) || maximum < 0 ||
                glm::length(direction) < 1e-8f)
                return JS_ThrowTypeError(c, "finite ray and nonnegative range required");
            auto hit = d->Raycast(origin, direction, maximum);
            if (!hit.hit)
                return JS_NULL;
            auto v = JS_NewObject(c);
            JS_SetPropertyStr(c, v, "point", Vec(c, glm::vec3(hit.point)));
            JS_SetPropertyStr(c, v, "normal", Vec(c, glm::vec3(hit.normal)));
            JS_SetPropertyStr(c, v, "distance", JS_NewFloat64(c, hit.distance));
            auto location = JS_NewObject(c);
            JS_SetPropertyStr(c, location, "revision",
                              JS_NewFloat64(c, double(hit.location.topologyRevision)));
            JS_SetPropertyStr(c, location, "epoch",
                              JS_NewString(c, std::to_string(hit.location.generation).c_str()));
            JS_SetPropertyStr(c, location, "triangle", JS_NewUint32(c, hit.location.triangle));
            JS_SetPropertyStr(c, location, "weights", Vec(c, glm::vec3(hit.location.weights)));
            JS_SetPropertyStr(c, v, "location", location);
            return v;
        }
        if (!s->fixed)
            return JS_ThrowTypeError(c, "deformable mutation belongs in fixedUpdate");
        if (op == "deformableEnabled") {
            if (!JS_IsBool(arg(3)))
                return JS_ThrowTypeError(c, "boolean required");
            world.SetDeformableEnabled(id, JS_ToBool(c, arg(3)));
            return JS_TRUE;
        }
        if (op == "deformableReset")
            return world.ResetDeformable(id, error) ? JS_TRUE
                                                    : JS_ThrowTypeError(c, "%s", error.c_str());
        if (op == "deformableRelease")
            return JS_NewBool(c, d->Release(String(c, arg(3))));
        if (op == "deformableForce" || op == "deformableImpulse") {
            glm::vec3 value;
            if (!ReadVec(c, arg(3), value))
                return JS_ThrowTypeError(c, "finite vector required");
            bool ok = false;
            if (d->asset->fracture && d->asset->fracture->rigid) {
                auto group = String(c, arg(4));
                auto it = d->asset->groups.find(group);
                if (!group.empty() && it == d->asset->groups.end())
                    return JS_FALSE;
                double total = 0;
                std::vector<double> weights(d->asset->fracture->parts.size(), 0);
                for (unsigned p = 0; p < weights.size(); ++p)
                    if (!d->fracture.removed[p])
                        for (auto n : d->asset->fracture->parts[p].nodes)
                            if (group.empty() || std::find(it->second.begin(), it->second.end(),
                                                           n) != it->second.end()) {
                                weights[p] += d->NodeMass(n);
                                total += d->NodeMass(n);
                            }
                if (total > 0) {
                    ok = true;
                    for (unsigned p = 0; p < weights.size(); ++p)
                        if (weights[p] > 0)
                            ok = world.LoadFracturePart(id, p, value * float(weights[p] / total),
                                                        op == "deformableImpulse") &&
                                 ok;
                }
            } else
                ok = op == "deformableForce" ? d->Force(String(c, arg(4)), value)
                                             : d->Impulse(String(c, arg(4)), value);
            return JS_NewBool(c, ok);
        }
        if (op == "deformableHitImpulse") {
            DeformableLocation location;
            auto epoch = JS_GetPropertyStr(c, arg(3), "epoch"),
                 triangle = JS_GetPropertyStr(c, arg(3), "triangle"),
                 weights = JS_GetPropertyStr(c, arg(3), "weights");
            glm::vec3 w, impulse;
            bool ok = ReadVec(c, weights, w) && ReadVec(c, arg(4), impulse) &&
                      JS_ToUint32(c, &location.triangle, triangle) == 0;
            try {
                location.generation = std::stoull(String(c, epoch));
            } catch (...) {
                ok = false;
            }
            JS_FreeValue(c, epoch);
            JS_FreeValue(c, triangle);
            JS_FreeValue(c, weights);
            location.weights = w;
            auto rev = JS_GetPropertyStr(c, arg(3), "revision");
            double revision = 0;
            if (!JS_IsUndefined(rev))
                ok = ok && JS_ToFloat64(c, &revision, rev) == 0 && std::isfinite(revision) &&
                     revision >= 0;
            location.topologyRevision = uint64_t(revision);
            JS_FreeValue(c, rev);
            if (d->asset->fracture && d->asset->fracture->rigid) {
                ok = ok && location.generation == d->generation &&
                     location.topologyRevision == d->fracture.revision &&
                     location.triangle < d->asset->triangles.size() &&
                     glm::all(glm::greaterThanEqual(w, glm::vec3(0))) &&
                     std::abs(w.x + w.y + w.z - 1) < 1e-6f;
                if (!ok)
                    return JS_FALSE;
                auto face = d->asset->triangles[location.triangle];
                unsigned part = d->asset->fracture->nodePart[face.x];
                auto entity = world.FracturePartEntity(id, part);
                if (!entity)
                    return JS_FALSE;
                glm::vec3 point(0);
                for (int k = 0; k < 3; ++k)
                    point += glm::vec3(d->positions[face[k]]) * w[k];
                world.Physics().ApplyImpulseAtPoint(world.RuntimeBody(entity), impulse, point);
                return JS_TRUE;
            }
            return JS_NewBool(c, ok && d->Impulse(location, impulse));
        }
        if (op == "deformableMaterial") {
            auto m = d->settings.material;
            const char *names[] = {"density",        "stretchCompliance", "shearCompliance",
                                   "bendCompliance", "volumeCompliance",  "damping",
                                   "thickness",      "friction",          "airDrag",
                                   "yieldStrain",    "plasticRate",       "maximumPlasticStrain"};
            double *values[] = {&m.density,        &m.stretchCompliance, &m.shearCompliance,
                                &m.bendCompliance, &m.volumeCompliance,  &m.damping,
                                &m.thickness,      &m.friction,          &m.airDrag,
                                &m.yieldStrain,    &m.plasticRate,       &m.maximumPlasticStrain};
            for (unsigned i = 0; i < 12; ++i) {
                auto v = JS_GetPropertyStr(c, arg(3), names[i]);
                bool ok = JS_IsUndefined(v) || JS_ToFloat64(c, values[i], v) == 0;
                JS_FreeValue(c, v);
                if (!ok)
                    return JS_ThrowTypeError(c, "invalid material number");
            }
            auto v = JS_GetPropertyStr(c, arg(3), "airVelocity");
            glm::vec3 air;
            bool has = !JS_IsUndefined(v), ok = !has || ReadVec(c, v, air);
            JS_FreeValue(c, v);
            if (!ok)
                return JS_ThrowTypeError(c, "invalid air velocity");
            if (has)
                m.airVelocity = air;
            return d->SetMaterial(m, error) ? JS_TRUE : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "deformableAttach") {
            if (d->asset->fracture && d->asset->fracture->rigid)
                return JS_ThrowTypeError(c, "rigid fracture supports are authored; runtime release "
                                            "and ordinary piece joints remain available");
            DeformableAttachment a;
            a.group = String(c, arg(3));
            if (!d->asset->groups.count(a.group))
                return JS_ThrowTypeError(c, "unknown attachment group");
            auto kind = JS_GetPropertyStr(c, arg(4), "kind"),
                 entity = JS_GetPropertyStr(c, arg(4), "targetId"),
                 joint = JS_GetPropertyStr(c, arg(4), "joint"),
                 offset = JS_GetPropertyStr(c, arg(4), "offset");
            auto name = String(c, kind);
            bool ok = name == "world" || name == "body" || name == "bone";
            a.kind = name == "body"   ? DeformableAttachment::Kind::Body
                     : name == "bone" ? DeformableAttachment::Kind::Bone
                                      : DeformableAttachment::Kind::World;
            if (a.kind != DeformableAttachment::Kind::World) {
                try {
                    a.target = std::stoull(String(c, entity));
                } catch (...) {
                    ok = false;
                }
                ok = ok && world.RuntimeDefinition(a.target);
                if (a.kind == DeformableAttachment::Kind::Body)
                    ok = ok && world.Physics().IsBodyEnabled(world.RuntimeBody(a.target));
                if (a.kind == DeformableAttachment::Kind::Bone) {
                    a.joint = String(c, joint);
                    auto *animation = world.RuntimeAnimation(a.target);
                    ok = ok && animation && animation->asset &&
                         FindSkeletonJoint(animation->asset->skeleton, a.joint) >= 0;
                }
            }
            glm::vec3 v{0};
            if (!JS_IsUndefined(offset)) {
                ok = ok && ReadVec(c, offset, v);
                a.offset = v;
            }
            JS_FreeValue(c, kind);
            JS_FreeValue(c, entity);
            JS_FreeValue(c, joint);
            JS_FreeValue(c, offset);
            for (auto field : {std::pair<const char *, double *>{"forceLimit", &a.forceLimit},
                               {"torqueLimit", &a.torqueLimit}}) {
                auto number = JS_GetPropertyStr(c, arg(4), field.first);
                if (!JS_IsUndefined(number))
                    ok = ok && JS_IsNumber(number) && JS_ToFloat64(c, field.second, number) == 0;
                JS_FreeValue(c, number);
            }
            if (a.forceLimit > 0 || a.torqueLimit > 0)
                ok = ok && a.kind == DeformableAttachment::Kind::Body &&
                     world.Physics().IsDynamicBody(world.RuntimeBody(a.target));
            if (!ok)
                return JS_ThrowTypeError(
                    c,
                    "invalid attachment target/options; finite strengths require a dynamic body");
            auto config = d->settings;
            size_t index = 0;
            while (index < config.attachments.size() && config.attachments[index].group != a.group)
                ++index;
            if (index == config.attachments.size())
                config.attachments.push_back(a);
            else
                config.attachments[index] = a;
            if (!ValidDeformableSettings(config, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            d->settings = std::move(config);
            d->released.resize(d->settings.attachments.size());
            d->released[index] = 0;
            d->attachmentState.resize(d->settings.attachments.size());
            d->attachmentState[index] = {};
            d->Wake();
            world.InvalidateDeformableTargets(id);
            return JS_TRUE;
        }
        return JS_ThrowTypeError(c, "unknown deformable operation");
    }

    if (op.rfind("powertrain", 0) == 0) {
        EntityId owner = 0;
        try {
            auto text = String(c, arg(1));
            size_t end = 0;
            owner = std::stoull(text, &end);
            if (end != text.size())
                owner = 0;
        } catch (...) {
            owner = 0;
        }
        auto *power = world.RuntimePowertrain(owner);
        if (op == "powertrainExists" || op == "powertrainValid")
            return JS_NewBool(c, power != nullptr);
        if (!power)
            return JS_ThrowReferenceError(c, "stale entity or missing powertrain component");
        std::string error;
        if (op == "powertrainEnabled") {
            if (!s->fixed)
                return JS_ThrowTypeError(c, "powertrain mutation belongs in fixedUpdate");
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "powertrain enabled requires boolean");
            auto next = power->settings;
            next.enabled = JS_ToBool(c, arg(2));
            if (!world.SetPowertrainSettings(owner, next, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            return JS_TRUE;
        }
        if (op == "powertrainSet") {
            if (!s->fixed)
                return JS_ThrowTypeError(c, "powertrain mutation belongs in fixedUpdate");
            std::string bytes;
            if (!s->Json(arg(2), bytes, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            // Detached, bounded plain-data patches. Resolve every name and
            // validate the complete copy before publishing a held command.
            try {
                using Json = nlohmann::json;
                const auto patch = Json::parse(bytes);
                auto next = power->command;
                auto keys = [](const Json &value, std::initializer_list<const char *> allowed) {
                    if (!value.is_object())
                        throw std::runtime_error("powertrain patch must be an object");
                    for (auto it = value.begin(); it != value.end(); ++it) {
                        bool known = false;
                        for (auto name : allowed)
                            known |= it.key() == name;
                        if (!known)
                            throw std::runtime_error("unknown powertrain patch field: " + it.key());
                    }
                };
                keys(patch, {"sources", "links", "brakes"});
                auto apply = [&](const char *key, const auto &definitions, auto callback) {
                    if (!patch.contains(key))
                        return;
                    const auto &list = patch.at(key);
                    if (!list.is_array() || list.size() > kPowertrainMaxRows)
                        throw std::runtime_error("bounded named powertrain patch array required");
                    std::set<std::string> seen;
                    for (const auto &value : list) {
                        if (!value.is_object() || !value.contains("name") ||
                            !value.at("name").is_string())
                            throw std::runtime_error("named powertrain patch required");
                        auto name = value.at("name").get<std::string>();
                        if (!seen.insert(name).second)
                            throw std::runtime_error("duplicate powertrain patch name");
                        auto found = std::find_if(
                            definitions.begin(), definitions.end(),
                            [&](const auto &definition) { return definition.name == name; });
                        if (found == definitions.end())
                            throw std::runtime_error("unknown powertrain member: " + name);
                        callback(size_t(found - definitions.begin()), value);
                    }
                };
                apply("sources", power->settings.sources, [&](size_t i, const Json &value) {
                    keys(value, {"name", "input", "customTorque", "starter"});
                    auto &command = next.sources[i];
                    if (value.contains("input"))
                        command.input = value.at("input").get<double>();
                    if (value.contains("customTorque"))
                        command.customTorque = value.at("customTorque").get<double>();
                    if (value.contains("starter"))
                        command.starter = value.at("starter").get<double>();
                });
                apply("links", power->settings.links, [&](size_t i, const Json &value) {
                    keys(value, {"name", "enabled", "engagement", "ratio"});
                    auto &command = next.links[i];
                    if (value.contains("enabled"))
                        command.enabled = value.at("enabled").get<bool>();
                    if (value.contains("engagement"))
                        command.engagement = value.at("engagement").get<double>();
                    if (value.contains("ratio")) {
                        command.overrideRatio = !value.at("ratio").is_null();
                        if (command.overrideRatio)
                            command.ratio = value.at("ratio").get<double>();
                    }
                });
                apply("brakes", power->settings.brakes, [&](size_t i, const Json &value) {
                    keys(value, {"name", "engagement"});
                    if (value.contains("engagement"))
                        next.brakes[i].engagement = value.at("engagement").get<double>();
                });
                if (!ValidatePowertrainCommand(power->settings, next, error))
                    throw std::runtime_error(error);
                power->command = std::move(next);
                return JS_TRUE;
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "%s", e.what());
            }
        }
        std::string bytes;
        if (op == "powertrainSettings")
            bytes = EncodePowertrainSettings(power->settings);
        else if (op == "powertrainState") {
            using Json = nlohmann::json;
            const auto &state = power->state;
            Json value = {{"available", state.available},
                          {"converged", state.converged},
                          {"diagnostic", state.diagnostic},
                          {"iterations", state.iterations},
                          {"residual", state.residual},
                          {"kineticEnergy", state.kineticEnergy},
                          {"constraintEnergyChange", state.constraintEnergyChange}};
            value["shafts"] = Json::array();
            value["sources"] = Json::array();
            value["links"] = Json::array();
            value["brakes"] = Json::array();
            for (size_t i = 0; i < state.shafts.size(); ++i) {
                const auto &shaft = state.shafts[i];
                value["shafts"].push_back(
                    {{"name", power->settings.shafts[i].name},
                     {"available", shaft.available},
                     {"angularVelocity", shaft.angularVelocity},
                     {"rpm", shaft.angularVelocity * 60 / (2 * 3.14159265358979323846)},
                     {"angle", shaft.angle},
                     {"torque", shaft.torque}});
            }
            for (size_t i = 0; i < state.sourceTorque.size(); ++i)
                value["sources"].push_back(
                    {{"name", power->settings.sources[i].name}, {"torque", state.sourceTorque[i]}});
            for (size_t i = 0; i < state.links.size(); ++i) {
                const auto &link = state.links[i];
                value["links"].push_back({{"name", power->settings.links[i].name},
                                          {"active", link.active},
                                          {"connected", link.connected},
                                          {"saturated", link.saturated},
                                          {"impulse", link.impulse},
                                          {"torque", link.torque},
                                          {"slip", link.slip}});
            }
            for (size_t i = 0; i < state.brakeTorque.size(); ++i)
                value["brakes"].push_back(
                    {{"name", power->settings.brakes[i].name}, {"torque", state.brakeTorque[i]}});
            bytes = value.dump();
        } else
            return JS_ThrowTypeError(c, "unknown powertrain operation");
        return JS_ParseJSON(c, bytes.data(), bytes.size(), "powertrain snapshot");
    }

    if (op.rfind("wheel", 0) == 0) {
        EntityId owner = 0;
        try {
            auto text = String(c, arg(1));
            size_t end = 0;
            owner = std::stoull(text, &end);
            if (end != text.size())
                owner = 0;
        } catch (...) {
            owner = 0;
        }
        auto *wheel = world.RuntimeWheel(owner);
        if (op == "wheelExists" || op == "wheelValid")
            return JS_NewBool(c, wheel != nullptr);
        if (!wheel)
            return JS_ThrowReferenceError(c, "stale entity or missing wheel component");
        if (op == "wheelSet" || op == "wheelConfigure" || op == "wheelEnabled") {
            if (!s->fixed)
                return JS_ThrowTypeError(c, "wheel mutation belongs in fixedUpdate");
            std::string error;
            if (op == "wheelSet") {
                if (JS_IsArray(arg(2)) ||
                    !KnownOptions(c, arg(2), {"driveTorque", "brakeTorque", "steeringAngle"}))
                    return JS_ThrowTypeError(
                        c, "wheel commands accept driveTorque/brakeTorque/steeringAngle only");
                auto next = wheel->command;
                const char *names[] = {"driveTorque", "brakeTorque", "steeringAngle"};
                float *values[] = {&next.driveTorque, &next.brakeTorque, &next.steeringAngle};
                for (unsigned i = 0; i < 3; ++i) {
                    auto v = JS_GetPropertyStr(c, arg(2), names[i]);
                    bool present = !JS_IsUndefined(v);
                    JS_FreeValue(c, v);
                    if (present && !Number(c, arg(2), names[i], *values[i]))
                        return JS_ThrowTypeError(c, "finite wheel command required");
                }
                if (!ValidateWheelCommand(next, error))
                    return JS_ThrowTypeError(c, "%s", error.c_str());
                wheel->command = next;
                return JS_TRUE;
            }
            auto next = wheel->settings;
            if (op == "wheelEnabled") {
                if (!JS_IsBool(arg(2)))
                    return JS_ThrowTypeError(c, "wheel enabled requires boolean");
                next.enabled = JS_ToBool(c, arg(2));
            } else {
                if (JS_IsArray(arg(2)) ||
                    !KnownOptions(c, arg(2),
                                  {"friction", "springStiffness", "damping", "rollingResistance", "reboundRatio", "longitudinalStiffness", "lateralStiffness"}))
                    return JS_ThrowTypeError(
                        c, "wheel configuration accepts "
                           "friction/springStiffness/damping/rollingResistance/reboundRatio/longitudinalStiffness/lateralStiffness only");
                const char *names[] = {"friction", "springStiffness", "damping",
                                       "rollingResistance", "reboundRatio", "longitudinalStiffness", "lateralStiffness"};
                float *values[] = {&next.friction, &next.springStiffness, &next.damping,
                                   &next.rollingResistance, &next.reboundRatio, &next.longitudinalStiffness, &next.lateralStiffness};
                for (unsigned i = 0; i < 7; ++i) {
                    auto v = JS_GetPropertyStr(c, arg(2), names[i]);
                    bool present = !JS_IsUndefined(v);
                    JS_FreeValue(c, v);
                    if (present && !Number(c, arg(2), names[i], *values[i]))
                        return JS_ThrowTypeError(c, "finite wheel setting required");
                }
            }
            if (!ValidateWheelSettings(next, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            if (!world.SetWheelSettings(owner, next))
                return JS_ThrowTypeError(c, "wheel configuration unavailable or rejected");
            return JS_TRUE;
        }
        auto result = JS_NewObject(c);
        auto number = [&](const char *key, float value) {
            JS_SetPropertyStr(c, result, key, JS_NewFloat64(c, value));
        };
        auto vector = [&](const char *key, glm::vec3 value) {
            JS_SetPropertyStr(c, result, key, Vec(c, value));
        };
        auto quaternion = [&](const char *key, glm::quat value) {
            auto q = JS_NewObject(c);
            JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, value.w));
            JS_SetPropertyStr(c, q, "x", JS_NewFloat64(c, value.x));
            JS_SetPropertyStr(c, q, "y", JS_NewFloat64(c, value.y));
            JS_SetPropertyStr(c, q, "z", JS_NewFloat64(c, value.z));
            JS_SetPropertyStr(c, result, key, q);
        };
        if (op == "wheelSettings") {
            const auto &config = wheel->settings;
            JS_SetPropertyStr(c, result, "enabled", JS_NewBool(c, config.enabled));
            JS_SetPropertyStr(c, result, "chassisId",
                              JS_NewString(c, std::to_string(config.chassis).c_str()));
            JS_SetPropertyStr(c, result, "part", JS_NewString(c, config.part.c_str()));
            vector("mount", config.mount);
            quaternion("orientation", config.orientation);
            number("radius", config.radius);
            number("width", config.width);
            number("minLength", config.minLength);
            number("restLength", config.restLength);
            number("maxLength", config.maxLength);
            number("springStiffness", config.springStiffness);
            number("damping", config.damping);
            number("reboundRatio", config.reboundRatio);
            number("longitudinalStiffness", config.longitudinalStiffness);
            number("lateralStiffness", config.lateralStiffness);
            number("spinInertia", config.spinInertia);
            number("friction", config.friction);
            number("rollingResistance", config.rollingResistance);
            const auto &categories = world.Categories();
            auto layer = categories.collision.names.find(config.collisionLayer);
            JS_SetPropertyStr(c, result, "collisionLayer",
                              layer == categories.collision.names.end()
                                  ? JS_NULL
                                  : JS_NewString(c, layer->second.c_str()));
            auto names = [&](const CategoryRegistry &registry, CategoryMask mask) {
                auto array = JS_NewArray(c);
                uint32_t n = 0;
                for (const auto &[id, name] : registry.names)
                    if (mask & CategoryBit(id))
                        JS_SetPropertyUint32(c, array, n++, JS_NewString(c, name.c_str()));
                return array;
            };
            JS_SetPropertyStr(c, result, "collisionMask",
                              names(categories.collision, config.collisionMask));
            JS_SetPropertyStr(c, result, "requiredTags",
                              names(categories.tags, config.requiredTags));
            JS_SetPropertyStr(c, result, "excludedTags",
                              names(categories.tags, config.excludedTags));
            return result;
        }
        if (op == "wheelState") {
            const auto &state = wheel->state;
            JS_SetPropertyStr(c, result, "available", JS_NewBool(c, state.available));
            JS_SetPropertyStr(c, result, "contact", JS_NewBool(c, state.contact));
            JS_SetPropertyStr(c, result, "initialOverlap", JS_NewBool(c, state.initialOverlap));
            JS_SetPropertyStr(c, result, "sleeping", JS_NewBool(c, state.sleeping));
            JS_SetPropertyStr(c, result, "diagnostic", JS_NewString(c, state.diagnostic.c_str()));
            number("slipRatio", state.slipRatio);
            number("slipAngle", state.slipAngle);
            number("gripUtilization", state.gripUtilization);
            JS_SetPropertyStr(
                c, result, "contactEntityId",
                JS_NewString(c, std::to_string(world.EntityIdOfBody(state.body)).c_str()));
            vector("point", state.point);
            vector("normal", state.normal);
            vector("groundVelocity", state.groundVelocity);
            vector("center", state.center);
            quaternion("rotation", state.rotation);
            number("length", state.length);
            number("normalForce", state.normalForce);
            number("angularVelocity", state.angularVelocity);
            number("angle", state.angle);
            number("longitudinalSlip", state.longitudinalSlip);
            number("lateralSlip", state.lateralSlip);
            number("longitudinalImpulse", state.longitudinalImpulse);
            number("lateralImpulse", state.lateralImpulse);
            return result;
        }
        JS_FreeValue(c, result);
        return JS_ThrowTypeError(c, "unknown wheel operation");
    }
    return JS_ThrowTypeError(c, "unhandled Mechanics operation");
}
} // namespace
void RegisterMechanicsBindings(BindingRegistry &registry) {
    registry.Add(
        "Mechanics",
        {"fractureExists",       "fractureValid",      "fractureState",     "fractureRemove",
         "fractureRelease",      "fractureForce",      "fractureImpulse",   "deformableExists",
         "deformableValid",      "deformableState",    "deformableRaycast", "deformableEnabled",
         "deformableReset",      "deformableRelease",  "deformableForce",   "deformableImpulse",
         "deformableHitImpulse", "deformableMaterial", "deformableAttach",  "powertrainExists",
         "powertrainValid",      "powertrainEnabled",  "powertrainSet",     "powertrainSettings",
         "powertrainState",      "wheelExists",        "wheelValid",        "wheelSet",
         "wheelConfigure",       "wheelEnabled",       "wheelSettings",     "wheelState"},
        HandleMechanics);
}
} // namespace ScriptBindings
