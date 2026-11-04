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

#include "../../InputSystem.h"
#include "../../SceneSession.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleInput(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
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

    if (op == "setTiltEnabled") {
        if (!JS_IsBool(arg(1)))
            return JS_ThrowTypeError(c, "input.setTiltEnabled requires a boolean");
        if (s->input)
            s->input->SetTiltEnabled(JS_ToBool(c, arg(1)));
        return JS_UNDEFINED;
    }

    if (op == "tilt") {
        const auto tilt = s->input ? s->input->Tilt() : std::nullopt;
        return tilt ? Vec(c, {tilt->x, tilt->y, tilt->z}) : JS_NULL;
    }

    if (op == "touches" || op == "pointer") {
        auto point = [&](float x, float y) {
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "x", JS_NewFloat64(c, x));
            JS_SetPropertyStr(c, o, "y", JS_NewFloat64(c, y));
            return o;
        };
        if (op == "pointer") {
            auto p = s->input ? s->input->Pointer() : InputPointer{};
            auto o = point(p.x, p.y);
            JS_SetPropertyStr(c, o, "down", JS_NewBool(c, p.down));
            return o;
        }
        auto out = JS_NewArray(c);
        uint32_t i = 0;
        if (s->input) {
            for (const auto &t : s->input->Touches()) {
                auto o = point(t.x, t.y);
                JS_SetPropertyStr(c, o, "id", JS_NewFloat64(c, double(t.id)));
                JS_SetPropertyStr(c, o, "pressure", JS_NewFloat64(c, t.pressure));
                JS_SetPropertyUint32(c, out, i++, o);
            }
        }
        return out;
    }

    if (op == "pointerCapture")
        return JS_NewBool(c, world.pointerCapture);

    if (op == "setPointerCapture") {
        if (!JS_IsBool(arg(1)))
            return JS_ThrowTypeError(c, "pointerCapture must be boolean");
        world.pointerCapture = JS_ToBool(c, arg(1));
        return JS_TRUE;
    }

    if (op == "stick" || op == "stickDelta" || op == "vector" || op == "stickSamples") {
        if (!JS_IsString(arg(1)))
            return JS_ThrowTypeError(c, "input.%s requires a string", op.c_str());
        const auto name = String(c, arg(1));
        if (op != "vector" && name != "left" && name != "right")
            return JS_ThrowTypeError(c, "stick side must be 'left' or 'right'");
        auto pair = [&](InputVector v) {
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "x", JS_NewFloat64(c, v.x));
            JS_SetPropertyStr(c, o, "y", JS_NewFloat64(c, v.y));
            return o;
        };
        if (op == "stick")
            return pair(s->input ? s->input->Stick(name) : InputVector{});
        if (op == "stickDelta")
            return pair(s->input ? s->input->StickDelta(name) : InputVector{});
        if (op == "vector")
            return pair(s->input ? s->input->Vector(name) : InputVector{});
        double after = 0;
        if (!JS_IsNumber(arg(2)) || JS_ToFloat64(c, &after, arg(2)) || !std::isfinite(after) ||
            after < 0 || after != std::floor(after) || after > 9007199254740991.0)
            return JS_ThrowTypeError(c, "stick sample cursor must be a nonnegative safe integer");
        const auto snapshot = s->input ? s->input->StickSamples(name, static_cast<uint64_t>(after))
                                       : InputStickSnapshot{};
        if (after > static_cast<double>(snapshot.sequence))
            return JS_ThrowTypeError(c, "stick sample cursor is ahead of this input service");
        auto out = JS_NewObject(c), samples = JS_NewArray(c);
        uint32_t index = 0;
        for (const auto &sample : snapshot.samples) {
            auto value = pair({sample.x, sample.y});
            JS_SetPropertyStr(c, value, "sequence",
                              JS_NewFloat64(c, static_cast<double>(sample.sequence)));
            JS_SetPropertyStr(c, value, "time", JS_NewFloat64(c, sample.time));
            JS_SetPropertyUint32(c, samples, index++, value);
        }
        JS_SetPropertyStr(c, out, "samples", samples);
        JS_SetPropertyStr(c, out, "sequence",
                          JS_NewFloat64(c, static_cast<double>(snapshot.sequence)));
        JS_SetPropertyStr(c, out, "reset", JS_NewBool(c, snapshot.reset));
        JS_SetPropertyStr(c, out, "overflow", JS_NewBool(c, snapshot.overflow));
        JS_SetPropertyStr(c, out, "capacity",
                          JS_NewUint32(c, static_cast<uint32_t>(snapshot.capacity)));
        return out;
    }

    if (op == "inputBindings") {
        auto result = JS_NewArray(c);
        uint32_t index = 0;
        if (s->input)
            for (const auto &entry : s->input->Map().entries) {
                auto value = JS_NewObject(c), bindings = JS_NewArray(c);
                uint32_t bindingIndex = 0;
                JS_SetPropertyStr(c, value, "name", JS_NewString(c, entry.name.c_str()));
                JS_SetPropertyStr(c, value, "axis", JS_NewBool(c, entry.axis));
                if (entry.vector)
                    JS_SetPropertyStr(c, value, "vector", JS_TRUE);
                for (const auto &binding : entry.bindings) {
                    auto item = JS_NewObject(c);
                    JS_SetPropertyStr(c, item, "control", JS_NewString(c, binding.control.c_str()));
                    JS_SetPropertyStr(c, item, "scale", JS_NewFloat64(c, binding.scale));
                    JS_SetPropertyStr(c, item, "deadzone", JS_NewFloat64(c, binding.deadzone));
                    if (entry.vector) {
                        JS_SetPropertyStr(c, item, "scaleY", JS_NewFloat64(c, binding.scaleY));
                        JS_SetPropertyStr(c, item, "circular", JS_NewBool(c, binding.circular));
                    }
                    JS_SetPropertyUint32(c, bindings, bindingIndex++, item);
                }
                JS_SetPropertyStr(c, value, "bindings", bindings);
                JS_SetPropertyUint32(c, result, index++, value);
            }
        return result;
    }

    if (op == "inputSetBindings") {
        if (!s->input)
            return JS_FALSE;
        InputMap map;
        std::string json, error;
        const bool reset = JS_IsNull(arg(1));
        if (!reset && (!s->Json(arg(1), json, error) ||
                       !InputMap::Parse("{\"kind\":\"input\",\"schema\":1,\"data\":" + json + "}",
                                        map, error)))
            return JS_ThrowTypeError(c, "Input bindings: %s", error.c_str());
        if (!s->input->RequestBindings(reset ? nullptr : &map, error))
            return JS_ThrowTypeError(c, "Input bindings: %s", error.c_str());
        return JS_TRUE;
    }

    if (op == "inputCanRumble") {
        auto out = JS_NewObject(c);
        JS_SetPropertyStr(c, out, "gamepad",
                          JS_NewBool(c, s->input && s->input->CanRumble(RumbleDevice::Gamepad)));
        JS_SetPropertyStr(c, out, "phone",
                          JS_NewBool(c, s->input && s->input->CanRumble(RumbleDevice::Phone)));
        return out;
    }

    if (op == "inputStopRumble") {
        if (s->input)
            s->input->StopRumble();
        return JS_UNDEFINED;
    }

    if (op == "inputRumble") {
        std::string json, error;
        if (!s->Json(arg(1), json, error))
            return JS_ThrowTypeError(c, "invalid rumble options: %s", error.c_str());
        try {
            const auto options = nlohmann::json::parse(json);
            if (!options.is_object())
                throw std::runtime_error("expected a rumble options object");
            for (auto field = options.begin(); field != options.end(); ++field)
                if (field.key() != "low" && field.key() != "high" && field.key() != "seconds" &&
                    field.key() != "device" && field.key() != "gamepad")
                    throw std::runtime_error("unknown rumble option: " + field.key());
            const auto name = options.value("device", std::string("auto"));
            const auto gamepad = options.value("gamepad", std::string());
            const double low = options.value("low", 0.0), high = options.value("high", 0.0);
            const double seconds = options.at("seconds").get<double>();
            if (!std::isfinite(low) || !std::isfinite(high) || low < 0 || low > 1 || high < 0 ||
                high > 1 || !std::isfinite(seconds) || seconds <= 0 || seconds > 5)
                throw std::runtime_error("strengths must be in 0..1 and duration in (0,5] seconds");
            RumbleDevice device = RumbleDevice::Auto;
            if (name == "gamepad")
                device = RumbleDevice::Gamepad;
            else if (name == "phone")
                device = RumbleDevice::Phone;
            else if (name != "auto")
                throw std::runtime_error("device must be auto, gamepad or phone");
            if (options.contains("gamepad") &&
                (device != RumbleDevice::Gamepad || gamepad.empty() || gamepad.size() > 64))
                throw std::runtime_error("gamepad requires a device token and device: gamepad");
            if (!s->input || (world.UIIfLoaded() && world.UIIfLoaded()->Paused()))
                return JS_FALSE;
            return JS_NewBool(c, s->input->Rumble(device, gamepad, float(low), float(high),
                                                  static_cast<unsigned>(
                                                      std::max(1.0, std::round(seconds * 1000)))));
        } catch (const std::exception &e) {
            return JS_ThrowTypeError(c, "invalid rumble options: %s", e.what());
        }
    }

    if (op == "held" || op == "pressed" || op == "released" || op == "axis") {
        auto name = String(c, arg(1));
        if (!s->input)
            return op == "axis" ? JS_NewFloat64(c, 0) : JS_FALSE;
        if (op == "axis")
            return JS_NewFloat64(c, s->input->Axis(name));
        auto a = s->fixed ? s->input->FixedAction(name) : s->input->Action(name);
        return JS_NewBool(c, op == "held" ? a.held : op == "pressed" ? a.pressed : a.released);
    }

    if (op == "playerDevices" || op == "playerCreate" || op == "playerList" ||
        op == "playerExists" || op == "playerRemove" || op == "playerInfo" ||
        op == "playerEntity" || op == "playerViewport" || op == "playerAssign" ||
        op == "playerUnassign" || op == "playerMap" || op == "playerView" || op == "playerInput" ||
        op == "playerProject" || op == "playerRay" || op == "uiPlayer") {
        auto token = [&](JSValueConst v) -> uint64_t {
            if (!JS_IsString(v))
                return 0;
            auto text = String(c, v);
            if (text.empty() || text.size() > 20 ||
                text.find_first_not_of("0123456789") != std::string::npos)
                return 0;
            try {
                return std::stoull(text);
            } catch (...) {
                return 0;
            }
        };
        auto hub = [&]() -> InputDeviceHub * {
            if (!s->input)
                return nullptr;
            auto &owned = world.Players().devices;
            if (!owned)
                owned = std::make_shared<InputDeviceHub>();
            s->input->localDevices = owned;
            return owned.get();
        };
        auto rect = [&](JSValueConst v, glm::vec4 &out) {
            return Number(c, v, "x", out.x) && Number(c, v, "y", out.y) &&
                   Number(c, v, "width", out.z) && Number(c, v, "height", out.w) &&
                   LocalPlayers::ValidRect(out);
        };
        auto rectValue = [&](glm::vec4 r) {
            auto out = JS_NewObject(c);
            for (auto field : {std::pair<const char *, float>{"x", r.x},
                               {"y", r.y},
                               {"width", r.z},
                               {"height", r.w}})
                JS_SetPropertyStr(c, out, field.first, JS_NewFloat64(c, field.second));
            return out;
        };
        auto readMap = [&](JSValueConst v, InputMap &map, std::string &error) {
            std::string json;
            if (!s->Json(v, json, error))
                return false;
            if (!InputMap::Parse("{\"kind\":\"input\",\"schema\":1,\"data\":" + json + "}", map,
                                 error))
                return false;
            if (map.entries.size() > 256) {
                error = "At most 256 player bindings";
                return false;
            }
            return true;
        };
        auto *service = world.PlayersIfLoaded();
        auto *p = service ? service->Find(token(arg(1))) : nullptr;
        std::string error;
        if (op == "playerDevices") {
            auto *devices = hub();
            auto out = JS_NewArray(c);
            if (!devices)
                return out;
            uint32_t n = 0;
            for (auto &pair : devices->Devices()) {
                auto &d = pair.second;
                auto value = JS_NewObject(c);
                for (auto field : {std::pair<const char *, std::string>{"id", d.id},
                                   {"kind", d.kind},
                                   {"name", d.name}})
                    JS_SetPropertyStr(c, value, field.first, JS_NewString(c, field.second.c_str()));
                JS_SetPropertyStr(c, value, "connected", JS_NewBool(c, d.connected));
                JS_SetPropertyUint32(c, out, n++, value);
            }
            return out;
        }
        if (op == "playerList") {
            auto out = JS_NewArray(c);
            uint32_t n = 0;
            if (service)
                for (auto &pair : service->All())
                    JS_SetPropertyUint32(c, out, n++,
                                         JS_NewString(c, std::to_string(pair.first).c_str()));
            return out;
        }
        if (op == "playerCreate") {
            if (!JS_IsString(arg(1)))
                return JS_ThrowTypeError(c, "player name string required");
            if (s->input)
                hub();
            InputMap map = s->input ? s->input->Map() : InputMap::Defaults();
            if (!JS_IsNull(arg(2)) && !readMap(arg(2), map, error))
                return JS_ThrowTypeError(c, "input map: %s", error.c_str());
            auto *created = world.Players().Create(String(c, arg(1)), map, error);
            return created ? JS_NewString(c, std::to_string(created->id).c_str())
                           : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "playerExists")
            return JS_NewBool(c, p != nullptr);
        if (op == "uiPlayer") {
            uint32_t handle = 0;
            JS_ToUint32(c, &handle, arg(1));
            if (!world.UI().Document(handle))
                return JS_ThrowReferenceError(c, "stale UI document");
            if (argc > 2) {
                const auto id = JS_IsNull(arg(2)) ? 0 : token(arg(2));
                if (!JS_IsNull(arg(2)) && (!service || !service->Find(id)))
                    return JS_ThrowReferenceError(c, "stale player");
                world.UI().SetPlayer(handle, id);
                if (id)
                    world.UI().PlayerViewport(id, service->Find(id)->viewport);
            }
            auto id = world.UI().Player(handle);
            return id ? JS_NewString(c, std::to_string(id).c_str()) : JS_NULL;
        }
        if (!p)
            return JS_ThrowReferenceError(c, "stale local player");
        if (op == "playerRemove") {
            world.UI().RemovePlayer(p->id);
            return JS_NewBool(c, service->Remove(p->id));
        }
        if (op == "playerInfo") {
            auto out = JS_NewObject(c), devices = JS_NewArray(c);
            uint32_t n = 0;
            for (auto &a : p->devices)
                JS_SetPropertyUint32(c, devices, n++, JS_NewString(c, a.device.c_str()));
            JS_SetPropertyStr(c, out, "name", JS_NewString(c, p->name.c_str()));
            JS_SetPropertyStr(c, out, "devices", devices);
            JS_SetPropertyStr(c, out, "viewError", JS_NewString(c, p->renderError.c_str()));
            return out;
        }
        if (op == "playerEntity") {
            if (argc > 2) {
                const auto id = JS_IsNull(arg(2)) ? 0 : token(arg(2));
                if (!JS_IsNull(arg(2)) && (!id || !world.RuntimeDefinition(id)))
                    return JS_ThrowReferenceError(c, "stale controlled entity");
                p->entity = id;
            }
            return p->entity && world.RuntimeDefinition(p->entity)
                       ? JS_NewString(c, std::to_string(p->entity).c_str())
                       : JS_NULL;
        }
        if (op == "playerViewport") {
            if (argc > 2) {
                glm::vec4 r;
                if (!rect(arg(2), r) || !service->SetViewport(p->id, r, error))
                    return JS_ThrowTypeError(c, "normalized viewport required");
                if (world.UIIfLoaded())
                    world.UI().PlayerViewport(p->id, r);
            }
            return rectValue(p->viewport);
        }
        if (op == "playerMap") {
            InputMap map;
            if (!readMap(arg(2), map, error) || !p->input.SetMap(map, error))
                return JS_ThrowTypeError(c, "input map: %s", error.c_str());
            p->baseline = true;
            p->touchSources.clear();
            return JS_TRUE;
        }
        if (op == "playerUnassign")
            return JS_NewBool(c, service->Unassign(p->id, String(c, arg(2))));
        if (op == "playerAssign") {
            if (!hub() || !JS_IsString(arg(2)) || !KnownOptions(c, arg(3), {"controls", "region"}))
                return JS_ThrowTypeError(c, "device and controls/region options required");
            LocalPlayers::Assignment a;
            a.device = String(c, arg(2));
            auto controls = JS_GetPropertyStr(c, arg(3), "controls"),
                 region = JS_GetPropertyStr(c, arg(3), "region");
            bool valid = true;
            if (!JS_IsUndefined(controls)) {
                auto length = JS_GetPropertyStr(c, controls, "length");
                uint32_t n = 0;
                valid = JS_IsArray(controls) && JS_ToUint32(c, &n, length) == 0 && n <= 128;
                JS_FreeValue(c, length);
                for (uint32_t i = 0; valid && i < n; ++i) {
                    auto v = JS_GetPropertyUint32(c, controls, i);
                    valid = JS_IsString(v);
                    if (valid)
                        a.controls.insert(String(c, v));
                    JS_FreeValue(c, v);
                }
            }
            if (!JS_IsUndefined(region))
                valid &= rect(region, a.region);
            JS_FreeValue(c, controls);
            JS_FreeValue(c, region);
            if (!valid || !service->Assign(p->id, *hub(), std::move(a), error))
                return JS_ThrowTypeError(c, "device assignment: %s", error.c_str());
            return JS_TRUE;
        }
        if (op == "playerView") {
            if (JS_IsNull(arg(2))) {
                p->view.reset();
                p->renderError.clear();
                p->attemptedSize = {0, 0};
                return JS_TRUE;
            }
            SceneTransform pose;
            double fov = 70;
            float nearPlane = .1f, farPlane = 500;
            bool valid = JS_IsObject(arg(2)) && readTransform(arg(2), pose) &&
                         JS_ToFloat64(c, &fov, arg(3)) == 0 &&
                         KnownOptions(c, arg(4),
                                      {"near", "far", "projection", "height", "pixelPerfect",
                                       "referenceWidth", "referenceHeight", "pixelsPerUnit"});
            CameraLens lens;
            valid &= ReadCameraLens(c, arg(4), lens);
            for (auto field :
                 {std::pair<const char *, float *>{"near", &nearPlane}, {"far", &farPlane}}) {
                auto value = JS_GetPropertyStr(c, arg(4), field.first);
                if (!JS_IsUndefined(value))
                    valid &= Number(c, arg(4), field.first, *field.second);
                JS_FreeValue(c, value);
            }
            if (!valid ||
                !service->SetView(p->id, pose, float(fov), nearPlane, farPlane, error, lens))
                return JS_ThrowTypeError(c, "invalid player view/range");
            return JS_TRUE;
        }
        if (op == "playerProject" || op == "playerRay") {
            if (!p->view || world.viewportWidth <= 0 || world.viewportHeight <= 0)
                return JS_NULL;
            const auto &pose = *p->view;
            const auto size =
                LocalPlayers::ViewSize(p->viewport, world.viewportWidth, world.viewportHeight);
            const auto frame = BuildCameraFrame(pose.position, pose.rotation, p->fov, p->nearPlane,
                                                p->farPlane, p->lens, size.x, size.y);
            if (op == "playerRay") {
                glm::vec2 point;
                glm::vec3 origin, direction;
                if (!Number(c, arg(2), "x", point.x) || !Number(c, arg(2), "y", point.y) ||
                    point.x < 0 || point.x > 1 || point.y < 0 || point.y > 1)
                    return JS_ThrowTypeError(c, "normalized viewport-local point required");
                if (!frame.Ray(point, origin, direction))
                    return JS_NULL;
                auto out = JS_NewObject(c);
                JS_SetPropertyStr(c, out, "origin", Vec(c, origin));
                JS_SetPropertyStr(c, out, "direction", Vec(c, direction));
                return out;
            }
            glm::vec3 point;
            if (!ReadVec(c, arg(2), point))
                return JS_ThrowTypeError(c, "finite world point required");
            const auto projected = frame.Project(point);
            auto out = JS_NewObject(c);
            for (auto field : {std::pair<const char *, double>{
                                   "x", p->viewport.x + projected.position.x * p->viewport.z},
                               {"y", p->viewport.y + projected.position.y * p->viewport.w},
                               {"depth", projected.depth},
                               {"distance", projected.distance}})
                JS_SetPropertyStr(c, out, field.first, JS_NewFloat64(c, field.second));
            JS_SetPropertyStr(c, out, "behind", JS_NewBool(c, projected.behind));
            JS_SetPropertyStr(c, out, "inside", JS_NewBool(c, projected.inside));
            return out;
        }
        if (op == "playerInput") {
            const auto method = String(c, arg(2)), name = String(c, arg(3));
            const bool blocked =
                world.UIIfLoaded() &&
                (world.UIIfLoaded()->Paused() || world.UIIfLoaded()->Paused(p->id)) && !s->inUI;
            if (method == "held" || method == "pressed" || method == "released") {
                const auto state = blocked    ? InputActionState{}
                                   : s->fixed ? p->input.FixedAction(name)
                                              : p->input.Action(name);
                return JS_NewBool(c, method == "held"      ? state.held
                                     : method == "pressed" ? state.pressed
                                                           : state.released);
            }
            if (method == "axis")
                return JS_NewFloat64(c, blocked ? 0 : p->input.Axis(name));
            if (method == "vector" || method == "stick") {
                if (method == "stick" && name != "left" && name != "right")
                    return JS_ThrowTypeError(c, "stick side left/right required");
                const auto v = blocked              ? InputVector{}
                               : method == "vector" ? p->input.Vector(name)
                                                    : p->input.Stick(name);
                auto out = JS_NewObject(c);
                JS_SetPropertyStr(c, out, "x", JS_NewFloat64(c, v.x));
                JS_SetPropertyStr(c, out, "y", JS_NewFloat64(c, v.y));
                return out;
            }
            if (method == "pointer") {
                const auto v = p->input.Pointer();
                auto out = JS_NewObject(c);
                JS_SetPropertyStr(c, out, "x", JS_NewFloat64(c, v.x));
                JS_SetPropertyStr(c, out, "y", JS_NewFloat64(c, v.y));
                JS_SetPropertyStr(c, out, "down", JS_NewBool(c, !blocked && v.down));
                return out;
            }
            if (method == "touches") {
                auto out = JS_NewArray(c);
                uint32_t n = 0;
                if (!blocked)
                    for (auto &t : p->input.Touches()) {
                        auto v = JS_NewObject(c);
                        JS_SetPropertyStr(c, v, "id", JS_NewFloat64(c, double(t.id)));
                        for (auto pair : {std::pair<const char *, float>{"x", t.x},
                                          {"y", t.y},
                                          {"pressure", t.pressure}})
                            JS_SetPropertyStr(c, v, pair.first, JS_NewFloat64(c, pair.second));
                        JS_SetPropertyUint32(c, out, n++, v);
                    }
                return out;
            }
            return JS_ThrowTypeError(c, "unknown player input observation");
        }
    }
    return JS_ThrowTypeError(c, "unhandled Input operation");
}
} // namespace
void RegisterInputBindings(BindingRegistry &registry) {
    registry.Add("Input", {"setTiltEnabled", "tilt",
                           "touches",        "pointer",
                           "pointerCapture", "setPointerCapture",
                           "stick",          "stickDelta",
                           "vector",         "stickSamples",
                           "inputBindings",  "inputSetBindings",
                           "inputCanRumble", "inputStopRumble",
                           "inputRumble",    "held",
                           "pressed",        "released",
                           "axis",           "playerDevices",
                           "playerCreate",   "playerList",
                           "playerExists",   "playerRemove",
                           "playerInfo",     "playerEntity",
                           "playerViewport", "playerAssign",
                           "playerUnassign", "playerMap",
                           "playerView",     "playerInput",
                           "playerProject",  "playerRay",
                           "uiPlayer"},
                 HandleInput);
}
} // namespace ScriptBindings
