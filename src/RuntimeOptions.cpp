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

namespace {
bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
}  // namespace

bool ParseRuntimeOptions(int argc, char** argv, RuntimeOptions& out, std::string& outError) {
    out = RuntimeOptions{};
    if (argc > 2) {
        outError = "usage: judas [project.judasproj | scene.judas]";
        return false;
    }
    if (const char* script = std::getenv("JUDAS_TEST_SCRIPT")) out.testScriptPath = script;

    // --- Milestone 30: which project, which scene ---
    std::string explicitScene;
    std::string projectFile;
    if (argc == 2) {
        const std::string argument = argv[1];
        if (EndsWith(argument, kProjectFileExtension)) {
            projectFile = argument;
        } else if (EndsWith(argument, ".judas")) {
            explicitScene = argument;
            projectFile = Project::FindProjectFileFor(argument);
        } else {
            outError = "judas expects a .judasproj project file or a .judas scene file, got '" + argument + "'";
            return false;
        }
    } else {
        projectFile = Project::FindProjectFileFor(".");
    }
    if (!projectFile.empty() && !out.project.Load(projectFile, outError)) return false;

    // The scene: explicit, else a legacy environment selection from the
    // project's scenes directory, else the project's startup scene.
    const auto sceneInProject = [&](const std::string& file) {
        return out.project.IsLoaded() ? out.project.ScenesDir() + "/" + file : "assets/scenes/" + file;
    };
    // The legacy environment switches select demonstration scenes only
    // when no argument was given (the pre-M30 command line); an explicit
    // project launch always starts the project's startup scene.
    const bool legacySelection = argc == 1;
    if (!explicitScene.empty()) {
        out.scenePath = explicitScene;
    } else if (legacySelection && out.IsTestRun()) {
        out.scenePath = sceneInProject(EnvSet("JUDAS_TERRAIN_PREVIEW") ? "terrain.judas" : "classic.judas");
        if (EnvSet("JUDAS_TERRAIN_PREVIEW") && EnvSet("JUDAS_TERRAIN_ROTATED")) {
            out.scenePath = sceneInProject("terrain_rotated.judas");
        }
    } else if (legacySelection && EnvSet("JUDAS_CLASSIC_DEMO")) {
        const char* fluidGravity = std::getenv("JUDAS_FLUID_GRAVITY");
        const std::string mode = fluidGravity ? fluidGravity : "normal";
        if (mode == "normal") out.scenePath = sceneInProject("classic.judas");
        else if (mode == "rotated") out.scenePath = sceneInProject("classic_fluid_rotated.judas");
        else if (mode == "zero") out.scenePath = sceneInProject("classic_fluid_zero.judas");
        else {
            outError = "JUDAS_FLUID_GRAVITY must be normal, rotated, or zero.";
            return false;
        }
    } else if (legacySelection && (EnvSet("JUDAS_ATMOSPHERIC_PASS") || EnvSet("JUDAS_TERRAIN_ROTATED"))) {
        const bool pass = EnvSet("JUDAS_ATMOSPHERIC_PASS");
        const bool rotated = EnvSet("JUDAS_TERRAIN_ROTATED");
        out.scenePath = sceneInProject(std::string("terrain") + (pass ? "_atmospheric_pass" : "") +
                                       (rotated ? "_rotated" : "") + ".judas");
    } else if (out.project.IsLoaded()) {
        out.scenePath = out.project.StartupScenePath();
        if (out.scenePath.empty()) {
            outError = "project '" + out.project.Settings().name + "' has no startup scene (set it in Project Settings)";
            return false;
        }
    } else {
        outError = "no project: run `judas <project.judasproj>` or `judas <scene.judas>`, or start inside a project directory";
        return false;
    }

    if (EnvSet("JUDAS_WORLD_OFFSET")) {
        glm::dvec3 offset;
        if (!ParseWorldOffset(std::getenv("JUDAS_WORLD_OFFSET"), offset, outError)) return false;
        out.worldOriginOverride = offset;
    }
    if (const char* path = std::getenv("JUDAS_TERRAIN_SCREENSHOT")) out.terrainScreenshotPath = path;
    out.worldStatePath = out.project.IsLoaded() ? out.project.WorldStatePathForScene(out.scenePath)
                                                : DefaultWorldStatePath(out.scenePath);
    if (const char* state = std::getenv("JUDAS_WORLD_STATE")) {
        out.worldStatePath = std::string(state) == "none" ? std::string() : state;
    }
    out.liveTelemetry = EnvSet("JUDAS_LIVE_TELEMETRY");
    out.fluidDiagnostics = EnvSet("JUDAS_FLUID_DIAGNOSTICS");
    out.atmosphereDiagnostics = EnvSet("JUDAS_ATMOSPHERE_DIAGNOSTICS");
    out.fireDiagnostics = EnvSet("JUDAS_FIRE_DIAGNOSTICS");
    return true;
}
