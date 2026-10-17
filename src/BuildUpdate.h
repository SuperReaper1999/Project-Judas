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
