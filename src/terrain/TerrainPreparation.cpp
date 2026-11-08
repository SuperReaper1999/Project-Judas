#include "TerrainPreparation.h"
#include "CollisionAsset.h"
#include "NavigationSystem.h"
#include "SceneFingerprint.h"
#include "TerrainAsset.h"
#include "TerrainMesher.h"
#include "TerrainNavigation.h"
#include <chrono>
#include <cstring>

namespace TerrainV2 {
namespace {
std::string GeometryFingerprint(const Chunk &chunk) {
    std::string bytes = "Judas.Terrain.ChunkGeometry.1";
    bytes.reserve(bytes.size() + chunk.mesh.vertices.size() * 12 + chunk.mesh.indices.size() * 4 +
                  16);
    auto integer = [&](uint32_t v) {
        for (unsigned b = 0; b < 4; ++b)
            bytes.push_back(char(v >> (8 * b)));
    };
    integer(chunk.key.x);
    integer(chunk.key.z);
    integer(uint32_t(chunk.mesh.vertices.size()));
    integer(uint32_t(chunk.mesh.indices.size()));
    for (const auto &vertex : chunk.mesh.vertices)
        for (unsigned axis = 0; axis < 3; ++axis) {
            float value = vertex.position[axis];
            if (value == 0)
                value = 0;
            uint32_t bits;
            std::memcpy(&bits, &value, 4);
            integer(bits);
        }
    for (auto index : chunk.mesh.indices)
        integer(index);
    return SceneFingerprintSha256(bytes);
}
} // namespace
bool PrepareGeometry(Preparation &work, const std::vector<ChunkKey> &keys,
                     const std::function<bool()> &cancelled) {
    const auto start = std::chrono::steady_clock::now();
    if (!work.source) {
        work.error = "missing immutable terrain source";
        return false;
    }
    if (!work.lod)
        work.lod = std::make_shared<const HeightLod>(*work.source);
    std::vector<Chunk> prepared;
    if (!BuildChunks(*work.source, keys, prepared, work.error, cancelled))
        return false;
    for (auto &chunk : prepared) {
        if (cancelled && cancelled()) {
            work.error = "cancelled";
            return false;
        }
        PreparedChunk p;
        p.chunk = std::move(chunk);
        if (!p.chunk.mesh.indices.empty()) {
            p.collision = std::make_shared<CollisionAsset>();
            if (!CookCollision(p.chunk.mesh, {}, *p.collision, work.error))
                return false;
            p.collision->sourceFingerprint = GeometryFingerprint(p.chunk);
        }
        work.chunks.push_back(std::move(p));
    }
    work.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}
size_t SourcePayloadBytes(const Source &source, const HeightLod *lod) {
    size_t bytes = sizeof(source) + source.heights.size() * sizeof(float) +
                   source.weights.size() * sizeof(glm::vec4) + source.edits.size() * sizeof(Edit);
    if (lod)
        bytes += lod->Bytes();
    return bytes;
}
size_t PreparedChunk::PayloadBytes() const {
    size_t bytes = sizeof(*this) + chunk.mesh.vertices.size() * sizeof(MeshVertex) +
                   chunk.mesh.indices.size() * sizeof(uint32_t);
    if (collision) {
        const auto &c = *collision;
        bytes += sizeof(c) + c.vertices.size() * sizeof(glm::dvec3) +
                 c.faces.size() * sizeof(CollisionFace) + c.edges.size() * sizeof(c.edges[0]) +
                 c.satEdges.size() * sizeof(uint32_t) + c.nodes.size() * sizeof(CollisionNode) +
                 c.order.size() * sizeof(uint32_t) + c.polygons.size() * sizeof(CollisionPolygon);
        for (const auto &polygon : c.polygons)
            bytes += polygon.vertices.size() * sizeof(uint32_t);
    }
    return bytes;
}
size_t Preparation::PayloadBytes() const {
    size_t bytes = sizeof(*this);
    if (source)
        bytes += SourcePayloadBytes(*source, lod.get());
    for (const auto &chunk : chunks)
        bytes += chunk.PayloadBytes();
    bytes += NavigationPayloadBytes(navigation.get());
    return bytes;
}
bool PrepareAsset(const std::string &path, const std::string &identity, PreparedAsset &output,
                  std::string &error, const std::function<bool()> &cancelled,
                  const NavigationInput *navigation) {
    AssetPages pages;
    Source source;
    if (!pages.Open(path, error) || !pages.ReadAll(source, error))
        return false;
    if (size_t(source.ChunksX()) * source.ChunksZ() > 1024) {
        error = "terrain active chunk budget exceeds 1024";
        return false;
    }
    PreparedAsset next;
    next.asset = identity;
    next.fingerprint = pages.Index().fingerprint;
    next.geometry.source = std::make_shared<const Source>(std::move(source));
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < next.geometry.source->ChunksZ(); ++z)
        for (unsigned x = 0; x < next.geometry.source->ChunksX(); ++x)
            keys.push_back({x, z});
    if (!PrepareGeometry(next.geometry, keys, cancelled)) {
        error = next.geometry.error;
        return false;
    }
    if (navigation) {
        const auto start = std::chrono::steady_clock::now();
        auto input = *navigation;
        for (const auto &part : next.geometry.chunks)
            input.chunks[part.chunk.key] = {part.chunk.anchor, part.collision};
        if (!PrepareNavigation(input, next.geometry.navigation, error, cancelled))
            return false;
        next.geometry.navigationCache =
            NavigationSystem::PrepareSurface(next.geometry.navigation, error, cancelled);
        if (!next.geometry.navigationCache)
            return false;
        next.geometry.milliseconds +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
    }
    output = std::move(next);
    error.clear();
    return true;
}
} // namespace TerrainV2
