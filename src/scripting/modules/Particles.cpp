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
JSValue HandleParticles(BindingContext &context, const std::string &op, int argc,
                        JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };

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

    if (op == "burst") {
        uint32_t n = 0;
        JS_ToUint32(c, &n, arg(2));
        if (n > 65536)
            return JS_ThrowTypeError(c, "burst too large");
        return JS_NewBool(c, world.EmitParticleBurst(id, n));
    }

    if (op == "particleInfo") {
        for (const auto &emitter : world.VisualEmitters())
            if (emitter.id == id) {
                const auto &p = emitter.pool.settings;
                auto out = JS_NewObject(c);
                for (auto [key, value] : {std::pair<const char *, bool>{"enabled", p.enabled},
                                          {"loop", p.loop},
                                          {"localSpace", p.localSpace},
                                          {"useGravity", p.useGravity}})
                    JS_SetPropertyStr(c, out, key, JS_NewBool(c, value));
                for (auto [key, value] : {std::pair<const char *, double>{"rate", p.rate},
                                          {"lifetime", p.lifetime},
                                          {"size", p.size},
                                          {"endSize", p.endSize},
                                          {"burst", p.burst * 1.0},
                                          {"maxParticles", p.maxParticles * 1.0},
                                          {"seed", p.seed * 1.0}})
                    JS_SetPropertyStr(c, out, key, JS_NewFloat64(c, value));
                for (auto [key, value] :
                     {std::pair<const char *, glm::vec3>{"velocity", p.velocity},
                      {"velocityVariation", p.velocityVariation},
                      {"spread", p.spread},
                      {"acceleration", p.acceleration}})
                    JS_SetPropertyStr(c, out, key, Vec(c, value));
                for (auto [key, value] : {std::pair<const char *, glm::vec4>{"color", p.color},
                                          {"endColor", p.endColor}}) {
                    auto rgba = JS_NewObject(c);
                    JS_SetPropertyStr(c, rgba, "r", JS_NewFloat64(c, value.r));
                    JS_SetPropertyStr(c, rgba, "g", JS_NewFloat64(c, value.g));
                    JS_SetPropertyStr(c, rgba, "b", JS_NewFloat64(c, value.b));
                    JS_SetPropertyStr(c, rgba, "a", JS_NewFloat64(c, value.a));
                    JS_SetPropertyStr(c, out, key, rgba);
                }
                JS_SetPropertyStr(c, out, "textureAsset",
                                  p.textureAsset.empty() ? JS_NULL
                                                         : JS_NewString(c, p.textureAsset.c_str()));
                return out;
            }
        return JS_NULL;
    }

    if (op == "particles") {
        auto options = arg(2);
        if (!KnownOptions(c, options,
                          {"enabled", "loop", "localSpace", "useGravity", "rate", "lifetime",
                           "size", "endSize", "velocity", "velocityVariation", "spread",
                           "acceleration", "color", "endColor", "burst", "maxParticles", "seed",
                           "textureAsset"}))
            return JS_ThrowTypeError(c, "unknown particle setting");
        for (auto &emitter : world.VisualEmitters())
            if (emitter.id == id) {
                auto settings = emitter.pool.settings;
                for (auto [key, value] :
                     {std::pair<const char *, bool *>{"enabled", &settings.enabled},
                      {"loop", &settings.loop},
                      {"localSpace", &settings.localSpace},
                      {"useGravity", &settings.useGravity}}) {
                    auto property = JS_GetPropertyStr(c, options, key);
                    const bool valid = JS_IsUndefined(property) || JS_IsBool(property) ||
                                       std::string_view(key) == "enabled";
                    if (!JS_IsUndefined(property) && valid)
                        *value = JS_ToBool(c, property);
                    JS_FreeValue(c, property);
                    if (!valid)
                        return JS_ThrowTypeError(c, "boolean particle setting required");
                }
                for (auto [key, value] : {std::pair<const char *, float *>{"rate", &settings.rate},
                                          {"size", &settings.size},
                                          {"endSize", &settings.endSize},
                                          {"lifetime", &settings.lifetime}}) {
                    auto property = JS_GetPropertyStr(c, options, key);
                    const bool present = !JS_IsUndefined(property);
                    JS_FreeValue(c, property);
                    if (present && !Number(c, options, key, *value))
                        return JS_ThrowTypeError(c, "finite particle scalar required");
                }
                for (auto [key, value] :
                     {std::pair<const char *, glm::vec3 *>{"velocity", &settings.velocity},
                      {"velocityVariation", &settings.velocityVariation},
                      {"spread", &settings.spread},
                      {"acceleration", &settings.acceleration}}) {
                    auto property = JS_GetPropertyStr(c, options, key);
                    const bool valid = JS_IsUndefined(property) || ReadVec(c, property, *value);
                    JS_FreeValue(c, property);
                    if (!valid)
                        return JS_ThrowTypeError(c, "finite particle vector required");
                }
                for (auto [key, value] :
                     {std::pair<const char *, glm::vec4 *>{"color", &settings.color},
                      {"endColor", &settings.endColor}}) {
                    auto property = JS_GetPropertyStr(c, options, key);
                    const bool valid =
                        JS_IsUndefined(property) ||
                        (Number(c, property, "r", value->r) && Number(c, property, "g", value->g) &&
                         Number(c, property, "b", value->b) && Number(c, property, "a", value->a));
                    JS_FreeValue(c, property);
                    if (!valid)
                        return JS_ThrowTypeError(c, "finite particle RGBA required");
                }
                auto integer = [&](const char *key, double maximum, auto &out) {
                    auto property = JS_GetPropertyStr(c, options, key);
                    double number = 0;
                    const bool present = !JS_IsUndefined(property);
                    const bool valid =
                        !present || (JS_IsNumber(property) && !JS_ToFloat64(c, &number, property) &&
                                     std::isfinite(number) && number >= 0 && number <= maximum &&
                                     number == std::floor(number));
                    JS_FreeValue(c, property);
                    if (valid && present)
                        out = static_cast<std::decay_t<decltype(out)>>(number);
                    return valid;
                };
                if (!integer("burst", 100000, settings.burst) ||
                    !integer("maxParticles", 100000, settings.maxParticles) ||
                    !integer("seed", UINT32_MAX, settings.seed))
                    return JS_ThrowRangeError(c, "invalid particle burst, capacity or seed");
                auto texture = JS_GetPropertyStr(c, options, "textureAsset");
                const bool validTexture =
                    JS_IsUndefined(texture) || JS_IsNull(texture) || JS_IsString(texture);
                if (JS_IsNull(texture))
                    settings.textureAsset.clear();
                else if (JS_IsString(texture))
                    settings.textureAsset = String(c, texture);
                JS_FreeValue(c, texture);
                if (!validTexture || !world.SetParticleSettings(id, settings))
                    return JS_ThrowTypeError(c, "invalid particle settings or texture asset");
                return JS_TRUE;
            }
        return JS_FALSE;
    }
    return JS_ThrowTypeError(c, "unhandled Particles operation");
}
} // namespace
void RegisterParticlesBindings(BindingRegistry &registry) {
    registry.Add("Particles", {"burst", "particleInfo", "particles"}, HandleParticles);
}
} // namespace ScriptBindings
