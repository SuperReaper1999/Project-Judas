#pragma once
#include "TerrainSource.h"
#include <fstream>
#include <string_view>

namespace TerrainV2 {
struct SamplePage {
    unsigned x = 0, z = 0, width = 0, height = 0;
    std::vector<float> heights;
    std::vector<glm::vec4> weights;
};
struct PageRecord {
    unsigned x = 0, z = 0, width = 0, height = 0;
    uint64_t offset = 0;
    uint32_t bytes = 0;
    std::string digest;
};
struct AssetIndex {
    std::string fingerprint; // digest of the validated header and page digests
    Source metadata;         // deliberately contains no sample arrays
    std::vector<PageRecord> pages;
};
// Versioned, deterministic little-endian source pages, independent of graphics,
// collision products and script execution. Each page has an integrity digest.
bool EncodeAsset(const Source &, std::vector<uint8_t> &, std::string &);
bool DecodeAsset(std::string_view, Source &, std::string &);
bool IsAsset(std::string_view);
// One owning worker per reader. Keeps its opened file snapshot, so publishing
// a replacement path cannot mix one revision's header with another's pages.
class AssetPages {
  public:
    bool Open(const std::string &, std::string &);
    const AssetIndex &Index() const { return index; }
    bool ReadPage(size_t, SamplePage &, std::string &);
    bool ReadAll(Source &, std::string &);

  private:
    std::ifstream file;
    AssetIndex index;
};
bool LoadAsset(const std::string &, Source &, std::string &);
} // namespace TerrainV2
