#pragma once

#include <string>

// Engine provenance is deliberately separate from content/save/network schema
// compatibility. A new build is not automatically an incompatible game update.
struct BuildIdentity {
    const char* version;
    const char* maturity;
    const char* channel;
    const char* buildId;
    const char* sourceRevision;
    const char* sourceTree;
    bool sourceDirty;
    const char* sourceFingerprint;
    unsigned sourceUntrackedCount;
    const char* sourceUntrackedFingerprint;
    const char* sourceOrigin;
    const char* platform;
    const char* architecture;
    const char* configuration;
    const char* compiler;
    const char* compilerVersion;
    const char* releaseName;
};

const BuildIdentity& GetBuildIdentity();
std::string BuildIdentityJson();
std::string BuildIdentitySummary(const char* product);

// Run before application/window/resource initialization. --build-info remains
// available to older runtime exporters; JSON is the authoritative new surface.
bool HandleBuildIdentityArguments(int argc, char** argv, const char* product);

// Releases require known, clean provenance and no observed untracked build inputs.
// Development builds may retain dirty provenance and matching untracked inputs.
bool BuildIdentityReleaseEligible(const BuildIdentity& identity, std::string& reason);

// Export checks the selected executable's response, never a neighboring file.
// Dirty development checkpoints are allowed when exporter/runtime match exactly.
bool ValidateRuntimeBuildIdentity(const std::string& response, std::string& reason);
