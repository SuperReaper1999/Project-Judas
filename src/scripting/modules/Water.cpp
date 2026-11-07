#include "../../RuntimeWorld.h"
#include "../../fluid/GameWaterRuntime.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"
#include "../BindingValues.h"
#include <charconv>
#include <cmath>
#include <limits>

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleWater(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
    auto *js = context.js;
    auto &world = *context.world;
    auto arg = [&](int index) { return index < argc ? argv[index] : JS_UNDEFINED; };
    auto *system = world.GridWater();
    if (op == "gridWaterOwners") {
        auto result = JS_NewArray(js);
        if (JS_IsException(result))
            return result;
        if (system) {
            unsigned i = 0;
            for (auto owner : system->Owners()) {
                if (JS_SetPropertyUint32(js, result, i++,
                                         JS_NewString(js, std::to_string(owner).c_str())) < 0) {
                    JS_FreeValue(js, result);
                    return JS_EXCEPTION;
                }
            }
        }
        return result;
    }
    const auto text = String(js, arg(1));
    std::uint64_t id = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        !world.RuntimeDefinition(id)) {
        if (op == "gridWaterExists")
            return JS_FALSE;
        return JS_ThrowReferenceError(js, "stale or invalid grid-water entity");
    }
    const auto *grid = system ? system->Find(id) : nullptr;
    if (op == "gridWaterExists")
        return JS_NewBool(js, grid != nullptr);
    if (op == "gridWaterState") {
        if (!grid)
            return JS_NULL;
        auto result = JS_NewObject(js);
        if (JS_IsException(result))
            return result;
        bool okay = true;
        auto set = [&](const char *name, JSValue value) {
            if (!okay) {
                JS_FreeValue(js, value);
                return;
            }
            if (JS_IsException(value)) {
                okay = false;
                return;
            }
            okay = JS_SetPropertyStr(js, result, name, value) >= 0;
        };
        const auto &stats = grid->Stats();
        const auto &settings = grid->Configuration();
        const auto runtime = system->Stats(id);
        double stored = 0, air = grid->ExteriorAirMass();
        for (const auto &cell : grid->Cells()) {
            stored += cell.displaced;
            air += cell.airMass;
        }
        for (const auto &value : {std::pair<const char *, double>{"quantity", grid->Quantity()},
                                  {"initialQuantity", grid->InitialQuantity()},
                                  {"addedQuantity", grid->AddedQuantity()},
                                  {"removedQuantity", grid->RemovedQuantity()},
                                  {"storedDisplacement", stored},
                                  {"airMass", air},
                                  {"density", settings.density},
                                  {"cellSize", settings.cellSize},
                                  {"simulationMs", runtime.totalMs},
                                  {"solverMs", stats.milliseconds},
                                  {"preparationMs", runtime.preparationMs},
                                  {"couplingMs", runtime.couplingMs},
                                  {"geometryBakes", double(runtime.geometryBakes)},
                                  {"pressureMs", stats.pressureMs},
                                  {"pressureResidual", stats.pressureResidual},
                                  {"pressureIterations", double(stats.iterations)},
                                  {"bytes", double(runtime.bytes)}})
            set(value.first, JS_NewFloat64(js, value.second));
        set("cells", Vec(js, glm::vec3(settings.cells)));
        set("paused", JS_NewBool(js, system->Paused(id)));
        set("error", JS_NewString(js, runtime.error.c_str()));
        if (!okay) {
            JS_FreeValue(js, result);
            return JS_EXCEPTION;
        }
        return result;
    }
    if (!grid)
        return JS_ThrowReferenceError(js, "entity has no active Grid Water V2; enable Advanced "
                                          "Fluid Dynamics in an Experimental build");
    if (!context.fixed || context.inPresentation || context.stopping || world.NetworkClient())
        return JS_ThrowTypeError(
            js, "grid-water changes require fixedUpdate in an authoritative world");
    glm::vec3 point;
    double radius = 0;
    if (!ReadVec(js, arg(2), point) || !std::isfinite(glm::dot(point, point)))
        return JS_ThrowTypeError(js, "grid water requires a finite local-world point");
    if (op == "gridWaterImpulse") {
        glm::vec3 impulse;
        if (!ReadVec(js, arg(3), impulse) || !std::isfinite(glm::dot(impulse, impulse)) ||
            JS_ToFloat64(js, &radius, arg(4)) || !std::isfinite(radius) || radius < 0 ||
            radius > std::numeric_limits<float>::max())
            return JS_ThrowTypeError(js, "grid-water impulse requires finite momentum and "
                                         "nonnegative engine-representable radius");
        return JS_NewBool(js, system->AddImpulse(id, point, impulse, float(radius)));
    }
    if (op == "gridWaterChange") {
        double volume = 0;
        if (JS_ToFloat64(js, &volume, arg(3)) || !std::isfinite(volume) ||
            JS_ToFloat64(js, &radius, arg(4)) || !std::isfinite(radius) || radius <= 0 ||
            radius > std::numeric_limits<float>::max())
            return JS_ThrowTypeError(js, "grid-water source requires finite signed cubic metres "
                                         "and a positive engine-representable radius");
        return JS_NewFloat64(js, system->ChangeWater(id, point, float(radius), volume));
    }
    return JS_ThrowTypeError(js, "unhandled Water operation");
}
} // namespace
void RegisterWaterBindings(BindingRegistry &registry) {
    registry.Add("Water",
                 {"gridWaterOwners", "gridWaterExists", "gridWaterState", "gridWaterImpulse",
                  "gridWaterChange"},
                 HandleWater);
}
} // namespace ScriptBindings
