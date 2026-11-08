#include "authoring/TerrainEditorPreview.h"
#include "terrain/TerrainMesher.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

bool OriginalTerrainPick(const MeshData &, glm::vec3, glm::vec3, glm::vec3 &);
using Clock = std::chrono::steady_clock;
namespace {
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void Report(const char *path, std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    std::cout << "{\"path\":\"" << path << "\",\"samples\":" << samples.size()
              << ",\"median_ms\":" << samples[samples.size() / 2]
              << ",\"p95_ms\":" << samples[size_t(std::ceil(samples.size() * .95)) - 1]
              << ",\"worst_ms\":" << samples.back() << "}\n";
}
} // namespace
int main() {
    try {
        auto source = TerrainV2::Create(513, 513, 2);
        for (unsigned z = 0; z < source.samplesZ; ++z)
            for (unsigned x = 0; x < source.samplesX; ++x)
                source.heights[size_t(z) * source.samplesX + x] = 35 * std::sin(x * .017f) +
                                                                  20 * std::sin(z * .023f) +
                                                                  8 * std::sin((x + z) * .04f);
        std::vector<TerrainV2::ChunkKey> keys;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x)
                keys.push_back({x, z});
        std::string error;
        std::vector<TerrainV2::Chunk> chunks;
        auto start = Clock::now();
        if (!TerrainV2::BuildChunks(source, keys, chunks, error))
            throw std::runtime_error(error);
        const double geometryMs = Milliseconds(start);
        // Match the original editor's std::map<ChunkKey, Resident> order.
        std::sort(chunks.begin(), chunks.end(), [](auto &a, auto &b) { return a.key < b.key; });
        start = Clock::now();
        MeshData merged;
        for (const auto &c : chunks) {
            unsigned offset = unsigned(merged.vertices.size());
            for (auto v : c.mesh.vertices) {
                v.position += c.anchor;
                merged.vertices.push_back(v);
            }
            for (auto i : c.mesh.indices)
                merged.indices.push_back(offset + i);
        }
        const double mergeMs = Milliseconds(start);
        start = Clock::now();
        std::shared_ptr<const TerrainV2::EditorPreview> prepared;
        if (!TerrainV2::PrepareEditorPreview(source, keys, {}, prepared, error))
            throw std::runtime_error(error);
        const double prepareMs = Milliseconds(start);
        if (merged.indices != prepared->Mesh()->indices ||
            merged.vertices.size() != prepared->Mesh()->vertices.size())
            throw std::runtime_error("snapshot changes original preview ordering");
        for (size_t i = 0; i < merged.vertices.size(); ++i)
            if (merged.vertices[i].position != prepared->Mesh()->vertices[i].position)
                throw std::runtime_error("snapshot changes original preview positions");
        std::vector<double> before, after;
        size_t tested = 0, hits = 0;
        for (unsigned i = 0; i < 120; ++i) {
            glm::vec3 origin(-490.f + float((i * 97) % 980), 180, -490.f + float((i * 61) % 980));
            glm::vec3 direction = i % 3 == 0 ? glm::vec3(.13f, -.6f, -.08f)
                                             : glm::vec3(0, i % 11 == 0 ? 1.f : -1.f, 0);
            glm::vec3 reference{}, actual{};
            bool oldHit = false, newHit = false;
            auto old = [&] {
                auto t = Clock::now();
                oldHit = OriginalTerrainPick(merged, origin, direction, reference);
                before.push_back(Milliseconds(t));
            };
            auto next = [&] {
                auto t = Clock::now();
                newHit = prepared->Pick(origin, direction, actual, &tested);
                after.push_back(Milliseconds(t));
            };
            if (i % 2) {
                next();
                old();
            } else {
                old();
                next();
            }
            if (oldHit != newHit || (oldHit && reference != actual))
                throw std::runtime_error(
                    "production index differs from the frozen original picker");
            hits += oldHit;
        }
        Report("original_merged_hover", before);
        Report("production_cached_ranges", after);
        std::cout << "{\"triangles\":" << merged.indices.size() / 3
                  << ",\"chunks\":" << prepared->Chunks().size()
                  << ",\"original_geometry_ms\":" << geometryMs
                  << ",\"original_editor_merge_ms\":" << mergeMs
                  << ",\"complete_new_preparation_ms\":" << prepareMs
                  << ",\"bounds_bytes\":" << prepared->BoundsBytes()
                  << ",\"original_merged_capacity_bytes\":"
                  << merged.vertices.capacity() * sizeof(MeshVertex) +
                         merged.indices.capacity() * sizeof(uint32_t)
                  << ",\"new_merged_capacity_bytes\":"
                  << prepared->Mesh()->vertices.capacity() * sizeof(MeshVertex) +
                         prepared->Mesh()->indices.capacity() * sizeof(uint32_t)
                  << ",\"hits\":" << hits
                  << ",\"exact_original_points\":true,\"indexed_triangles_total\":" << tested
                  << ",\"reference_triangles_total\":" << before.size() * merged.indices.size() / 3
                  << "}\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
