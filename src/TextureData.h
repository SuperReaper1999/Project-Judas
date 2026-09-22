#pragma once

#include <cstdint>
#include <vector>

// Judas-owned CPU-side texture representation — what src/TextureLoader.h
// produces and what Renderer::CreateTexture consumes to upload a GPU
// texture object. Mirrors MeshData's role for meshes: nothing above this
// point needs to know stb_image exists.
//
// Always RGBA/8-bit-per-channel regardless of the source image's own
// channel count (see TextureLoader.cpp) — sensible, uniform channel
// handling without a matrix of GPU upload formats to support for one
// texture.
struct TextureData {
    int width = 0;
    int height = 0;
    static constexpr int kChannels = 4;  // always RGBA — see above
    std::vector<std::uint8_t> pixels;    // width * height * kChannels bytes
};
