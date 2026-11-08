#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>

namespace TerrainV2 {
// Own only an exclusively created private directory, never a caller's path.
struct TemporaryDirectory {
    std::filesystem::path path;
    explicit TemporaryDirectory(const char *prefix = "judas-terrain-pages") {
        static std::atomic<uint64_t> sequence{1};
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            path = std::filesystem::temp_directory_path() /
                   (std::string(prefix) + "-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                    "-" + std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(path))
                return;
        }
        throw std::runtime_error("cannot allocate private terrain spill directory");
    }
    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
} // namespace TerrainV2
