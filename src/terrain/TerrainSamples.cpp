#include "TerrainSamples.h"
#include "SaveStorage.h"
#include "TerrainAsset.h"
#include "TerrainMetadata.h"
#include "TerrainTemporary.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace TerrainV2 {
namespace {
std::atomic<uint64_t> sequence{1};
} // namespace
struct SampleStorage::Backing {
    uint64_t identity = sequence.fetch_add(1);
    // Members are destroyed in reverse order. Close the native file before
    // deleting its private directory (Windows cannot unlink an open reader).
    std::shared_ptr<TemporaryDirectory> temporary;
    AssetPages file;
    std::shared_ptr<PageCache<SamplePage>> cache;
    std::unique_ptr<SaveStorage> journal;
    mutable std::mutex journalMutex;
};
bool SampleStorage::Open(const std::string &path, size_t budget, Source &output,
                         std::string &error) {
    try {
        if (budget < 128 * 1024 || budget > 64 * 1024 * 1024)
            throw std::runtime_error("terrain sample budget must be 128 KiB..64 MiB");
        auto store = std::make_shared<SampleStorage>();
        store->backing = std::make_shared<Backing>();
        if (!store->backing->file.Open(path, error))
            return false;
        store->backing->cache = std::make_shared<PageCache<SamplePage>>(budget);
        auto source = store->backing->file.Index().metadata;
        source.storage = store;
        output = std::move(source);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool SampleStorage::Spill(const Source &source, size_t budget, Source &output, std::string &error) {
    if (source.storage && source.storage->Changes().size() <= MaximumRasterChanges) {
        output = source;
        return true;
    }
    try {
        auto directory = std::make_shared<TemporaryDirectory>();
        const auto path = directory->path / "source.judasterrain";
        std::vector<uint8_t> bytes;
        if (!EncodeAsset(source, bytes, error))
            return false;
        {
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            file.flush();
            if (!file)
                throw std::runtime_error("cannot write private terrain base pages");
        }
        Source next;
        if (!Open(path.string(), budget, next, error))
            return false;
        auto store = std::const_pointer_cast<SampleStorage>(next.storage);
        store->backing->temporary = std::move(directory);
        output = std::move(next);
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
std::shared_ptr<const SampleStorage> SampleStorage::Base() const {
    auto next = std::make_shared<SampleStorage>();
    next->backing = backing;
    return next;
}
RasterSample SampleStorage::Read(size_t index) const {
    const auto &data = backing->file.Index();
    if (index >= size_t(data.metadata.samplesX) * data.metadata.samplesZ)
        throw std::out_of_range("terrain sample outside native pages");
    if (auto changed = changes.find(uint32_t(index)); changed != changes.end())
        return changed->second;
    const auto x = unsigned(index % data.metadata.samplesX),
               z = unsigned(index / data.metadata.samplesX);
    const auto width = data.pages.front().width, height = data.pages.front().height;
    const auto columns = (data.metadata.samplesX + width - 1) / width;
    const size_t pageIndex = size_t(z / height) * columns + x / width;
    // Tight extraction/LOD loops repeatedly read the same immutable native
    // page. A thread-local WEAK cursor avoids a cache lock for each corner
    // sample without pinning anything beyond this Read call or its world.
    struct Cursor {
        uint64_t owner = 0;
        size_t index = 0;
        std::weak_ptr<const SamplePage> page;
    };
    thread_local Cursor cursor;
    std::shared_ptr<const SamplePage> page;
    if (cursor.owner == backing->identity && cursor.index == pageIndex)
        page = cursor.page.lock();
    if (!page) {
        page = backing->cache->Get(pageIndex, [&] {
            auto result = std::make_shared<SamplePage>();
            std::string error;
            if (!backing->file.ReadPage(pageIndex, *result, error))
                throw std::runtime_error("terrain sample page: " + error);
            const auto bytes = sizeof(SamplePage) + result->heights.capacity() * sizeof(float) +
                               result->weights.capacity() * sizeof(glm::vec4);
            return std::make_pair(result, bytes);
        });
        cursor = {backing->identity, pageIndex, page};
    }
    const size_t at = size_t(z - page->z) * page->width + x - page->x;
    return {page->heights[at], page->weights[at]};
}
std::shared_ptr<const SampleStorage> SampleStorage::WithChanges(const RasterChanges &delta) const {
    auto next = std::make_shared<SampleStorage>(*this);
    for (const auto &[index, value] : delta)
        next->changes[index] = value;
    if (next->changes.size() > 4 * 1024 * 1024)
        throw std::runtime_error("terrain sample override allocation exceeds 4M samples");
    return next;
}
std::shared_ptr<const SampleStorage> SampleStorage::Window(unsigned x0, unsigned z0, unsigned x1,
                                                           unsigned z1) const {
    auto next = std::make_shared<SampleStorage>();
    next->backing = backing;
    const auto nx = backing->file.Index().metadata.samplesX;
    for (unsigned z = z0; z <= z1; ++z) {
        auto it = changes.lower_bound(uint32_t(size_t(z) * nx + x0));
        const auto end = size_t(z) * nx + x1;
        for (; it != changes.end() && it->first <= end; ++it)
            next->changes.insert(*it);
    }
    return next;
}
CacheStatistics SampleStorage::Statistics() const { return backing->cache->Statistics(); }
std::string SampleStorage::BaseFingerprint() const { return backing->file.Index().fingerprint; }
bool SampleStorage::Matches(unsigned nx, unsigned nz) const {
    const auto &source = backing->file.Index().metadata;
    return source.samplesX == nx && source.samplesZ == nz;
}
bool SampleStorage::Persist(const Source &source, std::string &error) const {
    try {
        std::lock_guard<std::mutex> lock(backing->journalMutex);
        if (!backing->temporary)
            backing->temporary = std::make_shared<TemporaryDirectory>();
        if (!backing->journal)
            backing->journal =
                std::make_unique<SaveStorage>((backing->temporary->path / "journal").string());
        GameSnapshot snapshot;
        snapshot.project = "Judas.Terrain.PrivatePages";
        snapshot.scene = "terrain";
        snapshot.content = BaseFingerprint();
        SaveArchive archive;
        auto metadata = SerializeMetadata(source);
        auto samples = changes;
        archive(metadata, samples);
        snapshot.participants["terrain"] = {1, std::move(archive.bytes)};
        return backing->journal->Write("current", snapshot, error);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
size_t SampleCount(const Source &source) {
    return source.storage ? size_t(source.samplesX) * source.samplesZ : source.heights.size();
}
float HeightSample(const Source &source, size_t index) {
    return source.storage ? source.storage->Read(index).height : source.heights.at(index);
}
glm::vec4 WeightSample(const Source &source, size_t index) {
    return source.storage ? source.storage->Read(index).weights : source.weights.at(index);
}
void SetSamples(Source &source, const RasterChanges &changes) {
    if (source.storage)
        source.storage = source.storage->WithChanges(changes);
    else
        for (const auto &[index, value] : changes) {
            source.heights.at(index) = value.height;
            source.weights.at(index) = value.weights;
        }
}
namespace {
template <class Changed>
void VisitSampleDifferences(const Source &a, const Source &b, bool materials, Changed changed) {
    if (a.storage && a.storage == b.storage)
        return;
    if (a.storage && b.storage && a.storage->BaseFingerprint() == b.storage->BaseFingerprint()) {
        const auto &left = a.storage->Changes(), &right = b.storage->Changes();
        auto i = left.begin(), j = right.begin();
        while (i != left.end() || j != right.end()) {
            const auto index = j == right.end()  ? i->first
                               : i == left.end() ? j->first
                                                 : std::min(i->first, j->first);
            const bool hasLeft = i != left.end() && i->first == index,
                       hasRight = j != right.end() && j->first == index;
            const float x = hasLeft ? i->second.height : HeightSample(a, index),
                        y = hasRight ? j->second.height : HeightSample(b, index);
            if (x != y ||
                (materials && (hasLeft ? i->second.weights : WeightSample(a, index)) !=
                                  (hasRight ? j->second.weights : WeightSample(b, index))))
                changed(index);
            if (hasLeft)
                ++i;
            if (hasRight)
                ++j;
        }
    } else {
        for (size_t i = 0; i < SampleCount(a); ++i)
            if (HeightSample(a, i) != HeightSample(b, i) ||
                (materials && WeightSample(a, i) != WeightSample(b, i)))
                changed(uint32_t(i));
    }
}
} // namespace
std::vector<uint32_t> ChangedHeights(const Source &a, const Source &b) {
    std::vector<uint32_t> changed;
    VisitSampleDifferences(a, b, false, [&](uint32_t index) { changed.push_back(index); });
    return changed;
}
std::optional<glm::uvec4> ChangedSampleBounds(const Source &a, const Source &b) {
    std::optional<glm::uvec4> bounds;
    VisitSampleDifferences(a, b, true, [&](uint32_t index) {
        const unsigned x = index % a.samplesX, z = index / a.samplesX;
        if (!bounds)
            bounds = glm::uvec4(x, z, x, z);
        else {
            (*bounds)[0] = std::min((*bounds)[0], x);
            (*bounds)[1] = std::min((*bounds)[1], z);
            (*bounds)[2] = std::max((*bounds)[2], x);
            (*bounds)[3] = std::max((*bounds)[3], z);
        }
    });
    return bounds;
}
bool SameHeights(const Source &a, const Source &b) {
    if (SampleCount(a) != SampleCount(b))
        return false;
    if (!a.storage && !b.storage)
        return a.heights == b.heights;
    return ChangedHeights(a, b).empty();
}
} // namespace TerrainV2
