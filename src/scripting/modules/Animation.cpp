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

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleAnimation(BindingContext &context, const std::string &op, int argc,
                        JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    auto readTransform = [&](JSValueConst value, SceneTransform &t) {
        auto p = JS_GetPropertyStr(c, value, "position"),
             r = JS_GetPropertyStr(c, value, "rotation"),
             scale = JS_GetPropertyStr(c, value, "scale");
        bool ok = JS_IsUndefined(p) || ReadVec(c, p, t.position);
        if (!JS_IsUndefined(r))
            ok = ok && Number(c, r, "w", t.rotation.w) && Number(c, r, "x", t.rotation.x) &&
                 Number(c, r, "y", t.rotation.y) && Number(c, r, "z", t.rotation.z) &&
                 std::isfinite(glm::dot(t.rotation, t.rotation)) &&
                 glm::dot(t.rotation, t.rotation) > 1e-12f;
        if (!JS_IsUndefined(scale))
            ok =
                ok && ReadVec(c, scale, t.scale) && t.scale.x > 0 && t.scale.y > 0 && t.scale.z > 0;
        JS_FreeValue(c, p);
        JS_FreeValue(c, r);
        JS_FreeValue(c, scale);
        return ok;
    };

    EntityId id = 0;
    try {
        auto text = String(c, arg(1));
        size_t end;
        id = std::stoull(text, &end);
        if (end != text.size())
            id = 0;
    } catch (...) {
        id = 0;
    }
    const auto *definition = world.RuntimeDefinition(id);
    if (!definition)
        return JS_ThrowReferenceError(c, "stale or invalid entity %llu", (unsigned long long)id);

    if (op.rfind("ragdoll", 0) == 0) {
        if (op == "ragdollExists")
            return JS_NewBool(c, definition->ragdoll.has_value());
        if (!definition->ragdoll)
            return JS_ThrowTypeError(c, "entity has no ragdoll mapping");
        if (op == "ragdollContactEvents") {
            if (argc < 3)
                return JS_NewBool(c, definition->ragdoll->receiveContactEvents);
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "boolean required");
            return JS_NewBool(c, world.SetRagdollContactEvents(id, JS_ToBool(c, arg(2))));
        }
        if (op == "ragdollBalanceDiagnostics") {
            if (!definition->ragdoll->balance)
                return JS_ThrowTypeError(c, "balance configuration unavailable");
            if (argc < 3)
                return JS_NewBool(c, definition->ragdoll->balance->visualize);
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "balance diagnostics requires boolean");
            return JS_NewBool(c, world.SetPhysicalBalanceDiagnostics(id, JS_ToBool(c, arg(2))));
        }
        if (op == "ragdollBalanceConfigure") {
            std::optional<BalanceDefinition> settings;
            std::string error, json;
            if (!JS_IsNull(arg(2))) {
                BalanceDefinition parsed;
                if (!s->Json(arg(2), json, error) || !ParseBalanceDefinition(json, parsed, error))
                    return JS_ThrowTypeError(c, "balance settings: %s", error.c_str());
                settings = std::move(parsed);
            }
            return world.ConfigurePhysicalBalance(id, settings, error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "ragdollBalanceCancel") {
            world.CancelPhysicalBalance(id);
            return JS_TRUE;
        }
        if (op == "ragdollBalanceRequest") {
            RuntimeWorld::BalanceRequest request;
            const auto task = String(c, arg(2));
            if (task == "stand")
                request.task = BalanceTask::Stand;
            else if (task == "recover")
                request.task = BalanceTask::Recover;
            else
                return JS_ThrowTypeError(c, "balance task must be stand/recover");
            std::string error, json;
            if (!s->Json(arg(3), json, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            try {
                auto options = nlohmann::json::parse(json);
                if (!options.is_object())
                    throw std::runtime_error("balance options must be an object");
                for (auto it = options.begin(); it != options.end(); ++it)
                    if (it.key() != "stepping" && it.key() != "effort" &&
                        it.key() != "motorHandoff")
                        throw std::runtime_error("unknown balance option: " + it.key());
                request.stepping = options.value("stepping", true);
                request.effort = options.value("effort", true);
                request.motorHandoff = options.value("motorHandoff", false);
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "%s", e.what());
            }
            return world.RequestPhysicalBalance(id, request, error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "ragdollBalanceState") {
            const auto *state = world.PhysicalBalanceSnapshot(id);
            BalanceHistory history;
            BalanceObservation observation;
            if (state) {
                history = state->controller.History();
                observation = state->controller.Observation();
            }
            const char *statuses[] = {"idle", "executing", "succeeded", "failed", "cancelled"};
            const char *tasks[] = {"none", "stand", "recover"};
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "task", JS_NewString(c, tasks[int(history.task)]));
            JS_SetPropertyStr(c, o, "status", JS_NewString(c, statuses[int(history.progress)]));
            JS_SetPropertyStr(c, o, "pending",
                              state && state->pending
                                  ? JS_NewString(c, tasks[int(state->pending->task)])
                                  : JS_NULL);
            JS_SetPropertyStr(c, o, "diagnostic",
                              JS_NewString(c, state ? state->diagnostic.c_str() : ""));
            JS_SetPropertyStr(c, o, "available", JS_NewBool(c, observation.available));
            JS_SetPropertyStr(c, o, "com", Vec(c, observation.com));
            JS_SetPropertyStr(c, o, "velocity", Vec(c, observation.velocity));
            JS_SetPropertyStr(c, o, "gravity", Vec(c, observation.gravity));
            JS_SetPropertyStr(c, o, "up", Vec(c, observation.up));
            JS_SetPropertyStr(c, o, "mass", JS_NewFloat64(c, observation.mass));
            JS_SetPropertyStr(c, o, "height", JS_NewFloat64(c, observation.height));
            JS_SetPropertyStr(c, o, "upright", JS_NewFloat64(c, observation.upright));
            JS_SetPropertyStr(c, o, "speed", JS_NewFloat64(c, observation.speed));
            JS_SetPropertyStr(c, o, "saturated", JS_NewUint32(c, observation.saturated));
            JS_SetPropertyStr(c, o, "elapsed", JS_NewFloat64(c, history.elapsed));
            JS_SetPropertyStr(c, o, "settled", JS_NewFloat64(c, history.settled));
            JS_SetPropertyStr(c, o, "referenceTime", JS_NewFloat64(c, history.referenceTime));
            auto supports = JS_NewArray(c);
            for (unsigned f = 0; f < 2; ++f) {
                const auto &support = observation.supports[f];
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(
                    c, v, "entityId",
                    JS_NewString(c, std::to_string(world.EntityIdOfBody(support.body)).c_str()));
                JS_SetPropertyStr(c, v, "point", Vec(c, support.point));
                JS_SetPropertyStr(c, v, "normal", Vec(c, support.normal));
                JS_SetPropertyStr(c, v, "velocity", Vec(c, support.velocity));
                JS_SetPropertyStr(c, v, "loadImpulse", JS_NewFloat64(c, support.load));
                JS_SetPropertyStr(c, v, "contacts", JS_NewUint32(c, support.contacts));
                JS_SetPropertyUint32(c, supports, f, v);
            }
            JS_SetPropertyStr(c, o, "supports", supports);
            auto step = JS_NewObject(c);
            JS_SetPropertyStr(c, step, "foot",
                              history.swing >= 0 ? JS_NewInt32(c, history.swing) : JS_NULL);
            JS_SetPropertyStr(c, step, "time", JS_NewFloat64(c, history.swingTime));
            JS_SetPropertyStr(c, step, "lifted", JS_NewBool(c, history.lifted));
            JS_SetPropertyStr(c, step, "landings", JS_NewUint32(c, history.landings));
            auto target = world.Physics().GetTransform(history.plannedSupport);
            JS_SetPropertyStr(c, step, "landing",
                              world.Physics().IsBodyEnabled(history.plannedSupport)
                                  ? Vec(c, target.position + target.rotation * history.landingLocal)
                                  : JS_NULL);
            JS_SetPropertyStr(c, o, "step", step);
            return o;
        }
        if (op == "ragdollPhysicalConfigure") {
            std::optional<PhysicalAnimationSettings> settings;
            std::string error, json;
            if (!JS_IsNull(arg(2))) {
                PhysicalAnimationSettings parsed;
                if (!s->Json(arg(2), json, error) ||
                    !ParsePhysicalAnimationSettings(json, parsed, error))
                    return JS_ThrowTypeError(c, "physical settings: %s", error.c_str());
                settings = std::move(parsed);
            }
            return world.ConfigurePhysicalAnimation(id, settings, error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "ragdollMode") {
            PhysicalAnimationRequest request;
            std::string error, json;
            if (!JS_IsString(arg(2)) ||
                !ParsePhysicalAnimationMode(String(c, arg(2)), request.mode))
                return JS_ThrowTypeError(c, "mode must be animation/partial/active/passive");
            if (!s->Json(arg(3), json, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            try {
                auto options = nlohmann::json::parse(json);
                if (!options.is_object())
                    throw std::runtime_error("mode options must be an object");
                for (auto it = options.begin(); it != options.end(); ++it)
                    if (it.key() != "fade" && it.key() != "motorHandoff" &&
                        it.key() != "resumeMotor" && it.key() != "placement")
                        throw std::runtime_error("unknown mode option: " + it.key());
                request.fade = options.value("fade", request.fade);
                request.motorHandoff = options.value("motorHandoff", false);
                request.resumeMotor = options.value("resumeMotor", false);
                if (options.contains("placement")) {
                    auto place = options.at("placement");
                    if (!place.is_object() || place.size() != 2)
                        throw std::runtime_error("placement needs position and rotation");
                    auto p = place.at("position"), q = place.at("rotation");
                    if (!p.is_array() || p.size() != 3 || !q.is_array() || q.size() != 4)
                        throw std::runtime_error("placement arrays have lengths 3/4");
                    PhysicalAnimationPlacement value;
                    value.position = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                    value.rotation = {q[3].get<float>(), q[0].get<float>(), q[1].get<float>(),
                                      q[2].get<float>()};
                    request.placement = value;
                }
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "%s", e.what());
            }
            return world.RequestPhysicalAnimation(id, request, error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "ragdollPhysicalState") {
            auto state = world.PhysicalAnimationSnapshot(id);
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "mode", JS_NewString(c, PhysicalAnimationModeName(state.mode)));
            JS_SetPropertyStr(c, o, "pending",
                              state.pending
                                  ? JS_NewString(c, PhysicalAnimationModeName(state.pending->mode))
                                  : JS_NULL);
            JS_SetPropertyStr(c, o, "diagnostic", JS_NewString(c, state.diagnostic.c_str()));
            auto observations = JS_NewArray(c);
            unsigned index = 0;
            for (const auto &drive : state.observations) {
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(c, v, "joint", JS_NewString(c, drive.joint.c_str()));
                JS_SetPropertyStr(c, v, "region", JS_NewString(c, drive.region.c_str()));
                JS_SetPropertyStr(c, v, "angleError", JS_NewFloat64(c, drive.angleError));
                JS_SetPropertyStr(c, v, "torque", JS_NewFloat64(c, drive.torque));
                JS_SetPropertyStr(c, v, "saturated", JS_NewBool(c, drive.saturated));
                JS_SetPropertyStr(c, v, "sleeping", JS_NewBool(c, drive.sleeping));
                JS_SetPropertyUint32(c, observations, index++, v);
            }
            JS_SetPropertyStr(c, o, "drives", observations);
            return o;
        }
        if (op == "ragdollActive")
            return JS_NewBool(c, world.RagdollActive(id));
        if (op == "ragdollBody") {
            if (!JS_IsString(arg(2)))
                return JS_ThrowTypeError(c, "joint key required");
            auto body = world.RagdollBody(id, String(c, arg(2)));
            return JS_NewString(c, std::to_string(body).c_str());
        }
        std::string error;
        bool ok = false;
        if (op == "ragdollEnter")
            ok = world.EnterRagdoll(id, error);
        else if (op == "ragdollLeave") {
            double seconds = 0;
            if (JS_ToFloat64(c, &seconds, arg(2)) != 0 || !std::isfinite(seconds) || seconds < 0 ||
                seconds > 3600)
                return JS_ThrowTypeError(c, "invalid return duration");
            ok = world.LeaveRagdoll(id, float(seconds), error);
        } else if (op == "ragdollEnabled") {
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "boolean required");
            ok = world.SetRagdollEnabled(id, JS_ToBool(c, arg(2)), error);
        }
        if (!ok)
            return JS_ThrowTypeError(c, "ragdoll: %s", error.c_str());
        return JS_TRUE;
    }

    if (op.rfind("animation", 0) == 0) {
        if (op == "animationExists")
            return JS_NewBool(c, definition->animation.has_value());
        auto *instance = world.RuntimeAnimation(id);
        if (!instance)
            return JS_ThrowTypeError(c, "entity has no animated mesh");
        if (op == "animationIKConfigure" || op == "animationIKTargets") {
            std::string error, json;
            std::optional<FullBodyIKSettings> settings;
            if (op == "animationIKConfigure") {
                if (!JS_IsNull(arg(2))) {
                    FullBodyIKSettings parsed;
                    if (!s->Json(arg(2), json, error) ||
                        !ParseFullBodyIKSettings(json, parsed, error))
                        return JS_ThrowTypeError(c, "%s", error.c_str());
                    settings = std::move(parsed);
                }
                return world.ConfigureFullBodyIK(id, settings, error)
                           ? JS_TRUE
                           : JS_ThrowTypeError(c, "%s", error.c_str());
            }
            if (!definition->animation->fullBodyIK)
                return JS_ThrowTypeError(c, "configureIK first");
            auto copy = *definition->animation->fullBodyIK;
            if (!s->Json(arg(2), json, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            try {
                auto config = nlohmann::json::parse(SerializeFullBodyIKSettings(copy));
                config["targets"] = nlohmann::json::parse(json);
                if (!ParseFullBodyIKSettings(config.dump(), copy, error))
                    return JS_ThrowTypeError(c, "%s", error.c_str());
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "%s", e.what());
            }
            return world.SetFullBodyIKTargets(id, copy.targets, error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "animationIKStatus") {
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "ready", JS_NewBool(c, bool(instance->asset)));
            JS_SetPropertyStr(c, o, "enabled",
                              JS_NewBool(c, definition->animation->fullBodyIK &&
                                                definition->animation->fullBodyIK->enabled));
            JS_SetPropertyStr(c, o, "diagnostic", JS_NewString(c, instance->error.c_str()));
            auto targets = JS_NewArray(c);
            if (instance->fullBodyIK) {
                const auto &result = instance->fullBodyIK->result;
                JS_SetPropertyStr(c, o, "converged", JS_NewBool(c, result.converged));
                JS_SetPropertyStr(c, o, "iterations", JS_NewInt32(c, result.iterations));
                JS_SetPropertyStr(c, o, "solveMicroseconds",
                                  JS_NewFloat64(c, result.solveMicroseconds));
                JS_SetPropertyStr(c, o, "rootCorrection", Vec(c, result.rootCorrection));
                unsigned i = 0;
                for (const auto &target : result.targets) {
                    auto v = JS_NewObject(c);
                    JS_SetPropertyStr(c, v, "id", JS_NewString(c, target.id.c_str()));
                    JS_SetPropertyStr(c, v, "status", JS_NewString(c, target.status.c_str()));
                    JS_SetPropertyStr(c, v, "positionError",
                                      JS_NewFloat64(c, target.positionError));
                    JS_SetPropertyStr(c, v, "orientationError",
                                      JS_NewFloat64(c, target.orientationError));
                    JS_SetPropertyStr(c, v, "actualPosition", Vec(c, target.actualPosition));
                    auto q = Vec(c, {target.actualOrientation.x, target.actualOrientation.y,
                                     target.actualOrientation.z});
                    JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, target.actualOrientation.w));
                    JS_SetPropertyStr(c, v, "actualOrientation", q);
                    JS_SetPropertyUint32(c, targets, i++, v);
                }
            } else {
                JS_SetPropertyStr(c, o, "converged", JS_FALSE);
                JS_SetPropertyStr(c, o, "iterations", JS_NewInt32(c, 0));
                JS_SetPropertyStr(c, o, "solveMicroseconds", JS_NewFloat64(c, 0));
                JS_SetPropertyStr(c, o, "rootCorrection", Vec(c, glm::vec3(0)));
            }
            JS_SetPropertyStr(c, o, "targets", targets);
            return o;
        }
        if (op == "animationAdvance" || op == "animationSample") {
            if (op == "animationAdvance") {
                if (!s->fixed || !s->animationControl)
                    return JS_ThrowTypeError(c, "animation advance belongs in fixedUpdate");
                double seconds = 0;
                if (JS_ToFloat64(c, &seconds, arg(2)) || !std::isfinite(seconds) || seconds <= 0 ||
                    seconds > 1)
                    return JS_ThrowRangeError(c, "advance requires 0 < seconds <= 1");
                std::string error;
                if (!world.AdvanceAnimation(id, float(seconds), error))
                    return JS_ThrowTypeError(c, "%s", error.c_str());
            }
            const auto &step = instance->step;
            auto result = JS_NewObject(c);
            auto motion = JS_NewObject(c);
            JS_SetPropertyStr(c, motion, "translation", Vec(c, step.motion.translation));
            auto rotation =
                Vec(c, {step.motion.rotation.x, step.motion.rotation.y, step.motion.rotation.z});
            JS_SetPropertyStr(c, rotation, "w", JS_NewFloat64(c, step.motion.rotation.w));
            JS_SetPropertyStr(c, motion, "rotation", rotation);
            JS_SetPropertyStr(c, result, "motion", motion);
            auto events = JS_NewArray(c);
            unsigned index = 0;
            for (const auto &hit : step.events) {
                auto e = JS_NewObject(c);
                for (const auto &field :
                     std::vector<std::pair<const char *, std::string>>{{"id", hit.event.id},
                                                                       {"name", hit.event.name},
                                                                       {"data", hit.event.data},
                                                                       {"clip", hit.clip},
                                                                       {"source", hit.source}})
                    JS_SetPropertyStr(c, e, field.first, JS_NewString(c, field.second.c_str()));
                JS_SetPropertyStr(c, e, "time", JS_NewFloat64(c, hit.event.time));
                JS_SetPropertyStr(c, e, "weight", JS_NewFloat64(c, hit.weight));
                JS_SetPropertyStr(c, e, "reverse", JS_NewBool(c, hit.reverse));
                JS_SetPropertyUint32(c, events, index++, e);
            }
            JS_SetPropertyStr(c, result, "events", events);
            JS_SetPropertyStr(c, result, "overflow", JS_NewBool(c, step.overflow));
            return result;
        }
        auto &player = instance->playback;
        if (op == "animationRootMotion") {
            if (!JS_IsString(arg(2)) || !JS_IsBool(arg(5)))
                return JS_ThrowTypeError(c, "clip name and loop flag required");
            double from = 0, to = 0;
            if (JS_ToFloat64(c, &from, arg(3)) || JS_ToFloat64(c, &to, arg(4)) ||
                !std::isfinite(from) || !std::isfinite(to) || std::abs(from) > 1e9 ||
                std::abs(to) > 1e9)
                return JS_ThrowRangeError(
                    c, "finite root-motion interval within +/-1e9 seconds required");
            auto name = String(c, arg(2));
            auto it = std::find_if(instance->asset->clips.begin(), instance->asset->clips.end(),
                                   [&](auto &clip) { return clip.name == name; });
            if (it == instance->asset->clips.end())
                return JS_NULL;
            JointTransform pose;
            try {
                pose = RootMotionInterval(*it, from, to, JS_ToBool(c, arg(5)));
            } catch (const std::exception &e) {
                return JS_ThrowRangeError(c, "%s", e.what());
            }
            auto result = JS_NewObject(c);
            JS_SetPropertyStr(c, result, "translation", Vec(c, pose.translation));
            auto q = Vec(c, {pose.rotation.x, pose.rotation.y, pose.rotation.z});
            JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, pose.rotation.w));
            JS_SetPropertyStr(c, result, "rotation", q);
            JS_SetPropertyStr(c, result, "extracted", JS_NewBool(c, !it->motion.empty()));
            return result;
        }

        if (op == "animationJointPose") {
            const auto space = String(c, arg(3));
            if (space != "local" && space != "model" && space != "world")
                return JS_ThrowTypeError(c, "joint space must be local/model/world");
            SceneTransform pose;
            float alpha = JS_ToBool(c, arg(4)) && s->inPresentation ? s->presentationAlpha : 1;
            if (!world.JointPose(id, String(c, arg(2)), space, alpha, pose))
                return JS_NULL;
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "position", Vec(c, pose.position));
            JS_SetPropertyStr(c, o, "scale", Vec(c, pose.scale));
            auto q = Vec(c, {pose.rotation.x, pose.rotation.y, pose.rotation.z});
            JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, pose.rotation.w));
            JS_SetPropertyStr(c, o, "rotation", q);
            return o;
        }
        if (op == "animationLimb" || op == "animationRemoveLimb") {
            LimbIKSettings k;
            k.id = String(c, arg(2));
            if (definition->animation)
                for (auto &old : definition->animation->limbs)
                    if (old.id == k.id)
                        k = old;
            if (op == "animationLimb") {
                auto o = arg(3);
                for (auto item : {std::pair<const char *, std::string *>{"root", &k.root},
                                  {"middle", &k.middle},
                                  {"end", &k.end}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    if (!JS_IsUndefined(v)) {
                        if (!JS_IsString(v)) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "joint key must be string");
                        }
                        *item.second = String(c, v);
                    }
                    JS_FreeValue(c, v);
                }
                for (auto item : {std::pair<const char *, glm::vec3 *>{"target", &k.target},
                                  {"pole", &k.pole}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    bool ok = JS_IsUndefined(v) || ReadVec(c, v, *item.second);
                    JS_FreeValue(c, v);
                    if (!ok)
                        return JS_ThrowTypeError(c, "finite IK vector required");
                }
                auto v = JS_GetPropertyStr(c, o, "weight");
                bool has = !JS_IsUndefined(v);
                JS_FreeValue(c, v);
                if (has && !Number(c, o, "weight", k.weight))
                    return JS_ThrowTypeError(c, "finite IK weight required");
                v = JS_GetPropertyStr(c, o, "order");
                if (!JS_IsUndefined(v)) {
                    double value;
                    if (JS_ToFloat64(c, &value, v) || !std::isfinite(value) ||
                        std::floor(value) != value || value < 1 || value > 999) {
                        JS_FreeValue(c, v);
                        return JS_ThrowTypeError(c, "IK order must be 1..999");
                    }
                    k.order = int(value);
                }
                JS_FreeValue(c, v);
                v = JS_GetPropertyStr(c, o, "enabled");
                if (!JS_IsUndefined(v)) {
                    if (!JS_IsBool(v)) {
                        JS_FreeValue(c, v);
                        return JS_ThrowTypeError(c, "IK enabled must be boolean");
                    }
                    k.enabled = JS_ToBool(c, v);
                }
                JS_FreeValue(c, v);
            }
            std::string error;
            return world.SetLimbIK(id, k, op == "animationRemoveLimb", error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "animationInfo") {
            auto result = JS_NewObject(c);
            JS_SetPropertyStr(c, result, "ready", JS_NewBool(c, bool(instance->asset)));
            JS_SetPropertyStr(c, result, "playing",
                              JS_NewBool(c, player.playing && !player.stopped));
            JS_SetPropertyStr(c, result, "loop", JS_NewBool(c, player.loop));
            JS_SetPropertyStr(c, result, "speed", JS_NewFloat64(c, player.speed));
            JS_SetPropertyStr(c, result, "time", JS_NewFloat64(c, player.time));
            JS_SetPropertyStr(c, result, "clip", JS_NewString(c, player.clip.c_str()));
            auto clips = JS_NewArray(c);
            uint32_t i = 0;
            if (instance->asset)
                for (const auto &clip : instance->asset->clips) {
                    auto value = JS_NewObject(c);
                    JS_SetPropertyStr(c, value, "name", JS_NewString(c, clip.name.c_str()));
                    JS_SetPropertyStr(c, value, "duration", JS_NewFloat64(c, clip.duration));
                    auto markers = JS_NewArray(c);
                    unsigned markerIndex = 0;
                    for (const auto &e : clip.events) {
                        auto marker = JS_NewObject(c);
                        JS_SetPropertyStr(c, marker, "id", JS_NewString(c, e.id.c_str()));
                        JS_SetPropertyStr(c, marker, "name", JS_NewString(c, e.name.c_str()));
                        JS_SetPropertyStr(c, marker, "data", JS_NewString(c, e.data.c_str()));
                        JS_SetPropertyStr(c, marker, "time", JS_NewFloat64(c, e.time));
                        JS_SetPropertyUint32(c, markers, markerIndex++, marker);
                    }
                    JS_SetPropertyStr(c, value, "events", markers);
                    JS_SetPropertyUint32(c, clips, i++, value);
                }
            JS_SetPropertyStr(c, result, "clips", clips);
            JS_SetPropertyStr(c, result, "transitioning",
                              JS_NewBool(c, instance->mixer.Transitioning()));
            JS_SetPropertyStr(c, result, "transitionFraction",
                              JS_NewFloat64(c, instance->mixer.Fraction()));
            JS_SetPropertyStr(c, result, "error", JS_NewString(c, instance->error.c_str()));
            auto joints = JS_NewArray(c);
            i = 0;
            if (instance->asset)
                for (size_t node = 0; node < instance->asset->skeleton.names.size(); ++node)
                    JS_SetPropertyUint32(
                        c, joints, i++,
                        JS_NewString(
                            c, SkeletonJointKey(instance->asset->skeleton, int(node)).c_str()));
            JS_SetPropertyStr(c, result, "joints", joints);
            auto layers = JS_NewArray(c);
            i = 0;
            for (const auto &l : instance->layers) {
                auto o = JS_NewObject(c);
                JS_SetPropertyStr(c, o, "id", JS_NewString(c, l.settings.id.c_str()));
                JS_SetPropertyStr(c, o, "clip", JS_NewString(c, l.settings.clip.c_str()));
                JS_SetPropertyStr(c, o, "weight", JS_NewFloat64(c, l.settings.weight));
                JS_SetPropertyStr(c, o, "enabled", JS_NewBool(c, l.settings.enabled));
                JS_SetPropertyStr(c, o, "additive", JS_NewBool(c, l.settings.additive));
                JS_SetPropertyUint32(c, layers, i++, o);
            }
            JS_SetPropertyStr(c, result, "layers", layers);
            return result;
        }
        if (op == "animationSet") {
            auto copy = player;
            auto v = JS_GetPropertyStr(c, arg(2), "speed");
            bool has = !JS_IsUndefined(v);
            JS_FreeValue(c, v);
            if (has && !Number(c, arg(2), "speed", copy.speed))
                return JS_ThrowTypeError(c, "invalid playback speed");
            v = JS_GetPropertyStr(c, arg(2), "loop");
            if (!JS_IsUndefined(v)) {
                if (!JS_IsBool(v)) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "loop must be boolean");
                }
                copy.loop = JS_ToBool(c, v);
            }
            JS_FreeValue(c, v);
            player = copy;
            return JS_TRUE;
        }
        if (op == "animationPause") {
            player.playing = false;
            instance->mixer.paused = true;
            return JS_TRUE;
        }
        if (!instance->asset)
            return JS_FALSE; // request remains in the ordinary async resource path
        if (op == "animationFade") {
            instance->step = {};
            double seconds;
            if (!JS_IsString(arg(2)) || JS_ToFloat64(c, &seconds, arg(3)) != 0 ||
                !std::isfinite(seconds) || seconds < 0 || seconds > 3600)
                return JS_ThrowTypeError(c, "invalid fade");
            auto clip = String(c, arg(2));
            if (std::none_of(instance->asset->clips.begin(), instance->asset->clips.end(),
                             [&](const auto &v) { return v.name == clip; }))
                return JS_ThrowTypeError(c, "unknown clip");
            if (seconds > 0 && instance->mixer.outgoing.size() >= 16)
                return JS_ThrowRangeError(c, "too many interrupted fade contributors");
            instance->mixer.CrossFade(player, clip, float(seconds));
            world.ResolveAnimationPose(*instance, 0);
            return JS_TRUE;
        }
        if (op == "animationLayer" || op == "animationRemoveLayer") {
            if (!JS_IsString(arg(2)))
                return JS_ThrowTypeError(c, "layer ID required");
            AnimationLayerSettings settings;
            settings.id = String(c, arg(2));
            for (const auto &l : instance->layers)
                if (l.settings.id == settings.id)
                    settings = l.settings;
            if (op == "animationLayer") {
                if (!JS_IsObject(arg(3)))
                    return JS_ThrowTypeError(c, "layer settings required");
                for (auto field : {"clip", "referenceClip"}) {
                    auto v = JS_GetPropertyStr(c, arg(3), field);
                    if (!JS_IsUndefined(v)) {
                        if (!JS_IsString(v)) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "invalid layer string");
                        }
                        (std::string(field) == "clip" ? settings.clip : settings.referenceClip) =
                            String(c, v);
                    }
                    JS_FreeValue(c, v);
                }
                for (auto item : {std::pair<const char *, float *>{"weight", &settings.weight},
                                  {"speed", &settings.speed},
                                  {"time", &settings.time},
                                  {"referenceTime", &settings.referenceTime}}) {
                    auto v = JS_GetPropertyStr(c, arg(3), item.first);
                    bool present = !JS_IsUndefined(v);
                    JS_FreeValue(c, v);
                    if (present && !Number(c, arg(3), item.first, *item.second))
                        return JS_ThrowTypeError(c, "invalid layer number");
                }
                for (auto item : {std::pair<const char *, bool *>{"enabled", &settings.enabled},
                                  {"additive", &settings.additive}}) {
                    auto v = JS_GetPropertyStr(c, arg(3), item.first);
                    if (!JS_IsUndefined(v)) {
                        if (!JS_IsBool(v)) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "invalid layer flag");
                        }
                        *item.second = JS_ToBool(c, v);
                    }
                    JS_FreeValue(c, v);
                }
                auto mask = JS_GetPropertyStr(c, arg(3), "mask");
                if (!JS_IsUndefined(mask)) {
                    if (!JS_IsArray(mask)) {
                        JS_FreeValue(c, mask);
                        return JS_ThrowTypeError(c, "mask must be an array of joint keys");
                    }
                    auto len = JS_GetPropertyStr(c, mask, "length");
                    uint32_t n = 0;
                    JS_ToUint32(c, &n, len);
                    JS_FreeValue(c, len);
                    if (n > kModelNodeLimit) {
                        JS_FreeValue(c, mask);
                        return JS_ThrowRangeError(c, "too many joints");
                    }
                    settings.mask.clear();
                    for (uint32_t i = 0; i < n; ++i) {
                        auto v = JS_GetPropertyUint32(c, mask, i);
                        if (!JS_IsString(v)) {
                            JS_FreeValue(c, v);
                            JS_FreeValue(c, mask);
                            return JS_ThrowTypeError(c, "joint key must be a string");
                        }
                        settings.mask.push_back(String(c, v));
                        JS_FreeValue(c, v);
                    }
                }
                JS_FreeValue(c, mask);
            }
            std::string error;
            if (!world.SetAnimationLayer(id, settings, op == "animationRemoveLayer", error))
                return JS_ThrowTypeError(c, "layer: %s", error.c_str());
            return JS_TRUE;
        }
        if (op == "animationPlay") {
            if (!JS_IsString(arg(2)))
                return JS_ThrowTypeError(c, "clip name must be a string");
            auto clip = String(c, arg(2));
            if (!clip.empty()) {
                if (std::none_of(instance->asset->clips.begin(), instance->asset->clips.end(),
                                 [&](const auto &v) { return v.name == clip; }))
                    return JS_ThrowTypeError(c, "unknown animation clip");
                player.time = 0;
                player.clip = clip;
                instance->mixer.Clear();
                instance->step = {};
            }
            player.playing = true;
            player.stopped = false;
            instance->mixer.paused = false;
        } else if (op == "animationStop") {
            player.Stop(*instance->asset);
            instance->step = {};
            instance->mixer.Clear();
            instance->mixer.paused = true;
        } else if (op == "animationSeek") {
            double seconds;
            if (JS_ToFloat64(c, &seconds, arg(2)) != 0 || !std::isfinite(seconds) || seconds < 0 ||
                seconds > 1e20)
                return JS_ThrowTypeError(c, "invalid animation seek");
            player.Seek(*instance->asset, float(seconds));
            instance->step = {};
        } else
            return JS_ThrowTypeError(c, "unknown animation operation");
        world.ResolveAnimationPose(*instance, 0);
        return JS_TRUE;
    }

    if (op == "socketSet" || op == "socketClear") {
        SceneSocketComponent socket;
        std::string error;
        if (op == "socketSet") {
            try {
                socket.target = std::stoull(String(c, arg(2)));
            } catch (...) {
                return JS_ThrowTypeError(c, "socket target required");
            }
            socket.joint = String(c, arg(3));
            if (!readTransform(arg(4), socket.offset))
                return JS_ThrowTypeError(c, "invalid socket offset");
        }
        return world.SetSocket(id, socket, op == "socketClear", error)
                   ? JS_TRUE
                   : JS_ThrowTypeError(c, "%s", error.c_str());
    }
    return JS_ThrowTypeError(c, "unhandled Animation operation");
}
} // namespace
void RegisterAnimationBindings(BindingRegistry &registry) {
    registry.Add("Animation",
                 {"ragdollExists",
                  "ragdollContactEvents",
                  "ragdollBalanceDiagnostics",
                  "ragdollBalanceConfigure",
                  "ragdollBalanceCancel",
                  "ragdollBalanceRequest",
                  "ragdollBalanceState",
                  "ragdollPhysicalConfigure",
                  "ragdollMode",
                  "ragdollPhysicalState",
                  "ragdollActive",
                  "ragdollBody",
                  "ragdollEnter",
                  "ragdollLeave",
                  "ragdollEnabled",
                  "animationExists",
                  "animationIKConfigure",
                  "animationIKTargets",
                  "animationIKStatus",
                  "animationAdvance",
                  "animationSample",
                  "animationRootMotion",
                  "animationJointPose",
                  "animationLimb",
                  "animationRemoveLimb",
                  "animationInfo",
                  "animationSet",
                  "animationPause",
                  "animationFade",
                  "animationLayer",
                  "animationRemoveLayer",
                  "animationPlay",
                  "animationStop",
                  "animationSeek",
                  "socketSet",
                  "socketClear"},
                 HandleAnimation);
}
} // namespace ScriptBindings
