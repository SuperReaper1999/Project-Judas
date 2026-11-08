#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

struct RuntimeTerrainStatus {
    bool ready = false, busy = false, navigation = false;
    uint64_t revision = 0;
    size_t samples = 0, triangles = 0, bytes = 0;
    size_t chunks = 0, volumeChunks = 0, pendingChunks = 0, uploadedBytes = 0;
    size_t sourcePageBytes = 0, sourcePageBudget = 0, sourcePages = 0;
    size_t collisionPageBytes = 0, collisionPageBudget = 0, collisionPages = 0;
    size_t gpuPageBytes = 0, gpuPageBudget = 0, gpuPages = 0;
    uint64_t sourceEvictions = 0, collisionEvictions = 0, gpuEvictions = 0;
    double pagePrepareMs = 0, worstPagePrepareMs = 0;
    double prepareMs = 0, publishMs = 0;
    std::string error;
};
