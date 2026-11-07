#include "TerrainPreview.h"
#include "AssetDatabase.h"
#include "TerrainLod.h"
#include "TerrainMesher.h"
#include "TextureLoader.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace TerrainV2 {
bool BuildPreviewTexture(const Source &source, const AssetDatabase *assets, TextureData &output,
                         std::string &error) {
    std::array<TextureData, 4> textures;
    for (unsigned i = 0; i < 4; ++i)
        if (!source.layerTextures[i].empty()) {
            const auto *asset = assets ? assets->Find(source.layerTextures[i]) : nullptr;
            if (!asset || asset->type != AssetType::Texture || asset->missing) {
                error = "terrain preview missing material texture: " + source.layerTextures[i];
                return false;
            }
            std::ifstream f(asset->path, std::ios::binary | std::ios::ate);
            if (!f || f.tellg() < 0 || f.tellg() > 16 * 1024 * 1024) {
                error = "terrain material texture exceeds source byte budget";
                return false;
            }
            std::string bytes(size_t(f.tellg()), '\0');
            f.seekg(0);
            f.read(bytes.data(), bytes.size());
            if (!f || !DecodeTextureFromMemory(reinterpret_cast<const uint8_t *>(bytes.data()),
                                               bytes.size(), asset->path, textures[i], error))
                return false;
        }
    TextureData image;
    image.width = image.height = 512;
    image.pixels.resize(size_t(image.width) * image.height * 4);
    const float width = (source.samplesX - 1) * source.spacing,
                depth = (source.samplesZ - 1) * source.spacing;
    for (int z = 0; z < image.height; ++z)
        for (int x = 0; x < image.width; ++x) {
            float u = float(x) / (image.width - 1), v = float(z) / (image.height - 1);
            const auto w = Weights(source, (u - .5f) * width, (v - .5f) * depth);
            glm::vec3 colour(0);
            for (unsigned k = 0; k < 4; ++k) {
                glm::vec3 sample(1);
                const auto &t = textures[k];
                if (!t.pixels.empty()) {
                    float tx = (u - .5f) * width * .25f, tz = (v - .5f) * depth * .25f;
                    unsigned ix = std::min(unsigned((tx - std::floor(tx)) * t.width),
                                           unsigned(t.width - 1)),
                             iz = std::min(unsigned((tz - std::floor(tz)) * t.height),
                                           unsigned(t.height - 1));
                    const auto index = (size_t(iz) * t.width + ix) * 4;
                    for (unsigned c = 0; c < 3; ++c)
                        sample[c] = std::pow(t.pixels[index + c] / 255.f, 2.2f);
                }
                colour += sample * source.layerColours[k] * w[k];
            }
            const auto index = (size_t(z) * image.width + x) * 4;
            for (unsigned c = 0; c < 3; ++c)
                image.pixels[index + c] =
                    uint8_t(std::lround(std::pow(std::clamp(colour[c], 0.f, 1.f), 1 / 2.2f) * 255));
            image.pixels[index + 3] = 255;
        }
    output = std::move(image);
    error.clear();
    return true;
}
std::string EncodePreviewTga(const TextureData &t) {
    std::string out(18, '\0');
    out[2] = 2;
    out[12] = char(t.width & 255);
    out[13] = char(t.width >> 8);
    out[14] = char(t.height & 255);
    out[15] = char(t.height >> 8);
    out[16] = 32;
    out[17] = 8;
    for (size_t i = 0; i < t.pixels.size(); i += 4) {
        out += char(t.pixels[i + 2]);
        out += char(t.pixels[i + 1]);
        out += char(t.pixels[i]);
        out += char(t.pixels[i + 3]);
    }
    return out;
}
bool BuildPreviewMesh(const Source &source, MeshData &output, std::string &error) {
    if (!Validate(source, error))
        return false;
    std::set<ChunkKey> volume;
    for (const auto &edit : source.edits)
        for (auto key : Affected(source, edit))
            volume.insert(key);
    HeightLod lod(source);
    const float extent = std::max(source.samplesX - 1, source.samplesZ - 1) * source.spacing;
    const auto patches = source.heights.size() <= 32768
                             ? lod.Select({}, volume)
                             : lod.Select({{{0, extent * 4, extent * 4}, 600, 0}}, volume);
    std::vector<ChunkKey> keys(volume.begin(), volume.end());
    std::vector<Chunk> chunks;
    if (!BuildChunks(source, keys, chunks, error))
        return false;
    MeshData mesh;
    auto append = [&](const MeshData &data, glm::vec3 anchor) {
        const auto base = unsigned(mesh.vertices.size());
        for (auto v : data.vertices) {
            v.position += anchor;
            v.uv = v.uv1;
            mesh.vertices.push_back(v);
        }
        for (auto i : data.indices)
            mesh.indices.push_back(base + i);
    };
    const auto baseGrid = HeightGrid(source.chunkCells);
    const glm::vec2 half((source.samplesX - 1) * source.spacing * .5f,
                         (source.samplesZ - 1) * source.spacing * .5f);
    for (const auto &patch : patches) {
        const auto k = patch.key;
        if (k.level == 0 && volume.count({k.x, k.z}))
            continue;
        const unsigned stride = 1u << k.level;
        auto grid = baseGrid;
        for (auto &v : grid.vertices) {
            const auto uv = v.uv;
            glm::vec2 sample(float(k.x * source.chunkCells), float(k.z * source.chunkCells));
            sample += uv * float(stride * source.chunkCells);
            float h = LodHeight(source, sample, stride);
            if (uv.x == 0)
                h = LodHeight(source, sample, unsigned(patch.edgeStrides.x));
            if (uv.x == 1)
                h = LodHeight(source, sample, unsigned(patch.edgeStrides.y));
            if (uv.y == 0)
                h = LodHeight(source, sample, unsigned(patch.edgeStrides.z));
            if (uv.y == 1)
                h = LodHeight(source, sample, unsigned(patch.edgeStrides.w));
            auto p = sample * source.spacing - half;
            v.position = {p.x, h, p.y};
            const float d = source.spacing;
            v.normal = glm::normalize(
                glm::vec3(Height(source, p.x - d, p.y) - Height(source, p.x + d, p.y), 2 * d,
                          Height(source, p.x, p.y - d) - Height(source, p.x, p.y + d)));
            v.uv1 = sample / glm::vec2(source.samplesX - 1, source.samplesZ - 1);
        }
        append(grid, {});
    }
    for (const auto &c : chunks)
        append(c.mesh, c.anchor);
    if (mesh.indices.size() / 3 > 1048576) {
        error =
            "terrain editor preview exceeds one million triangles; use smaller editable regions";
        return false;
    }
    mesh.primitives.emplace_back(0, unsigned(mesh.indices.size()), -1, "terrain preview");
    output = std::move(mesh);
    error.clear();
    return true;
}
} // namespace TerrainV2
