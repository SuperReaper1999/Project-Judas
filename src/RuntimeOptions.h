#pragma once

#include <optional>
#include <string>

#include <glm/glm.hpp>

// Milestone 28: everything the `judas` runtime reads from its command line
// and environment, resolved in one place before any engine system exists.
//
// Scene selection: an explicit `judas <path.judas>` argument wins.
// Otherwise the pre-M28 environment switches keep selecting the same
// demonstrations they always did, now as scene files under assets/scenes:
//
//   (default)                      terrain.judas
//   JUDAS_CLASSIC_DEMO=1           classic.judas
//   JUDAS_FLUID_GRAVITY=rotated    classic_fluid_rotated.judas   (classic only)
//   JUDAS_FLUID_GRAVITY=zero       classic_fluid_zero.judas      (classic only)
//   JUDAS_ATMOSPHERIC_PASS=1       terrain_atmospheric_pass.judas
//   JUDAS_TERRAIN_ROTATED=1        the *_rotated variant of a terrain scene
//   JUDAS_TEST_SCRIPT=...          classic.judas, or terrain.judas with
//                                  JUDAS_TERRAIN_PREVIEW=1 (harness runs)
//
// JUDAS_WORLD_OFFSET (far | x,y,z) overrides the scene's authored origin.
struct RuntimeOptions {
    std::string scenePath;
    std::optional<glm::dvec3> worldOriginOverride;
    std::string testScriptPath;  // empty: interactive
    bool IsTestRun() const { return !testScriptPath.empty(); }
    std::string terrainScreenshotPath;
    bool liveTelemetry = false;
    bool fluidDiagnostics = false;
    bool atmosphereDiagnostics = false;
    bool fireDiagnostics = false;
};

// Returns false (with a message) on a malformed value.
bool ParseRuntimeOptions(int argc, char** argv, RuntimeOptions& outOptions, std::string& outError);
bool ParseWorldOffset(const char* value, glm::dvec3& outOffset, std::string& outError);
