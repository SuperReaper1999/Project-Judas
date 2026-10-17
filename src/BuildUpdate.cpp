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
