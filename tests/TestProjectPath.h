#pragma once
#include <filesystem>
#include <string>

// Frozen release-test projects travel with engine-only source archives. They
// are read-only inputs; tests put all authored output under their supplied cwd.
inline std::filesystem::path TestProjectPath(const std::string& name) {
    return std::filesystem::path(__FILE__).parent_path() / "fixtures" / "releases" / "sdk" / name;
}
