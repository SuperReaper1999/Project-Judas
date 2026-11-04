#pragma once
#include "EditorApplication.h"
#include "WorldState.h"
struct EditorApplication::StabilizationAutomation {
    std::string output;
    std::string baseline;
    std::string fingerprint;
    std::string assetId;
    std::string assetPath;
    std::string saveBytes;
    SceneObjectId moving = 0, removed = 0, coarse = 0, mesh = 0;
    EntityId created = 0;
    EntityPhysicalState dormantState, coarseBefore;
    std::size_t playSteps = 0, phaseStep = 0;
    unsigned int phase = 0, frames = 0, checks = 0, failures = 0;

    bool Check(bool ok, const char *name) {
        ++checks;
        failures += ok ? 0u : 1u;
        std::fprintf(stderr, "FTFT6 CHECK %s %s\n", name, ok ? "PASS" : "FAIL");
        return ok;
    }
};
