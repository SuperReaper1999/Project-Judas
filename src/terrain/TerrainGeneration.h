#pragma once
#include "TerrainSource.h"
#include <string_view>

namespace TerrainV2 {
struct LandscapeRecipe {
    enum class Preset { Flat, Highlands, Island };
    Preset preset = Preset::Highlands;
    unsigned samples = 129, seed = 7;
    float spacing = 1, base = 2, height = 32;
};
// Generation and image resampling produce ordinary editable source data.
// Neither owns rendering, physics, files, or a second procedural runtime.
bool Generate(const LandscapeRecipe &, Source &, std::string &);
// Grayscale image row zero maps to negative local Z. Resample deliberately to
// the recipe's chosen lattice; preserve 16-bit input instead of an RGBA8 detour.
bool ImportHeightmap(std::string_view encoded, const LandscapeRecipe &, Source &, std::string &);
} // namespace TerrainV2
