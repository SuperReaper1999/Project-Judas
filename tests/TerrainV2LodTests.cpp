#include "terrain/TerrainLod.h"
#include "terrain/TerrainMesher.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const char *text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
void Coverage(const TerrainV2::Source &s, const std::vector<TerrainV2::HeightPatch> &patches) {
    std::vector<unsigned> count(size_t(s.ChunksX()) * s.ChunksZ());
    for (const auto &p : patches) {
        const auto side = 1u << p.key.level;
        Check(p.key.x + side <= s.ChunksX() && p.key.z + side <= s.ChunksZ(),
              "LOD never extends beyond rectangular source");
        for (unsigned z = p.key.z; z < p.key.z + side; ++z)
            for (unsigned x = p.key.x; x < p.key.x + side; ++x)
                ++count[size_t(z) * s.ChunksX() + x];
    }
    Check(std::all_of(count.begin(), count.end(), [](auto n) { return n == 1; }),
          "every terrain chunk is covered exactly once");
}
} // namespace
int main() {
    try {
        auto s = TerrainV2::Create(257, 129, .5f);
        for (unsigned z = 0; z < s.samplesZ; ++z)
            for (unsigned x = 0; x < s.samplesX; ++x)
                s.heights[size_t(z) * s.samplesX + x] = 2 * std::sin(x * .07f) * std::cos(z * .09f);
        TerrainV2::HeightLod lod(s);
        const TerrainV2::View near{{-46, 3, -16}, 600, 0}, other{{46, 3, 16}, 600, 0},
            far{{0, 800, 0}, 600, 0};
        auto distant = lod.Select({far}, {}), close = lod.Select({near}, {}),
             split = lod.Select({near, other}, {});
        Coverage(s, distant);
        Coverage(s, close);
        Coverage(s, split);
        Check(distant.size() < close.size(), "distant camera uses fewer patches");
        Check(split.size() >= close.size(),
              "independent cameras cannot reduce another view's detail");
        auto pixelError = [&](const std::vector<TerrainV2::HeightPatch> &patches,
                              const TerrainV2::View &v) {
            for (const auto &p : patches) {
                const unsigned step = 1u << p.key.level;
                for (unsigned z = p.key.z * s.chunkCells; z <= (p.key.z + step) * s.chunkCells; ++z)
                    for (unsigned x = p.key.x * s.chunkCells; x <= (p.key.x + step) * s.chunkCells;
                         ++x) {
                        const float h = s.heights[size_t(z) * s.samplesX + x];
                        glm::vec3 point((float(x) - float(s.samplesX - 1) * .5f) * s.spacing, h,
                                        (float(z) - float(s.samplesZ - 1) * .5f) * s.spacing);
                        double distance = std::max(.05, double(glm::length(point - v.position)));
                        if (std::abs(h - TerrainV2::LodHeight(s, {x, z}, step)) *
                                v.pixelsPerRadian / distance >
                            2.001)
                            throw std::runtime_error(
                                "selected LOD violates declared source height error bound");
                    }
            }
            Check(true, "sampled source-space geometric error is bounded for every selected patch");
        };
        pixelError(close, near);
        pixelError(split, near);
        pixelError(split, other);
        std::set<TerrainV2::ChunkKey> volumes{{8, 4}};
        auto excavated = lod.Select({far}, volumes);
        Coverage(s, excavated);
        unsigned seam = 0;
        for (const auto &p : excavated) {
            const unsigned side = 1u << p.key.level;
            if (int(p.key.x) <= 9 && p.key.x + side > 7 && int(p.key.z) <= 5 &&
                p.key.z + side > 3) {
                Check(p.key.level == 0,
                      "excavation and immediate neighbours keep exact source resolution");
                ++seam;
            }
        }
        Check(seam == 9, "volume seam includes complete surrounding heightfield ring");
        unsigned stitched = 0;
        for (const auto &p : close) {
            const unsigned step = 1u << p.key.level;
            for (unsigned edge = 0; edge < 4; ++edge) {
                unsigned coarse = unsigned(p.edgeStrides[edge]);
                if (coarse <= step)
                    continue;
                ++stitched;
                for (unsigned i = 0; i <= s.chunkCells; ++i) {
                    glm::vec2 point(p.key.x * s.chunkCells, p.key.z * s.chunkCells);
                    if (edge < 2) {
                        point.y += i * step;
                        if (edge == 1)
                            point.x += s.chunkCells * step;
                    } else {
                        point.x += i * step;
                        if (edge == 3)
                            point.y += s.chunkCells * step;
                    }
                    // Independent linear interpolation on the neighbour's edge.
                    const unsigned axis = edge < 2 ? 1 : 0;
                    auto lo = point, hi = point;
                    lo[axis] = std::floor(point[axis] / coarse) * coarse;
                    hi[axis] = lo[axis] + coarse;
                    auto raw = [&](glm::vec2 v) {
                        return s
                            .heights[size_t(std::min(unsigned(v.y), s.samplesZ - 1)) * s.samplesX +
                                     std::min(unsigned(v.x), s.samplesX - 1)];
                    };
                    float t = (point[axis] - lo[axis]) / coarse,
                          expected = raw(lo) * (1 - t) + raw(hi) * t;
                    Check(std::abs(TerrainV2::LodHeight(s, point, coarse) - expected) < 2e-6f,
                          "fine edge lies on the actual coarse edge, no skirts or holes");
                }
            }
        }
        Check(stitched > 0, "fixture exercises real mixed-detail boundaries");
        auto image = TerrainV2::HeightImage(s);
        const auto &bytes = image.levels.at(0).bytes;
        Check(image.levels.size() == 1 && bytes.size() == s.heights.size() * 4,
              "height storage has no averaged mip chain");
        for (size_t i = 0; i < s.heights.size(); ++i) {
            uint32_t bits = 0;
            for (unsigned b = 0; b < 4; ++b)
                bits |= uint32_t(bytes[i * 4 + b]) << (b * 8);
            float decoded;
            std::memcpy(&decoded, &bits, 4);
            if (decoded != s.heights[i])
                throw std::runtime_error("height sample lost precision");
        }
        Check(true, "GPU byte decode retains every signed binary32 source sample exactly");
        auto grid = TerrainV2::HeightGrid(16);
        Check(grid.vertices.size() == 289 && grid.indices.size() == 1536,
              "fixed shared grid does not grow with world size");
        auto small = TerrainV2::Create(17, 17, .5f, 8);
        std::vector<TerrainV2::Chunk> batch;
        std::string error;
        Check(TerrainV2::BuildChunks(small, {{0, 0}, {1, 0}}, batch, error),
              "validated immutable preparation batch");
        Check(batch.size() == 2, "batch publishes all requested chunks together");
        const auto firstIndices = batch.front().mesh.indices;
        Check(!TerrainV2::BuildChunks(small, {{0, 0}, {99, 0}}, batch, error),
              "bad later chunk rejects the whole batch");
        Check(batch.size() == 2 && batch.front().mesh.indices == firstIndices,
              "failed batch retains previous complete products");
        Check(!TerrainV2::BuildChunks(small, {{0, 0}}, batch, error, [] { return true; }),
              "cancelled batch is rejected");
        Check(batch.size() == 2 && batch.front().mesh.indices == firstIndices,
              "cancellation retains previous complete products");
        small.heights.back() = std::numeric_limits<float>::quiet_NaN();
        Check(!TerrainV2::BuildChunks(small, {{0, 0}}, batch, error),
              "batch validates the complete source including unrequested samples");
        auto orthographic = lod.Select({{{0, 1000, 0}, 1, 200}}, {});
        Check(orthographic.size() >= close.size(),
              "orthographic detail ignores perspective distance");
        std::cout << "METRIC far_patches=" << distant.size() << " near_patches=" << close.size()
                  << " split_patches=" << split.size() << " shared_grid_bytes="
                  << grid.vertices.size() * sizeof(MeshVertex) + grid.indices.size() * 4
                  << " height_bytes=" << bytes.size() << " hierarchy_bytes_estimate=" << lod.Bytes()
                  << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
