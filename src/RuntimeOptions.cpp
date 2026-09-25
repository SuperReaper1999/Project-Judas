#include "RuntimeOptions.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "WorldState.h"

namespace {
bool EnvSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0';
}
}  // namespace

bool ParseWorldOffset(const char* value, glm::dvec3& outOffset, std::string& outError) {
    if (value == nullptr || *value == '\0') {
        outOffset = glm::dvec3(0.0);
        return true;
    }
    if (std::string(value) == "far") {
        outOffset = glm::dvec3(1.0e9, -2.0e9, 3.0e9);
        return true;
    }
    double x = 0.0, y = 0.0, z = 0.0;
    char trailing = '\0';
    if (std::sscanf(value, "%lf,%lf,%lf%c", &x, &y, &z, &trailing) != 3 || !std::isfinite(x) ||
        !std::isfinite(y) || !std::isfinite(z)) {
        outError = "JUDAS_WORLD_OFFSET must be far or x,y,z in metres.";
        return false;
    }
    outOffset = glm::dvec3(x, y, z);
    return true;
}

bool ParseRuntimeOptions(int argc, char** argv, RuntimeOptions& out, std::string& outError) {
    out = RuntimeOptions{};
    if (argc > 2) {
        outError = "usage: judas [scene.judas]";
        return false;
    }
    if (const char* script = std::getenv("JUDAS_TEST_SCRIPT")) out.testScriptPath = script;

    if (argc == 2) {
        out.scenePath = argv[1];
    } else if (out.IsTestRun()) {
        out.scenePath = EnvSet("JUDAS_TERRAIN_PREVIEW") ? "assets/scenes/terrain.judas"
                                                        : "assets/scenes/classic.judas";
        if (EnvSet("JUDAS_TERRAIN_PREVIEW") && EnvSet("JUDAS_TERRAIN_ROTATED")) {
            out.scenePath = "assets/scenes/terrain_rotated.judas";
        }
    } else if (EnvSet("JUDAS_CLASSIC_DEMO")) {
        const char* fluidGravity = std::getenv("JUDAS_FLUID_GRAVITY");
        const std::string mode = fluidGravity ? fluidGravity : "normal";
        if (mode == "normal") out.scenePath = "assets/scenes/classic.judas";
        else if (mode == "rotated") out.scenePath = "assets/scenes/classic_fluid_rotated.judas";
        else if (mode == "zero") out.scenePath = "assets/scenes/classic_fluid_zero.judas";
        else {
            outError = "JUDAS_FLUID_GRAVITY must be normal, rotated, or zero.";
            return false;
        }
    } else {
        const bool pass = EnvSet("JUDAS_ATMOSPHERIC_PASS");
        const bool rotated = EnvSet("JUDAS_TERRAIN_ROTATED");
        out.scenePath = std::string("assets/scenes/terrain") + (pass ? "_atmospheric_pass" : "") +
                        (rotated ? "_rotated" : "") + ".judas";
    }

    if (EnvSet("JUDAS_WORLD_OFFSET")) {
        glm::dvec3 offset;
        if (!ParseWorldOffset(std::getenv("JUDAS_WORLD_OFFSET"), offset, outError)) return false;
        out.worldOriginOverride = offset;
    }
    if (const char* path = std::getenv("JUDAS_TERRAIN_SCREENSHOT")) out.terrainScreenshotPath = path;
    out.worldStatePath = DefaultWorldStatePath(out.scenePath);
    if (const char* state = std::getenv("JUDAS_WORLD_STATE")) {
        out.worldStatePath = std::string(state) == "none" ? std::string() : state;
    }
    out.liveTelemetry = EnvSet("JUDAS_LIVE_TELEMETRY");
    out.fluidDiagnostics = EnvSet("JUDAS_FLUID_DIAGNOSTICS");
    out.atmosphereDiagnostics = EnvSet("JUDAS_ATMOSPHERE_DIAGNOSTICS");
    out.fireDiagnostics = EnvSet("JUDAS_FIRE_DIAGNOSTICS");
    return true;
}
