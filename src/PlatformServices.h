#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Platform services only. Simulation and authored identities do not depend on them.
const char* RuntimeExecutableName();
const char* RuntimePlatformName();
std::string PlatformUserDataDirectory(std::string& error);
#ifdef __ANDROID__
// Android forbids opening filesystem /. Begin at its OS-provided private root;
// callers still walk every requested descendant using openat/O_NOFOLLOW.
bool AndroidStorageAnchor(const std::filesystem::path& requested,
    std::filesystem::path& anchor,std::filesystem::path& relative,std::string& error);
#endif
std::string ImportFileStamp(const std::filesystem::path& path);
std::string CreateExportStagingDirectory(const std::filesystem::path& parent);
std::string CreateImportStagingFile(const std::filesystem::path& output);
void ReplaceStagedFile(const std::filesystem::path& source, const std::filesystem::path& destination);
#ifdef _WIN32
bool WindowsLaunchProcess(const std::filesystem::path& executable,
                          const std::vector<std::string>& arguments, std::string& error);
bool WindowsCaptureBuildInfo(const std::filesystem::path& executable, std::string& output);
#endif
