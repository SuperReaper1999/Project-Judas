#include "CollisionAsset.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "TerrainRuntime.h"
#include "TerrainSamples.h"
#include "WorldPresentation.h"
#include <glm/gtc/matrix_transform.hpp>
namespace TerrainV2 {
void Runtime::ReleaseGraphics() {
    auto *renderer = world.Resources() ? world.Resources()->GetRenderer() : nullptr;
    if (renderer)
        renderPages.Clear(*renderer);
}
void Runtime::CommitPresentation() {
    auto *renderer = world.Resources() ? world.Resources()->GetRenderer() : nullptr;
    if (renderer)
        renderPages.Clear(*renderer);
    drawPatches.Reset();
}
void Runtime::PrepareViews(const std::vector<PlanetView> &views, glm::dvec3 origin, float seconds) {
    if (!source || !lod)
        return;
    if (source->chart) {
        drawPatches.Reset();
        return;
    }
    const auto *d = Definition();
    if (!d)
        return;
    std::vector<View> localViews;
    localViews.reserve(views.size());
    for (const auto &v : views) {
        View local;
        local.position = glm::vec3(glm::conjugate(glm::dquat(d->transform.rotation)) *
                                   (v.eye - origin - glm::dvec3(d->transform.position)));
        local.pixelsPerRadian = v.pixels / (2 * std::tan(v.fov * .5));
        if (v.orthographicHeight > 0)
            local.orthographicPixelsPerMetre = v.pixels / v.orthographicHeight;
        localViews.push_back(local);
    }
    std::set<ChunkKey> volume;
    for (const auto &[k, r] : chunks)
        if (r.geometry.chunk.volumetric)
            volume.insert(k);
    drawPatches.Update(lod->Select(localViews, volume), seconds);
}
void Runtime::Draw(Renderer &renderer) const {
    if (!source || !world.IsPublished(owner) || !world.RenderVisible(owner) ||
        (source->chart && !bindingPublished))
        return;
    auto *d = Definition();
    if (!d || !d->render)
        return;
    renderer.SetRenderLayer(world.RenderLayerOf(owner));
    renderer.SetMaterialBindings(BuildRenderMaterialBindings(world.Resources(), *d->render));
    auto texture = world.Resources() ? world.Resources()->TryGetTexture(d->render->textureAsset)
                                     : TextureHandle{};
    std::array<TextureHandle, 4> layers;
    for (unsigned i = 0; i < 4; ++i)
        if (!source->layerTextures[i].empty()) {
            world.Resources()->RequestTexture(source->layerTextures[i]);
            layers[i] = world.Resources()->TryGetTexture(source->layerTextures[i]);
        }
    TextureHandle normal;
    if (!source->materialDetail.normalTexture.empty()) {
        world.Resources()->RequestTexture(source->materialDetail.normalTexture);
        normal = world.Resources()->TryGetTexture(source->materialDetail.normalTexture);
    }
    renderer.BeginTerrainMaterial({}, layers, source->layerColours,
                                  glm::inverse(glm::translate(glm::mat4(1), d->transform.position) *
                                               glm::mat4_cast(d->transform.rotation)),
                                  source->materialDetail, normal);
    RendererProfileScope gpu(renderer, "Terrain V2 geometry");
    std::vector<HeightPatch> fallback;
    if (!source->chart && drawPatches.Current().empty() && lod) {
        std::set<ChunkKey> volume;
        for (const auto &[k, r] : chunks)
            if (r.geometry.chunk.volumetric)
                volume.insert(k);
        fallback = lod->Select({}, volume);
    }
    auto draw = [&](const std::vector<HeightPatch> &patches, glm::vec2 coverage, bool incoming) {
        for (const auto &patch : patches) {
            const auto k = patch.key;
            const bool shared = drawPatches.Shared(k);
            if (incoming && shared)
                continue;
            auto it = chunks.find({k.x, k.z});
            if (k.level == 0 && it != chunks.end() && it->second.geometry.chunk.volumetric)
                continue;
            for (const auto &key : PartitionRenderPatch(*source, patch)) {
                const auto tile = key.patch;
                const unsigned cells = key.cells ? key.cells : source->chunkCells,
                               cellsZ = key.cellsZ ? key.cellsZ : cells,
                               firstX = key.firstX == UINT32_MAX ? tile.x * source->chunkCells
                                                                 : key.firstX,
                               firstZ = key.firstZ == UINT32_MAX ? tile.z * source->chunkCells
                                                                 : key.firstZ;
                const float stride = float(key.stride ? key.stride : 1u << tile.level),
                            width = cells * source->spacing * stride,
                            depth = cellsZ * source->spacing * stride;
                const glm::vec3 anchor(
                    firstX * source->spacing - (source->samplesX - 1) * source->spacing * .5f, 0,
                    firstZ * source->spacing - (source->samplesZ - 1) * source->spacing * .5f);
                VisualBounds bounds;
                bounds.Include({0, patch.minimum, 0});
                bounds.Include({width, patch.maximum, depth});
                const auto frame =
                    glm::translate(glm::mat4(1),
                                   d->transform.position + d->transform.rotation * anchor) *
                    glm::mat4_cast(d->transform.rotation);
                if (!renderer.IsVisible(TransformBounds(bounds, frame)))
                    continue;
                const auto &page = renderPages.Acquire(renderer, *source, key);
                renderer.SetTerrainWeightPage(page.weights, page.weightRect);
                renderer.DrawTerrainGrid(
                    page.heights, cells, {float(firstX), float(firstZ), stride, float(cells)},
                    key.edges, {float(source->samplesX), float(source->samplesZ)}, source->spacing,
                    d->transform.position + d->transform.rotation * anchor, d->transform.rotation,
                    width, patch.minimum, patch.maximum, d->render->color, d->render->alpha,
                    shared ? glm::vec2(0, 1) : coverage, true, cellsZ, depth);
            }
        }
    };
    draw(drawPatches.Current().empty() ? fallback : drawPatches.Current(),
         {drawPatches.Active() ? drawPatches.Fraction() : 0, 1}, false);
    if (drawPatches.Active() && drawPatches.Fraction() > 0)
        draw(drawPatches.Incoming(), {0, drawPatches.Fraction()}, true);
    for (const auto &[_, r] : chunks) {
        const auto &c = r.geometry.chunk;
        if (!r.triangles || (!c.volumetric && !source->chart))
            continue;
        if (r.geometry.collision) {
            VisualBounds bounds;
            bounds.Include(glm::vec3(r.geometry.collision->minimum));
            bounds.Include(glm::vec3(r.geometry.collision->maximum));
            const auto frame = glm::translate(glm::mat4(1), d->transform.position +
                                                                d->transform.rotation * c.anchor) *
                               glm::mat4_cast(d->transform.rotation);
            if (!renderer.IsVisible(TransformBounds(bounds, frame)))
                continue;
        }
        const auto &page = renderPages.Acquire(
            renderer, *source, {{c.key.x, c.key.z, 0}, glm::vec4(1), false}, [&] {
                auto product = r.geometry.recipe->Acquire();
                return std::shared_ptr<const Chunk>(product, &product->chunk);
            });
        renderer.SetTerrainWeightPage(page.weights, page.weightRect);
        renderer.DrawMesh(page.mesh, d->transform.position + d->transform.rotation * c.anchor,
                          d->transform.rotation, glm::vec3(1), texture, d->render->color,
                          d->render->alpha);
    }
    gpu.End();
    renderer.EndTerrainMaterial();
}
} // namespace TerrainV2
