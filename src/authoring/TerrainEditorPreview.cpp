#include "TerrainEditorPreview.h"
#include "TerrainRayPicking.h"
#include "terrain/TerrainMesher.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace TerrainV2 {
namespace {
bool Finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool Intersects(glm::dvec3 lo, glm::dvec3 hi, glm::dvec3 origin, glm::dvec3 direction) {
    double near = 0, far = std::numeric_limits<double>::infinity();
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0) {
            if (origin[axis] < lo[axis] || origin[axis] > hi[axis])
                return false;
        } else {
            double a = (lo[axis] - origin[axis]) / direction[axis];
            double b = (hi[axis] - origin[axis]) / direction[axis];
            if (a > b)
                std::swap(a, b);
            near = std::max(near, a);
            far = std::min(far, b);
            if (near > far)
                return false;
        }
    }
    return true;
}
} // namespace

size_t EditorPreview::BoundsBytes() const {
    // POD range storage plus an estimate for map node links; allocator
    // bookkeeping and the source/mesh products are accounted separately.
    return ranges.capacity() * sizeof(Range) +
           bounds.size() * (sizeof(decltype(bounds)::value_type) + 4 * sizeof(void *));
}

bool EditorPreview::Pick(glm::vec3 origin, glm::vec3 direction, glm::vec3 &hit,
                         size_t *testedTriangles) const {
    if (!mesh || !Finite(origin) || !Finite(direction))
        return false;
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto &range : ranges) {
        if (!Intersects(range.minimum, range.maximum, origin, direction))
            continue;
        if (testedTriangles)
            *testedTriangles += range.count / 3;
        PickTerrainRange(*mesh, range.first, range.count, origin, direction, nearest);
    }
    if (!std::isfinite(nearest))
        return false;
    hit = origin + direction * nearest;
    return true;
}

bool PrepareEditorPreview(const Source &source, const std::vector<ChunkKey> &keys,
                          std::shared_ptr<const EditorPreview> previous,
                          std::shared_ptr<const EditorPreview> &output, std::string &error,
                          const std::function<bool()> &cancelled) {
    auto stop = [&] {
        if (cancelled && cancelled()) {
            error = "Terrain preview preparation cancelled";
            return true;
        }
        return false;
    };
    if (stop())
        return false;
    std::vector<Chunk> changed;
    // Retain the original editor batch budget and exact extraction operator.
    if (!BuildChunks(source, keys, changed, error, cancelled, 1048576))
        return false;
    auto prepared = std::make_shared<EditorPreview>();
    if (previous)
        for (const auto &[key, chunk] : previous->chunks)
            if (key.x < source.ChunksX() && key.z < source.ChunksZ())
                prepared->chunks.emplace(key, chunk);
    for (auto &chunk : changed) {
        const auto key = chunk.key;
        prepared->chunks[key] = std::make_shared<const Chunk>(std::move(chunk));
    }
    if (prepared->chunks.size() != size_t(source.ChunksX()) * source.ChunksZ()) {
        error = "Terrain preview requires every source chunk before publication";
        return false;
    }
    size_t vertices = 0, indices = 0;
    for (const auto &[_, chunk] : prepared->chunks) {
        if (chunk->mesh.vertices.size() > UINT32_MAX - vertices ||
            chunk->mesh.indices.size() > std::numeric_limits<size_t>::max() - indices) {
            error = "Terrain preview exceeds the shared mesh index format";
            return false;
        }
        vertices += chunk->mesh.vertices.size();
        indices += chunk->mesh.indices.size();
    }
    auto meshData = std::make_shared<MeshData>();
    auto &mesh = *meshData;
    mesh.vertices.reserve(vertices);
    mesh.indices.reserve(indices);
    prepared->ranges.reserve(prepared->chunks.size());
    for (const auto &[_, chunk] : prepared->chunks) {
        if (stop())
            return false;
        const uint32_t offset = uint32_t(mesh.vertices.size());
        EditorPreview::Range range;
        range.first = mesh.indices.size();
        range.count = chunk->mesh.indices.size();
        range.minimum = glm::dvec3(std::numeric_limits<double>::infinity());
        range.maximum = -range.minimum;
        for (auto vertex : chunk->mesh.vertices) {
            vertex.position += chunk->anchor;
            if (!Finite(vertex.position)) {
                error = "Terrain preview contains a nonfinite position";
                return false;
            }
            // Bounds refer to the SAME rounded positions used by the old merged
            // picker, not a separately reprojected approximation of the chunks.
            range.minimum = glm::min(range.minimum, glm::dvec3(vertex.position));
            range.maximum = glm::max(range.maximum, glm::dvec3(vertex.position));
            mesh.vertices.push_back(vertex);
        }
        for (auto index : chunk->mesh.indices) {
            if (index >= chunk->mesh.vertices.size()) {
                error = "Terrain preview contains an invalid triangle index";
                return false;
            }
            mesh.indices.push_back(offset + index);
        }
        if (range.count) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                range.minimum[axis] = std::nextafter(float(range.minimum[axis]), -INFINITY);
                range.maximum[axis] = std::nextafter(float(range.maximum[axis]), INFINITY);
            }
            prepared->bounds.emplace(chunk->key, std::make_pair(glm::vec3(range.minimum), glm::vec3(range.maximum)));
            prepared->ranges.push_back(range);
        }
    }
    if (stop())
        return false;
    prepared->source = std::make_shared<const Source>(source);
    if (!source.chart) {
        const auto *old = previous ? &previous->PublishedSource() : nullptr;
        // Excavation changes volume ownership, not the base height hierarchy.
        // Share immutable metadata when the entire height mapping is unchanged.
        if (old && previous->lod && old->samplesX == source.samplesX &&
            old->samplesZ == source.samplesZ && old->chunkCells == source.chunkCells &&
            old->spacing == source.spacing && old->storage == source.storage &&
            old->heights == source.heights)
            prepared->lod = previous->lod;
        else
            prepared->lod = std::make_shared<HeightLod>(source);
    }
    prepared->mesh = std::move(meshData);
    output = std::move(prepared);
    error.clear();
    return true;
}
} // namespace TerrainV2
