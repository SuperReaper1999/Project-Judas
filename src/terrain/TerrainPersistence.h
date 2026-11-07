#pragma once
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <map>
#include <string_view>
namespace TerrainV2 {
inline constexpr std::string_view ReferenceMagic("JTRREF2\0", 8);
inline constexpr std::string_view RasterReferenceMagic("JTRREF3\0", 8);
inline bool IsReferenceSnapshot(std::string_view bytes) {
    return bytes.substr(0, 8) == ReferenceMagic || bytes.substr(0, 8) == RasterReferenceMagic;
}
// Exact changed samples relative to an immutable native base. Sorted indices
// give deterministic saves; repeated dabs replace values rather than grow a log.
struct RasterSample {
    float height = 0;
    glm::vec4 weights{1, 0, 0, 0};
    template <class Archive> void Save(Archive &archive) { archive(height, weights); }
};
using RasterChanges = std::map<uint32_t, RasterSample>;
inline constexpr size_t MaximumRasterChanges = 65536;
} // namespace TerrainV2
