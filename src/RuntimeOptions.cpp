#include <stdexcept>
#include "RuntimeOptions.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "WorldState.h"
#include "GamePackage.h"
#include "EnginePaths.h"
#include <filesystem>
#include <vector>

namespace {
bool EnvSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0';
}
}  // namespace

bool ParseWindowSize(const char* value, int& width, int& height, std::string& outError) {
    if (!value) return true;
    const std::string size(value);
    const auto separator = size.find('x');
    if (separator == std::string::npos || separator == 0 || separator + 1 == size.size() ||
        size.find_first_not_of("0123456789x") != std::string::npos ||
        size.find('x', separator + 1) != std::string::npos) {
        outError = "JUDAS_WINDOW_SIZE must be WIDTHxHEIGHT (each 1..8192)";
        return false;
    }
    try {
        const auto w = std::stoul(size.substr(0, separator));
        const auto h = std::stoul(size.substr(separator + 1));
        if (w < 1 || h < 1 || w > 8192 || h > 8192) throw std::out_of_range("size");
        width = int(w); height = int(h);
        return true;
    } catch (...) {
        outError = "JUDAS_WINDOW_SIZE must be WIDTHxHEIGHT (each 1..8192)";
        return false;
    }
}

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
#ifndef __ANDROID__
    if (!ParseWindowSize(std::getenv("JUDAS_WINDOW_SIZE"), out.windowWidth, out.windowHeight, outError)) return false;
#endif
    std::vector<char*> filtered{argv[0]};
    for(int i=1;i<argc;++i){
        const std::string option=argv[i];
        if(option=="--no-mods")out.noMods=true;
        else if(option=="--host"||option=="--join"){
            if(!out.networkMode.empty()||i+1>=argc){outError="Use --host PORT or --join IPv4:PORT once";return false;}
            out.networkMode=option=="--host"?"host":"join";std::string endpoint=argv[++i],port=endpoint;
            if(option=="--join"){auto colon=endpoint.find(':');if(colon==std::string::npos){outError="--join requires IPv4:PORT";return false;}out.networkAddress=endpoint.substr(0,colon);port=endpoint.substr(colon+1);}
            else out.networkAddress="0.0.0.0";
            try{if(port.empty()||port.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("port");auto n=std::stoul(port);if(n<1||n>65535)throw std::runtime_error("port");out.networkPort=unsigned(n);}catch(...){outError="Network port must be1..65535";return false;}
        }else filtered.push_back(argv[i]);
    }
    argc=int(filtered.size());argv=filtered.data();
    if (argc > 2) {
        outError = "usage: judas [project.judasproj | scene.judas]";
        return false;
    }
    if (const char* script = std::getenv("JUDAS_TEST_SCRIPT")) out.testScriptPath = script;

    // --- Milestone 30: which project, which scene ---
    GamePackage package;
    const std::string packageRoot = EngineExecutableDir();
    const bool packaged = std::filesystem::exists(std::filesystem::path(packageRoot) / kGamePackageMarker);
    if (packaged && !ReadGamePackage(packageRoot, package, outError)) return false;
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
        projectFile = packaged ? (std::filesystem::path(packageRoot) / package.projectFile).string()
                               : Project::FindProjectFileFor(".");
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
    const bool legacySelection = argc == 1 && !packaged;
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
    if (packaged && out.project.IsLoaded()) {
        std::string saves;
        if (!PackageSaveDirectory(package, saves, outError)) return false;
        out.project.SetRuntimeSaveDirectory(saves);
    }
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
