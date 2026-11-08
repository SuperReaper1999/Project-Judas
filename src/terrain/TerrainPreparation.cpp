#include "TerrainPreparation.h"
#include "CollisionAsset.h"
#include "NavigationSystem.h"
#include "SceneFingerprint.h"
#include "TerrainAsset.h"
#include "TerrainCollision.h"
#include "TerrainMesher.h"
#include "TerrainNavigation.h"
#include "TerrainSamples.h"
#include <chrono>
#include <cstring>

namespace TerrainV2 {
bool PrepareGeometry(Preparation &work, const std::vector<ChunkKey> &keys,
                     const std::function<bool()> &cancelled) try {
    const auto start = std::chrono::steady_clock::now();
    if (!work.source) {
        work.error = "missing immutable terrain source";
        return false;
    }
    if (!work.source->storage || work.source->storage->Changes().size() > MaximumRasterChanges) {
        Source paged;
        if (!SampleStorage::Spill(*work.source, 8 * 1024 * 1024, paged, work.error))
            return false;
        work.source = std::make_shared<const Source>(std::move(paged));
    }
    // The published revision is recoverable before any cache entry can leave
    // memory. Failure leaves the previous world and its journal untouched.
    if (!work.source->storage->Persist(*work.source, work.error))
        return false;
    if (!Validate(*work.source, work.error))
        return false;
    if (!work.lod)
        work.lod = std::make_shared<const HeightLod>(*work.source);
    if (!work.cache)
        work.cache = std::make_shared<GeometryCache>(64 * 1024 * 1024);
    for (auto key : keys) {
        if (cancelled && cancelled()) {
            work.error = "cancelled";
            return false;
        }
        auto recipe = PrepareRecipe(*work.source, key, work.cache, work.error);
        if (!recipe)
            return false;
        auto page = recipe->Acquire();
        PreparedChunk p;
        p.chunk.key = key;
        p.chunk.anchor = page->chunk.anchor;
        p.chunk.volumetric = page->chunk.volumetric;
        p.triangles = page->triangles;
        p.collision = CollisionPage(recipe, *page);
        p.recipe = std::move(recipe);
        work.chunks.push_back(std::move(p));
    }
    work.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
} catch (const std::exception &e) {
    work.error = e.what();
    return false;
}
size_t SourcePayloadBytes(const Source &source, const HeightLod *lod) {
    size_t bytes = sizeof(source) + source.heights.size() * sizeof(float) +
                   source.weights.size() * sizeof(glm::vec4) + source.edits.size() * sizeof(Edit);
    if (source.storage)
        bytes += source.storage->Changes().size() *
                 (sizeof(RasterSample) + sizeof(uint32_t) + 4 * sizeof(void *));
    for (const auto &id : source.layerTextures)
        bytes += id.size();
    bytes += source.materialDetail.normalTexture.size();
    if (source.chart)
        bytes += source.chart->rim.size() * sizeof(float) + source.chart->surfaceFingerprint.size();
    if (lod)
        bytes += lod->Bytes();
    return bytes;
}
size_t PreparedChunk::PayloadBytes() const {
    return sizeof(*this) + (recipe ? recipe->Bytes() : 0) +
           (collision ? sizeof(CollisionAsset) : 0);
}
size_t Preparation::PayloadBytes() const {
    size_t bytes = sizeof(*this);
    if (source)
        bytes += SourcePayloadBytes(*source, lod.get());
    for (const auto &chunk : chunks)
        bytes += chunk.PayloadBytes();
    if (source && source->storage)
        bytes += source->storage->Statistics().bytes;
    if (cache)
        bytes += cache->Statistics().bytes;
    bytes += NavigationPayloadBytes(navigation.get());
    return bytes;
}
bool PrepareAsset(const std::string &path, const std::string &identity, PreparedAsset &output,
                  std::string &error, const std::function<bool()> &cancelled,
                  const NavigationInput *navigation) try {
    AssetPages pages;
    Source source;
    if (!pages.Open(path, error) || !SampleStorage::Open(path, 8 * 1024 * 1024, source, error))
        return false;
    if (size_t(source.ChunksX()) * source.ChunksZ() > 1024) {
        error = "terrain active chunk budget exceeds 1024";
        return false;
    }
    PreparedAsset next;
    next.asset = identity;
    next.fingerprint = pages.Index().fingerprint;
    next.geometry.source = std::make_shared<const Source>(std::move(source));
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < next.geometry.source->ChunksZ(); ++z)
        for (unsigned x = 0; x < next.geometry.source->ChunksX(); ++x)
            keys.push_back({x, z});
    if (!PrepareGeometry(next.geometry, keys, cancelled)) {
        error = next.geometry.error;
        return false;
    }
    if (navigation) {
        const auto start = std::chrono::steady_clock::now();
        auto input = *navigation;
        for (const auto &part : next.geometry.chunks)
            input.chunks[part.chunk.key] = {part.chunk.anchor, part.collision};
        if (!PrepareNavigation(input, next.geometry.navigation, error, cancelled))
            return false;
        next.geometry.navigationCache =
            NavigationSystem::PrepareSurface(next.geometry.navigation, error, cancelled);
        if (!next.geometry.navigationCache)
            return false;
        next.geometry.milliseconds +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
    }
    output = std::move(next);
    error.clear();
    return true;
} catch (const std::exception &e) {
    error = e.what();
    return false;
}
} // namespace TerrainV2
