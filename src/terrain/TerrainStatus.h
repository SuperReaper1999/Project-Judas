#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

struct RuntimeTerrainStatus {
    bool ready = false, busy = false, navigation = false;
    uint64_t revision = 0;
    size_t samples = 0, triangles = 0, bytes = 0;
    size_t chunks = 0, volumeChunks = 0, pendingChunks = 0, uploadedBytes = 0;
    double prepareMs = 0, publishMs = 0;
    std::string error;
};
