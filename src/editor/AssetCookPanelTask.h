#pragma once
#include "JobSystem.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// Worker prepares bytes; the owning asset workflow publishes only in Edit mode.
struct AssetCookPanelTask {
    std::atomic<bool> ready{false};
    std::string root, source, destination, recipe, error;
    std::vector<uint8_t> bytes;
    JobHandle job;
};
