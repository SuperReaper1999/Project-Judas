#include "../../AssetDatabase.h"
#include "../../ResourceManager.h"
#include "../../RuntimeWorld.h"
#include "../../particles/ParticleImpacts.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"
#include "../BindingValues.h"
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
const char *BackendName(ParticleBackend value) {
    return value == ParticleBackend::Cpu ? "cpu" : value == ParticleBackend::Auto ? "auto" : "gpu";
}
const char *ResponseName(ParticleResponse value) {
    switch (value) {
    case ParticleResponse::Bounce: return "bounce";
    case ParticleResponse::Slide: return "slide";
    case ParticleResponse::Stick: return "stick";
    case ParticleResponse::Remove: return "remove";
    }
    return "invalid";
}
const char *ImpactName(ParticleImpactDelivery value) {
    return value == ParticleImpactDelivery::Off ? "off" :
           value == ParticleImpactDelivery::Cosmetic ? "cosmetic" : "reliable";
}
bool ReadLayers(JSContext *context, JSValueConst input, const CategoryRegistry &registry,
                std::uint64_t &mask) {
    if (JS_IsUndefined(input)) return true;
    if (JS_IsNull(input)) { mask = kAllCategories; return true; }
    if (JS_IsString(input)) {
        const auto text = String(context, input);
        std::uint64_t candidate = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), candidate);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            return false;
        mask = candidate;
        return true;
    }
    if (!JS_IsArray(input)) return false;
    const auto length = JS_GetPropertyStr(context, input, "length");
    std::uint32_t count = 0;
    const bool valid = !JS_ToUint32(context, &count, length) && count <= 64;
    JS_FreeValue(context, length);
    if (!valid) return false;
    std::uint64_t candidate = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto item = JS_GetPropertyUint32(context, input, index);
        const int layer = JS_IsString(item) ? registry.Find(String(context, item)) : -1;
        JS_FreeValue(context, item);
        if (layer < 0) return false;
        candidate |= CategoryBit(unsigned(layer));
    }
    mask = candidate;
    return true;
}
JSValue PreciseVector(JSContext *context, glm::dvec3 value) {
    auto out = JS_NewObject(context);
    if (JS_IsException(out)) return out;
    if (JS_SetPropertyStr(context, out, "x", JS_NewFloat64(context, value.x)) < 0 ||
        JS_SetPropertyStr(context, out, "y", JS_NewFloat64(context, value.y)) < 0 ||
        JS_SetPropertyStr(context, out, "z", JS_NewFloat64(context, value.z)) < 0) {
        JS_FreeValue(context, out);
        return JS_EXCEPTION;
    }
    return out;
}
JSValue Rgba(JSContext *context, glm::vec4 value) {
    auto out = JS_NewObject(context);
    if (JS_IsException(out)) return out;
    for (auto [name, component] : {std::pair<const char *, float>{"r", value.r},
                                  {"g", value.g}, {"b", value.b}, {"a", value.a}})
        if (JS_SetPropertyStr(context, out, name, JS_NewFloat64(context, component)) < 0) {
            JS_FreeValue(context, out);
            return JS_EXCEPTION;
        }
    return out;
}
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
        double count = 0;
        if (!JS_IsNumber(arg(2)) || JS_ToFloat64(c, &count, arg(2)) || !std::isfinite(count) ||
            count < 0 || count > 100000 || count != std::floor(count))
            return JS_ThrowRangeError(c, "particle burst count must be an integer from 0 to 100000");
        return JS_NewBool(c, world.EmitParticleBurst(id, unsigned(count)));
    }
    if (op == "particleControl") {
        if (!JS_IsString(arg(2)))
            return JS_ThrowTypeError(c, "particle control requires play, pause, stop, clear or restart");
        const auto command = String(c, arg(2));
        if (command != "play" && command != "pause" && command != "stop" &&
            command != "clear" && command != "restart")
            return JS_ThrowTypeError(c, "unknown particle control command");
        return JS_NewBool(c, world.ControlParticles(id, command));
    }
    if (op == "particleStats") {
        if (!definition->particleEmitter) return JS_NULL;
        const auto stats = world.GetParticleRuntimeStats(id);
        auto out = JS_NewObject(c);
        JS_SetPropertyStr(c, out, "requested", JS_NewString(c, BackendName(stats.requested)));
        // Required GPU can be unavailable without ever running CPU simulation.
        // Report absence rather than reusing the native enum's default value.
        JS_SetPropertyStr(c, out, "active",
                          stats.requested == ParticleBackend::Gpu && stats.gpuBytes == 0
                              ? JS_NULL
                              : JS_NewString(c, stats.active == ParticleBackend::Gpu ? "gpu" : "cpu"));
        for (auto [name, value] : {std::pair<const char *, std::uint64_t>{"live", stats.live},
                                   {"simulated", stats.simulated}, {"contacts", stats.contacts},
                                   {"queryFailures", stats.queryFailures},
                                   {"queuedImpacts", stats.queuedImpacts},
                                   {"deliveredImpacts", stats.deliveredImpacts},
                                   {"cosmeticDropped", stats.cosmeticDropped},
                                   {"snapshotBodies", stats.snapshotBodies},
                                   {"snapshotColliders", stats.snapshotColliders},
                                   {"snapshotTriangles", stats.snapshotTriangles},
                                   {"gpuBytes", stats.gpuBytes},
                                   {"geometryUploads", stats.geometryUploads},
                                   {"stateTransferBytes", stats.stateTransferBytes}})
            JS_SetPropertyStr(c, out, name, JS_NewFloat64(c, double(value)));
        for (auto [name, value] : {std::pair<const char *, bool>{"paused", stats.paused},
                                   {"backpressure", stats.backpressure}, {"pendingStep", stats.pendingStep}})
            JS_SetPropertyStr(c, out, name, JS_NewBool(c, value));
        JS_SetPropertyStr(c, out, "simulationMs", JS_NewFloat64(c, stats.simulationMs));
        JS_SetPropertyStr(c, out, "snapshotMs", JS_NewFloat64(c, stats.snapshotMs));
        JS_SetPropertyStr(c, out, "diagnostic", JS_NewString(c, stats.diagnostic.c_str()));
        return out;
    }
    if (op == "particleImpacts") {
        // Do not consume reliable native records until the complete detached
        // JavaScript array exists. Allocation/property failure leaves them queued.
        const auto *records = world.PeekParticleImpacts(id);
        auto out = JS_NewArray(c);
        if (JS_IsException(out) || !records || records->empty()) return out;
        const auto lastSequence = records->back().sequence;
        std::uint32_t index = 0;
        for (const auto &impact : *records) {
            auto item = JS_NewObject(c);
            if (JS_IsException(item)) { JS_FreeValue(c, out); return item; }
            const auto set = [&](const char *name, JSValue value) {
                return !JS_IsException(value) && JS_SetPropertyStr(c, item, name, value) >= 0;
            };
            bool okay = true;
            for (auto [name, value] : {std::pair<const char *, std::uint64_t>{"sequence", impact.sequence},
                                       {"tick", impact.tick}, {"receiver", impact.receiver},
                                       {"geometryRevision", impact.geometryRevision}}) {
                if (!set(name, JS_NewString(c, std::to_string(value).c_str()))) { okay = false; break; }
            }
            okay = okay && set("childKey", JS_NewUint32(c, impact.childKey)) &&
                   set("feature", impact.feature == UINT32_MAX ? JS_NULL : JS_NewUint32(c, impact.feature)) &&
                   set("position", PreciseVector(c, impact.position)) &&
                   set("localPosition", PreciseVector(c, impact.localPosition)) &&
                   set("normal", PreciseVector(c, glm::dvec3(impact.normal))) &&
                   set("localNormal", PreciseVector(c, glm::dvec3(impact.localNormal))) &&
                   set("velocity", PreciseVector(c, glm::dvec3(impact.velocity))) &&
                   set("surfaceVelocity", PreciseVector(c, glm::dvec3(impact.surfaceVelocity))) &&
                   set("color", Rgba(c, impact.color)) && set("size", JS_NewFloat64(c, impact.size)) &&
                   set("age", JS_NewFloat64(c, impact.age)) &&
                   set("count", JS_NewFloat64(c, double(impact.count))) &&
                   set("weight", JS_NewFloat64(c, impact.weight));
            if (!okay) { JS_FreeValue(c, item); JS_FreeValue(c, out); return JS_EXCEPTION; }
            // The setter owns item on both success and failure.
            if (JS_SetPropertyUint32(c, out, index++, item) < 0) {
                JS_FreeValue(c, out);
                return JS_EXCEPTION;
            }
        }
        if (!world.AcknowledgeParticleImpacts(id, lastSequence)) {
            JS_FreeValue(c, out);
            return JS_ThrowInternalError(c, "particle impact queue changed before delivery acknowledgement");
        }
        return out;
    }

    if (op == "particleInfo") {
        for (const auto &emitter : world.VisualEmitters())
            if (emitter.id == id) {
                const auto &p = emitter.pool.settings;
                auto out = JS_NewObject(c);
                for (auto [key, value] : {std::pair<const char *, bool>{"enabled", p.enabled},
                                          {"loop", p.loop},
                                          {"localSpace", p.localSpace},
                                          {"useGravity", p.useGravity},
                                          {"collision", p.collision},
                                          {"collisionRegion", p.collisionRegion},
                                          {"useWind", p.useWind}})
                    JS_SetPropertyStr(c, out, key, JS_NewBool(c, value));
                for (auto [key, value] : {std::pair<const char *, double>{"rate", p.rate},
                                          {"lifetime", p.lifetime},
                                          {"size", p.size},
                                          {"endSize", p.endSize},
                                          {"burst", p.burst * 1.0},
                                          {"maxParticles", p.maxParticles * 1.0},
                                          {"seed", p.seed * 1.0},
                                          {"collisionRadius", p.collisionRadius},
                                          {"restitution", p.restitution},
                                          {"friction", p.friction},
                                          {"maxContacts", p.maxContacts * 1.0},
                                          {"impactCellSize", p.impactCellSize},
                                          {"impactWeight", p.impactWeight},
                                          {"maxImpactCells", p.maxImpactCells * 1.0}})
                    JS_SetPropertyStr(c, out, key, JS_NewFloat64(c, value));
                for (auto [key, value] :
                     {std::pair<const char *, glm::vec3>{"velocity", p.velocity},
                      {"velocityVariation", p.velocityVariation},
                      {"spread", p.spread},
                      {"acceleration", p.acceleration},
                      {"collisionRegionOffset", p.collisionRegionOffset},
                      {"collisionRegionHalfExtents", p.collisionRegionHalfExtents}})
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
                JS_SetPropertyStr(c, out, "backend", JS_NewString(c, BackendName(p.backend)));
                JS_SetPropertyStr(c, out, "response", JS_NewString(c, ResponseName(p.response)));
                JS_SetPropertyStr(c, out, "impacts", JS_NewString(c, ImpactName(p.impacts)));
                JS_SetPropertyStr(c, out, "collisionLayers", JS_NewString(c, std::to_string(p.collisionLayers).c_str()));
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
                           "textureAsset", "backend", "collision", "useWind", "collisionRadius",
                           "restitution", "friction", "response", "collisionLayers", "maxContacts",
                           "impacts", "impactCellSize", "impactWeight", "maxImpactCells",
                           "collisionRegion", "collisionRegionOffset", "collisionRegionHalfExtents"}))
            return JS_ThrowTypeError(c, "unknown particle setting");
        for (auto &emitter : world.VisualEmitters())
            if (emitter.id == id) {
                auto settings = emitter.pool.settings;
                for (auto [key, value] :
                     {std::pair<const char *, bool *>{"enabled", &settings.enabled},
                      {"loop", &settings.loop},
                      {"localSpace", &settings.localSpace},
                      {"useGravity", &settings.useGravity},
                      {"collision", &settings.collision},
                      {"collisionRegion", &settings.collisionRegion},
                      {"useWind", &settings.useWind}}) {
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
                                          {"lifetime", &settings.lifetime},
                                          {"collisionRadius", &settings.collisionRadius},
                                          {"restitution", &settings.restitution},
                                          {"friction", &settings.friction},
                                          {"impactCellSize", &settings.impactCellSize},
                                          {"impactWeight", &settings.impactWeight}}) {
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
                      {"acceleration", &settings.acceleration},
                      {"collisionRegionOffset", &settings.collisionRegionOffset},
                      {"collisionRegionHalfExtents", &settings.collisionRegionHalfExtents}}) {
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
                    !integer("seed", UINT32_MAX, settings.seed) ||
                    !integer("maxContacts", 32, settings.maxContacts) ||
                    !integer("maxImpactCells", 65536, settings.maxImpactCells))
                    return JS_ThrowRangeError(c, "invalid particle burst, capacity, seed, contact or impact-cell limit");
                const auto enumeration = [&](const char *key, auto &value,
                                               std::initializer_list<const char *> names) {
                    const auto property = JS_GetPropertyStr(c, options, key);
                    if (JS_IsUndefined(property)) { JS_FreeValue(c, property); return true; }
                    const bool string = JS_IsString(property);
                    const auto text = string ? String(c, property) : std::string{};
                    JS_FreeValue(c, property);
                    unsigned index = 0;
                    for (const auto *name : names) {
                        if (string && text == name) { value = std::decay_t<decltype(value)>(index); return true; }
                        ++index;
                    }
                    return false;
                };
                if (!enumeration("backend", settings.backend, {"cpu", "auto", "gpu"}) ||
                    !enumeration("response", settings.response, {"bounce", "slide", "stick", "remove"}) ||
                    !enumeration("impacts", settings.impacts, {"off", "cosmetic", "reliable"}))
                    return JS_ThrowTypeError(c, "unknown particle backend, collision response or impact delivery");
                const auto layers = JS_GetPropertyStr(c, options, "collisionLayers");
                const bool validLayers = ReadLayers(c, layers, world.Categories().collision, settings.collisionLayers);
                JS_FreeValue(c, layers);
                if (!validLayers)
                    return JS_ThrowTypeError(c, "particle collisionLayers requires registered layer names, an exact decimal mask, or null");
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
    registry.Add("Particles", {"burst", "particleInfo", "particles", "particleStats", "particleImpacts", "particleControl"}, HandleParticles);
}
} // namespace ScriptBindings
