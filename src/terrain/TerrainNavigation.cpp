#include "TerrainNavigation.h"
#include "CollisionAsset.h"
#include "SceneFingerprint.h"
#include "TerrainMesher.h"
#include <algorithm>
#include <cmath>

namespace TerrainV2 {
namespace {
void AppendMesh(const MeshData &mesh, glm::vec3 anchor, const SceneTransform &owner,
                const SceneTransform &frame, NavigationGeometry &geometry) {
    const auto offset = int(geometry.vertices.size() / 3);
    const auto inverse = glm::inverse(frame.rotation);
    for (const auto &vertex : mesh.vertices) {
        const auto point = inverse * (owner.position + owner.rotation * (anchor + vertex.position) -
                                      frame.position);
        geometry.vertices.insert(geometry.vertices.end(), {point.x, point.y, point.z});
    }
    for (auto index : mesh.indices)
        geometry.triangles.push_back(offset + int(index));
}
} // namespace
bool AppendNavigationSource(const Source &source, const SceneTransform &owner,
                            const SceneTransform &frame, NavigationGeometry &geometry,
                            std::string &error) {
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < source.ChunksZ(); ++z)
        for (unsigned x = 0; x < source.ChunksX(); ++x)
            keys.push_back({x, z});
    std::vector<Chunk> chunks;
    if (!BuildChunks(source, keys, chunks, error))
        return false;
    for (const auto &chunk : chunks)
        AppendMesh(chunk.mesh, chunk.anchor, owner, frame, geometry);
    return true;
}
bool ValidateNavigationInput(const NavigationInput &input, std::string &error) {
    if (!ValidateNavigationBakeSettings(input.settings, input.profile, error))
        return false;
    const double tilesX =
        std::ceil(std::ceil(2. * input.settings.halfExtents.x / input.settings.cellSize) /
                  input.settings.tileSize);
    const double tilesZ =
        std::ceil(std::ceil(2. * input.settings.halfExtents.z / input.settings.cellSize) /
                  input.settings.tileSize);
    const double radiusCells = std::ceil(double(input.profile.radius) / input.settings.cellSize);
    const double width = input.settings.tileSize + 2 * (radiusCells + 3);
    if (width * width * tilesX * tilesZ > 16 * 1024 * 1024) {
        error = "terrain navigation exceeds its 16M-column preparation budget; reduce coverage, "
                "use coarser cells or split regions";
        return false;
    }
    error.clear();
    return true;
}
bool PrepareNavigation(const NavigationInput &input, std::shared_ptr<const NavigationData> &output,
                       std::string &error, const std::function<bool()> &cancelled) {
    if (!ValidateNavigationInput(input, error))
        return false;
    NavigationGeometry geometry;
    std::string identity = "Judas.NativeTerrainNavigation.1";
    // Recast needs a border outside each tile for radius erosion. Keep geometry
    // crossing the declared coverage bounds, not just triangles with an interior
    // vertex. This is broadphase rejection, never a centroid visibility shortcut.
    const auto border =
        (std::ceil(input.profile.radius / input.settings.cellSize) + 3) * input.settings.cellSize;
    const auto extent = glm::dvec3(input.settings.halfExtents) + glm::dvec3(border);
    for (const auto &[key, chunk] : input.chunks) {
        if (cancelled && cancelled()) {
            error = "cancelled";
            return false;
        }
        if (!chunk.collision)
            continue;
        const auto &collision = *chunk.collision;
        const auto anchor = glm::dvec3(chunk.anchor);
        if (glm::any(glm::lessThan(collision.maximum + anchor, -extent)) ||
            glm::any(glm::greaterThan(collision.minimum + anchor, extent)))
            continue;
        identity += collision.sourceFingerprint;
        const auto offset = int(geometry.vertices.size() / 3);
        for (auto point : collision.vertices) {
            const auto p = glm::vec3(point + anchor);
            geometry.vertices.insert(geometry.vertices.end(), {p.x, p.y, p.z});
        }
        for (const auto &face : collision.faces)
            for (auto index : face.vertices)
                geometry.triangles.push_back(offset + int(index));
    }
    geometry.fingerprint = SceneFingerprintSha256(identity);
    NavigationBakeUpdate update;
    update.previous = input.previous.get();
    auto changed = [&](const NavigationChunk &chunk) {
        if (chunk.collision) {
            const auto anchor = glm::dvec3(chunk.anchor);
            update.changed.push_back({glm::vec3(chunk.collision->minimum + anchor),
                                      glm::vec3(chunk.collision->maximum + anchor)});
        }
    };
    if (input.previous) {
        // Pointer equality is a proof for these immutable prepared cooks. Any
        // replacement covers BOTH old and new extents, including removed/empty
        // chunks and radial mappings. Unchanged draw LOD is irrelevant here.
        for (const auto &[key, old] : input.previousChunks) {
            auto current = input.chunks.find(key);
            if (current == input.chunks.end() || old.anchor != current->second.anchor ||
                old.collision != current->second.collision) {
                changed(old);
                if (current != input.chunks.end())
                    changed(current->second);
            }
        }
        for (const auto &[key, current] : input.chunks)
            if (!input.previousChunks.count(key))
                changed(current);
    }
    auto data = std::make_shared<NavigationData>();
    if (geometry.triangles.empty()) {
        data->settings = input.settings;
        data->profile = input.profile;
        data->fingerprint = geometry.fingerprint;
    } else if (!BakeNavigationGeometry(geometry, input.settings, input.profile, *data, error, true,
                                       cancelled, input.previous ? &update : nullptr))
        return false;
    if (cancelled && cancelled()) {
        error = "cancelled";
        return false;
    }
    output = std::move(data);
    error.clear();
    return true;
}
size_t NavigationPayloadBytes(const NavigationData *data) {
    if (!data)
        return 0;
    size_t bytes = sizeof(*data);
    for (const auto &layer : data->layers)
        bytes += sizeof(layer) + layer.size();
    return bytes;
}
} // namespace TerrainV2
