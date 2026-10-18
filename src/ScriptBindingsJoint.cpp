#include "ScriptBindings.h"
#include "ScriptBindingValues.h"
#include "RuntimeWorld.h"

#include <stdexcept>

using ScriptBindingValues::Number;
using ScriptBindingValues::ReadVec;
using ScriptBindingValues::String;

JSValue ScriptBindings::Joint(JSContext *c, const std::string &op, int argc, JSValueConst *argv,
                              RuntimeWorld &world) {
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    // Existing dispatch branches begin.
    if (op == "jointCreate" || op == "jointConfigure" || op == "jointDestroy" || op == "joint" ||
        op == "jointValid" || op == "jointState" || op == "jointSet") {
        uint64_t id = 0;
        try {
            const auto text = String(c, arg(1));
            size_t end;
            id = std::stoull(text, &end);
            if (end != text.size())
                id = 0;
        } catch (...) {
            id = 0;
        }
        if (op == "jointCreate" || op == "jointConfigure" || op == "jointDestroy") {
            EntityId owner = id;
            SceneJointComponent definition;
            if (op != "jointCreate") {
                owner = 0;
                for (const auto &o : world.ScriptObjects())
                    if (o.joint && world.RuntimeJoint(o.id).id == id) {
                        owner = o.id;
                        definition = *o.joint;
                        break;
                    }
                if (!owner)
                    return JS_ThrowReferenceError(c, "stale or unowned joint");
            } else {
                auto *d = world.RuntimeDefinition(owner);
                if (!d)
                    return JS_ThrowReferenceError(c, "stale joint owner");
                if (d->joint)
                    return JS_ThrowTypeError(c, "owner already has a joint");
            }
            if (op != "jointDestroy") {
                auto o = arg(2);
                auto &settings = definition.settings;
                for (auto item : {std::pair<const char *, EntityId *>{"bodyAId", &definition.bodyA},
                                  {"bodyBId", &definition.bodyB}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    if (!JS_IsUndefined(v)) {
                        try {
                            auto text = String(c, v);
                            size_t end = 0;
                            *item.second = std::stoull(text, &end);
                            if (end != text.size())
                                throw std::runtime_error("identity");
                        } catch (...) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "stable entity required");
                        }
                    }
                    JS_FreeValue(c, v);
                }
                auto type = JS_GetPropertyStr(c, o, "type");
                if (!JS_IsUndefined(type)) {
                    auto name = String(c, type);
                    if (name == "fixed")
                        settings.type = JointType::Fixed;
                    else if (name == "hinge")
                        settings.type = JointType::Hinge;
                    else if (name == "ball")
                        settings.type = JointType::Ball;
                    else if (name == "slider")
                        settings.type = JointType::Slider;
                    else {
                        JS_FreeValue(c, type);
                        return JS_ThrowTypeError(c, "unknown joint type");
                    }
                }
                JS_FreeValue(c, type);
                for (auto item :
                     {std::pair<const char *, glm::vec3 *>{"anchorA", &settings.anchorA},
                      {"anchorB", &settings.anchorB}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    bool ok = JS_IsUndefined(v) || ReadVec(c, v, *item.second);
                    JS_FreeValue(c, v);
                    if (!ok)
                        return JS_ThrowTypeError(c, "invalid joint anchor");
                }
                for (auto item : {std::pair<const char *, glm::quat *>{"frameA", &settings.frameA},
                                  {"frameB", &settings.frameB}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    bool ok =
                        JS_IsUndefined(v) ||
                        (Number(c, v, "x", item.second->x) && Number(c, v, "y", item.second->y) &&
                         Number(c, v, "z", item.second->z) && Number(c, v, "w", item.second->w));
                    JS_FreeValue(c, v);
                    if (!ok)
                        return JS_ThrowTypeError(c, "invalid joint frame");
                }
                for (auto item : {std::pair<const char *, float *>{"lower", &settings.lower},
                                  {"upper", &settings.upper},
                                  {"speed", &settings.speed},
                                  {"maxForce", &settings.maxForce},
                                  {"rest", &settings.rest},
                                  {"stiffness", &settings.stiffness},
                                  {"damping", &settings.damping},
                                  {"rotationalResistance", &settings.rotationalResistance}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    bool has = !JS_IsUndefined(v);
                    JS_FreeValue(c, v);
                    if (has && !Number(c, o, item.first, *item.second))
                        return JS_ThrowTypeError(c, "invalid joint number");
                }
                for (auto item : {std::pair<const char *, bool *>{"enabled", &settings.enabled},
                                  {"limits", &settings.limits},
                                  {"motor", &settings.motor},
                                  {"spring", &settings.spring}}) {
                    auto v = JS_GetPropertyStr(c, o, item.first);
                    if (!JS_IsUndefined(v)) {
                        if (!JS_IsBool(v)) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "invalid joint flag");
                        }
                        *item.second = JS_ToBool(c, v);
                    }
                    JS_FreeValue(c, v);
                }
            }
            if (op == "jointConfigure") {
                auto *current = world.RuntimeDefinition(owner);
                if (current && current->joint &&
                    (current->joint->bodyA != definition.bodyA ||
                     current->joint->bodyB != definition.bodyB))
                    return JS_ThrowTypeError(
                        c, "configure retains participants; destroy/create to replace them");
            }
            std::string error;
            if (!world.SetRuntimeJoint(owner, definition, op == "jointDestroy", error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            return op == "jointCreate"
                       ? JS_NewString(c, std::to_string(world.RuntimeJoint(owner).id).c_str())
                       : JS_TRUE;
        }
        if (op == "joint") {
            auto h = world.RuntimeJoint(id);
            return h.IsValid() ? JS_NewString(c, std::to_string(h.id).c_str()) : JS_NULL;
        }
        JointHandle h{id};
        JointState state;
        bool valid = world.Physics().GetJoint(h, state);
        if (op == "jointValid")
            return JS_NewBool(c, valid);
        if (!valid)
            return JS_ThrowReferenceError(c, "stale joint handle");
        if (op == "jointSet") {
            auto settings = state.settings;
            for (auto entry : {std::pair<const char *, bool *>{"enabled", &settings.enabled},
                               {"limits", &settings.limits},
                               {"motor", &settings.motor},
                               {"spring", &settings.spring}}) {
                auto value = JS_GetPropertyStr(c, arg(2), entry.first);
                if (!JS_IsUndefined(value)) {
                    if (!JS_IsBool(value)) {
                        JS_FreeValue(c, value);
                        return JS_ThrowTypeError(c, "joint flag must be boolean");
                    }
                    *entry.second = JS_ToBool(c, value);
                }
                JS_FreeValue(c, value);
            }
            for (auto entry : {std::pair<const char *, float *>{"lower", &settings.lower},
                               {"upper", &settings.upper},
                               {"speed", &settings.speed},
                               {"maxForce", &settings.maxForce},
                               {"rest", &settings.rest},
                               {"stiffness", &settings.stiffness},
                               {"damping", &settings.damping},
                               {"rotationalResistance", &settings.rotationalResistance}}) {
                auto value = JS_GetPropertyStr(c, arg(2), entry.first);
                bool present = !JS_IsUndefined(value);
                JS_FreeValue(c, value);
                if (present && !Number(c, arg(2), entry.first, *entry.second))
                    return JS_ThrowTypeError(c, "invalid joint number");
            }
            if (!world.Physics().SetJoint(h, settings))
                return JS_ThrowTypeError(c, "invalid joint settings");
            return JS_TRUE;
        }
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "active", JS_NewBool(c, state.active));
        JS_SetPropertyStr(c, result, "enabled", JS_NewBool(c, state.settings.enabled));
        JS_SetPropertyStr(c, result, "coordinate", JS_NewFloat64(c, state.coordinate));
        JS_SetPropertyStr(c, result, "motorImpulse", JS_NewFloat64(c, state.motorImpulse));
        JS_SetPropertyStr(c, result, "type", JS_NewInt32(c, int(state.settings.type)));
        return result;
    }

    // Existing dispatch branches end.
    return JS_ThrowTypeError(c, "unknown joint operation");
}
