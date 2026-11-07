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

#include "../../PlanetTiles.h"
#include "../../terrain/TerrainSource.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleTerrain(BindingContext &context, const std::string &op, int argc,
                      JSValueConst *argv) {
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

    if (op == "terrainState" || op == "terrainCreate" || op == "terrainBrush" || op == "terrainLandscape") {
        EntityId owner = 0;
        try {
            auto identity = String(c, arg(1));
            if (identity.empty() || identity.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("identity");
            owner = std::stoull(identity);
        } catch (...) {
            return JS_ThrowReferenceError(c, "invalid terrain owner");
        }
        if (!world.RuntimeDefinition(owner))
            return JS_ThrowReferenceError(c, "stale terrain owner");
        if (op == "terrainState") {
            auto *state = world.LiveTerrain().Status(owner);
            if (!state)
                return JS_NULL;
            auto result = JS_NewObject(c);
            for (auto [key, value] : {std::pair<const char *, bool>{"ready", state->ready},
                                      {"busy", state->busy},
                                      {"navigation", state->navigation}})
                JS_SetPropertyStr(c, result, key, JS_NewBool(c, value));
            for (auto [key, value] :
                 {std::pair<const char *, double>{"revision", double(state->revision)},
                  {"samples", double(state->samples)},
                  {"triangles", double(state->triangles)},
                  {"bytes", double(state->bytes)},
                  {"chunks", double(state->chunks)},
                  {"volumeChunks", double(state->volumeChunks)},
                  {"pendingChunks", double(state->pendingChunks)},
                  {"prepareMs", state->prepareMs},
                  {"publishMs", state->publishMs}})
                JS_SetPropertyStr(c, result, key, JS_NewFloat64(c, value));
            JS_SetPropertyStr(c, result, "error", JS_NewString(c, state->error.c_str()));
            return result;
        }
        std::string error;
        if (op == "terrainLandscape") {
            TerrainV2::Source source;
            if (!JS_IsString(arg(2)) || !TerrainV2::Parse(String(c, arg(2)), source, error))
                return JS_ThrowTypeError(c, "invalid landscape source: %s", error.c_str());
            if (!world.LiveTerrain().CreateLandscape(owner, std::move(source), error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            return JS_TRUE;
        }
        if (op == "terrainCreate") {
            auto options = arg(2);
            auto source = JS_GetPropertyStr(c, options, "source");
            VolumeTerrainSource volume;
            if (!JS_IsUndefined(source)) {
                bool ok =
                    JS_IsString(source) && ParseVolumeTerrain(String(c, source), volume, error);
                JS_FreeValue(c, source);
                if (!ok)
                    return JS_ThrowTypeError(c, "invalid volume source: %s", error.c_str());
            } else {
                JS_FreeValue(c, source);
                auto n = JS_GetPropertyStr(c, options, "resolution"),
                     ext = JS_GetPropertyStr(c, options, "extent");
                glm::vec3 count, extent;
                bool ok = ReadVec(c, n, count) && ReadVec(c, ext, extent);
                JS_FreeValue(c, n);
                JS_FreeValue(c, ext);
                if (!ok || glm::any(glm::lessThan(count, glm::vec3(3))) ||
                    glm::any(glm::greaterThan(count, glm::vec3(32))) ||
                    count != glm::floor(count) || glm::any(glm::lessThan(extent, glm::vec3(.3f))) ||
                    glm::any(glm::greaterThan(extent, glm::vec3(64))))
                    return JS_ThrowTypeError(
                        c, "terrain requires integer resolution 3..32 and extent .3..64");
                try {
                    volume = CreateVolumeTerrain(glm::uvec3(count), extent);
                } catch (const std::exception &ex) {
                    return JS_ThrowTypeError(c, "%s", ex.what());
                }
            }
            auto nav = JS_GetPropertyStr(c, options, "navigation"),
                 profile = JS_GetPropertyStr(c, options, "profile");
            bool valid = JS_IsUndefined(nav) || JS_IsBool(nav);
            bool enabled = JS_IsBool(nav) && JS_ToBool(c, nav);
            double profileId = 0;
            valid &= JS_IsUndefined(profile) ||
                     (JS_IsNumber(profile) && JS_ToFloat64(c, &profileId, profile) == 0 &&
                      std::isfinite(profileId) && profileId >= 0 && profileId <= 65535 &&
                      profileId == std::floor(profileId));
            JS_FreeValue(c, nav);
            JS_FreeValue(c, profile);
            if (!valid)
                return JS_ThrowTypeError(c, "invalid terrain navigation/profile");
            if (!world.LiveTerrain().ReadyToSave())
                return JS_FALSE;
            if (!world.LiveTerrain().Create(owner, std::move(volume), enabled, unsigned(profileId),
                                            error))
                return JS_ThrowTypeError(c, "%s", error.c_str());
            return JS_TRUE;
        }
        glm::vec3 point;
        float radius = 0, strength = 1;
        auto options = arg(3);
        auto kind = JS_GetPropertyStr(c, options, "kind");
        auto name = String(c, kind);
        JS_FreeValue(c, kind);
        VolumeBrush brush;
        if (name == "remove")
            brush.kind = VolumeBrushKind::Remove;
        else if (name == "add")
            brush.kind = VolumeBrushKind::Add;
        else if (name == "smooth")
            brush.kind = VolumeBrushKind::Smooth;
        else
            return JS_ThrowTypeError(c, "terrain brush kind must be add/remove/smooth");
        auto value = JS_GetPropertyStr(c, options, "strength");
        bool hasStrength = !JS_IsUndefined(value);
        JS_FreeValue(c, value);
        if (!ReadVec(c, arg(2), point) || !Number(c, options, "radius", radius) ||
            (hasStrength && !Number(c, options, "strength", strength)))
            return JS_ThrowTypeError(c, "finite terrain brush point/radius/strength required");
        brush.radius = radius;
        brush.strength = strength;
        const auto *state = world.LiveTerrain().Status(owner);
        if (world.LiveTerrain().IsLandscape(owner) ? (state && state->busy)
                                                  : !world.LiveTerrain().ReadyToSave())
            return JS_FALSE;
        if (!world.LiveTerrain().Brush(owner, point, brush, error))
            return JS_ThrowTypeError(c, "%s", error.c_str());
        return JS_TRUE;
    }

    if (op == "planetInfo" || op == "planetSample" || op == "planetCollision" ||
        op == "planetClear") {
        EntityId id = 0;
        try {
            id = std::stoull(String(c, arg(1)));
        } catch (...) {
            return JS_ThrowReferenceError(c, "invalid planet entity");
        }
        auto *planet = world.Planet(id);
        if (!planet)
            return JS_ThrowReferenceError(c, "stale entity or missing planet terrain");
        if (op == "planetClear") {
            world.ClearPlanetCollision(id);
            return JS_TRUE;
        }
        if (op == "planetSample" || op == "planetCollision") {
            glm::dvec3 point;
            if (!doubleVector(arg(2), point) ||
                glm::length(point - planet->surface.settings.center) < 1e-10)
                return JS_ThrowTypeError(c, "finite absolute surface point required");
            if (op == "planetCollision") {
                std::string error;
                bool ready = world.PlanetCollision(id, point, error);
                if (!ready && !error.empty())
                    return JS_ThrowTypeError(c, "%s", error.c_str());
                return JS_NewBool(c, ready);
            }
            auto sample = planet->surface.Sample(point);
            auto o = JS_NewObject(c);
            JS_SetPropertyStr(c, o, "position", preciseVector(sample.point));
            JS_SetPropertyStr(c, o, "normal", preciseVector(sample.normal));
            JS_SetPropertyStr(c, o, "height", JS_NewFloat64(c, sample.height));
            JS_SetPropertyStr(c, o, "distance", JS_NewFloat64(c, sample.distance));
            return o;
        }
        const auto &stats = planet->State();
        auto o = JS_NewObject(c);
        for (const auto &entry : std::vector<std::pair<const char *, double>>{
                 {"radius", planet->surface.settings.radius},
                 {"leaves", stats.leaves},
                 {"resident", stats.resident},
                 {"triangles", stats.triangles},
                 {"colliders", stats.colliders},
                 {"maximumPixelError", stats.maximumPixelError},
                 {"prepareMs", stats.prepareMs},
                 {"collisionMs", stats.collisionMs}})
            JS_SetPropertyStr(c, o, entry.first, JS_NewFloat64(c, entry.second));
        JS_SetPropertyStr(c, o, "center", preciseVector(planet->surface.settings.center));
        JS_SetPropertyStr(c, o, "collisionReady", JS_NewBool(c, stats.collisionReady));
        JS_SetPropertyStr(c, o, "budgetLimited", JS_NewBool(c, stats.budgetLimited));
        JS_SetPropertyStr(c, o, "error", JS_NewString(c, stats.error.c_str()));
        return o;
    }
    return JS_ThrowTypeError(c, "unhandled Terrain operation");
}
} // namespace
void RegisterTerrainBindings(BindingRegistry &registry) {
    registry.Add("Terrain",
                 {"terrainState", "terrainCreate", "terrainLandscape", "terrainBrush", "planetInfo", "planetSample",
                  "planetCollision", "planetClear"},
                 HandleTerrain);
}
} // namespace ScriptBindings
