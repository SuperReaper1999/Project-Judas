#include "BuildUpdate.h"
#include "../third_party/nlohmann/json.hpp"

BuildUpdateResult ParseBuildUpdateComparison(const std::string& response) {
    BuildUpdateResult result;
    const auto document = nlohmann::json::parse(response, nullptr, false);
    if (!document.is_object() || !document.contains("status") || !document["status"].is_string()
        || !document.contains("ahead_by") || !document["ahead_by"].is_number_unsigned()) {
        result.diagnostic = "Update service returned an invalid comparison";
        return result;
    }
    const auto status = document["status"].get<std::string>();
    const auto ahead = document["ahead_by"].get<unsigned long long>();
    if (ahead > 1000000 || (status != "ahead" && status != "behind"
        && status != "identical" && status != "diverged") ||
        (status == "ahead" && ahead == 0) ||
        ((status == "behind" || status == "identical") && ahead != 0)) {
        result.diagnostic = "Update service returned an unsupported comparison";
        return result;
    }
    result.available = true;
    result.newer = status == "ahead" && ahead > 0;
    result.commitsAhead = static_cast<unsigned>(ahead);
    result.diagnostic = status == "diverged" ? "Local and channel histories diverge; compare them before updating"
        : status == "behind" ? "This build is ahead of the selected channel"
        : result.newer ? "A newer build is available on this channel"
        : "No newer build found on this channel";
    return result;
}

#include <algorithm>
#include <array>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

