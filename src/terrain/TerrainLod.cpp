#include "TerrainLod.h"
#include "TerrainSamples.h"
#include "TextureData.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace TerrainV2 {
float LodHeight(const Source &s, glm::vec2 p, unsigned step) {
    auto a = glm::floor(p / float(step)) * float(step), t = (p - a) / float(step);
    auto height = [&](glm::vec2 v) {
        return HeightSample(s, size_t(std::min(unsigned(v.y), s.samplesZ - 1)) * s.samplesX +
                                   std::min(unsigned(v.x), s.samplesX - 1));
    };
    float h00 = height(a), h10 = height(a + glm::vec2(step, 0)),
          h01 = height(a + glm::vec2(0, step)), h11 = height(a + glm::vec2(step));
    return t.x + t.y <= 1 ? h00 + (h10 - h00) * t.x + (h01 - h00) * t.y
                          : h11 + (h01 - h11) * (1 - t.x) + (h10 - h11) * (1 - t.y);
}
HeightLod::HeightLod(const Source &s)
    : nx(s.ChunksX()), nz(s.ChunksZ()), cells(s.chunkCells), rootLevel(0), spacing(s.spacing) {
    unsigned side = 1;
    while (side < std::max(nx, nz)) {
        side *= 2;
        ++rootLevel;
    }
    if (s.storage) {
        // Bounded worker scratch, released before publication. The hierarchy
        // revisits heights at every level; repeatedly acquiring native pages
        // here is unnecessary. Materials and collision are never expanded.
        Source heights = s;
        heights.heights.resize(SampleCount(s));
        for (size_t i = 0; i < heights.heights.size(); ++i)
            heights.heights[i] = HeightSample(s, i);
        heights.storage.reset();
        Build(heights, {0, 0, rootLevel});
    } else
        Build(s, {0, 0, rootLevel});
}
HeightLod::Node HeightLod::Build(const Source &s, PatchKey key) {
    if (key.x >= nx || key.z >= nz)
        return {};
    const unsigned side = 1u << key.level, step = side;
    Node result;
    result.complete = key.x + side <= nx && key.z + side <= nz;
    result.minimum = std::numeric_limits<float>::max();
    result.maximum = -result.minimum;
    if (key.level) {
        const unsigned half = side / 2;
        for (unsigned z = 0; z < 2; ++z)
            for (unsigned x = 0; x < 2; ++x) {
                auto child = Build(s, {key.x + x * half, key.z + z * half, key.level - 1});
                if (key.x + x * half < nx && key.z + z * half < nz) {
                    result.minimum = std::min(result.minimum, child.minimum);
                    result.maximum = std::max(result.maximum, child.maximum);
                }
            }
    }
    const unsigned x0 = key.x * cells, z0 = key.z * cells,
                   x1 = std::min((key.x + side) * cells, s.samplesX - 1),
                   z1 = std::min((key.z + side) * cells, s.samplesZ - 1);
    for (unsigned z = z0; z <= z1; ++z)
        for (unsigned x = x0; x <= x1; ++x) {
            float h = HeightSample(s, size_t(z) * s.samplesX + x);
            result.minimum = std::min(result.minimum, h);
            result.maximum = std::max(result.maximum, h);
            if (result.complete)
                result.error =
                    std::max(result.error, std::abs(h - LodHeight(s, {float(x), float(z)}, step)));
        }
    nodes.emplace(key, result);
    return result;
}
void HeightLod::Choose(PatchKey key, const std::vector<View> &views,
                       const std::vector<unsigned> &volume, double threshold,
                       std::vector<HeightPatch> &out) const {
    auto it = nodes.find(key);
    if (it == nodes.end())
        return;
    const auto &node = it->second;
    const unsigned side = 1u << key.level;
    bool split = !node.complete || views.empty();
    // Keep the volume seam and its adjacent heightfield at native resolution.
    const unsigned x0 = key.x ? key.x - 1 : 0, z0 = key.z ? key.z - 1 : 0;
    const unsigned x1 = std::min(nx, key.x + side + 1), z1 = std::min(nz, key.z + side + 1);
    auto occupied = [&](unsigned x, unsigned z) { return volume[size_t(z) * (nx + 1) + x]; };
    split |= occupied(x1, z1) + occupied(x0, z0) > occupied(x0, z1) + occupied(x1, z0);
    glm::vec3 lo((int(key.x) * int(cells) - int(nx * cells) / 2.f) * spacing, node.minimum,
                 (int(key.z) * int(cells) - int(nz * cells) / 2.f) * spacing);
    glm::vec3 hi =
        lo + glm::vec3(side * cells * spacing, node.maximum - node.minimum, side * cells * spacing);
    for (const auto &view : views) {
        double distance = glm::length(view.position - glm::clamp(view.position, lo, hi));
        const double projected =
            node.error * (view.orthographicPixelsPerMetre > 0
                              ? view.orthographicPixelsPerMetre
                              : view.pixelsPerRadian / std::max(.05, distance));
        if (projected > threshold)
            split = true;
    }
    if (split && key.level) {
        const auto half = side / 2;
        for (unsigned z = 0; z < 2; ++z)
            for (unsigned x = 0; x < 2; ++x)
                Choose({key.x + x * half, key.z + z * half, key.level - 1}, views, volume,
                       threshold, out);
    } else
        out.push_back({key, glm::vec4(side), node.minimum, node.maximum});
}
std::vector<HeightPatch> HeightLod::Select(const std::vector<View> &views,
                                           const std::set<ChunkKey> &volume, double error) const {
    std::vector<HeightPatch> out;
    std::vector<unsigned> occupied(size_t(nx + 1) * (nz + 1));
    for (auto k : volume)
        if (k.x < nx && k.z < nz)
            occupied[size_t(k.z + 1) * (nx + 1) + k.x + 1] = 1;
    for (unsigned z = 1; z <= nz; ++z)
        for (unsigned x = 1; x <= nx; ++x)
            occupied[size_t(z) * (nx + 1) + x] += occupied[size_t(z - 1) * (nx + 1) + x] +
                                                  occupied[size_t(z) * (nx + 1) + x - 1] -
                                                  occupied[size_t(z - 1) * (nx + 1) + x - 1];
    Choose({0, 0, rootLevel}, views, occupied, error, out);
    // At most one patch per finest chunk. A finest-cell lookup avoids quadratic
    // neighbour searches and supports rectangular sources and any LOD delta.
    std::vector<size_t> owner(size_t(nx) * nz);
    for (size_t i = 0; i < out.size(); ++i) {
        const auto k = out[i].key;
        const auto side = 1u << k.level;
        for (unsigned z = k.z; z < std::min(nz, k.z + side); ++z)
            for (unsigned x = k.x; x < std::min(nx, k.x + side); ++x)
                owner[size_t(z) * nx + x] = i;
    }
    for (auto &p : out) {
        const auto k = p.key;
        const auto side = 1u << k.level;
        auto stride = [&](unsigned x, unsigned z) {
            return float(1u << out[owner[size_t(z) * nx + x]].key.level);
        };
        for (unsigned i = 0; i < side; ++i) {
            if (k.x)
                p.edgeStrides.x = std::max(p.edgeStrides.x, stride(k.x - 1, k.z + i));
            if (k.x + side < nx)
                p.edgeStrides.y = std::max(p.edgeStrides.y, stride(k.x + side, k.z + i));
            if (k.z)
                p.edgeStrides.z = std::max(p.edgeStrides.z, stride(k.x + i, k.z - 1));
            if (k.z + side < nz)
                p.edgeStrides.w = std::max(p.edgeStrides.w, stride(k.x + i, k.z + side));
        }
    }
    return out;
}
size_t HeightLod::Bytes() const {
    return nodes.size() * (sizeof(PatchKey) + sizeof(Node) + 4 * sizeof(void *));
}
MeshData HeightGrid(unsigned cells, unsigned cellsZ) {
    if (!cellsZ)
        cellsZ = cells;
    if (cells < 1 || cells > 64 || cellsZ < 1 || cellsZ > 64)
        throw std::invalid_argument("terrain render grid cells outside1..64");
    MeshData m;
    for (unsigned z = 0; z <= cellsZ; ++z)
        for (unsigned x = 0; x <= cells; ++x) {
            MeshVertex v;
            v.position = {float(x) / cells, 0, float(z) / cellsZ};
            v.normal = {0, 1, 0};
            v.uv = {float(x) / cells, float(z) / cellsZ};
            v.tangent = {1, 0, 0, 1};
            m.vertices.push_back(v);
        }
    for (unsigned z = 0; z < cellsZ; ++z)
        for (unsigned x = 0; x < cells; ++x) {
            unsigned a = z * (cells + 1) + x, b = a + 1, c = a + cells + 1, d = c + 1;
            m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
        }
    return m;
}
TextureData HeightImage(const Source &s) {
    // Lossless binary32 samples in portable RGBA8 storage. texelFetch decodes
    // the bits; no filtered/mipmapped height interpretation is permitted.
    TextureData image;
    image.width = s.samplesX;
    image.height = s.samplesZ;
    TextureMip level;
    level.width = image.width;
    level.height = image.height;
    level.bytes.reserve(SampleCount(s) * 4);
    for (size_t i = 0; i < SampleCount(s); ++i) {
        float h = HeightSample(s, i);
        uint32_t bits;
        std::memcpy(&bits, &h, 4);
        for (unsigned b = 0; b < 4; ++b)
            level.bytes.push_back(uint8_t(bits >> (8 * b)));
    }
    image.levels.push_back(std::move(level));
    return image;
}
} // namespace TerrainV2
