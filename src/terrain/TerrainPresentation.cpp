#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "TerrainRuntime.h"
#include "WorldPresentation.h"
#include <glm/gtc/matrix_transform.hpp>
namespace TerrainV2 {
void Runtime::ReleaseGraphics() {
    auto *renderer = world.Resources() ? world.Resources()->GetRenderer() : nullptr;
    if (renderer) {
        if (heightTexture.IsValid())
            renderer->DestroyTexture(heightTexture);
        if (weightTexture.IsValid())
            renderer->DestroyTexture(weightTexture);
    }
    heightTexture = {};
    weightTexture = {};
    for (auto &[_, r] : chunks)
        ReleaseMesh(r);
}
void Runtime::CommitPresentation() {
    auto *renderer = world.Resources() ? world.Resources()->GetRenderer() : nullptr;
    if (renderer && lod != pending->lod && heightTexture.IsValid()) {
        renderer->DestroyTexture(heightTexture);
        heightTexture = {};
    }
    if (renderer && source && source->weights != pending->source->weights &&
        weightTexture.IsValid()) {
        renderer->DestroyTexture(weightTexture);
        weightTexture = {};
    }
    drawPatches.Reset();
}
void Runtime::ReleaseMesh(Resident &r) const {
    if (r.mesh.IsValid() && world.Resources() && world.Resources()->GetRenderer())
        world.Resources()->GetRenderer()->DestroyMesh(r.mesh);
    r.mesh = {};
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
    if (!weightTexture.IsValid()) {
        TextureData image;
        image.width = int(source->samplesX);
        image.height = int(source->samplesZ);
        for (auto w : source->weights)
            for (unsigned k = 0; k < 4; ++k)
                image.pixels.push_back(uint8_t(std::lround(w[k] * 255)));
        if (weightTexture.IsValid())
            renderer.DestroyTexture(weightTexture);
        weightTexture = renderer.CreateTexture(image);
        appearanceRevision = status.revision;
    }
    if (!source->chart && !heightTexture.IsValid())
        heightTexture = renderer.CreateTexture(HeightImage(*source));
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
    renderer.BeginTerrainMaterial(weightTexture, layers, source->layerColours,
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
            const float stride = float(1u << k.level),
                        width = source->chunkCells * source->spacing * stride;
            glm::vec3 anchor(k.x * source->chunkCells * source->spacing -
                                 (source->samplesX - 1) * source->spacing * .5f,
                             0,
                             k.z * source->chunkCells * source->spacing -
                                 (source->samplesZ - 1) * source->spacing * .5f);
            renderer.DrawTerrainGrid(
                heightTexture, source->chunkCells,
                {float(k.x * source->chunkCells), float(k.z * source->chunkCells), stride,
                 float(source->chunkCells)},
                patch.edgeStrides, {float(source->samplesX), float(source->samplesZ)},
                source->spacing, d->transform.position + d->transform.rotation * anchor,
                d->transform.rotation, width, patch.minimum, patch.maximum, d->render->color,
                d->render->alpha, shared ? glm::vec2(0, 1) : coverage);
        }
    };
    draw(drawPatches.Current().empty() ? fallback : drawPatches.Current(),
         {drawPatches.Active() ? drawPatches.Fraction() : 0, 1}, false);
    if (drawPatches.Active() && drawPatches.Fraction() > 0)
        draw(drawPatches.Incoming(), {0, drawPatches.Fraction()}, true);
    for (const auto &[_, r] : chunks) {
        auto &c = r.geometry.chunk;
        if (c.mesh.indices.empty() || (!c.volumetric && !source->chart))
            continue;
        if (!r.mesh.IsValid())
            r.mesh = renderer.CreateMesh(c.mesh);
        renderer.DrawMesh(r.mesh, d->transform.position + d->transform.rotation * c.anchor,
                          d->transform.rotation, glm::vec3(1), texture, d->render->color,
                          d->render->alpha);
    }
    gpu.End();
    renderer.EndTerrainMaterial();
}
} // namespace TerrainV2
