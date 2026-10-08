#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Desktop services only. Simulation and authored identities do not depend on them.
const char* RuntimeExecutableName();
const char* RuntimePlatformName();
std::string PlatformUserDataDirectory(std::string& error);
std::string ImportFileStamp(const std::filesystem::path& path);
std::string CreateExportStagingDirectory(const std::filesystem::path& parent);
std::string CreateImportStagingFile(const std::filesystem::path& output);
void ReplaceStagedFile(const std::filesystem::path& source, const std::filesystem::path& destination);
#ifdef _WIN32
bool WindowsLaunchProcess(const std::filesystem::path& executable,
                          const std::vector<std::string>& arguments, std::string& error);
bool WindowsCaptureBuildInfo(const std::filesystem::path& executable, std::string& output);
#endif