namespace {
const std::string releases = "https://github.com/SuperReaper1999/Project-Judas/releases/";
struct Version {
    std::array<std::string, 3> core;
    std::vector<std::string> prerelease;
};
bool Digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
int Lexical(const std::string& a, const std::string& b) { return a == b ? 0 : a < b ? -1 : 1; }
int Numeric(const std::string& a, const std::string& b) {
    return a.size() == b.size() ? Lexical(a, b) : a.size() < b.size() ? -1 : 1;
}
bool ParseVersion(const std::string& s, Version& result) {
    if (s.empty() || s.size() > 128) return false;
    static const std::regex syntax(R"((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?)");
    std::smatch match;
    if (!std::regex_match(s, match, syntax)) return false;
    for (unsigned i = 0; i < 3; ++i) result.core[i] = match[i + 1].str();
    if (match[4].matched) {
        std::istringstream fields(match[4].str());
        std::string field;
        while (std::getline(fields, field, '.')) {
            if (Digits(field) && field.size() > 1 && field[0] == '0') return false;
            result.prerelease.push_back(field);
        }
    }
    return true;
}
bool Channel(const std::string& s) { return s == "Alpha" || s == "Beta" || s == "Experimental" || s == "Stable"; }
bool Hex(const std::string& s, size_t size) {
    return s.size() == size && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
} // namespace

bool ValidSemanticVersion(const std::string& version) { Version parsed; return ParseVersion(version, parsed); }
int CompareSemanticVersions(const std::string& left, const std::string& right) {
    Version a, b;
    if (!ParseVersion(left, a) || !ParseVersion(right, b)) throw std::invalid_argument("Invalid semantic version");
    for (unsigned i = 0; i < 3; ++i) if (const int c = Numeric(a.core[i], b.core[i])) return c;
    if (a.prerelease.empty() || b.prerelease.empty())
        return a.prerelease.empty() == b.prerelease.empty() ? 0 : a.prerelease.empty() ? 1 : -1;
    for (size_t i = 0; i < std::min(a.prerelease.size(), b.prerelease.size()); ++i) {
        const auto& x = a.prerelease[i]; const auto& y = b.prerelease[i];
        const bool xn = Digits(x), yn = Digits(y);
        const int c = xn && yn ? Numeric(x, y) : xn != yn ? (xn ? -1 : 1) : Lexical(x, y);
        if (c) return c;
    }
    return a.prerelease.size() == b.prerelease.size() ? 0 : a.prerelease.size() < b.prerelease.size() ? -1 : 1;
}
std::string ReleaseManifestEndpoint(const std::string& channel) {
    return Channel(channel) ? releases + "download/update-manifests/" + channel + ".json" : std::string{};
}
ReleaseUpdateResult ParseReleaseUpdate(const std::string& response, const std::string& installed,
    const std::string& channel, const std::string& platform, const std::string& architecture) {
    ReleaseUpdateResult result;
    result.diagnostic = "Release metadata is invalid or incompatible; no update offered";
    if (response.size() > 262144 || !Channel(channel) || !ValidSemanticVersion(installed)) return result;
    try {
        const auto j = nlohmann::json::parse(response, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
            if (depth > 16) throw std::invalid_argument("Release metadata nesting exceeds protocol limit");
            return true;
        });
        if (!j.is_object() || !j.at("schema").is_number_integer() || j.at("schema") != 1 || j.at("channel") != channel) return result;
        const auto version = j.at("version").get<std::string>();
        Version parsed, local;
        if (!ParseVersion(version, parsed) || !ParseVersion(installed, local)) return result;
        const auto stage = parsed.prerelease.empty() ? "stable" : parsed.prerelease[0];
        if ((channel == "Stable" && !parsed.prerelease.empty()) || (channel == "Alpha" && stage != "alpha") ||
            (channel == "Beta" && stage != "beta")) return result;
        if (!Hex(j.at("source_revision").get<std::string>(), 40)) return result;
        const auto& compatibility = j.at("compatibility");
        if (!compatibility.at("update_protocol").is_number_integer() || compatibility.at("update_protocol") != 1 ||
            !compatibility.at("engine_major").is_number_unsigned() ||
            compatibility.at("engine_major").dump() != parsed.core[0]) return result;
        const auto tag = "v" + version;
        if (j.at("tag") != tag || j.at("notes_url") != releases + "tag/" + tag) return result;
        const auto notes = j.at("notes").get<std::string>();
        if (notes.empty() || notes.size() > 8192 || notes.find('\0') != std::string::npos) return result;
        const auto& artifacts = j.at("artifacts");
        if (!artifacts.is_array() || artifacts.empty() || artifacts.size() > 32) return result;
        std::set<std::string> names;
        static const std::regex filename(R"([A-Za-z0-9][A-Za-z0-9._+\-]{0,180})");
        for (const auto& a : artifacts) {
            const auto name = a.at("name").get<std::string>();
            const auto hash = a.at("sha256").get<std::string>();
            if (!std::regex_match(name, filename) || name.find("..") != std::string::npos ||
                !names.insert(name).second || !Hex(hash, 64) || !a.at("bytes").is_number_unsigned() ||
                a.at("bytes").get<uint64_t>() == 0 || a.at("bytes").get<uint64_t>() > 100ULL * 1024 * 1024 * 1024 ||
                a.at("url") != releases + "download/" + tag + "/" + name) return result;
            const auto target = a.at("platform").get<std::string>();
            const auto arch = a.at("architecture").get<std::string>();
            if ((target != "Linux" && target != "Windows" && target != "Source") || (arch != "x86_64" && arch != "any")) return result;
            if (target == platform && arch == architecture) {
                if (!result.downloadUrl.empty()) return ReleaseUpdateResult{}; // Ambiguous target.
                result.downloadUrl = a.at("url").get<std::string>();
                result.artifactName = name; result.sha256 = hash;
            }
        }
        result.valid = true; result.version = version; result.channel = channel;
        result.compatible = local.core[0] == parsed.core[0];
        result.newer = CompareSemanticVersions(version, installed) > 0 && result.compatible && !result.downloadUrl.empty();
        result.notes = notes; result.notesUrl = j.at("notes_url").get<std::string>();
        result.diagnostic = !result.compatible ? "A different major version exists; review compatibility manually" :
            result.downloadUrl.empty() ? "No release download for this platform" : result.newer ?
            "A newer published release is available; nothing has been installed" : "No newer published release on this channel";
    } catch (const std::exception&) { return ReleaseUpdateResult{}; }
    return result;
}
