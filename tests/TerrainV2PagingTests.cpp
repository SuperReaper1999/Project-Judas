#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainRenderPages.h"
#include "terrain/TerrainSamples.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        using namespace TerrainV2;
        for (unsigned cells = 4; cells <= 64; ++cells)
            for (unsigned level = 0; level <= 5; ++level) {
                Source layout;
                layout.chunkCells = cells;
                layout.samplesX = layout.samplesZ = (cells << level) + 1;
                HeightPatch patch{{0, 0, level}, glm::vec4(1u << level), 0, 0};
                size_t triangles = 0;
                for (const auto &page : PartitionRenderPatch(layout, patch)) {
                    const unsigned n = page.cells ? page.cells : cells;
                    const unsigned stride = page.stride ? page.stride : 1u << page.patch.level;
                    const unsigned nz = page.cellsZ ? page.cellsZ : n;
                    triangles += n * nz * 2;
                    Check(n >= 1 && n <= 64 && nz >= 1 && nz <= 64 && n * stride <= 256 &&
                              nz * stride <= 256,
                          "material partition preserves a supported exact geometric grid");
                }
                Check(triangles == cells * cells * 2,
                      "material partition adds no geometric triangles");
            }
        PageCache<unsigned> cache(8);
        auto value = [](unsigned n) {
            return [n] {
                return std::make_pair(std::make_shared<const unsigned>(n), sizeof(unsigned));
            };
        };
        auto pinned = cache.Get(1, value(11));
        Check(*cache.Get(2, value(22)) == 22, "second cache page");
        Check(*cache.Get(3, value(33)) == 33 && *pinned == 11,
              "LRU skips an active consumer rather than releasing its storage");
        Check(cache.Statistics().bytes == 8 && cache.Statistics().evictions == 1 &&
                  cache.Statistics().pinnedBytes == 4,
              "bounded cache accounting");
        auto second = cache.Get(3, value(33));
        bool rejected = false;
        try {
            cache.Get(4, value(44));
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        Check(rejected && *pinned == 11 && *second == 33,
              "pin pressure rejects explicitly while preserving borrowed products");
        PageCache<unsigned> concurrent(16);
        concurrent.Get(1, value(11));
        std::promise<void> entered, release;
        const auto released = release.get_future().share();
        auto loading = std::async(std::launch::async, [&] {
            return concurrent.Get(2, [&] {
                entered.set_value();
                released.wait();
                return std::make_pair(std::make_shared<const unsigned>(22), sizeof(unsigned));
            });
        });
        entered.get_future().wait();
        auto hit = std::async(std::launch::async, [&] { return concurrent.Get(1, value(11)); });
        const bool available = hit.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
        release.set_value();
        const auto loaded = loading.get(), cached = hit.get();
        Check(available && *loaded == 22 && *cached == 11,
              "a blocked worker load cannot block a complete cached query");
        auto dense = Create(513, 513, 1, 32);
        for (size_t i = 0; i < dense.heights.size(); ++i) {
            dense.heights[i] = float(i % 193) / 7;
            const float w = float(i % 17) / 16;
            dense.weights[i] = {w, 1 - w, 0, 0};
        }
        Source paged;
        std::string error;
        Check(SampleStorage::Spill(dense, 128 * 1024, paged, error), "spill samples: " + error);
        Check(paged.heights.empty() && paged.weights.empty() && paged.storage,
              "runtime source retains no complete arrays");
        for (unsigned pass = 0; pass < 3; ++pass)
            for (unsigned z = 0; z < dense.samplesZ; z += 31)
                for (unsigned x = 0; x < dense.samplesX; x += 37) {
                    const size_t i = size_t(z) * dense.samplesX + x;
                    Check(HeightSample(paged, i) == dense.heights[i] &&
                              WeightSample(paged, i) == dense.weights[i],
                          "reload retains exact height and material weights");
                }
        const auto stats = paged.storage->Statistics();
        Check(stats.bytes <= stats.budget && stats.peakBytes <= stats.budget &&
                  stats.evictions > 100 && stats.entries <= 2,
              "sustained traversal evicts under 128 KiB instead of retaining all pages");
        auto old = paged;
        RasterChanges edits{{42, {100, {0, 0, 1, 0}}}, {512 * 513 + 512, {-10, {0, 1, 0, 0}}}};
        SetSamples(paged, edits);
        Check(HeightSample(old, 42) == dense.heights[42] && HeightSample(paged, 42) == 100,
              "immutable source revision cannot alter a leased old revision");
        Check(paged.storage->Persist(paged, error), "persist private journal: " + error);
        auto window = paged.storage->Window(0, 0, 64, 64);
        Check(window->Changes().size() == 1 && window->Read(42).height == 100,
              "chunk recipe retains only local edits, sharing bounded base pages");
        Check(paged.storage->Base()->Read(42).height == dense.heights[42],
              "base snapshot removes overrides without copying all samples");
        Check(ChangedSampleBounds(old, paged) == glm::uvec4(42, 0, 512, 512),
              "sparse publication finds the inclusive changed sample rectangle");
        Check(!ChangedSampleBounds(paged, paged),
              "unchanged backing does not invalidate any render samples");
        Source paint = old;
        SetSamples(paint, {{42, {HeightSample(old, 42), {0, 0, 1, 0}}}});
        Check(ChangedHeights(old, paint).empty() &&
                  ChangedSampleBounds(old, paint) == glm::uvec4(42, 0, 42, 0),
              "material-only edits invalidate images without changing physical heights");
        Check(ChangedSampleBounds(paint, old) == glm::uvec4(42, 0, 42, 0),
              "restoring a removed sparse override also invalidates its image");
        Source densePaint = dense;
        densePaint.weights[42] = {0, 0, 1, 0};
        Check(ChangedSampleBounds(dense, densePaint) == glm::uvec4(42, 0, 42, 0),
              "dense authoring and paged material bounds agree");
        std::vector<uint8_t> original, encoded;
        Check(EncodeAsset(dense, original, error) && EncodeAsset(old, encoded, error) &&
                  original == encoded,
              "paged source preserves exact native encoding");
        Source roundtrip;
        Check(EncodeAsset(paged, encoded, error) &&
                  DecodeAsset({reinterpret_cast<const char *>(encoded.data()), encoded.size()},
                              roundtrip, error) &&
                  roundtrip.heights[42] == 100 && roundtrip.weights[42].z == 1,
              "changed and evicted pages survive cold native encoding");
        SurfaceBrush brush;
        brush.kind = SurfaceBrush::Kind::Raise;
        brush.radius = 2;
        brush.strength = .2f;
        Source reference = dense;
        Source edited = old;
        std::vector<ChunkKey> changedA, changedB;
        Check(ApplySurfaceBrush(reference, {0, 0, 0}, brush, changedA, error) &&
                  ApplySurfaceBrush(edited, {0, 0, 0}, brush, changedB, error),
              "surface brush supports native paging: " + error);
        Check(EncodeAsset(reference, original, error) && EncodeAsset(edited, encoded, error) &&
                  original == encoded && changedA == changedB,
              "paged sculpting is byte-identical to authoring vectors");
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
