#include "TerrainPersistence.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "TerrainAsset.h"
#include "TerrainMetadata.h"
#include "TerrainRuntime.h"
namespace TerrainV2 {
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
            if (!pages.ReadAll(base, error) || !ParseMetadata(metadata, next, error))
                throw std::runtime_error(error);
            if (next.samplesX != base.samplesX || next.samplesZ != base.samplesZ ||
                next.chunkCells != base.chunkCells || next.spacing != base.spacing ||
                next.bottom != base.bottom || next.top != base.top)
                throw std::runtime_error(
                    "saved terrain base lattice does not match its immutable asset");
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
    if (pending) {
        error = "terrain restore while edit pending";
        return false;
    }
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < next.ChunksZ(); ++z)
        for (unsigned x = 0; x < next.ChunksX(); ++x)
            keys.push_back({x, z});
    if (!Submit(std::make_shared<const Source>(std::move(next)), std::move(keys), error, true))
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
