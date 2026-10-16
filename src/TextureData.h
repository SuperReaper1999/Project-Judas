#pragma once

#include <cstdint>
#include <vector>

// Judas-owned CPU-side texture representation — what src/TextureLoader.h
// produces and what Renderer::CreateTexture consumes to upload a GPU
// texture object. Mirrors MeshData's role for meshes: nothing above this
// point needs to know stb_image exists.
//
// Source images decode to RGBA8 pixels. Cooked images carry a complete
// prefiltered RGBA8, BC1 or BC3 mip chain in levels instead. The renderer
// expands block formats on devices without the corresponding GPU support.
enum class TextureStorage : unsigned { Rgba8, BC1, BC3 };
struct TextureMip {int width=0,height=0;std::vector<std::uint8_t> bytes;};
struct TextureData {
    int width = 0;
    int height = 0;
    static constexpr int kChannels = 4;  // decoded source pixel channels
    std::vector<std::uint8_t> pixels;    // width * height * kChannels bytes
    TextureStorage storage=TextureStorage::Rgba8;
    std::vector<TextureMip> levels; // prefiltered full chain, row zero remains bottom
    bool HasImage()const{return !pixels.empty()||!levels.empty();}
};
