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

#include "../../ProductionFluidCoupling.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleEnvironment(BindingContext &context, const std::string &op, int argc,
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

    auto weatherProfileValue = [&](const WeatherProfile &p) {
        auto result = JS_NewObject(c);
        for (const auto &field :
             std::vector<std::pair<const char *, double>>{{"cloudCoverage", p.cloudCoverage},
                                                          {"cloudDensity", p.cloudDensity},
                                                          {"cloudBase", p.cloudBase},
                                                          {"cloudThickness", p.cloudThickness},
                                                          {"rain", p.rain},
                                                          {"snow", p.snow},
                                                          {"fog", p.fog},
                                                          {"gust", p.gust},
                                                          {"lightningRate", p.lightningRate},
                                                          {"tornado", p.tornado},
                                                          {"tornadoRadius", p.tornadoRadius},
                                                          {"tornadoHeight", p.tornadoHeight},
                                                          {"tornadoSpeed", p.tornadoSpeed}})
            JS_SetPropertyStr(c, result, field.first, JS_NewFloat64(c, field.second));
        JS_SetPropertyStr(c, result, "wind", preciseVector(p.wind));
        return result;
    };

    auto liquidHandle = [&](JSValueConst value) {
        LiquidHandle h;
        std::istringstream text(String(c, value));
        char separator = 0;
        if (!(text >> h.generation >> separator >> h.id) || separator != ':')
            return LiquidHandle{};
        text >> std::ws;
        return text.eof() ? h : LiquidHandle{};
    };

    if (op == "weatherQuality") {
        if (argc > 1) {
            double quality = 0;
            if (!JS_IsNumber(arg(1)) || JS_ToFloat64(c, &quality, arg(1)) != 0 || quality < 0 ||
                quality > 2 || std::floor(quality) != quality)
                return JS_ThrowTypeError(c, "weather quality must be 0, 1 or 2");
            world.weatherQuality = unsigned(quality);
        }
        return JS_NewUint32(c, world.weatherQuality);
    }

    if (op == "weatherSample") {
        glm::dvec3 local;
        if (!doubleVector(arg(1), local))
            return JS_ThrowTypeError(c, "finite local weather query point required");
        const auto absolute = world.Settings().worldOrigin + local;
        if (glm::any(glm::greaterThan(glm::abs(absolute), glm::dvec3(1.01e12))))
            return JS_ThrowTypeError(c, "weather query exceeds absolute precision range");
        const auto sample = world.SampleWeather(absolute);
        if (!sample.active)
            return JS_NULL;
        auto result = JS_NewObject(c);
        JS_SetPropertyStr(c, result, "wind", preciseVector(sample.wind));
        JS_SetPropertyStr(c, result, "up", preciseVector(sample.up));
        JS_SetPropertyStr(c, result, "tornadoPosition",
                          preciseVector(sample.tornadoPosition - world.Settings().worldOrigin));
        for (const auto &field : std::vector<std::pair<const char *, double>>{
                 {"altitude", sample.altitude},
                 {"airDensity", sample.airDensity},
                 {"rain", sample.profile.rain},
                 {"snow", sample.profile.snow},
                 {"fog", sample.profile.fog},
                 {"cloudCoverage", sample.profile.cloudCoverage},
                 {"lightningFlash", sample.lightningFlash}})
            JS_SetPropertyStr(c, result, field.first, JS_NewFloat64(c, field.second));
        return result;
    }

    if (op == "weatherExists" || op == "weatherState" || op == "weatherEnabled" ||
        op == "weatherTransition" || op == "weatherStrike") {
        EntityId owner = 0;
        try {
            const auto identity = String(c, arg(1));
            if (identity.empty() || identity.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("weather identity");
            owner = std::stoull(identity);
            if (!owner)
                throw std::invalid_argument("zero weather identity");
        } catch (...) {
            if (op == "weatherExists")
                return JS_FALSE;
            return JS_ThrowReferenceError(c, "invalid weather owner");
        }
        auto *field = world.RuntimeWeather(owner);
        if (op == "weatherExists")
            return JS_NewBool(c, field != nullptr);
        if (!field)
            return JS_ThrowReferenceError(c, "stale entity or absent weather component");
        if (op == "weatherState") {
            const auto &state = field->State();
            const auto *definition = world.RuntimeDefinition(owner);
            const auto sample = field->Sample(world.Settings().worldOrigin +
                                              glm::dvec3(definition->transform.position));
            auto result = JS_NewObject(c);
            JS_SetPropertyStr(c, result, "current", weatherProfileValue(field->Current()));
            JS_SetPropertyStr(c, result, "target", weatherProfileValue(state.to));
            JS_SetPropertyStr(c, result, "time", JS_NewFloat64(c, state.time));
            JS_SetPropertyStr(c, result, "transitionElapsed",
                              JS_NewFloat64(c, state.transitionElapsed));
            JS_SetPropertyStr(c, result, "transitionDuration",
                              JS_NewFloat64(c, state.transitionDuration));
            JS_SetPropertyStr(c, result, "tornadoPosition",
                              preciseVector(sample.tornadoPosition - world.Settings().worldOrigin));
            JS_SetPropertyStr(c, result, "lightningSequence",
                              JS_NewString(c, std::to_string(state.lightning.sequence).c_str()));
            JS_SetPropertyStr(
                c, result, "lightningPosition",
                preciseVector(state.lightning.position - world.Settings().worldOrigin));
            JS_SetPropertyStr(c, result, "lightningFlash", JS_NewFloat64(c, sample.lightningFlash));
            return result;
        }
        if (op == "weatherEnabled" && argc == 2)
            return JS_NewBool(c, field->Settings().enabled);
        if (!s->fixed)
            return JS_ThrowTypeError(c, "authoritative weather mutation requires fixedUpdate");
        if (op == "weatherEnabled") {
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "weather enabled requires Boolean");
            field->SetEnabled(JS_ToBool(c, arg(2)) != 0);
            return JS_TRUE;
        }
        if (op == "weatherStrike") {
            glm::dvec3 local;
            if (JS_IsUndefined(arg(2))) {
                const auto &frame = field->Settings().frame;
                local = frame.center - world.Settings().worldOrigin;
                if (frame.radial)
                    local += glm::normalize(frame.up) * frame.radius;
            } else if (!doubleVector(arg(2), local))
                return JS_ThrowTypeError(c, "finite local lightning position required");
            try {
                field->TriggerLightning(world.Settings().worldOrigin + local);
            } catch (const std::exception &e) {
                return JS_ThrowTypeError(c, "%s", e.what());
            }
            return JS_TRUE;
        }
        auto patch = arg(2);
        if (!KnownOptions(c, patch,
                          {"cloudCoverage", "cloudDensity", "cloudBase", "cloudThickness", "rain",
                           "snow", "fog", "wind", "gust", "lightningRate", "tornado",
                           "tornadoRadius", "tornadoHeight", "tornadoSpeed"}))
            return JS_ThrowTypeError(c, "weather transition accepts known profile fields only");
        auto profile = field->Current();
        const std::vector<std::pair<const char *, double WeatherProfile::*>> fields = {
            {"cloudCoverage", &WeatherProfile::cloudCoverage},
            {"cloudDensity", &WeatherProfile::cloudDensity},
            {"cloudBase", &WeatherProfile::cloudBase},
            {"cloudThickness", &WeatherProfile::cloudThickness},
            {"rain", &WeatherProfile::rain},
            {"snow", &WeatherProfile::snow},
            {"fog", &WeatherProfile::fog},
            {"gust", &WeatherProfile::gust},
            {"lightningRate", &WeatherProfile::lightningRate},
            {"tornado", &WeatherProfile::tornado},
            {"tornadoRadius", &WeatherProfile::tornadoRadius},
            {"tornadoHeight", &WeatherProfile::tornadoHeight},
            {"tornadoSpeed", &WeatherProfile::tornadoSpeed}};
        for (const auto &member : fields) {
            auto value = JS_GetPropertyStr(c, patch, member.first);
            if (!JS_IsUndefined(value)) {
                double number = 0;
                const bool ok = JS_IsNumber(value) && JS_ToFloat64(c, &number, value) == 0 &&
                                std::isfinite(number);
                JS_FreeValue(c, value);
                if (!ok)
                    return JS_ThrowTypeError(c, "finite weather scalar required: %s", member.first);
                profile.*member.second = number;
            } else
                JS_FreeValue(c, value);
        }
        auto wind = JS_GetPropertyStr(c, patch, "wind");
        if (!JS_IsUndefined(wind) && !doubleVector(wind, profile.wind)) {
            JS_FreeValue(c, wind);
            return JS_ThrowTypeError(c, "weather wind requires finite {x,y,z}");
        }
        JS_FreeValue(c, wind);
        double duration = 1;
        if (!JS_IsNumber(arg(3)) || JS_ToFloat64(c, &duration, arg(3)) != 0 ||
            !std::isfinite(duration))
            return JS_ThrowTypeError(c, "finite weather transition duration required");
        try {
            field->Transition(profile, duration);
        } catch (const std::exception &e) {
            return JS_ThrowTypeError(c, "%s", e.what());
        }
        return JS_TRUE;
    }

    if (op == "liquidValid")
        return JS_NewBool(c, world.Liquids().Get(liquidHandle(arg(1))) != nullptr);

    if (op == "liquidState" || op == "liquidEnabled" || op == "liquidTransfer" ||
        op == "liquidImpulse" || op == "liquidSurfaceEnabled") {
        auto h = liquidHandle(arg(1));
        auto *state = world.Liquids().Get(h);
        if (!state)
            return JS_ThrowReferenceError(c, "stale liquid handle");
        if (op == "liquidEnabled") {
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "boolean required");
            return JS_NewBool(c, world.Liquids().SetEnabled(h, JS_ToBool(c, arg(2))));
        }
        if (op == "liquidImpulse") {
            if (!s->fixed)
                return JS_ThrowTypeError(c, "surface impulse belongs in fixedUpdate");
            glm::vec3 p, v;
            if (!ReadVec(c, arg(2), p) || !ReadVec(c, arg(3), v))
                return JS_ThrowTypeError(c, "finite point and impulse required");
            return JS_NewBool(c, world.Liquids().SurfaceImpulse(h, p, v));
        }
        if (op == "liquidSurfaceEnabled") {
            if (!JS_IsBool(arg(2)))
                return JS_ThrowTypeError(c, "boolean required");
            return JS_NewBool(c, world.Liquids().SurfaceEnabled(h, JS_ToBool(c, arg(2))));
        }
        if (op == "liquidTransfer") {
            if (!s->fixed)
                return JS_ThrowTypeError(c, "liquid transfer belongs in fixedUpdate");
            auto other = liquidHandle(arg(2));
            if (!world.Liquids().Get(other))
                return JS_ThrowReferenceError(c, "stale destination");
            double volume;
            if (JS_ToFloat64(c, &volume, arg(3)) || !std::isfinite(volume) || volume < 0)
                return JS_ThrowTypeError(c, "finite nonnegative volume required");
            std::optional<glm::vec3> sourcePoint, destinationPoint;
            auto options = arg(4);
            for (auto entry :
                 {std::pair<const char *, std::optional<glm::vec3> *>{"sourcePoint", &sourcePoint},
                  {"destinationPoint", &destinationPoint}}) {
                auto v = JS_GetPropertyStr(c, options, entry.first);
                if (!JS_IsUndefined(v)) {
                    glm::vec3 p;
                    if (!ReadVec(c, v, p)) {
                        JS_FreeValue(c, v);
                        return JS_ThrowTypeError(c, "finite exchange point required");
                    }
                    *entry.second = p;
                }
                JS_FreeValue(c, v);
            }
            return JS_NewFloat64(
                c, world.Liquids().Transfer(h, other, volume, sourcePoint, destinationPoint));
        }
        auto v = JS_NewObject(c);
        JS_SetPropertyStr(c, v, "entityId", JS_NewString(c, std::to_string(state->entity).c_str()));
        JS_SetPropertyStr(c, v, "enabled", JS_NewBool(c, state->enabled));
        JS_SetPropertyStr(c, v, "container", JS_NewBool(c, state->container));
        JS_SetPropertyStr(c, v, "equilibriumValid", JS_NewBool(c, state->equilibriumValid));
        JS_SetPropertyStr(c, v, "material", JS_NewString(c, state->material.id.c_str()));
        JS_SetPropertyStr(c, v, "density", JS_NewFloat64(c, state->material.density));
        JS_SetPropertyStr(c, v, "volume", JS_NewFloat64(c, state->volume));
        JS_SetPropertyStr(c, v, "capacity", JS_NewFloat64(c, state->capacity));
        JS_SetPropertyStr(c, v, "stableCapacity", JS_NewFloat64(c, state->stableCapacity));
        JS_SetPropertyStr(c, v, "coordinate", JS_NewFloat64(c, state->q));
        auto surface = state->dynamicSurface;
        JSValue info = JS_NULL;
        if (surface) {
            info = JS_NewObject(c);
            JS_SetPropertyStr(c, info, "enabled", JS_NewBool(c, surface->enabled));
            for (auto field :
                 {std::pair<const char *, double>{"cells", double(surface->cells.size())},
                  {"faces", double(surface->discharge.size())},
                  {"volume", surface->Volume()},
                  {"iterations", double(surface->stats.iterations)},
                  {"retries", double(surface->stats.retries)},
                  {"limitedFaces", double(surface->stats.limitedFaces)},
                  {"residual", surface->stats.residual},
                  {"partitionError", surface->Volume() - state->volume},
                  {"stepSeconds", surface->stats.seconds}})
                JS_SetPropertyStr(c, info, field.first, JS_NewFloat64(c, field.second));
        }
        JS_SetPropertyStr(c, v, "surface", info);
        return v;
    }

    if (op == "liquidAccounting") {
        auto a = world.Liquids().Accounting(String(c, arg(1)));
        auto v = JS_NewObject(c);
        for (auto field : {std::pair<const char *, double>{"reservoirs", a.reservoirs},
                           {"containers", a.containers},
                           {"detached", a.detached},
                           {"total", a.total},
                           {"expected", a.expected},
                           {"error", a.error},
                           {"tolerance", a.tolerance}}) {
            JS_SetPropertyStr(c, v, field.first, JS_NewFloat64(c, field.second));
        }
        return v;
    }

    if (op == "liquidSample" || op == "liquidPresentedSample") {
        glm::vec3 p;
        if (!ReadVec(c, arg(1), p))
            return JS_ThrowTypeError(c, "finite point required");
        auto sample = world.Liquids().Sample(
            p, {}, op == "liquidPresentedSample" && s->inPresentation ? s->presentationAlpha : 1);
        if (!sample)
            return JS_NULL;
        auto v = JS_NewObject(c);
        JS_SetPropertyStr(c, v, "entityId",
                          JS_NewString(c, std::to_string(sample->entity).c_str()));
        JS_SetPropertyStr(c, v, "material", JS_NewString(c, sample->material.id.c_str()));
        JS_SetPropertyStr(c, v, "density", JS_NewFloat64(c, sample->material.density));
        JS_SetPropertyStr(c, v, "depth", JS_NewFloat64(c, sample->depth));
        JS_SetPropertyStr(c, v, "coordinate", JS_NewFloat64(c, sample->coordinate));
        JS_SetPropertyStr(c, v, "surfacePoint", Vec(c, sample->surfacePoint));
        JS_SetPropertyStr(c, v, "normal", Vec(c, sample->normal));
        JS_SetPropertyStr(c, v, "up", Vec(c, sample->up));
        JS_SetPropertyStr(c, v, "velocity", Vec(c, sample->velocity));
        return v;
    }

    if (op == "liquidErrors" || op == "liquidConnections") {
        auto v = JS_NewArray(c);
        uint32_t i = 0;
        if (op == "liquidErrors")
            for (auto [id, error] : world.Liquids().Errors()) {
                auto e = JS_NewObject(c);
                JS_SetPropertyStr(c, e, "entityId", JS_NewString(c, std::to_string(id).c_str()));
                JS_SetPropertyStr(c, e, "message", JS_NewString(c, error.c_str()));
                JS_SetPropertyUint32(c, v, i++, e);
            }
        else
            for (auto [id, active] : world.Liquids().Connections()) {
                auto e = JS_NewObject(c);
                JS_SetPropertyStr(c, e, "entityId", JS_NewString(c, std::to_string(id).c_str()));
                JS_SetPropertyStr(c, e, "active", JS_NewBool(c, active));
                JS_SetPropertyUint32(c, v, i++, e);
            }
        return v;
    }

    if (op == "liquidSubmerged") {
        EntityId id = 0;
        try {
            id = std::stoull(String(c, arg(1)));
        } catch (...) {
            return JS_ThrowReferenceError(c, "invalid entity");
        }
        auto *o = world.RuntimeDefinition(id);
        if (!o || !o->body)
            return JS_ThrowReferenceError(c, "missing collider");
        auto h = world.RuntimeBody(id);
        if (!h.IsValid())
            return JS_ThrowReferenceError(c, "stale collider");
        if (!world.Physics().IsBodyEnabled(h)) {
            auto v = JS_NewObject(c);
            JS_SetPropertyStr(c, v, "volume", JS_NewFloat64(c, 0));
            JS_SetPropertyStr(c, v, "center", Vec(c, glm::vec3(0)));
            JS_SetPropertyStr(c, v, "buoyancy", Vec(c, glm::vec3(0)));
            return v;
        }
        auto pose = world.Physics().GetTransform(h);
        LiquidGeometry geometry;
        if (o->body->shape == SceneShape::Box)
            geometry =
                LiquidBox(-glm::dvec3(o->body->halfExtents), glm::dvec3(o->body->halfExtents));
        else if (o->body->shape == SceneShape::Compound)
            for (auto box : o->body->compoundBoxes) {
                if (box.type != ShapeType::Box)
                    return JS_ThrowTypeError(
                        c, "submerged requires box children; no hull volume proxy");
                auto g = LiquidBox(-glm::dvec3(box.halfExtents), glm::dvec3(box.halfExtents));
                for (auto &cell : g.cells)
                    for (auto &p : cell)
                        p = glm::dvec3(box.localCenter) + glm::dquat(box.rotation) * p;
                geometry.cells.insert(geometry.cells.end(), g.cells.begin(), g.cells.end());
            }
        else
            return JS_ThrowTypeError(c, "submerged supports box/compound colliders");
        for (auto &t : geometry.cells)
            for (auto &p : t)
                p = glm::dvec3(pose.position) + glm::dquat(pose.rotation) * p;
        auto a = world.Liquids().Submerged(geometry);
        auto v = JS_NewObject(c);
        JS_SetPropertyStr(c, v, "volume", JS_NewFloat64(c, a.volume));
        JS_SetPropertyStr(c, v, "center", Vec(c, a.center));
        JS_SetPropertyStr(c, v, "buoyancy", Vec(c, a.buoyancy));
        return v;
    }

    if (op == "fluidSample") {
        glm::vec3 p, up, tangent;
        double height, radius;
        if (!ReadVec(c, arg(1), p) || !ReadVec(c, arg(2), up) || !ReadVec(c, arg(5), tangent) ||
            JS_ToFloat64(c, &height, arg(3)) || JS_ToFloat64(c, &radius, arg(4)) ||
            !std::isfinite(height) || !std::isfinite(radius) || height < 0 || radius <= 0 ||
            glm::length(up) < 1e-6f || glm::length(glm::cross(up, tangent)) < 1e-6f)
            return JS_ThrowTypeError(c, "invalid liquid query");
        auto sample = world.HasFluid()
                          ? world.FluidCoupling().SampleField(p, glm::normalize(up), float(height),
                                                              float(radius), tangent)
                          : FluidFieldSample{};
        if (!world.WaterSwimmingEnabled())
            sample = {};
        else {
            auto basic = world.SampleWater(p, glm::normalize(up), float(height), float(radius));
            if (basic.immersion > sample.fraction)
                sample = {basic.immersion, basic.density, basic.velocity, {0, 0, 0}};
        }
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "immersion", JS_NewFloat64(c, sample.fraction));
        JS_SetPropertyStr(c, o, "density", JS_NewFloat64(c, sample.density));
        JS_SetPropertyStr(c, o, "velocity", Vec(c, sample.velocity));
        JS_SetPropertyStr(c, o, "acceleration", Vec(c, sample.acceleration));
        return o;
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

    if (op == "liquidOwner") {
        auto h = world.Liquids().Handle(id);
        if (!world.Liquids().Get(h))
            return JS_NULL;
        return JS_NewString(c, (std::to_string(h.generation) + ":" + std::to_string(h.id)).c_str());
    }

    if (op == "basicWater" || op == "basicWaterSet") {
        if (!definition->basicWater)
            return op == "basicWater" ? JS_NULL : JS_FALSE;
        auto settings = *definition->basicWater;
        const std::pair<const char *, glm::vec3 *> vectors[] = {
            {"halfExtents", &settings.halfExtents},
            {"color", &settings.color},
            {"flow", &settings.flow}};
        const std::pair<const char *, float *> scalars[] = {
            {"waveAmplitude", &settings.waveAmplitude},
            {"wavelength", &settings.wavelength},
            {"waveSpeed", &settings.waveSpeed},
            {"opacity", &settings.opacity},
            {"density", &settings.density}};
        if (op == "basicWater") {
            auto result = JS_NewObject(c);
            JS_SetPropertyStr(c, result, "enabled", JS_NewBool(c, settings.enabled));
            for (auto [key, value] : vectors)
                JS_SetPropertyStr(c, result, key, Vec(c, *value));
            for (auto [key, value] : scalars)
                JS_SetPropertyStr(c, result, key, JS_NewFloat64(c, *value));
            return result;
        }
        if (s->inPresentation || world.NetworkClient())
            return JS_ThrowTypeError(c, "basic water settings require authoritative gameplay");
        auto options = arg(2);
        if (!KnownOptions(c, options,
                          {"enabled", "halfExtents", "color", "flow", "waveAmplitude", "wavelength",
                           "waveSpeed", "opacity", "density"}))
            return JS_ThrowTypeError(c, "unknown basic water setting");
        auto enabled = JS_GetPropertyStr(c, options, "enabled");
        const bool valid = JS_IsUndefined(enabled) || JS_IsBool(enabled);
        if (JS_IsBool(enabled))
            settings.enabled = JS_ToBool(c, enabled);
        JS_FreeValue(c, enabled);
        if (!valid)
            return JS_ThrowTypeError(c, "basic water enabled must be boolean");
        for (auto [key, value] : vectors) {
            auto property = JS_GetPropertyStr(c, options, key);
            const bool validVector = JS_IsUndefined(property) || ReadVec(c, property, *value);
            JS_FreeValue(c, property);
            if (!validVector)
                return JS_ThrowTypeError(c, "finite basic water vector required");
        }
        for (auto [key, value] : scalars) {
            auto property = JS_GetPropertyStr(c, options, key);
            const bool present = !JS_IsUndefined(property);
            JS_FreeValue(c, property);
            if (present && !Number(c, options, key, *value))
                return JS_ThrowTypeError(c, "finite basic water scalar required");
        }
        std::string error;
        if (!ValidateBasicWater(settings, error))
            return JS_ThrowTypeError(c, "%s", error.c_str());
        return JS_NewBool(c, world.SetBasicWater(id, settings));
    }
    return JS_ThrowTypeError(c, "unhandled Environment operation");
}
} // namespace
void RegisterEnvironmentBindings(BindingRegistry &registry) {
    registry.Add(
        "Environment",
        {"weatherQuality",       "weatherSample",     "weatherExists",   "weatherState",
         "weatherEnabled",       "weatherTransition", "weatherStrike",   "liquidValid",
         "liquidState",          "liquidEnabled",     "liquidTransfer",  "liquidImpulse",
         "liquidSurfaceEnabled", "liquidAccounting",  "liquidSample",    "liquidPresentedSample",
         "liquidErrors",         "liquidConnections", "liquidSubmerged", "fluidSample",
         "liquidOwner",          "basicWater",        "basicWaterSet"},
        HandleEnvironment);
}
} // namespace ScriptBindings
