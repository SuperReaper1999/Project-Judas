#pragma once

#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "Project.h"

// Milestone 28: everything the `judas` runtime reads from its command line
// and environment, resolved in one place before any engine system exists.
//
// Milestone 30: the runtime launches a PROJECT.
//
//   judas <game.judasproj>   opens the project and plays its startup scene
//   judas <scene.judas>      plays that scene inside the nearest enclosing
//                            project (the first ancestor directory holding
//                            exactly one .judasproj); a scene outside any
//                            project has no asset database, so mesh assets
//                            cannot resolve
//   judas                    the project enclosing the working directory;
//                            its startup scene unless one of the legacy
//                            environment switches below picks another
//                            scene from the project's scenes directory
//                            (the switches apply ONLY to this no-argument
//                            form; an explicit project launch always
//                            starts the startup scene)
//
// The pre-M28 environment switches keep selecting the same demonstrations
// they always did, as scene files under the project's scenes-dir (for the
// technology-demonstration project, assets/scenes):
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
// JUDAS_WORLD_STATE (path | none) selects the world-state delta file.
struct RuntimeOptions {
    // Milestone 30: the launched project (IsLoaded() false when the scene
    // sits outside any project) and the resolved scene file path.
    Project project;
    std::string scenePath;
    std::optional<glm::dvec3> worldOriginOverride;
    std::string testScriptPath;  // empty: interactive
    bool IsTestRun() const { return !testScriptPath.empty(); }
    std::string terrainScreenshotPath;
    // Milestone 29: where the runtime reads/writes the world-state delta
    // for the selected scene. Default <project saves-dir>/<scene stem>.judasstate
    // (saves/<stem>.judasstate outside a project); JUDAS_WORLD_STATE=<path>
    // overrides, JUDAS_WORLD_STATE=none disables.
    std::string worldStatePath;
    bool liveTelemetry = false;
    bool fluidDiagnostics = false;
    bool atmosphereDiagnostics = false;
    bool fireDiagnostics = false;
};

// Returns false (with a message) on a malformed value.
bool ParseRuntimeOptions(int argc, char** argv, RuntimeOptions& outOptions, std::string& outError);
bool ParseWorldOffset(const char* value, glm::dvec3& outOffset, std::string& outError);
