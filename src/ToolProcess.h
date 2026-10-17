#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// Shell-free, bounded probes of our tools or read-only platform utilities.
// On timeout only the process started by this call is terminated/reaped.
bool CaptureToolOutput(const std::filesystem::path& executable,
                       const std::vector<std::string>& arguments,
                       std::string& output, std::string& error,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
                       std::size_t maximumBytes = 16384);

// Windows must not discover curl.exe in a project's working directory. Return
// the OS system-directory executable (or empty on lookup failure). Linux keeps
// the normal explicitly optional PATH utility.
std::filesystem::path SystemHttpsProbeExecutable();
