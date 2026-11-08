#include "TerrainGeometryPages.h"
#include "SceneFingerprint.h"
#include "TerrainCollision.h"
#include "TerrainMesher.h"
#include "TerrainSamples.h"
#include <algorithm>
#include <atomic>
#include <cstring>

namespace TerrainV2 {
namespace {
std::atomic<uint64_t> identities{1};
std::string GeometryFingerprint(const Chunk &chunk) {
    std::string bytes = "Judas.Terrain.ChunkGeometry.1";
    bytes.reserve(bytes.size() + chunk.mesh.vertices.size() * 12 + chunk.mesh.indices.size() * 4 +
                  16);
    auto integer = [&](uint32_t v) {
        for (unsigned b = 0; b < 4; ++b)
            bytes.push_back(char(v >> (8 * b)));
    };
    integer(chunk.key.x);
    integer(chunk.key.z);
    integer(uint32_t(chunk.mesh.vertices.size()));
    integer(uint32_t(chunk.mesh.indices.size()));
    for (const auto &vertex : chunk.mesh.vertices)
        for (unsigned axis = 0; axis < 3; ++axis) {
            float value = vertex.position[axis];
            if (value == 0)
                value = 0;
            uint32_t bits;
            std::memcpy(&bits, &value, 4);
            integer(bits);
        }
    for (auto index : chunk.mesh.indices)
        integer(index);
    return SceneFingerprintSha256(bytes);
}
} // namespace
size_t GeometryPage::Bytes() const {
    const auto &c = collision;
    size_t bytes =
        sizeof(*this) + chunk.mesh.vertices.capacity() * sizeof(MeshVertex) +
        chunk.mesh.indices.capacity() * sizeof(uint32_t) +
        c.vertices.capacity() * sizeof(glm::dvec3) + c.faces.capacity() * sizeof(CollisionFace) +
        c.edges.capacity() * sizeof(c.edges[0]) + c.satEdges.capacity() * sizeof(uint32_t) +
        c.nodes.capacity() * sizeof(CollisionNode) + c.order.capacity() * sizeof(uint32_t) +
        c.polygons.capacity() * sizeof(CollisionPolygon);
    for (const auto &p : c.polygons)
        bytes += p.vertices.capacity() * sizeof(uint32_t);
    return bytes;
}
std::shared_ptr<const GeometryPage> GeometryRecipe::Acquire() const {
    return cache->Get(identity, [&] {
        auto page = stored ? stored->Load() : Build();
        return std::make_pair(page, page->Bytes());
    });
}
std::shared_ptr<const GeometryPage> GeometryRecipe::Build() const {
    auto page = std::make_shared<GeometryPage>();
    std::string error;
    if (!ExtractValidatedChunk(source, key, page->chunk, error))
        throw std::runtime_error("terrain geometry page: " + error);
    page->triangles = page->chunk.mesh.indices.size() / 3;
    if (!page->chunk.mesh.indices.empty()) {
        const bool structured =
            !page->chunk.volumetric && !source.chart &&
            CookHeightfieldCollision(page->chunk.mesh, source.chunkCells, page->collision, error);
        if (!structured && !CookCollision(page->chunk.mesh, {}, page->collision, error))
            throw std::runtime_error("terrain collision page: " + error);
        page->collision.sourceFingerprint = GeometryFingerprint(page->chunk);
        if (!fingerprint.empty() && page->collision.sourceFingerprint != fingerprint)
            throw std::runtime_error("terrain collision page changed within an immutable revision");
    }
    // Ordinary visual grids do not consume the extraction mesh. Retain
    // only the exact cook; caves and curved patches still need draw data.
    if (!page->chunk.volumetric && !source.chart)
        page->chunk.mesh = {};
    return page;
}
size_t GeometryRecipe::Bytes() const {
    return sizeof(*this) + source.edits.capacity() * sizeof(Edit) +
           (source.storage
                ? source.storage->Changes().size() * (sizeof(RasterSample) + 4 * sizeof(void *))
                : 0) +
           (source.chart ? source.chart->rim.capacity() * sizeof(float) : 0);
}
std::shared_ptr<GeometryRecipe> PrepareRecipe(const Source &input, ChunkKey key,
                                              std::shared_ptr<GeometryCache> cache,
                                              std::string &error) {
    if (!input.storage) {
        error = "terrain geometry recipe requires native sample storage";
        return {};
    }
    auto recipe = std::make_shared<GeometryRecipe>();
    recipe->identity = identities.fetch_add(1);
    recipe->source = input;
    recipe->key = key;
    recipe->cache = std::move(cache);
    const unsigned x = key.x * input.chunkCells, z = key.z * input.chunkCells;
    recipe->source.storage =
        input.storage->Window(x > 2 ? x - 2 : 0, z > 2 ? z - 2 : 0,
                              std::min(input.samplesX - 1, x + input.chunkCells + 2),
                              std::min(input.samplesZ - 1, z + input.chunkCells + 2));
    auto &edits = recipe->source.edits;
    edits.erase(std::remove_if(edits.begin(), edits.end(),
                               [&](const Edit &e) { return !Touches(input, key, e); }),
                edits.end());
    edits.shrink_to_fit();
    auto page = recipe->Build();
    recipe->fingerprint = page->collision.sourceFingerprint;
    if (page->chunk.volumetric || input.chart)
        recipe->stored = GeometrySpill::Store(*page, recipe->identity, recipe->cache->spill);
    recipe->cache->Put(recipe->identity, page, page->Bytes());
    return recipe;
}
std::shared_ptr<CollisionAsset> CollisionPage(const std::shared_ptr<GeometryRecipe> &recipe,
                                              const GeometryPage &page) {
    if (page.collision.faces.empty())
        return {};
    auto result = std::make_shared<CollisionAsset>();
    const auto &c = page.collision;
    result->twoSided = c.twoSided;
    result->sourceFingerprint = c.sourceFingerprint;
    result->settingsFingerprint = c.settingsFingerprint;
    result->minimum = c.minimum;
    result->maximum = c.maximum;
    result->pageRadius = 0;
    for (auto vertex : c.vertices)
        result->pageRadius = std::max(result->pageRadius, glm::length(vertex));
    result->acquirePage = [recipe] {
        auto product = recipe->Acquire();
        return std::shared_ptr<const CollisionAsset>(product, &product->collision);
    };
    return result;
}
} // namespace TerrainV2
