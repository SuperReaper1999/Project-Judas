#include "terrain/TerrainMesher.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

// Matches the former editor worker (full validation for every chunk) against
// one immutable preparation batch. Excludes JSON IO, merging, GPU upload and
// collision cooking. Run without concurrent builds; not a timing release gate.
namespace {
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
bool Single(const TerrainV2::Source &source, const std::vector<TerrainV2::ChunkKey> &keys,
            std::vector<TerrainV2::Chunk> &output, std::string &error) {
    std::vector<TerrainV2::Chunk> fresh;
    fresh.reserve(keys.size());
    for (auto key : keys) {
        TerrainV2::Chunk chunk;
        if (!TerrainV2::BuildChunk(source, key, chunk, error))
            return false;
        fresh.push_back(std::move(chunk));
    }
    output = std::move(fresh);
    return true;
}
void Same(const std::vector<TerrainV2::Chunk> &a, const std::vector<TerrainV2::Chunk> &b) {
    if (a.size() != b.size())
        throw std::runtime_error("chunk count differs");
    for (size_t i = 0; i < a.size(); ++i) {
        const auto &x = a[i], &y = b[i];
        if (!(x.key == y.key) || x.anchor != y.anchor || x.volumetric != y.volumetric ||
            x.mesh.indices != y.mesh.indices || x.mesh.vertices.size() != y.mesh.vertices.size())
            throw std::runtime_error("chunk geometry differs");
        for (size_t j = 0; j < x.mesh.vertices.size(); ++j) {
            const auto &p = x.mesh.vertices[j], &q = y.mesh.vertices[j];
            if (p.position != q.position || p.normal != q.normal || p.uv != q.uv ||
                p.uv1 != q.uv1 || p.tangent != q.tangent)
                throw std::runtime_error("vertex data differs");
        }
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        const unsigned n = argc > 1 ? unsigned(std::stoul(argv[1])) : 513;
        auto source = TerrainV2::Create(n, n, 2);
        for (unsigned z = 0; z < n; ++z)
            for (unsigned x = 0; x < n; ++x)
                source.heights[size_t(z) * n + x] = 12 * std::sin(x * .035f) * std::cos(z * .027f);
        std::vector<TerrainV2::ChunkKey> keys;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x)
                keys.push_back({x, z});
        std::vector<TerrainV2::Chunk> old, batch;
        std::string error;
        if (!Single(source, keys, old, error) ||
            !TerrainV2::BuildChunks(source, keys, batch, error))
            throw std::runtime_error(error);
        Same(old, batch);
        for (unsigned run = 0; run < 30; ++run) {
            double singleMs = 0, batchMs = 0;
            auto single = [&] {
                const auto start = Clock::now();
                if (!Single(source, keys, old, error))
                    throw std::runtime_error(error);
                singleMs = Ms(start);
            };
            auto grouped = [&] {
                const auto start = Clock::now();
                if (!TerrainV2::BuildChunks(source, keys, batch, error))
                    throw std::runtime_error(error);
                batchMs = Ms(start);
            };
            if (run % 2) {
                grouped();
                single();
            } else {
                single();
                grouped();
            }
            Same(old, batch);
            std::cout << "{\"samples\":" << n << ",\"chunks\":" << keys.size() << ",\"run\":" << run
                      << ",\"former_worker_ms\":" << singleMs << ",\"batch_worker_ms\":" << batchMs
                      << ",\"identical_geometry\":true}\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
