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

    if (op == "fluidSample") {
        glm::vec3 p, up, tangent;
        double height, radius;
        if (!ReadVec(c, arg(1), p) || !ReadVec(c, arg(2), up) || !ReadVec(c, arg(5), tangent) ||
            JS_ToFloat64(c, &height, arg(3)) || JS_ToFloat64(c, &radius, arg(4)) ||
            !std::isfinite(height) || !std::isfinite(radius) || height < 0 || radius <= 0 ||
            glm::length(up) < 1e-6f || glm::length(glm::cross(up, tangent)) < 1e-6f)
            return JS_ThrowTypeError(c, "invalid liquid query");
        auto sample = world.SampleWater(p,glm::normalize(up),float(height),float(radius));
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "immersion", JS_NewFloat64(c, sample.immersion));
        JS_SetPropertyStr(c, o, "density", JS_NewFloat64(c, sample.density));
        JS_SetPropertyStr(c, o, "velocity", Vec(c, sample.velocity));
        JS_SetPropertyStr(c, o, "acceleration", Vec(c, glm::vec3(0)));
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
         "weatherEnabled",       "weatherTransition", "weatherStrike", "fluidSample",          "basicWater",        "basicWaterSet"},
        HandleEnvironment);
}
} // namespace ScriptBindings
