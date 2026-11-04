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
JSValue HandleNavigation(BindingContext &context, const std::string &op, int argc,
                         JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };
    auto navPath = [&](const NavigationPath &path) {
        auto v = JS_NewObject(c);
        const char *status = path.status == NavigationPath::Status::Complete  ? "complete"
                             : path.status == NavigationPath::Status::Partial ? "partial"
                                                                              : "failed";
        JS_SetPropertyStr(c, v, "status", JS_NewString(c, status));
        JS_SetPropertyStr(c, v, "distance", JS_NewFloat64(c, path.distance));
        JS_SetPropertyStr(c, v, "revision", JS_NewUint32(c, path.revision));
        auto corners = JS_NewArray(c);
        uint32_t i = 0;
        for (auto p : path.corners) {
            auto corner = JS_NewObject(c);
            JS_SetPropertyStr(c, corner, "position", Vec(c, p.position));
            JS_SetPropertyStr(c, corner, "linkId", JS_NewString(c, std::to_string(p.link).c_str()));
            JS_SetPropertyStr(c, corner, "linkEnd", Vec(c, p.linkEnd));
            JS_SetPropertyUint32(c, corners, i++, corner);
        }
        JS_SetPropertyStr(c, v, "corners", corners);
        auto areas = JS_NewArray(c);
        i = 0;
        for (auto id : path.areas)
            JS_SetPropertyUint32(c, areas, i++, JS_NewUint32(c, id));
        JS_SetPropertyStr(c, v, "areas", areas);
        return v;
    };

    auto navFilter = [&](JSValueConst options, NavigationFilter &f) {
        const auto &config = world.Navigation().Configuration();
        auto p = JS_GetPropertyStr(c, options, "profile");
        if (!JS_IsUndefined(p)) {
            if (JS_IsString(p)) {
                auto name = String(c, p);
                bool found = false;
                for (auto [id, profile] : config.profiles)
                    if (profile.name == name) {
                        f.profile = id;
                        found = true;
                    }
                if (!found) {
                    JS_FreeValue(c, p);
                    return false;
                }
            } else if (JS_ToUint32(c, &f.profile, p) != 0) {
                JS_FreeValue(c, p);
                return false;
            }
        }
        JS_FreeValue(c, p);
        if (!config.profiles.count(f.profile))
            return false;
        for (auto entry : {std::pair<const char *, CategoryMask *>{"includeAreas", &f.include},
                           {"excludeAreas", &f.exclude}}) {
            auto list = JS_GetPropertyStr(c, options, entry.first);
            if (!JS_IsUndefined(list)) {
                if (!JS_IsArray(list)) {
                    JS_FreeValue(c, list);
                    return false;
                }
                auto len = JS_GetPropertyStr(c, list, "length");
                uint32_t n = 0;
                JS_ToUint32(c, &n, len);
                JS_FreeValue(c, len);
                *entry.second = 0;
                if (n > 62) {
                    JS_FreeValue(c, list);
                    return false;
                }
                for (uint32_t i = 0; i < n; ++i) {
                    auto v = JS_GetPropertyUint32(c, list, i);
                    int id = config.areas.Find(String(c, v));
                    JS_FreeValue(c, v);
                    if (id < 0) {
                        JS_FreeValue(c, list);
                        return false;
                    }
                    *entry.second |= CategoryBit(id);
                }
            }
            JS_FreeValue(c, list);
        }
        auto costs = JS_GetPropertyStr(c, options, "costs");
        if (!JS_IsUndefined(costs)) {
            if (!JS_IsObject(costs)) {
                JS_FreeValue(c, costs);
                return false;
            }
            for (auto [id, name] : config.areas.names) {
                auto v = JS_GetPropertyStr(c, costs, name.c_str());
                if (!JS_IsUndefined(v)) {
                    double value;
                    if (JS_ToFloat64(c, &value, v) != 0 || !std::isfinite(value) || value < 1 ||
                        value > 1e6) {
                        JS_FreeValue(c, v);
                        JS_FreeValue(c, costs);
                        return false;
                    }
                    f.costs[id] = float(value);
                }
                JS_FreeValue(c, v);
            }
        }
        JS_FreeValue(c, costs);
        return true;
    };

    if (op == "navSample" || op == "navPath" || op == "navRaycast") {
        glm::vec3 a, b;
        NavigationFilter f;
        if (!ReadVec(c, arg(1), a) || !navFilter(arg(3), f))
            return JS_ThrowTypeError(c, "invalid navigation point/filter");
        if (op != "navSample" && !ReadVec(c, arg(2), b))
            return JS_ThrowTypeError(c, "invalid navigation endpoint");
        if (op == "navPath")
            return navPath(world.Navigation().FindPath(a, b, f));
        std::optional<NavigationLocation> location;
        if (op == "navSample") {
            double range;
            if (JS_ToFloat64(c, &range, arg(2)) != 0 || !std::isfinite(range) || range <= 0 ||
                range > 100)
                return JS_ThrowTypeError(c, "invalid sample range");
            location = world.Navigation().Sample(a, float(range), f);
        } else
            location = world.Navigation().Raycast(a, b, f);
        if (!location)
            return JS_NULL;
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "position", Vec(c, location->position));
        JS_SetPropertyStr(c, result, "surfaceId",
                          JS_NewString(c, std::to_string(location->surface).c_str()));
        JS_SetPropertyStr(c, result, "area", JS_NewUint32(c, location->area));
        return result;
    }

    if (op == "navAreas" || op == "navProfiles" || op == "navErrors") {
        auto result = JS_NewArray(c);
        uint32_t i = 0;
        const auto &config = world.Navigation().Configuration();
        if (op == "navErrors") {
            for (auto [id, error] : world.Navigation().Errors()) {
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(c, v, "entityId", JS_NewString(c, std::to_string(id).c_str()));
                JS_SetPropertyStr(c, v, "message", JS_NewString(c, error.c_str()));
                JS_SetPropertyUint32(c, result, i++, v);
            }
        } else if (op == "navAreas") {
            for (auto [id, name] : config.areas.names) {
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(c, v, "id", JS_NewUint32(c, id));
                JS_SetPropertyStr(c, v, "name", JS_NewString(c, name.c_str()));
                JS_SetPropertyUint32(c, result, i++, v);
            }
        } else
            for (auto [id, p] : config.profiles) {
                auto v = JS_NewObject(c);
                JS_SetPropertyStr(c, v, "id", JS_NewUint32(c, id));
                JS_SetPropertyStr(c, v, "name", JS_NewString(c, p.name.c_str()));
                JS_SetPropertyStr(c, v, "radius", JS_NewFloat64(c, p.radius));
                JS_SetPropertyStr(c, v, "height", JS_NewFloat64(c, p.height));
                JS_SetPropertyUint32(c, result, i++, v);
            }
        return result;
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
    if (!definition)
        return JS_ThrowReferenceError(c, "stale or invalid entity %llu", (unsigned long long)id);

    if (op == "navObstacleSet" || op == "navLinkSet") {
        if (s->inPresentation || world.NetworkClient())
            return JS_ThrowTypeError(c, "navigation edits require authoritative gameplay");
        auto options = arg(2);
        auto readBool = [&](const char *key, bool &out) {
            auto value = JS_GetPropertyStr(c, options, key);
            const bool valid = JS_IsUndefined(value) || JS_IsBool(value);
            if (JS_IsBool(value))
                out = JS_ToBool(c, value);
            JS_FreeValue(c, value);
            return valid;
        };
        auto readVector = [&](const char *key, glm::vec3 &out) {
            auto value = JS_GetPropertyStr(c, options, key);
            const bool valid = JS_IsUndefined(value) || ReadVec(c, value, out);
            JS_FreeValue(c, value);
            return valid;
        };
        auto readScalar = [&](const char *key, float &out) {
            auto value = JS_GetPropertyStr(c, options, key);
            double number = 0;
            const bool present = !JS_IsUndefined(value);
            const bool valid =
                !present ||
                (JS_IsNumber(value) && !JS_ToFloat64(c, &number, value) && std::isfinite(number) &&
                 number > 0 && number <= 1e20 && float(number) > 0);
            JS_FreeValue(c, value);
            if (valid && present)
                out = float(number);
            return valid;
        };
        if (op == "navObstacleSet") {
            if (!definition->navigationObstacle)
                return JS_FALSE;
            auto settings = *definition->navigationObstacle;
            if (!KnownOptions(
                    c, options,
                    {"enabled", "cylinder", "halfExtents", "radius", "height", "updateDistance"}) ||
                !readBool("enabled", settings.enabled) ||
                !readBool("cylinder", settings.cylinder) ||
                !readVector("halfExtents", settings.halfExtents) ||
                !readScalar("radius", settings.radius) || !readScalar("height", settings.height) ||
                !readScalar("updateDistance", settings.updateDistance))
                return JS_ThrowTypeError(c, "invalid navigation obstacle patch");
            std::string error;
            if (!world.SetNavigationObstacleSettings(id, settings, error))
                return JS_ThrowTypeError(
                    c, "%s", error.empty() ? "invalid navigation obstacle patch" : error.c_str());
            return JS_TRUE;
        }
        if (!definition->navigationLink)
            return JS_FALSE;
        auto settings = *definition->navigationLink;
        if (!KnownOptions(
                c, options,
                {"enabled", "bidirectional", "localStart", "localEnd", "radius", "area"}) ||
            !readBool("enabled", settings.enabled) ||
            !readBool("bidirectional", settings.bidirectional) ||
            !readVector("localStart", settings.start) || !readVector("localEnd", settings.end) ||
            !readScalar("radius", settings.radius))
            return JS_ThrowTypeError(c, "invalid navigation link patch");
        auto value = JS_GetPropertyStr(c, options, "area");
        double area = 0;
        const bool present = !JS_IsUndefined(value);
        const bool valid =
            !present || (JS_IsNumber(value) && !JS_ToFloat64(c, &area, value) &&
                         std::isfinite(area) && area >= 0 && area < 62 && area == std::floor(area));
        JS_FreeValue(c, value);
        if (!valid)
            return JS_ThrowTypeError(c, "invalid navigation area ID");
        if (present)
            settings.area = unsigned(area);
        if (!world.SetNavigationLinkSettings(id, settings))
            return JS_ThrowTypeError(c, "invalid navigation link or unregistered area");
        return JS_TRUE;
    }

    if (op == "navEnabled") {
        if (!JS_IsBool(arg(3)))
            return JS_ThrowTypeError(c, "boolean required");
        return JS_NewBool(c,
                          world.SetNavigationEnabled(id, String(c, arg(2)), JS_ToBool(c, arg(3))));
    }

    if (op == "navObstacleInfo") {
        if (!definition->navigationObstacle)
            return JS_NULL;
        const auto &n = *definition->navigationObstacle;
        auto v = JS_NewObject(c);
        JS_SetPropertyStr(c, v, "enabled", JS_NewBool(c, n.enabled));
        JS_SetPropertyStr(c, v, "cylinder", JS_NewBool(c, n.cylinder));
        JS_SetPropertyStr(c, v, "halfExtents", Vec(c, n.halfExtents));
        JS_SetPropertyStr(c, v, "radius", JS_NewFloat64(c, n.radius));
        JS_SetPropertyStr(c, v, "height", JS_NewFloat64(c, n.height));
        JS_SetPropertyStr(c, v, "updateDistance", JS_NewFloat64(c, n.updateDistance));
        return v;
    }

    if (op == "navLinkInfo") {
        if (!definition->navigationLink)
            return JS_NULL;
        const auto &n = *definition->navigationLink;
        auto t = world.PresentedTransform(id, definition->transform, 1);
        auto v = JS_NewObject(c);
        JS_SetPropertyStr(c, v, "enabled", JS_NewBool(c, n.enabled));
        JS_SetPropertyStr(c, v, "bidirectional", JS_NewBool(c, n.bidirectional));
        JS_SetPropertyStr(c, v, "start", Vec(c, t.position + t.rotation * n.start));
        JS_SetPropertyStr(c, v, "end", Vec(c, t.position + t.rotation * n.end));
        JS_SetPropertyStr(c, v, "area", JS_NewUint32(c, n.area));
        JS_SetPropertyStr(c, v, "radius", JS_NewFloat64(c, n.radius));
        JS_SetPropertyStr(c, v, "localStart", Vec(c, n.start));
        JS_SetPropertyStr(c, v, "localEnd", Vec(c, n.end));
        return v;
    }

    if (op == "navAgentExists")
        return JS_NewBool(c, definition->navigationAgent.has_value());

    if (op == "navAgentState" || op == "navDestination" || op == "navClear" || op == "navStopped" ||
        op == "navCompleteLink" || op == "navConfigure") {
        if (!definition->navigationAgent)
            return JS_ThrowTypeError(c, "entity has no navigation agent");
        if (definition->navigationAgent->enabled)
            world.Navigation().RegisterAgent(id);
        auto *a = world.Navigation().Agent(id);
        if (!a || !definition->navigationAgent->enabled)
            return JS_ThrowReferenceError(c, "disabled/unregistered navigation agent");
        if (op == "navDestination") {
            glm::vec3 v;
            if (!ReadVec(c, arg(2), v))
                return JS_ThrowTypeError(c, "finite destination required");
            return JS_NewBool(c, world.Navigation().SetDestination(id, v));
        }
        if (op == "navClear") {
            world.Navigation().ClearDestination(id);
            return JS_TRUE;
        }
        if (op == "navStopped") {
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "boolean required");
            a->stopped = JS_ToBool(c, arg(2));
            return JS_TRUE;
        }
        if (op == "navCompleteLink")
            return JS_NewBool(c, world.Navigation().CompleteLink(id));
        if (op == "navConfigure") {
            NavigationFilter f;
            f.profile = definition->navigationAgent->profile;
            f.include = definition->navigationAgent->areas;
            f.costs = definition->navigationAgent->costs;
            if (!navFilter(arg(2), f))
                return JS_ThrowTypeError(c, "invalid navigation filter");
            auto config = *definition->navigationAgent;
            config.profile = f.profile;
            config.areas = f.include & ~f.exclude;
            config.costs = f.costs;
            for (auto field : {std::pair<const char *, float *>{"speed", &config.speed},
                               {"arrival", &config.arrival},
                               {"cornerDistance", &config.cornerDistance},
                               {"repathSeconds", &config.repathSeconds}}) {
                auto v = JS_GetPropertyStr(c, arg(2), field.first);
                bool present = !JS_IsUndefined(v);
                JS_FreeValue(c, v);
                if (present && !Number(c, arg(2), field.first, *field.second))
                    return JS_ThrowTypeError(c, "invalid navigation setting");
            }
            auto v = JS_GetPropertyStr(c, arg(2), "avoidance");
            if (!JS_IsUndefined(v)) {
                if (!JS_IsBool(v)) {
                    JS_FreeValue(c, v);
                    return JS_ThrowTypeError(c, "boolean required");
                }
                config.avoidance = JS_ToBool(c, v);
            }
            JS_FreeValue(c, v);
            SceneObject check;
            check.navigationAgent = config;
            std::string error;
            if (!ValidateNavigationComponents(check, error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            world.SetNavigationAgentSettings(id, config);
            return JS_TRUE;
        }
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "stopped", JS_NewBool(c, a->stopped));
        JS_SetPropertyStr(c, result, "hasDestination", JS_NewBool(c, a->hasDestination));
        JS_SetPropertyStr(c, result, "destination", Vec(c, a->destination));
        JS_SetPropertyStr(c, result, "steering", Vec(c, a->steering));
        JS_SetPropertyStr(c, result, "remainingDistance", JS_NewFloat64(c, a->remaining));
        JS_SetPropertyStr(c, result, "reached",
                          JS_NewBool(c, a->hasDestination &&
                                            a->path.status == NavigationPath::Status::Complete &&
                                            a->remaining <= definition->navigationAgent->arrival));
        JS_SetPropertyStr(c, result, "nextCorner",
                          a->corner < a->path.corners.size()
                              ? Vec(c, a->path.corners[a->corner].position)
                              : JS_NULL);
        JS_SetPropertyStr(c, result, "onLink", JS_NewBool(c, a->onLink));
        JS_SetPropertyStr(
            c, result, "linkId",
            JS_NewString(c, std::to_string(a->onLink && a->corner < a->path.corners.size()
                                               ? a->path.corners[a->corner].link
                                               : 0)
                                .c_str()));
        JS_SetPropertyStr(c, result, "linkEnd",
                          Vec(c, a->onLink && a->corner < a->path.corners.size()
                                     ? a->path.corners[a->corner].linkEnd
                                     : glm::vec3(0)));
        JS_SetPropertyStr(c, result, "path", navPath(a->path));
        return result;
    }
    return JS_ThrowTypeError(c, "unhandled Navigation operation");
}
} // namespace
void RegisterNavigationBindings(BindingRegistry &registry) {
    registry.Add("Navigation",
                 {"navSample", "navPath", "navRaycast", "navAreas", "navProfiles", "navErrors",
                  "navObstacleSet", "navLinkSet", "navEnabled", "navObstacleInfo", "navLinkInfo",
                  "navAgentExists", "navAgentState", "navDestination", "navClear", "navStopped",
                  "navCompleteLink", "navConfigure"},
                 HandleNavigation);
}
} // namespace ScriptBindings
