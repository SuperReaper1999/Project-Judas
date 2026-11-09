#include "TerrainV2PreviewRenderer.h"
#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

void TerrainV2PreviewRenderer::Clear(Renderer &renderer) {
    pages.Clear(renderer);
    preview.reset();
}
void TerrainV2PreviewRenderer::Publish(
    Renderer &renderer, std::shared_ptr<const TerrainV2::EditorPreview> next) {
    if (preview && next) {
        std::vector<TerrainV2::ChunkKey> changed;
        glm::uvec4 samples{UINT32_MAX, UINT32_MAX, 0, 0};
        const auto &source = next->PublishedSource();
        for (const auto &[key, chunk] : next->Chunks()) {
            auto old = preview->Chunks().find(key);
            if (old == preview->Chunks().end() || old->second != chunk) {
                changed.push_back(key);
                const unsigned x = key.x * source.chunkCells, z = key.z * source.chunkCells;
                samples = {std::min(samples.x, x), std::min(samples.y, z),
                           std::max(samples.z, x + source.chunkCells),
                           std::max(samples.w, z + source.chunkCells)};
            }
        }
        const auto &old = preview->PublishedSource();
        if (old.samplesX != source.samplesX || old.samplesZ != source.samplesZ ||
            old.spacing != source.spacing || old.chunkCells != source.chunkCells ||
            bool(old.chart) != bool(source.chart))
            pages.Clear(renderer);
        else if (!changed.empty())
            pages.Invalidate(renderer, source, samples, changed);
    } else {
        pages.Clear(renderer);
    }
    preview = std::move(next);
}
void TerrainV2PreviewRenderer::Draw(Renderer &renderer, glm::vec3 position,
                                   glm::quat rotation, glm::vec3 colour,
                                   std::optional<TerrainV2::Edit> brush) {
    if (!preview) return;
    const auto &source = preview->PublishedSource();
    const auto transform = glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation);
    std::set<TerrainV2::ChunkKey> volume, fine;
    for (const auto &[key, chunk] : preview->Chunks())
        if (chunk->volumetric) volume.insert(key);
    fine = volume;
    if (brush)
        for (auto key : TerrainV2::Affected(source, *brush)) fine.insert(key);
    const auto projection = renderer.ProjectionMatrix();
    const auto eye = glm::vec3(glm::inverse(renderer.ViewMatrix())[3]);
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    TerrainV2::View view;
    view.position = glm::inverse(rotation) * (eye - position);
    if (std::abs(projection[3][3]) > .5f)
        view.orthographicPixelsPerMetre = viewport[3] * std::abs(projection[1][1]) * .5;
    else
        view.pixelsPerRadian = viewport[3] * std::abs(projection[1][1]) * .5;
    if (const auto *lod = preview->Lod()) {
        for (const auto &patch : lod->Select({view}, fine)) {
            if (patch.key.level == 0 && volume.count({patch.key.x, patch.key.z})) continue;
            for (const auto &key : TerrainV2::PartitionRenderPatch(source, patch)) {
                const unsigned cells = key.cells ? key.cells : source.chunkCells,
                               cellsZ = key.cellsZ ? key.cellsZ : cells,
                               stride = key.stride ? key.stride : 1u << key.patch.level,
                               x = key.firstX == UINT32_MAX ? key.patch.x * source.chunkCells : key.firstX,
                               z = key.firstZ == UINT32_MAX ? key.patch.z * source.chunkCells : key.firstZ;
                const float width = cells * source.spacing * stride,
                            depth = cellsZ * source.spacing * stride;
                const glm::vec3 anchor{x * source.spacing - (source.samplesX-1)*source.spacing*.5f,
                                       0, z * source.spacing - (source.samplesZ-1)*source.spacing*.5f};
                VisualBounds bounds; bounds.Include(anchor + glm::vec3(0,patch.minimum,0));
                bounds.Include(anchor + glm::vec3(width,patch.maximum,depth));
                if (!renderer.IsVisible(TransformBounds(bounds, transform))) continue;
                const auto &page = pages.Acquire(renderer, source, key);
                renderer.DrawTerrainGrid(page.heights, cells, {x,z,stride,cells}, key.edges,
                    {source.samplesX,source.samplesZ}, source.spacing,
                    position + rotation * anchor, rotation, width, patch.minimum, patch.maximum,
                    colour, 1, {0,1}, true, cellsZ, depth);
            }
        }
    }
    for (const auto &[key, chunk] : preview->Chunks()) {
        if (preview->Lod() && !chunk->volumetric) continue;
        if (chunk->mesh.indices.empty()) continue;
        VisualBounds bounds;
        const auto &range = preview->Bounds().at(key);
        bounds.Include(range.first); bounds.Include(range.second);
        if (!renderer.IsVisible(TransformBounds(bounds, transform))) continue;
        const auto &page = pages.Acquire(renderer, source, {{key.x,key.z,0},glm::vec4(1),false},
                                         [chunk]{ return chunk; });
        renderer.DrawMesh(page.mesh, position + rotation * chunk->anchor, rotation,
                          glm::vec3(1), {}, colour, 1);
    }
}
