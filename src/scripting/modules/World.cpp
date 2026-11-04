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

#include "../../SaveService.h"
#include "../../SceneSession.h"
#include "../../WorldStreaming.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleWorld(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    auto doubleVector = [&](JSValueConst value, glm::dvec3 &p) {
        for (int i = 0; i < 3; ++i) {
            auto n = JS_GetPropertyStr(c, value, i == 0 ? "x" : i == 1 ? "y" : "z");
            bool ok = JS_IsNumber(n) && JS_ToFloat64(c, &p[i], n) == 0 && std::isfinite(p[i]);
            JS_FreeValue(c, n);
            if (!ok)
                return false;
        }
        return true;
    };

    auto preciseVector = [&](glm::dvec3 p) {
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "x", JS_NewFloat64(c, p.x));
        JS_SetPropertyStr(c, o, "y", JS_NewFloat64(c, p.y));
        JS_SetPropertyStr(c, o, "z", JS_NewFloat64(c, p.z));
        return o;
    };

    auto tagMask = [&](JSValueConst array, CategoryMask &mask) {
        if (!JS_IsArray(array))
            return false;
        mask = 0;
        auto length = JS_GetPropertyStr(c, array, "length");
        uint32_t n = 0;
        JS_ToUint32(c, &n, length);
        JS_FreeValue(c, length);
        if (n > 64)
            return false;
        for (uint32_t i = 0; i < n; ++i) {
            auto item = JS_GetPropertyUint32(c, array, i);
            auto name = String(c, item);
            JS_FreeValue(c, item);
            int id = world.Categories().tags.Find(name);
            if (id < 0)
                return false;
            mask |= CategoryBit(id);
        }
        return true;
    };

    auto ids = [&](const std::vector<EntityId> &values) {
        auto array = JS_NewArray(c);
        uint32_t i = 0;
        for (auto id : values)
            JS_SetPropertyUint32(c, array, i++, JS_NewString(c, std::to_string(id).c_str()));
        return array;
    };

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

    if (op == "worldOrigin")
        return preciseVector(world.Settings().worldOrigin);

    if (op == "worldOriginState") {
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "pending", JS_NewBool(c, world.OriginPending()));
        JS_SetPropertyStr(c, o, "revision", JS_NewFloat64(c, double(world.OriginRevision())));
        JS_SetPropertyStr(c, o, "error", JS_NewString(c, world.OriginError().c_str()));
        return o;
    }

    if (op == "worldAbsolute" || op == "worldLocal" || op == "worldOriginRequest") {
        glm::dvec3 p;
        if (!doubleVector(arg(1), p))
            return JS_ThrowTypeError(c, "finite double position required");
        if (op == "worldOriginRequest") {
            std::string error;
            if (!world.RequestOrigin(p, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            return JS_TRUE;
        }
        return preciseVector(op == "worldAbsolute" ? world.Settings().worldOrigin + p
                                                   : p - world.Settings().worldOrigin);
    }

    if (op == "lifecycleBusy") {
        auto scene = world.SceneControl();
        return JS_NewBool(c, scene && scene->OperationInProgress());
    }

    if (op == "saveExclude" || op == "saveRequest" || op == "saveList" || op == "saveStatus" ||
        op == "saveCancel" || op == "saveReference" || op == "saveResolve") {
        if (op == "saveExclude") {
            EntityId id = 0;
            try {
                size_t end = 0;
                auto text = String(c, arg(1));
                id = std::stoull(text, &end);
                if (end != text.size())
                    id = 0;
            } catch (...) {
            }
            if (!id)
                return JS_ThrowTypeError(c, "entity identity required");
            std::string error;
            return world.SetTransientEntity(id, JS_ToBool(c, arg(2)), error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        auto scenes = world.SceneControl();
        auto *resources = world.Resources();
        if (!scenes || !resources || !resources->Jobs())
            return JS_ThrowTypeError(c, "save project service unavailable");
        auto *service = scenes->Saves(*resources);
        if (op == "saveReference" || op == "saveResolve") {
            std::string key = String(c, arg(1));
            std::string error;
            auto *stream = scenes->ComposedProject() ? scenes->Streaming(world, error) : nullptr;
            if (op == "saveReference") {
                uint64_t id = 0;
                try {
                    id = std::stoull(key);
                } catch (...) {
                }
                if (!world.RuntimeDefinition(id))
                    return JS_NULL;
                auto stable = stream ? stream->PersistentKey(id) : "entity:" + std::to_string(id);
                return JS_NewString(c, stable.c_str());
            }
            EntityId id = 0;
            if (stream)
                id = stream->ResolvePersistentKey(key);
            else if (key.rfind("entity:", 0) == 0) {
                try {
                    size_t end = 0;
                    auto tail = key.substr(7);
                    id = std::stoull(tail, &end);
                    if (end != tail.size() || !world.RuntimeDefinition(id))
                        id = 0;
                } catch (...) {
                    id = 0;
                }
            }
            return id ? JS_NewString(c, std::to_string(id).c_str()) : JS_NULL;
        }
        if (op == "saveRequest") {
            std::string error, json = "{}", name;
            bool migrate = false;
            auto options = arg(3);
            if (JS_IsObject(options)) {
                auto value = JS_GetPropertyStr(c, options, "name");
                if (!JS_IsUndefined(value))
                    name = String(c, value);
                JS_FreeValue(c, value);
                value = JS_GetPropertyStr(c, options, "metadata");
                if (!JS_IsUndefined(value) && !s->Json(value, json, error)) {
                    JS_FreeValue(c, value);
                    return JS_ThrowTypeError(c, "%s", error.c_str());
                }
                JS_FreeValue(c, value);
                value = JS_GetPropertyStr(c, options, "migrate");
                if (!JS_IsUndefined(value) && !JS_IsBool(value)) {
                    JS_FreeValue(c, value);
                    return JS_ThrowTypeError(c, "migrate requires boolean");
                }
                migrate = JS_ToBool(c, value) > 0;
                JS_FreeValue(c, value);
            }
            auto request =
                service->Request(String(c, arg(1)), String(c, arg(2)), name, json, error, migrate);
            return request ? JS_NewFloat64(c, double(request))
                           : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "saveList") {
            auto list = JS_NewArray(c);
            unsigned i = 0;
            for (auto &slot : service->List()) {
                auto o = JS_NewObject(c);
                for (auto item : {std::pair<const char *, const std::string *>{"id", &slot.id},
                                  {"name", &slot.name},
                                  {"scene", &slot.scene},
                                  {"status", &slot.status},
                                  {"error", &slot.error},
                                  {"content", &slot.content},
                                  {"compatibility", &slot.compatibility}})
                    JS_SetPropertyStr(c, o, item.first, JS_NewString(c, item.second->c_str()));
                JS_SetPropertyStr(c, o, "contentVersion", JS_NewUint32(c, slot.contentVersion));
                JS_SetPropertyStr(c, o, "modContext",
                                  slot.modContext.empty() ? JS_NULL
                                                          : JS_ParseJSON(c, slot.modContext.data(),
                                                                         slot.modContext.size(),
                                                                         "saved mod requirements"));
                JS_SetPropertyStr(c, o, "timestamp", JS_NewFloat64(c, double(slot.timestamp)));
                JS_SetPropertyStr(c, o, "recovered", JS_NewBool(c, slot.recovered));
                JS_SetPropertyStr(c, o, "metadata",
                                  JS_ParseJSON(c,
                                               slot.metadata.empty() ? "{}" : slot.metadata.c_str(),
                                               slot.metadata.empty() ? 2 : slot.metadata.size(),
                                               "slot metadata"));
                JS_SetPropertyUint32(c, list, i++, o);
            }
            return list;
        }
        double id = 0;
        if (JS_ToFloat64(c, &id, arg(1)) < 0 || !std::isfinite(id) || std::floor(id) != id ||
            id < 0 || id > 9007199254740991.)
            return JS_ThrowTypeError(c, "save request token required");
        if (op == "saveCancel")
            return JS_NewBool(c, service->Cancel(uint64_t(id)));
        auto *status = service->Status(uint64_t(id));
        if (!status)
            return JS_NULL;
        auto o = JS_NewObject(c);
        for (auto item :
             {std::pair<const char *, const std::string *>{"operation", &status->operation},
              {"slot", &status->slot},
              {"state", &status->state},
              {"error", &status->error},
              {"migrationReport", &status->migrationReport},
              {"waitingFor", &status->waitingFor}})
            JS_SetPropertyStr(c, o, item.first, JS_NewString(c, item.second->c_str()));
        for (auto item : {std::pair<const char *, double>{"id", double(status->id)},
                          {"migrationMs", status->migrationMs},
                          {"queuedMs", status->queuedMs},
                          {"identityMs", status->identityMs},
                          {"workerQueueMs", status->workerQueueMs},
                          {"elapsedMs", status->elapsedMs},
                          {"captureMs", status->captureMs},
                          {"workerMs", status->workerMs},
                          {"restoreMs", status->restoreMs},
                          {"bytes", double(status->bytes)}})
            JS_SetPropertyStr(c, o, item.first, JS_NewFloat64(c, item.second));
        JS_SetPropertyStr(c, o, "migrated", JS_NewBool(c, status->migrated));
        JS_SetPropertyStr(c, o, "recovered", JS_NewBool(c, status->recovered));
        return o;
    }

    if (op.rfind("region", 0) == 0) {
        auto control = world.SceneControl();
        std::string error;
        auto *stream = control ? control->Streaming(world, error) : nullptr;
        if (!stream)
            return JS_ThrowTypeError(c, "Streaming: %s", error.c_str());
        auto status = [&](const RegionStatus &st) {
            auto result = JS_NewObject(c);
            for (auto [key, value] : {std::pair<const char *, std::string>{"id", st.id},
                                      {"state", st.state},
                                      {"error", st.error}})
                JS_SetPropertyStr(c, result, key, JS_NewString(c, value.c_str()));
            JS_SetPropertyStr(c, result, "visualReady", JS_NewBool(c, st.visualReady));
            for (auto [key, value] :
                 {std::pair<const char *, double>{"entities", double(st.entities)},
                  {"installed", double(st.installed)},
                  {"bytes", double(st.bytes)},
                  {"retained", double(st.retained)},
                  {"demands", double(st.demands)},
                  {"preparationMs", st.preparationMs},
                  {"integrationMs", st.integrationMs},
                  {"largestUnitMs", st.largestUnitMs},
                  {"loadMs", st.loadMs}})
                JS_SetPropertyStr(c, result, key, JS_NewFloat64(c, value));
            auto pins = JS_NewArray(c);
            uint32_t i = 0;
            for (auto &p : st.pins)
                JS_SetPropertyUint32(c, pins, i++, JS_NewString(c, p.c_str()));
            JS_SetPropertyStr(c, result, "pins", pins);
            return result;
        };
        auto id = [&](int n) -> uint64_t {
            try {
                auto text = String(c, arg(n));
                size_t consumed = 0;
                auto value = std::stoull(text, &consumed);
                return consumed == text.size() ? value : 0;
            } catch (...) {
                return 0;
            }
        };
        if (op == "regionRequest") {
            if (!control->Accepting())
                return JS_ThrowTypeError(c, "Scene lifecycle ending");
            auto value = JS_GetPropertyStr(c, arg(2), "preload");
            bool preload = JS_ToBool(c, value);
            JS_FreeValue(c, value);
            auto token =
                stream->Request(String(c, arg(1)), preload, error, s->currentOwner, s->currentSlot);
            return token ? JS_NewString(c, std::to_string(token).c_str())
                         : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "regionStatus") {
            auto st = stream->Status(id(1));
            return st ? status(*st) : JS_NULL;
        }
        if (op == "regionActivate")
            return JS_NewBool(c, stream->Activate(id(1)));
        if (op == "regionRelease")
            return JS_NewBool(c, stream->Release(id(1)));
        if (op == "regionUnload")
            return JS_NewBool(c, stream->Unload(String(c, arg(1))));
        if (op == "regionList") {
            auto result = JS_NewArray(c);
            uint32_t i = 0;
            for (auto &st : stream->Regions())
                JS_SetPropertyUint32(c, result, i++, status(st));
            return result;
        }
        if (op == "regionStats") {
            auto result = JS_NewObject(c);
            auto st = stream->Stats();
            for (auto [key, value] : {std::pair<const char *, double>{"active", double(st.active)},
                                      {"pending", double(st.pending)},
                                      {"pendingBytes", double(st.pendingBytes)},
                                      {"liveBytes", double(st.liveBytes)},
                                      {"retainedBytes", double(st.retainedBytes)},
                                      {"resourceResidentBytes", double(st.resourceResidentBytes)},
                                      {"resourceCacheBudget", double(st.resourceCacheBudget)},
                                      {"integrationMs", st.integrationMs},
                                      {"largestUnitMs", st.largestUnitMs}})
                JS_SetPropertyStr(c, result, key, JS_NewFloat64(c, value));
            return result;
        }
        if (op == "regionOwner") {
            auto value = id(1);
            if (!world.RuntimeDefinition(value))
                return JS_ThrowReferenceError(c, "stale entity");
            return JS_NewString(c, stream->Owner(value).c_str());
        }
        if (op == "regionResolve") {
            auto value = stream->Resolve(String(c, arg(1)), id(2));
            return value ? JS_NewString(c, std::to_string(value).c_str()) : JS_NULL;
        }
        if (op == "regionPin")
            return JS_NewBool(
                c, stream->Pin(String(c, arg(1)), String(c, arg(2)), JS_ToBool(c, arg(3))));
        if (op == "regionAdopt") {
            bool ok = stream->Adopt(id(1), String(c, arg(2)), error);
            return ok ? JS_TRUE : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (op == "regionRemoveInterest") {
            stream->RemoveInterest(String(c, arg(1)));
            return JS_UNDEFINED;
        }
        if (op == "regionInterest") {
            glm::vec3 position;
            float load = 0, retain = 0, priority = 0;
            if (!ReadVec(c, arg(2), position) || !Number(c, arg(3), "load", load) ||
                !Number(c, arg(3), "retain", retain))
                return JS_ThrowTypeError(c, "interest requires position/load/retain");
            auto p = JS_GetPropertyStr(c, arg(3), "priority");
            bool has = !JS_IsUndefined(p);
            JS_FreeValue(c, p);
            if (has && (!Number(c, arg(3), "priority", priority) || priority < -100000 ||
                        priority > 100000 || std::floor(priority) != priority))
                return JS_ThrowTypeError(c, "invalid interest priority");
            return stream->Interest(String(c, arg(1)), position, load, retain, int(priority), error)
                       ? JS_TRUE
                       : JS_ThrowTypeError(c, "%s", error.c_str());
        }
    }

    if (op == "sceneCurrent" || op == "sceneList" || op == "sceneLoad" || op == "sceneReload" ||
        op == "sessionGet" || op == "sessionSet" || op == "sessionDelete") {
        auto scenes = world.SceneControl();
        if (!scenes)
            return JS_ThrowTypeError(c, "No project scene session is active");
        if (op == "sceneCurrent")
            return JS_NewString(c, scenes->Current().c_str());
        if (op == "sceneList") {
            auto a = JS_NewArray(c);
            uint32_t i = 0;
            for (auto &name : scenes->Scenes())
                JS_SetPropertyUint32(c, a, i++, JS_NewString(c, name.c_str()));
            return a;
        }
        std::string error;
        if (op == "sceneLoad" || op == "sceneReload") {
            bool ok = op == "sceneLoad" ? scenes->Request(String(c, arg(1)), error)
                                        : scenes->Reload(error);
            return ok ? JS_TRUE : JS_ThrowTypeError(c, "%s", error.c_str());
        }
        if (!JS_IsString(arg(1)))
            return JS_ThrowTypeError(c, "Session key must be a string");
        auto key = String(c, arg(1));
        if (op == "sessionGet") {
            auto json = scenes->Get(key);
            return JS_ParseJSON(c, json.c_str(), json.size(), "session");
        }
        if (op == "sessionDelete") {
            scenes->Erase(key);
            return JS_UNDEFINED;
        }
        std::string json;
        if (!s->Json(arg(2), json, error) || !scenes->Set(key, json, error))
            return JS_ThrowTypeError(c, "%s", error.c_str());
        return JS_UNDEFINED;
    }

    if (op == "localeInfo" || op == "localeChoose" || op == "localeFormat" ||
        op == "localeNumber" || op == "localeReload") {
        auto &locale = world.Localization();
        locale.Refresh();
        std::string error;
        if (op == "localeInfo") {
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "locale", JS_NewString(c, locale.Locale().c_str()));
            JS_SetPropertyStr(c, o, "revision", JS_NewFloat64(c, double(locale.Revision())));
            JS_SetPropertyStr(c, o, "direction", JS_NewString(c, locale.RTL() ? "rtl" : "ltr"));
            auto available = JS_NewArray(c);
            uint32_t n = 0;
            for (auto &p : locale.Configuration().locales)
                JS_SetPropertyUint32(c, available, n++, JS_NewString(c, p.first.c_str()));
            JS_SetPropertyStr(c, o, "available", available);
            return o;
        }
        if (op == "localeChoose") {
            if (!JS_IsString(arg(1)) || !locale.SetLocale(String(c, arg(1)), error))
                return JS_ThrowTypeError(c, "Localization: %s", error.c_str());
            return JS_TRUE;
        }
        if (op == "localeReload") {
            locale.Reload();
            return JS_UNDEFINED;
        }
        if (op == "localeNumber") {
            double value = 0;
            if (JS_ToFloat64(c, &value, arg(1)) || !std::isfinite(value))
                return JS_ThrowTypeError(c, "finite number required");
            NumberOptions options;
            auto read = [&](const char *key, int &n) {
                auto v = JS_GetPropertyStr(c, arg(2), key);
                bool ok = true;
                if (!JS_IsUndefined(v)) {
                    double d = 0;
                    ok = JS_ToFloat64(c, &d, v) == 0 && std::isfinite(d) && d == std::floor(d) &&
                         d >= 0 && d <= 12;
                    if (ok)
                        n = int(d);
                }
                JS_FreeValue(c, v);
                return ok;
            };
            if (!JS_IsObject(arg(2)) || !read("minimumFraction", options.minimumFraction) ||
                !read("maximumFraction", options.maximumFraction))
                return JS_ThrowTypeError(c, "invalid number options");
            auto grouping = JS_GetPropertyStr(c, arg(2), "grouping");
            if (!JS_IsUndefined(grouping)) {
                if (!JS_IsBool(grouping)) {
                    JS_FreeValue(c, grouping);
                    return JS_ThrowTypeError(c, "grouping requires boolean");
                }
                options.grouping = JS_ToBool(c, grouping);
            }
            JS_FreeValue(c, grouping);
            auto result = locale.Number(value, options, error);
            if (!error.empty())
                return JS_ThrowTypeError(c, "Localization: %s", error.c_str());
            return JS_NewStringLen(c, result.data(), result.size());
        }
        if (!JS_IsString(arg(1)) || !JS_IsObject(arg(2)))
            return JS_ThrowTypeError(c, "message key and named argument object required");
        MessageArguments arguments;
        JSPropertyEnum *props = nullptr;
        uint32_t count = 0;
        if (JS_GetOwnPropertyNames(c, &props, &count, arg(2),
                                   JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY))
            return JS_EXCEPTION;
        bool valid = count <= 32;
        for (uint32_t i = 0; i < count; ++i) {
            auto key = JS_AtomToString(c, props[i].atom);
            auto name = String(c, key);
            JS_FreeValue(c, key);
            auto value = JS_GetProperty(c, arg(2), props[i].atom);
            if (name.size() > 128)
                valid = false;
            if (JS_IsString(value))
                arguments[name] = String(c, value);
            else if (JS_IsNumber(value)) {
                double n = 0;
                if (JS_ToFloat64(c, &n, value) || !std::isfinite(n))
                    valid = false;
                else
                    arguments[name] = n;
            } else
                valid = false;
            JS_FreeValue(c, value);
            JS_FreeAtom(c, props[i].atom);
        }
        js_free(c, props);
        if (!valid)
            return JS_ThrowTypeError(c, "bounded string/finite-number message arguments required");
        auto result = locale.Format(String(c, arg(1)), arguments, error);
        if (!error.empty() && error.rfind("missing localization key", 0) != 0 &&
            error.rfind("localization catalog not yet published", 0) != 0)
            return JS_ThrowTypeError(c, "Localization: %s", error.c_str());
        return JS_NewStringLen(c, result.data(), result.size());
    }

    if (op == "elapsed")
        return JS_NewFloat64(c, world.SimulationTimeSeconds());

    if (op == "delta")
        return JS_NewFloat64(c, s->delta);

    if (op == "fixed")
        return JS_NewBool(c, s->fixed);

    if (op == "queryTags") {
        CategoryMask required, excluded;
        if (!tagMask(arg(1), required) || !tagMask(arg(2), excluded))
            return JS_ThrowTypeError(c, "unknown tag");
        return ids(world.QueryEntities(required, excluded));
    }

    if (op == "spawn") {
        SceneTransform t;
        if (!readTransform(arg(2), t))
            return JS_ThrowTypeError(c, "invalid transform");
        RuntimeWorld::PrefabSpawnOptions options;
        auto config = arg(3);
        if (!KnownOptions(c, config, {"velocity", "angularVelocity", "scripts"}))
            return JS_ThrowTypeError(c,
                                     "spawn options accept velocity/angularVelocity/scripts only");
        for (auto [key, destination] :
             {std::pair<const char *, std::optional<glm::vec3> *>{"velocity", &options.velocity},
              {"angularVelocity", &options.angularVelocity}}) {
            auto v = JS_GetPropertyStr(c, config, key);
            if (!JS_IsUndefined(v)) {
                glm::vec3 motion;
                if (!ReadVec(c, v, motion)) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "finite prefab motion required");
                }
                *destination = motion;
            }
            JS_FreeValue(c, v);
        }
        auto list = JS_GetPropertyStr(c, config, "scripts");
        std::string error;
        if (!JS_IsUndefined(list)) {
            if (!JS_IsArray(list)) {
                JS_FreeValue(c, list);
                return JS_ThrowTypeError(c, "scripts array required");
            }
            auto len = JS_GetPropertyStr(c, list, "length");
            uint32_t n = 0;
            JS_ToUint32(c, &n, len);
            JS_FreeValue(c, len);
            if (n > 256) {
                JS_FreeValue(c, list);
                return JS_ThrowRangeError(c, "script init limit");
            }
            for (uint32_t i = 0; i < n; ++i) {
                auto value = JS_GetPropertyUint32(c, list, i);
                RuntimeWorld::PrefabScriptInit init;
                auto src = JS_GetPropertyStr(c, value, "source"),
                     slot = JS_GetPropertyStr(c, value, "slot"),
                     props = JS_GetPropertyStr(c, value, "properties"),
                     state = JS_GetPropertyStr(c, value, "state");
                bool valid = KnownOptions(c, value, {"source", "slot", "properties", "state"});
                try {
                    auto decimal = [&](JSValueConst v) {
                        auto text = String(c, v);
                        if (text.empty() ||
                            text.find_first_not_of("0123456789") != std::string::npos)
                            throw std::runtime_error("decimal ID required");
                        return std::stoull(text);
                    };
                    if (!JS_IsUndefined(src))
                        init.source = decimal(src);
                    init.slot = decimal(slot);
                } catch (...) {
                    valid = false;
                }
                if (!JS_IsUndefined(props))
                    valid &= s->Json(props, init.properties, error);
                if (!JS_IsUndefined(state))
                    valid &= s->Json(state, init.state, error);
                JS_FreeValue(c, src);
                JS_FreeValue(c, slot);
                JS_FreeValue(c, props);
                JS_FreeValue(c, state);
                JS_FreeValue(c, value);
                if (!valid) {
                    JS_FreeValue(c, list);
                    return JS_ThrowTypeError(c, "invalid prefab script initialization: %s",
                                             error.c_str());
                }
                options.scripts.push_back(init);
            }
        }
        JS_FreeValue(c, list);
        auto id = world.SpawnPrefab(String(c, arg(1)), t, error, options);
        if (!id)
            return JS_ThrowTypeError(c, "spawn: %s", error.c_str());
        return JS_NewString(c, std::to_string(id).c_str());
    }

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
    if (op == "valid")
        return JS_NewBool(c, definition != nullptr);
    if (!definition)
        return JS_ThrowReferenceError(c, "stale or invalid entity %llu", (unsigned long long)id);

    if (op == "parent")
        return JS_NewString(c, std::to_string(definition->parent).c_str());

    if (op == "children") {
        std::vector<EntityId> result;
        for (const auto &o : world.ScriptObjects())
            if (o.parent == id)
                result.push_back(o.id);
        return ids(result);
    }

    if (op == "destroy") {
        std::string error;
        if (!world.DestroyHierarchy(id, error))
            return JS_ThrowTypeError(c, "destroy: %s", error.c_str());
        return JS_TRUE;
    }

    if (op == "transform" || op == "presentedTransform") {
        auto t = world.PresentedTransform(
            id, definition->transform,
            op == "presentedTransform" && s->inPresentation ? s->presentationAlpha : 1);
        if (op == "transform" && world.NetworkClient()) {
            EntityPhysicalState physical;
            if (world.GetEntityState(id, physical)) {
                t.position = physical.position;
                t.rotation = physical.rotation;
            }
        }
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "position", Vec(c, t.position));
        JS_SetPropertyStr(c, result, "scale", Vec(c, t.scale));
        auto q = Vec(c, {t.rotation.x, t.rotation.y, t.rotation.z});
        JS_SetPropertyStr(c, q, "w", JS_NewFloat64(c, t.rotation.w));
        JS_SetPropertyStr(c, result, "rotation", q);
        return result;
    }

    if (op == "setTransform") {
        auto t = world.PresentedTransform(id, definition->transform, 1);
        if (!readTransform(arg(2), t) || !world.SetRuntimeTransform(id, t))
            return JS_ThrowTypeError(c, "invalid transform");
        return JS_TRUE;
    }

    if (op == "hasTag" || op == "addTag" || op == "removeTag") {
        auto tag = world.Categories().tags.Find(String(c, arg(2)));
        if (tag < 0)
            return JS_ThrowTypeError(c, "unknown tag");
        return JS_NewBool(c, op == "hasTag"   ? world.HasTag(id, tag)
                             : op == "addTag" ? world.AddTag(id, tag)
                                              : world.RemoveTag(id, tag));
    }

    if (op == "classification") {
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "renderLayer", JS_NewUint32(c, world.RenderLayerOf(id)));
        unsigned layer = 0;
        CategoryMask mask = 0;
        world.Physics().GetCollisionFilter(world.RuntimeBody(id), layer, mask);
        JS_SetPropertyStr(c, o, "collisionLayer", JS_NewUint32(c, layer));
        JS_SetPropertyStr(c, o, "collisionMask", JS_NewString(c, std::to_string(mask).c_str()));
        return o;
    }
    return JS_ThrowTypeError(c, "unhandled World operation");
}
} // namespace
void RegisterWorldBindings(BindingRegistry &registry) {
    registry.Add("World",
                 {"worldOrigin",
                  "worldOriginState",
                  "worldAbsolute",
                  "worldLocal",
                  "worldOriginRequest",
                  "lifecycleBusy",
                  "saveExclude",
                  "saveRequest",
                  "saveList",
                  "saveStatus",
                  "saveCancel",
                  "saveReference",
                  "saveResolve",
                  "regionRequest",
                  "regionStatus",
                  "regionActivate",
                  "regionRelease",
                  "regionUnload",
                  "regionList",
                  "regionStats",
                  "regionOwner",
                  "regionResolve",
                  "regionPin",
                  "regionAdopt",
                  "regionRemoveInterest",
                  "regionInterest",
                  "sceneCurrent",
                  "sceneList",
                  "sceneLoad",
                  "sceneReload",
                  "sessionGet",
                  "sessionSet",
                  "sessionDelete",
                  "localeInfo",
                  "localeChoose",
                  "localeFormat",
                  "localeNumber",
                  "localeReload",
                  "elapsed",
                  "delta",
                  "fixed",
                  "queryTags",
                  "spawn",
                  "valid",
                  "parent",
                  "children",
                  "destroy",
                  "transform",
                  "presentedTransform",
                  "setTransform",
                  "hasTag",
                  "addTag",
                  "removeTag",
                  "classification"},
                 HandleWorld);
}
} // namespace ScriptBindings
