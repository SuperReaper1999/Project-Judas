#pragma once
#include <string>

struct BuildUpdateResult {
    bool available = false;
    bool newer = false;
    unsigned commitsAhead = 0;
    std::string diagnostic;
};

// A different SHA alone is not proof that the remote is newer. Parse GitHub's
// ancestry result so ahead/identical/behind/diverged stay distinct.
BuildUpdateResult ParseBuildUpdateComparison(const std::string& response);

// Published release metadata is distinct from branch ancestry. Unknown or
// malformed manifests never become update notifications.
struct ReleaseUpdateResult {
    bool valid = false, newer = false, compatible = false;
    std::string version, channel, notes, notesUrl, downloadUrl, artifactName, sha256;
    std::string diagnostic;
};
bool ValidSemanticVersion(const std::string& version);
// Call only for versions accepted by ValidSemanticVersion.
int CompareSemanticVersions(const std::string& left, const std::string& right);
std::string ReleaseManifestEndpoint(const std::string& channel);
ReleaseUpdateResult ParseReleaseUpdate(const std::string& response,
    const std::string& installed, const std::string& channel,
    const std::string& platform, const std::string& architecture);
