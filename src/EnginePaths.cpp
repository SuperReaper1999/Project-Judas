#include "EnginePaths.h"

#include <cstdlib>
#include <filesystem>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

std::string EngineExecutableDir() {
#if defined(__linux__)
    char buffer[4096];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length > 0) {
        buffer[length] = '\0';
        return fs::path(buffer).parent_path().generic_string();
    }
#endif
    return std::string();
}

std::string ResolveEngineDataPath(const std::string& relativePath) {
    std::vector<fs::path> candidates;
    if (const char* root = std::getenv("JUDAS_ENGINE_ROOT")) {
        if (*root) candidates.push_back(fs::path(root) / relativePath);
    }
    const std::string executableDir = EngineExecutableDir();
    if (!executableDir.empty()) {
        candidates.push_back(fs::path(executableDir) / relativePath);
        candidates.push_back(fs::path(executableDir) / ".." / relativePath);
    }
    candidates.push_back(fs::path(relativePath));
    std::error_code ec;
    for (const fs::path& candidate : candidates) {
        if (fs::exists(candidate, ec)) return candidate.lexically_normal().generic_string();
    }
    return candidates.back().generic_string();
}
