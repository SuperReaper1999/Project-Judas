#pragma once

#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "Project.h"

// Command-line/environment launch configuration. With no explicit argument,
// launches the enclosing project's startup scene. No built-in demo catalogue.
struct RuntimeOptions {
    // Milestone 30: the launched project (IsLoaded() false when the scene
    // sits outside any project) and the resolved scene file path.
    Project project;
    std::string networkMode,networkAddress="127.0.0.1";
    unsigned networkPort=27999;
    bool noMods=false;
    int windowWidth=1024,windowHeight=768; // desktop JUDAS_WINDOW_SIZE; Android uses its native surface
    std::string scenePath;
    std::optional<glm::dvec3> worldOriginOverride;
    std::string testScriptPath;  // empty: interactive
    bool IsTestRun() const { return !testScriptPath.empty(); }
    bool liveTelemetry = false;
    bool fluidDiagnostics = false;
    bool fireDiagnostics = false;
};

// Returns false (with a message) on a malformed value.
bool ParseRuntimeOptions(int argc, char** argv, RuntimeOptions& outOptions, std::string& outError);
bool ParseWorldOffset(const char* value, glm::dvec3& outOffset, std::string& outError);

// Strict desktop test/development size, bounded to avoid accidental huge allocations.
bool ParseWindowSize(const char* value, int& width, int& height, std::string& outError);
