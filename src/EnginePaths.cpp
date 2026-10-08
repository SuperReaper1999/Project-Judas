#include "EnginePaths.h"
#include "GamePackage.h"
#ifdef _WIN32
#include <windows.h>
#endif

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
#elif defined(_WIN32)
    std::vector<wchar_t> buffer(512);
    while (buffer.size() <= 32768) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
        if (!length) return {};
        if (length < buffer.size()) return fs::path(std::wstring(buffer.data(), length)).parent_path().generic_u8string();
        buffer.resize(buffer.size() * 2);
    }
#endif
    return std::string();
}

std::string ResolveEngineDataPath(const std::string& relativePath) {
    const fs::path packagedRoot = EngineExecutableDir();
    if (fs::exists(packagedRoot / kGamePackageMarker))
        return (packagedRoot / "engine" / relativePath).lexically_normal().generic_string();
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
