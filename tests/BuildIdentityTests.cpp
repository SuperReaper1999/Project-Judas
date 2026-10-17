#include "BuildIdentity.h"
#include "BuildUpdate.h"
#include "ToolProcess.h"
#include "ProcessArguments.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--probe-arguments") {
        nlohmann::json arguments = nlohmann::json::array();
        for (int index = 2; index < argc; ++index) arguments.push_back(argv[index]);
        std::puts(arguments.dump().c_str());
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--probe-timeout") {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--probe-oversize") {
        std::puts(std::string(65536, 'x').c_str());
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--probe-failure") return 23;
    if (HandleBuildIdentityArguments(argc, argv, "identity-test")) return 0;
    unsigned checks = 0, failures = 0;
    const auto check = [&](bool condition, const char* label) {
        ++checks;
        if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
    };
    const auto identity = GetBuildIdentity();
    const auto json = nlohmann::json::parse(BuildIdentityJson());
    check(json["schema"] == 1, "versioned identity schema");
    check(json["source_dirty"].is_boolean(), "dirty provenance is a boolean");
    check(json["version"] == identity.version && json["channel"] == identity.channel,
          "native and generated version/channel contract");
    check(std::string(identity.version).find("prealpha") != std::string::npos,
          "not an Alpha release");
    check(!BuildIdentitySummary("runtime").empty(), "human-readable identity");
    auto candidate = identity;
    std::string reason;
    candidate.sourceOrigin = "unknown";
    check(!BuildIdentityReleaseEligible(candidate, reason) && !reason.empty(), "unknown source rejected for release");
    candidate = identity; candidate.sourceDirty = true;
    check(!BuildIdentityReleaseEligible(candidate, reason), "dirty source rejected for release");
    candidate = identity; candidate.sourceOrigin = "git"; candidate.sourceRevision = "0123456789012345678901234567890123456789";
    candidate.sourceFingerprint = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    candidate.configuration = "Release";
    candidate.sourceUntrackedCount = 0;
    candidate.sourceDirty = false; candidate.channel = "Local";
    check(!BuildIdentityReleaseEligible(candidate, reason), "Local channel rejected for release");
    candidate.channel = "Experimental";
    check(BuildIdentityReleaseEligible(candidate, reason) && reason.empty(), "clean known Experimental release accepted");
    candidate.channel = "main";
    check(BuildIdentityReleaseEligible(candidate, reason), "clean known main release accepted without inferring human status");
    candidate.sourceUntrackedCount = 1;
    check(!BuildIdentityReleaseEligible(candidate, reason), "clean tracked source with untracked build input rejected for release");
    candidate.sourceUntrackedCount = 0; candidate.sourceOrigin = "archive";
    check(BuildIdentityReleaseEligible(candidate, reason), "verified clean source archive accepted for release");
    candidate.sourceOrigin = "git";
    candidate.sourceRevision = "invented";
    check(!BuildIdentityReleaseEligible(candidate, reason), "malformed source hash rejected for release");
    candidate.sourceRevision = identity.sourceRevision; candidate.configuration = "Debug";
    check(!BuildIdentityReleaseEligible(candidate, reason), "Debug build rejected for release");
    const auto update = ParseBuildUpdateComparison(R"({"status":"ahead","ahead_by":3})");
    check(update.available && update.newer && update.commitsAhead == 3, "unnumbered forward commits notify");
    check(!ParseBuildUpdateComparison(R"({"status":"identical","ahead_by":0})").newer, "same build does not notify");
    check(!ParseBuildUpdateComparison(R"({"status":"behind","ahead_by":0})").newer, "local newer build does not notify");
    check(!ParseBuildUpdateComparison(R"({"status":"diverged","ahead_by":3})").newer, "divergence is not called newer");
    check(!ParseBuildUpdateComparison(R"({"status":"ahead","ahead_by":-1})").available, "negative update count rejected");
    check(!ParseBuildUpdateComparison(R"({"status":"ahead","ahead_by":1000001})").available, "unbounded update count rejected");
    check(!ParseBuildUpdateComparison("not json").available, "invalid update response rejected");
    check(!ParseBuildUpdateComparison(R"({"status":"invented","ahead_by":1})").available, "unknown update state rejected");
    check(!ParseBuildUpdateComparison(R"({"status":"ahead","ahead_by":0})").available, "inconsistent ahead result rejected");
    check(!ParseBuildUpdateComparison(R"({"status":"identical","ahead_by":5})").available, "inconsistent identical result rejected");
    auto runtime = json;
    runtime["configuration"] = "Release";
    check(ValidateRuntimeBuildIdentity(runtime.dump(), reason), "matching runtime export identity accepted");
    runtime["source_fingerprint"] = "not the selected candidate";
    check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "stale runtime fingerprint rejected");
    runtime = json; runtime["channel"] = json["channel"] == "main" ? "Experimental" : "main";
    check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "different runtime release channel rejected");
    runtime = json; runtime["configuration"] = "Debug";
    check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "Debug runtime rejected by exporter");
    runtime = json; runtime["schema"] = "1";
    check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "wrongly typed runtime schema rejected");
    runtime = json; runtime.erase("source_tree");
    check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "missing runtime provenance rejected");
    runtime = json; runtime["source_origin"] = "unknown";
    check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "unknown runtime source rejected");
    if (json["source_origin"] == "git") {
        runtime = json; runtime["source_dirty"] = true;
        runtime["source_untracked_fingerprint"] = "different new build input bytes";
        check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "different untracked development source rejected");
        runtime["source_dirty"] = false;
        check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "clean tracked provenance cannot hide different untracked source");
        runtime = json; runtime["source_untracked_count"] = identity.sourceUntrackedCount + 1;
        check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "different untracked input count rejected");
        runtime = json; runtime.erase("source_untracked_fingerprint");
        check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "missing Git untracked observations rejected");
        runtime = json; runtime["source_origin"] = "archive";
        runtime["source_untracked_count"] = identity.sourceUntrackedCount + 1;
        check(!ValidateRuntimeBuildIdentity(runtime.dump(), reason), "different Git/archive observations cannot certify matching tools");
    }
    const auto executable = std::filesystem::absolute(argv[0]);
    check(QuoteWindowsProcessArgument(L"") == L"\"\"", "Windows empty argument is retained");
    check(QuoteWindowsProcessArgument(L"plain") == L"\"plain\"", "Windows plain argument is quoted");
    check(QuoteWindowsProcessArgument(L"two words") == L"\"two words\"", "Windows spaces stay inside one argument");
    check(QuoteWindowsProcessArgument(LR"(a"b)") == LR"("a\"b")", "Windows embedded quote is escaped");
    check(QuoteWindowsProcessArgument(LR"(path\)") == LR"("path\\")", "Windows trailing backslash is doubled");
    check(QuoteWindowsProcessArgument(LR"(a\"b)") == LR"("a\\\"b")", "Windows backslash before quote is doubled before escaping quote");
    check(QuoteWindowsProcessArgument(L"日本語 résumé") == L"\"日本語 résumé\"", "Windows quote helper preserves Unicode");
    check(QuoteWindowsProcessArgument(L"; & | $()") == L"\"; & | $()\"", "Windows shell metacharacters remain literal arguments");
    const auto httpsTool = SystemHttpsProbeExecutable();
#ifdef _WIN32
    check(httpsTool.empty() || (httpsTool.is_absolute() && httpsTool.filename() == L"curl.exe"),
          "Windows HTTPS utility never uses project-directory executable lookup");
#else
    check(httpsTool == "curl", "desktop HTTPS utility uses the normal optional PATH command");
#endif
    std::string output, error;
    const std::vector<std::string> arguments{"", "two words", u8"Judas 日本語 résumé",
        R"(quote"inside)", R"(C:\trailing\)", R"(slash\"quote)", "; & | $()"};
    std::vector<std::string> childArguments{"--probe-arguments"};
    childArguments.insert(childArguments.end(), arguments.begin(), arguments.end());
    const bool argumentsCaptured = CaptureToolOutput(executable, childArguments, output, error);
    check(argumentsCaptured, "actual child argument probe succeeds");
    check(argumentsCaptured && nlohmann::json::parse(output, nullptr, false) == nlohmann::json(arguments),
          "actual child preserves empty, spaced, Unicode, quoted and backslash arguments exactly");
    check(CaptureToolOutput(executable, {"--build-info-json"}, output, error), "bounded tool probe succeeds");
    check(nlohmann::json::parse(output)["build_id"] == json["build_id"], "probe reports actual executable identity");
    bool completeAtExit = true;
    for (unsigned trial = 0; trial < 16; ++trial) {
        const bool captured = CaptureToolOutput(executable, {"--build-info-json"}, output, error);
        completeAtExit &= captured && nlohmann::json::parse(output, nullptr, false) == json;
    }
    check(completeAtExit, "quick child exit retains complete final JSON across repeated probes");
    check(!CaptureToolOutput(executable, {"--probe-failure"}, output, error), "failed executable rejected");
    check(!CaptureToolOutput(executable, {"--probe-oversize"}, output, error,
          std::chrono::milliseconds(1000), 512), "oversize executable output rejected");
    const auto start = std::chrono::steady_clock::now();
    check(!CaptureToolOutput(executable, {"--probe-timeout"}, output, error,
          std::chrono::milliseconds(100)), "stalled executable rejected");
    check(std::chrono::steady_clock::now() - start < std::chrono::seconds(2), "stalled executable terminates within bound");
    check(!CaptureToolOutput(executable.parent_path() / "does-not-exist", {}, output, error), "missing executable rejected");
    std::printf("Build identity: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
