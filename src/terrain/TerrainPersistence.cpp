#include "TerrainPersistence.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "TerrainAsset.h"
#include "TerrainMetadata.h"
#include "TerrainRuntime.h"
#include <algorithm>
#include <set>
namespace TerrainV2 {
namespace {
bool SameLayout(const Source &a, const Source &b) {
    return a.samplesX == b.samplesX && a.samplesZ == b.samplesZ && a.chunkCells == b.chunkCells &&
           a.spacing == b.spacing && a.bottom == b.bottom && a.top == b.top && a.chart == b.chart;
}
bool ReferenceSamples(AssetPages &pages, const Source *cached, const RasterChanges &oldChanges,
                      const RasterChanges &changes, Source &base, std::string &error) {
    if (!cached)
        return pages.ReadAll(base, error);
    // A verified live base already owns every unchanged sample. An override in
    // BOTH versions will be replaced by the new complete sample. Only reverted
    // old overrides require reading original native pages again.
    base.heights = cached->heights;
    base.weights = cached->weights;
    const auto &index = pages.Index();
    const auto width = index.pages.front().width, height = index.pages.front().height;
    const auto columns = (index.metadata.samplesX + width - 1) / width;
    std::set<size_t> needed;
    for (const auto &[sample, _] : oldChanges)
        if (!changes.count(sample)) {
            const auto x = sample % index.metadata.samplesX, z = sample / index.metadata.samplesX;
            needed.insert(size_t(z / height) * columns + x / width);
        }
    for (auto page : needed) {
        SamplePage original;
        if (!pages.ReadPage(page, original, error))
            return false;
        for (unsigned z = 0; z < original.height; ++z) {
            const auto to = size_t(original.z + z) * index.metadata.samplesX + original.x;
            const auto from = size_t(z) * original.width;
            std::copy_n(original.heights.begin() + from, original.width, base.heights.begin() + to);
            std::copy_n(original.weights.begin() + from, original.width, base.weights.begin() + to);
        }
    }
    return true;
}
std::vector<ChunkKey> RestoreChunks(const Source *before, const Source &after) {
    std::set<ChunkKey> dirty;
    if (!before || !SameLayout(*before, after)) {
        for (unsigned z = 0; z < after.ChunksZ(); ++z)
            for (unsigned x = 0; x < after.ChunksX(); ++x)
                dirty.insert({x, z});
    } else {
        auto add = [&](const Edit &edit) {
            const auto affected = Affected(after, edit);
            dirty.insert(affected.begin(), affected.end());
        };
        // Height interpolation and normals affect the neighbouring samples too.
        const float halfX = (after.samplesX - 1) * after.spacing * .5f,
                    halfZ = (after.samplesZ - 1) * after.spacing * .5f;
        for (size_t i = 0; i < after.heights.size(); ++i)
            if (before->heights[i] != after.heights[i])
                add({Edit::Kind::Remove,
                     {float(i % after.samplesX) * after.spacing - halfX, 0,
                      float(i / after.samplesX) * after.spacing - halfZ},
                     after.spacing});
        // Rebuild both the removed and newly affected volume bands. The journal
        // order remains authoritative; this is only dependency invalidation.
        auto sameEdit = [](const Edit &a, const Edit &b) {
            return a.kind == b.kind && a.point == b.point && a.radius == b.radius;
        };
        size_t first = 0;
        while (first < before->edits.size() && first < after.edits.size() &&
               sameEdit(before->edits[first], after.edits[first]))
            ++first;
        for (size_t i = first; i < before->edits.size(); ++i)
            add(before->edits[i]);
        for (size_t i = first; i < after.edits.size(); ++i)
            add(after.edits[i]);
    }
    std::vector<ChunkKey> keys(dirty.begin(), dirty.end());
    std::sort(keys.begin(), keys.end(),
              [](auto a, auto b) { return a.z == b.z ? a.x < b.x : a.z < b.z; });
    return keys;
}
} // namespace
void Runtime::BindAsset(std::string id, std::string fingerprint) {
    if (pending) {
        pending->baseAsset = std::move(id);
        pending->baseFingerprint = std::move(fingerprint);
    } else {
        baseAsset = std::move(id);
        baseFingerprint = std::move(fingerprint);
    }
}
bool Runtime::Capture(std::string &bytes, std::string &error) const {
    if (pending) {
        error = "terrain edit pending";
        return false;
    }
    if (!source) {
        bytes.clear();
        return true;
    }
    if (!baseAsset.empty()) {
        try {
            SaveArchive archive;
            auto metadata = SerializeMetadata(*source);
            auto id = baseAsset, digest = baseFingerprint;
            archive(id, digest, metadata);
            if (!raster.empty()) {
                auto changes = raster;
                archive(changes);
            }
            if (archive.bytes.size() + 8 > 8 * 1024 * 1024)
                throw std::runtime_error("terrain reference exceeds the save string budget");
            bytes =
                std::string(raster.empty() ? ReferenceMagic : RasterReferenceMagic) + archive.bytes;
            error.clear();
            return true;
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    }
    std::vector<uint8_t> encoded;
    if (!EncodeAsset(*source, encoded, error))
        return false;
    if (encoded.size() > 8 * 1024 * 1024) {
        error = "terrain snapshot exceeds the save string budget; split editable terrain into "
                "streamed regions";
        return false;
    }
    bytes.assign(reinterpret_cast<const char *>(encoded.data()), encoded.size());
    return true;
}
bool Runtime::Restore(const std::string &bytes, std::string &error, bool asynchronous) {
    if (pending) {
        error = "terrain restore while edit pending";
        return false;
    }
    Source next;
    std::string assetId, digest;
    RasterChanges changes;
    if (IsReferenceSnapshot(bytes)) {
        try {
            SaveArchive archive(bytes.substr(ReferenceMagic.size()));
            std::string metadata;
            archive(assetId, digest, metadata);
            if (bytes.compare(0, 8, RasterReferenceMagic) == 0)
                archive(changes);
            archive.Finish();
            auto *assets = world.Resources() ? world.Resources()->Assets() : nullptr;
            auto *record = assets ? assets->Find(assetId) : nullptr;
            if (!record || record->missing || record->type != AssetType::Terrain)
                throw std::runtime_error(
                    "saved terrain base asset is missing or has the wrong type");
            AssetPages pages;
            Source base;
            if (!pages.Open(record->path, error) || pages.Index().fingerprint != digest)
                throw std::runtime_error(
                    "saved terrain base has changed; an explicit content migration is required");
            if (!ParseMetadata(metadata, next, error))
                throw std::runtime_error(error);
            if (!SameLayout(next, pages.Index().metadata))
                throw std::runtime_error(
                    "saved terrain base lattice does not match its immutable asset");
            const bool reusable = source && baseAsset == assetId && baseFingerprint == digest &&
                                  SameLayout(*source, next);
            if (!ReferenceSamples(pages, reusable ? source.get() : nullptr, raster, changes, base,
                                  error))
                throw std::runtime_error(error);
            next.heights = std::move(base.heights);
            next.weights = std::move(base.weights);
            for (const auto &[index, sample] : changes) {
                if (index >= next.heights.size())
                    throw std::runtime_error("saved terrain sample index is outside its base");
                next.heights[index] = sample.height;
                next.weights[index] = sample.weights;
            }
            if (!Validate(next, error))
                throw std::runtime_error(error);
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    } else if (!(IsAsset(bytes) ? DecodeAsset(bytes, next, error) : Parse(bytes, next, error)))
        return false;
    const bool replaceAll = !source || !SameLayout(*source, next);
    auto keys = RestoreChunks(source.get(), next);
    if (!Submit(std::make_shared<const Source>(std::move(next)), std::move(keys), error,
                replaceAll))
        return false;
    BindAsset(std::move(assetId), std::move(digest));
    pending->raster = std::move(changes);
    if (!asynchronous) {
        Wait();
        error = status.error;
        return error.empty() && status.ready;
    }
    return true;
}
} // namespace TerrainV2
