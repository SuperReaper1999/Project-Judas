#include "BuildUpdate.h"
#include "BuildIdentity.h"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
int main(int argc, char** argv) {
    if (HandleBuildIdentityArguments(argc, argv, "release-update-test")) return 0;
    if (argc != 2) { std::cerr << "Expected shared fixture path\n"; return 2; }
    std::ifstream file(argv[1]);
    const auto cases = nlohmann::json::parse(file);
    unsigned checks = 0, failures = 0;
    const auto check = [&](bool ok, const std::string& name) {
        ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << name << '\n'; }
    };
    for (const auto& row : cases) {
        auto result = ParseReleaseUpdate(row.at("manifest").dump(), "1.0.0-alpha.1",
            row.value("channel", "Alpha"), row.value("platform", "Linux"), "x86_64");
        check(result.valid == row.at("valid"), row.at("name"));
        check(result.newer == row.at("offer"), row.at("name").get<std::string>() + " offer");
    }
    const std::vector<std::string> order{"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.2", "1.0.0-alpha.10",
        "1.0.0-beta.1", "1.0.0-rc.1", "1.0.0", "1.0.1", "2.0.0"};
    for (size_t i = 1; i < order.size(); ++i) {
        check(CompareSemanticVersions(order[i - 1], order[i]) == -1, "SemVer forward ordering");
        check(CompareSemanticVersions(order[i], order[i - 1]) == 1, "SemVer reverse ordering");
    }
    check(CompareSemanticVersions("1.0.0+local", "1.0.0+other") == 0, "Build metadata does not affect precedence");
    for (const auto& bad : {"1", "v1.0.0", "01.0.0", "1.0.0-alpha.01", "1.0.0-", "1.0.0+", "1.0.0-x..a"})
        check(!ValidSemanticVersion(bad), "Invalid SemVer refused");
    check(!ParseReleaseUpdate("not JSON", "1.0.0", "Alpha", "Linux", "x86_64").valid, "Malformed/offline response");
    check(!ParseReleaseUpdate(std::string(262145, 'x'), "1.0.0", "Alpha", "Linux", "x86_64").valid, "Bounded response");
    check(ReleaseManifestEndpoint("main").empty(), "Git branch is not a release channel");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
