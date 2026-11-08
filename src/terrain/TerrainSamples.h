#pragma once
#include "TerrainCache.h"
#include "TerrainPersistence.h"
#include "TerrainSource.h"
#include <memory>

namespace TerrainV2 {
class SampleStorage {
  public:
    static bool Open(const std::string &path, size_t budget, Source &, std::string &);
    static bool Spill(const Source &, size_t budget, Source &, std::string &);
    RasterSample Read(size_t index) const;
    std::shared_ptr<const SampleStorage> Base() const;
    std::shared_ptr<const SampleStorage> WithChanges(const RasterChanges &) const;
    std::shared_ptr<const SampleStorage> Window(unsigned x0, unsigned z0, unsigned x1,
                                                unsigned z1) const;
    CacheStatistics Statistics() const;
    const RasterChanges &Changes() const { return changes; }
    std::string BaseFingerprint() const;
    bool Matches(unsigned nx, unsigned nz) const;
    // Durable private spill before publication. Base pages are immutable;
    // dirty samples/journal never depend on a resident cache entry.
    bool Persist(const Source &, std::string &) const;

  private:
    struct Backing;
    std::shared_ptr<Backing> backing;
    RasterChanges changes;
};
size_t SampleCount(const Source &);
float HeightSample(const Source &, size_t index);
glm::vec4 WeightSample(const Source &, size_t index);
void SetSamples(Source &, const RasterChanges &);
std::vector<uint32_t> ChangedHeights(const Source &, const Source &);
bool SameHeights(const Source &, const Source &);
} // namespace TerrainV2
