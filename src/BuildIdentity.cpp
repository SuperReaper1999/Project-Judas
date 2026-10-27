#include "BuildIdentity.h"
#include "JudasBuildIdentity.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

const BuildIdentity& GetBuildIdentity() {
    static const BuildIdentity identity{
        JUDAS_ID_VERSION, JUDAS_ID_MATURITY, JUDAS_ID_CHANNEL, JUDAS_ID_BUILD_ID,
        JUDAS_ID_REVISION, JUDAS_ID_TREE, JUDAS_ID_SOURCE_DIRTY,
        JUDAS_ID_SOURCE_FINGERPRINT, JUDAS_ID_SOURCE_UNTRACKED_COUNT,
        JUDAS_ID_SOURCE_UNTRACKED_FINGERPRINT, JUDAS_ID_SOURCE_ORIGIN,
        JUDAS_ID_PLATFORM, JUDAS_ID_ARCHITECTURE, JUDAS_BUILD_CONFIGURATION,
        JUDAS_ID_COMPILER, JUDAS_ID_COMPILER_VERSION, JUDAS_ID_RELEASE_NAME};
    return identity;
}

std::string BuildIdentityJson() {
    const auto& i = GetBuildIdentity();
    return nlohmann::json{{"schema", 1}, {"version", i.version}, {"maturity", i.maturity},
        {"release_name", i.releaseName}, {"channel", i.channel}, {"build_id", i.buildId}, {"source_revision", i.sourceRevision},
        {"source_tree", i.sourceTree}, {"source_dirty", i.sourceDirty},
        {"source_fingerprint", i.sourceFingerprint}, {"source_origin", i.sourceOrigin},
        {"source_untracked_count", i.sourceUntrackedCount},
        {"source_untracked_fingerprint", i.sourceUntrackedFingerprint},
        {"platform", i.platform}, {"architecture", i.architecture},
        {"configuration", i.configuration}, {"compiler", i.compiler},
        {"compiler_version", i.compilerVersion}}.dump(2);
}

std::string BuildIdentitySummary(const char* product) {
    const auto& i = GetBuildIdentity();
    return std::string(i.releaseName) + " / " + product + " (" + i.version + ")" + " [" + i.channel + "] " +
        i.sourceRevision + (i.sourceDirty ? " dirty" : " clean") + " " +
        i.platform + "/" + i.architecture + " " + i.configuration;
}

bool HandleBuildIdentityArguments(int argc, char** argv, const char* product) {
    if (argc != 2) return false;
    if (std::strcmp(argv[1], "--version") == 0) {
        std::puts(BuildIdentitySummary(product).c_str());
        return true;
    }
    if (std::strcmp(argv[1], "--build-info-json") == 0) {
        std::puts(BuildIdentityJson().c_str());
        return true;
    }
    if (std::strcmp(argv[1], "--build-info") == 0) {
        const auto& i = GetBuildIdentity();
        if (std::strcmp(product, "runtime") == 0)
            std::printf("Judas runtime %s %s\n", i.platform, i.configuration);
        else std::puts(BuildIdentitySummary(product).c_str());
        return true;
    }
    return false;
}

bool BuildIdentityReleaseEligible(const BuildIdentity& i, std::string& reason) {
    const auto hash = [](const char* value, std::size_t length) {
        return value && std::strlen(value) == length && std::all_of(value, value + length,
            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
    };
    if ((std::strcmp(i.sourceOrigin, "git") != 0 && std::strcmp(i.sourceOrigin, "archive") != 0) ||
        !hash(i.sourceRevision, 40) || !hash(i.sourceTree, 40) || !hash(i.sourceFingerprint, 64)) {
        reason = "Release packaging requires Git provenance or a verified JUDAS_SOURCE_INFO.json source archive";
        return false;
    }
    if (i.sourceDirty) {
        reason = "Release packaging requires a clean source checkpoint; development packages can explicitly retain dirty provenance";
        return false;
    }
    if (i.sourceUntrackedCount != 0) {
        reason = "Release packaging requires no untracked build inputs; checkpoint intended inputs or build a verified source archive that explicitly excludes unrelated files";
        return false;
    }
    if (std::strcmp(i.channel, "Local") == 0) {
        reason = "Release packaging requires an explicit Alpha/Beta/Experimental/Stable channel";
        return false;
    }
    if ((std::strcmp(i.channel, "Alpha") != 0 && std::strcmp(i.channel, "Beta") != 0 && std::strcmp(i.channel, "Stable") != 0 && std::strcmp(i.channel, "Experimental") != 0) ||
        std::strcmp(i.configuration, "Release") != 0) {
        reason = "Release packaging requires a supported channel and Release configuration";
        return false;
    }
    if ((std::strcmp(i.channel, "Stable") == 0 && std::strcmp(i.maturity, "stable") != 0) ||
        (std::strcmp(i.channel, "Alpha") == 0 && std::strcmp(i.maturity, "alpha") != 0) ||
        (std::strcmp(i.channel, "Beta") == 0 && std::strcmp(i.maturity, "beta") != 0)) {
        reason = "Build maturity does not match the selected release channel";
        return false;
    }
    reason.clear();
    return true;
}

bool ValidateRuntimeBuildIdentity(const std::string& response, std::string& reason) {
    const auto runtime = nlohmann::json::parse(response, nullptr, false);
    const auto ours = nlohmann::json::parse(BuildIdentityJson());
    if (!runtime.is_object() || !runtime.contains("schema") || !runtime["schema"].is_number_integer()
        || runtime["schema"] != 1) {
        reason = "Runtime does not expose supported build provenance; rebuild runtime and exporter together";
        return false;
    }
    for (const char* field : {"version", "channel", "source_revision", "source_tree", "source_fingerprint", "platform", "architecture"}) {
        if (!runtime.contains(field) || !runtime[field].is_string() || runtime[field] != ours[field]) {
            reason = std::string("Runtime/exporter build mismatch in ") + field + "; rebuild or select the matching runtime";
            return false;
        }
    }
    if (!runtime.contains("configuration") || runtime["configuration"] != "Release" ||
        !runtime.contains("source_origin") ||
        (runtime["source_origin"] != "git" && runtime["source_origin"] != "archive")) {
        reason = "Runtime requires a Release configuration with known Git/archive provenance";
        return false;
    }
    if (!runtime.contains("source_dirty") || !runtime["source_dirty"].is_boolean() ||
        !runtime.contains("channel") || !runtime["channel"].is_string() ||
        !runtime.contains("build_id") || !runtime["build_id"].is_string()) {
        reason = "Runtime provenance is incomplete";
        return false;
    }
    // An untracked source can be compiled by a glob even with clean tracked
    // provenance. Verified archives have zero observations; compare those too
    // rather than assuming unrelated extras cannot affect a Git-built tool.
    if (!runtime.contains("source_untracked_count") ||
        !runtime["source_untracked_count"].is_number_unsigned() ||
        runtime["source_untracked_count"] != ours["source_untracked_count"] ||
        !runtime.contains("source_untracked_fingerprint") ||
        runtime["source_untracked_fingerprint"] != ours["source_untracked_fingerprint"]) {
        reason = "Runtime/exporter untracked build inputs differ; checkpoint inputs or rebuild both tools";
        return false;
    }
    reason.clear();
    return true;
}
