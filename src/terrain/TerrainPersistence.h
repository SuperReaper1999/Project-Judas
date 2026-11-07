#pragma once
#include <string_view>
namespace TerrainV2 {
inline constexpr std::string_view ReferenceMagic("JTRREF2\0", 8);
inline bool IsReferenceSnapshot(std::string_view bytes) {
    return bytes.substr(0, 8) == ReferenceMagic;
}
} // namespace TerrainV2
