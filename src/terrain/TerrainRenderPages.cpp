#include "TerrainRenderPages.h"
#include "Renderer.h"
#include "TerrainSamples.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace TerrainV2 {
namespace {
void Float(TextureMip &image, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (unsigned b = 0; b < 4; ++b)
        image.bytes.push_back(uint8_t(bits >> (8 * b)));
}
TextureData Image(unsigned width, unsigned height, TextureMip level) {
    TextureData result;
    result.width = level.width = int(width);
    result.height = level.height = int(height);
    result.levels.push_back(std::move(level));
    return result;
}
} // namespace
size_t RenderPage::Bytes() const {
    size_t bytes = 0;
    for (const auto &level : heights.levels)
        bytes += level.bytes.size();
    for (const auto &level : weights.levels)
        bytes += level.bytes.size();
    return bytes;
}
namespace {
TerrainWeightKey Region(const Source &source, const RenderPageKey &key) {
    return {key.firstX == UINT32_MAX ? key.patch.x * source.chunkCells : key.firstX,
            key.firstZ == UINT32_MAX ? key.patch.z * source.chunkCells : key.firstZ,
            key.extentX ? key.extentX : source.chunkCells * (1u << key.patch.level),
            key.extentZ ? key.extentZ : source.chunkCells * (1u << key.patch.level)};
}
} // namespace
std::vector<RenderPageKey> PartitionRenderPatch(const Source &source, const HeightPatch &patch) {
    const unsigned stride = 1u << patch.key.level, extent = source.chunkCells * stride;
    if (extent <= 256)
        return {{patch.key, patch.edgeStrides, true}};
    // Cut on the ORIGINAL coarse grid vertices, never on an assumed power-of-two
    // authoring chunk. Rectangular final strips preserve every original triangle.
    const unsigned tileCells = 256 / stride;
    if (!tileCells)
        throw std::runtime_error("terrain coarse stride exceeds native material page coverage");
    std::vector<RenderPageKey> result;
    for (unsigned z = 0; z < source.chunkCells; z += tileCells)
        for (unsigned x = 0; x < source.chunkCells; x += tileCells) {
            const unsigned nx = std::min(tileCells, source.chunkCells - x),
                           nz = std::min(tileCells, source.chunkCells - z);
            auto edges = glm::vec4(stride);
            if (!x)
                edges.x = patch.edgeStrides.x;
            if (x + nx == source.chunkCells)
                edges.y = patch.edgeStrides.y;
            if (!z)
                edges.z = patch.edgeStrides.z;
            if (z + nz == source.chunkCells)
                edges.w = patch.edgeStrides.w;
            result.push_back(
                {patch.key, edges, true, stride, nx, patch.key.x * source.chunkCells + x * stride,
                 patch.key.z * source.chunkCells + z * stride, nx * stride, nz * stride, nz});
        }
    return result;
}
RenderPage BuildRenderPage(const Source &source, const RenderPageKey &key, bool includeWeights) {
    RenderPage result;
    const unsigned cells = key.cells ? key.cells : source.chunkCells,
                   cellsZ = key.cellsZ ? key.cellsZ : cells,
                   stride = key.stride ? key.stride : 1u << key.patch.level, n = cells + 1;
    const auto rect = Region(source, key);
    const unsigned x0 = rect[0], z0 = rect[1], extent = rect[2], depth = rect[3];
    auto raw = [&](int x, int z) {
        return HeightSample(source,
                            size_t(std::clamp(z, 0, int(source.samplesZ) - 1)) * source.samplesX +
                                unsigned(std::clamp(x, 0, int(source.samplesX) - 1)));
    };
    if (key.grid) {
        TextureMip level;
        level.bytes.reserve(size_t(n) * (cellsZ + 1) * 12);
        for (unsigned z = 0; z <= cellsZ; ++z)
            for (unsigned channel = 0; channel < 3; ++channel)
                for (unsigned x = 0; x < n; ++x) {
                    const unsigned sx = x0 + x * stride, sz = z0 + z * stride;
                    float h = raw(sx, sz);
                    if (!channel) {
                        if (!x)
                            h = LodHeight(source, {sx, sz}, unsigned(key.edges.x));
                        if (x == cells)
                            h = LodHeight(source, {sx, sz}, unsigned(key.edges.y));
                        if (!z)
                            h = LodHeight(source, {sx, sz}, unsigned(key.edges.z));
                        if (z == cellsZ)
                            h = LodHeight(source, {sx, sz}, unsigned(key.edges.w));
                    } else if (channel == 1)
                        h = (raw(int(sx) + 1, sz) - raw(int(sx) - 1, sz)) / (2 * source.spacing);
                    else
                        h = (raw(sx, int(sz) + 1) - raw(sx, int(sz) - 1)) / (2 * source.spacing);
                    Float(level, h);
                }
        result.heights = Image(3 * n, cellsZ + 1, std::move(level));
    }
    result.weightRect = {float(x0) / (source.samplesX - 1), float(z0) / (source.samplesZ - 1),
                         float(extent) / (source.samplesX - 1),
                         float(depth) / (source.samplesZ - 1)};
    if (!includeWeights)
        return result;
    if (extent > 256 || depth > 256)
        throw std::runtime_error("terrain material coverage must be partitioned into native pages");
    const unsigned resolution = extent + 1, rows = depth + 1;
    TextureMip weights;
    weights.bytes.reserve(size_t(resolution) * rows * 4);
    auto weight = [&](unsigned x, unsigned z) {
        return WeightSample(source, size_t(std::min(z, source.samplesZ - 1)) * source.samplesX +
                                        std::min(x, source.samplesX - 1));
    };
    for (unsigned z = 0; z < rows; ++z)
        for (unsigned x = 0; x < resolution; ++x) {
            const float sx = x0 + float(x) * extent / (resolution - 1), sz = z0 + float(z);
            const auto ix = unsigned(sx), iz = unsigned(sz);
            const auto w =
                glm::mix(glm::mix(weight(ix, iz), weight(ix + 1, iz), sx - ix),
                         glm::mix(weight(ix, iz + 1), weight(ix + 1, iz + 1), sx - ix), sz - iz);
            for (unsigned channel = 0; channel < 4; ++channel)
                weights.bytes.push_back(uint8_t(std::lround(w[channel] * 255)));
        }
    result.weights = Image(resolution, rows, std::move(weights));
    result.weightRect = {float(x0) / (source.samplesX - 1), float(z0) / (source.samplesZ - 1),
                         float(extent) / (source.samplesX - 1),
                         float(depth) / (source.samplesZ - 1)};
    return result;
}
void RenderPages::Release(Renderer &renderer, GpuTerrainPage &page) {
    renderer.DestroyTexture(page.heights);
    auto weight = weights.find(page.weightKey);
    if (weight != weights.end() && !--weight->second.references) {
        stats.bytes -= weight->second.bytes;
        renderer.DestroyTexture(weight->second.texture);
        weights.erase(weight);
    }
    renderer.DestroyMesh(page.mesh);
}
void RenderPages::Clear(Renderer &renderer) {
    renderer.EndMeshBatch();
    for (auto &[key, page] : pages)
        Release(renderer, page);
    pages.clear();
    stats.bytes = 0;
}
void RenderPages::Invalidate(Renderer &renderer, const Source &source,
                             std::optional<glm::uvec4> samples,
                             const std::vector<ChunkKey> &meshes) {
    renderer.EndMeshBatch();
    for (auto it = pages.begin(); it != pages.end();) {
        const auto &key = it->first;
        bool changed = !key.grid && std::find(meshes.begin(), meshes.end(),
                                              ChunkKey{key.patch.x, key.patch.z}) != meshes.end();
        if (samples) {
            const auto rect = Region(source, key);
            const auto halo =
                unsigned(std::max({1.f, key.edges.x, key.edges.y, key.edges.z, key.edges.w}));
            changed |= uint64_t((*samples)[2]) + halo >= rect[0] &&
                       uint64_t((*samples)[3]) + halo >= rect[1] &&
                       (*samples)[0] <= uint64_t(rect[0]) + rect[2] + halo &&
                       (*samples)[1] <= uint64_t(rect[1]) + rect[3] + halo;
        }
        if (!changed) {
            ++it;
            continue;
        }
        stats.bytes -= it->second.bytes;
        Release(renderer, it->second);
        it = pages.erase(it);
    }
}
const GpuTerrainPage &
RenderPages::Acquire(Renderer &renderer, const Source &source, const RenderPageKey &key,
                     const std::function<std::shared_ptr<const Chunk>()> &mesh) {
    if (auto found = pages.find(key); found != pages.end()) {
        ++stats.hits;
        found->second.used = ++clock;
        return found->second;
    }
    const auto start = std::chrono::steady_clock::now();
    ++stats.misses;
    const auto materialKey = Region(source, key);
    auto image = BuildRenderPage(source, key, !weights.count(materialKey));
    auto geometry = mesh ? mesh() : nullptr;
    const size_t imageWeightBytes =
        image.weights.levels.empty() ? 0 : image.weights.levels[0].bytes.size();
    const size_t bytes = image.Bytes() - imageWeightBytes +
                         (geometry ? geometry->mesh.vertices.size() * sizeof(MeshVertex) +
                                         geometry->mesh.indices.size() * sizeof(uint32_t)
                                   : 0);
    const size_t weightBytes = size_t(materialKey[2] + 1) * (materialKey[3] + 1) * 4;
    if (bytes + weightBytes > stats.budget)
        throw std::runtime_error("terrain render page exceeds its GPU budget");
    // Terrain draws are immediate between Begin/EndTerrainMaterial; flushing
    // here also prevents a caller from deleting a queued resource on eviction.
    renderer.EndMeshBatch();
    auto neededWeight = [&] { return weights.count(materialKey) ? size_t(0) : weightBytes; };
    while (stats.bytes + bytes + neededWeight() > stats.budget) {
        auto oldest =
            std::min_element(pages.begin(), pages.end(), [](const auto &a, const auto &b) {
                return a.second.used < b.second.used;
            });
        stats.bytes -= oldest->second.bytes;
        Release(renderer, oldest->second);
        pages.erase(oldest);
        ++stats.evictions;
    }
    if (!weights.count(materialKey) && image.weights.levels.empty())
        image = BuildRenderPage(source, key);
    GpuTerrainPage page;
    page.weightKey = materialKey;
    page.used = ++clock;
    page.bytes = bytes;
    page.weightRect = image.weightRect;
    if (key.grid)
        page.heights = renderer.CreateTexture(image.heights);
    if (!weights.count(materialKey)) {
        WeightPage weight;
        weight.texture = renderer.CreateTexture(image.weights);
        weight.rect = image.weightRect;
        weight.bytes = image.weights.levels[0].bytes.size();
        stats.bytes += weight.bytes;
        weights.emplace(materialKey, std::move(weight));
    }
    auto &weight = weights.at(materialKey);
    ++weight.references;
    page.weights = weight.texture;
    if (geometry)
        page.mesh = renderer.CreateMesh(geometry->mesh);
    stats.bytes += bytes;
    stats.peakBytes = std::max(stats.peakBytes, stats.bytes);
    prepareMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    worstPrepareMs = std::max(worstPrepareMs, prepareMs);
    return pages.emplace(key, std::move(page)).first->second;
}
} // namespace TerrainV2
