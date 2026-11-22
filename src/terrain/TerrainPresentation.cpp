#include "CollisionAsset.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "TerrainRuntime.h"
#include "TerrainSamples.h"
#include "WorldPresentation.h"
#include "lighting/ShadowSystem.h"
#include <algorithm>
#include <atomic>
#include <glm/gtc/matrix_transform.hpp>
namespace TerrainV2 {
namespace {
std::atomic<uint64_t> nextShadowSourceRevision{1};

template<class Value> void AppendShadowValue(std::string &key, const Value &value) {
    key.append(reinterpret_cast<const char *>(&value), sizeof(value));
}
void AppendShadowValue(std::string &key, const glm::vec2 &value) {
    AppendShadowValue(key, value.x);
    AppendShadowValue(key, value.y);
}
void AppendShadowValue(std::string &key, const glm::vec3 &value) {
    AppendShadowValue(key, value.x);
    AppendShadowValue(key, value.y);
    AppendShadowValue(key, value.z);
}
void AppendShadowValue(std::string &key, const glm::vec4 &value) {
    AppendShadowValue(key, glm::vec3(value));
    AppendShadowValue(key, value.w);
}
} // namespace
void Runtime::ReleaseGraphics() {
    auto *renderer = world.Resources() ? world.Resources()->GetRenderer() : nullptr;
    if (renderer)
        renderPages.Clear(*renderer);
}
void Runtime::CommitPresentation() {
    auto *renderer = world.Resources() ? world.Resources()->GetRenderer() : nullptr;
    if (renderer) {
        if (pending->preservePresentation)
            renderPages.Invalidate(*renderer, *pending->source, pending->changedSamples,
                                   pending->changedMeshes);
        else
            renderPages.Clear(*renderer);
    }
    // A replacement can reuse the same owner ID after streaming or reload.
    // Partial publications retain the unaffected finest-chunk revisions.
    const auto &next = *pending->source;
    if (!pending->preservePresentation) {
        shadowSourceRevision = nextShadowSourceRevision.fetch_add(1, std::memory_order_relaxed);
        shadowEditRevision = 0;
        const auto count = size_t(next.ChunksX()) * next.ChunksZ();
        shadowSampleRevisions.assign(count, 0);
        shadowMeshRevisions.assign(count, 0);
    } else {
        const auto revision = ++shadowEditRevision;
        if (const auto &rect = pending->changedSamples) {
            for (unsigned z = 0; z < next.ChunksZ(); ++z)
                for (unsigned x = 0; x < next.ChunksX(); ++x)
                    if (x * next.chunkCells <= rect->z && (x + 1) * next.chunkCells >= rect->x &&
                        z * next.chunkCells <= rect->w && (z + 1) * next.chunkCells >= rect->y)
                        shadowSampleRevisions[size_t(z) * next.ChunksX() + x] = revision;
        }
        for (const auto &key : pending->changedMeshes)
            shadowMeshRevisions[size_t(key.z) * next.ChunksX() + key.x] = revision;
    }
    drawPatches.Reset();
}
void Runtime::SyncMaterialDemand(bool sourceChanged) {
    auto *resources = world.Resources();
    if (!resources)
        return;
    const auto *definition = Definition();
    const bool active = source && definition && definition->render && world.IsPublished(owner) &&
                        world.RenderVisible(owner) && (!source->chart || bindingPublished);
    if (!sourceChanged && active == materialDemandActive)
        return;

    // Startup pumps can evict ready, unreferenced textures between hidden
    // frames. Keep the published terrain's material working set resident,
    // including between the refreshes of a low-cadence secondary camera.
    std::set<AssetId> next;
    if (active) {
        for (const auto &id : source->layerTextures)
            if (!id.empty())
                next.insert(id);
        if (!source->materialDetail.normalTexture.empty())
            next.insert(source->materialDetail.normalTexture);
    }
    for (const auto &id : next)
        if (!materialDemand.count(id))
            resources->AddRef(id);
    for (const auto &id : materialDemand)
        if (!next.count(id))
            resources->ReleaseRef(id);
    materialDemand = std::move(next);
    materialDemandActive = active;
}
void Runtime::PrepareViews(const std::vector<PlanetView> &views, glm::dvec3 origin, float seconds, float visualBias) {
    SyncMaterialDemand();
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
    drawPatches.Update(lod->Select(localViews, volume, 2*visualBias), seconds);
}
const SceneObject *Runtime::DrawDefinition() const {
    if (!source || !world.IsPublished(owner) || !world.RenderVisible(owner) ||
        (source->chart && !bindingPublished))
        return nullptr;
    auto *d = Definition();
    return d && d->render ? d : nullptr;
}

template<class Grid, class Mesh>
void Runtime::VisitDraws(const SceneObject &d, Grid grid, Mesh mesh) const {
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
                                   d.transform.position + d.transform.rotation * anchor) *
                    glm::mat4_cast(d.transform.rotation);
                grid(GridDraw{key, TransformBounds(bounds, frame),
                              d.transform.position + d.transform.rotation * anchor,
                              shared ? glm::vec2(0, 1) : coverage, cells, cellsZ, firstX, firstZ,
                              stride, width, depth, patch.minimum, patch.maximum});
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
        const auto position = d.transform.position + d.transform.rotation * c.anchor;
        VisualBounds bounds;
        if (r.geometry.collision) {
            bounds.Include(glm::vec3(r.geometry.collision->minimum));
            bounds.Include(glm::vec3(r.geometry.collision->maximum));
            bounds = TransformBounds(bounds, glm::translate(glm::mat4(1), position) *
                                              glm::mat4_cast(d.transform.rotation));
        }
        mesh(r, position, bounds);
    }
}

uint64_t Runtime::ShadowPageRevision(const GridDraw &grid) const {
    if (shadowEditRevision == 0)
        return 0;
    const unsigned halo = unsigned(std::max({1.f, grid.key.edges.x, grid.key.edges.y,
                                              grid.key.edges.z, grid.key.edges.w}));
    const unsigned extentX = unsigned(grid.cells * grid.stride);
    const unsigned extentZ = unsigned(grid.cellsZ * grid.stride);
    const unsigned firstX = grid.firstX > halo ? grid.firstX - halo : 0;
    const unsigned firstZ = grid.firstZ > halo ? grid.firstZ - halo : 0;
    const unsigned lastX = grid.firstX + extentX + halo;
    const unsigned lastZ = grid.firstZ + extentZ + halo;
    uint64_t revision = 0;
    for (unsigned z = firstZ / source->chunkCells; z < source->ChunksZ() && z <= lastZ / source->chunkCells; ++z)
        for (unsigned x = firstX / source->chunkCells; x < source->ChunksX() && x <= lastX / source->chunkCells; ++x)
            revision = std::max(revision, shadowSampleRevisions[size_t(z) * source->ChunksX() + x]);
    return revision;
}

void Runtime::AppendShadowCasters(std::vector<JudasLighting::ShadowCaster> &casters) const {
    const auto *definition = DrawDefinition();
    if (!definition)
        return;
    VisualBounds ownerBounds;
    bool unknownBounds = false;
    const auto appendBounds = [&](const VisualBounds &bounds) {
        if (bounds.Valid()) {
            ownerBounds.Include(bounds.min);
            ownerBounds.Include(bounds.max);
        } else {
            unknownBounds = true;
        }
    };
    const auto first = casters.size();
    VisitDraws(*definition, [&](const GridDraw &grid) {
        auto &caster = casters.emplace_back();
        caster.bounds = grid.bounds;
        caster.content = "terrain grid";
        AppendShadowValue(caster.content, owner);
        AppendShadowValue(caster.content, shadowSourceRevision);
        AppendShadowValue(caster.content, grid.firstX);
        AppendShadowValue(caster.content, grid.firstZ);
        AppendShadowValue(caster.content, grid.cells);
        AppendShadowValue(caster.content, grid.cellsZ);
        AppendShadowValue(caster.content, grid.stride);
        AppendShadowValue(caster.content, grid.key.edges);
        AppendShadowValue(caster.content, grid.coverage);
        AppendShadowValue(caster.content, ShadowPageRevision(grid));
        appendBounds(grid.bounds);
    }, [&](const Resident &resident, glm::vec3, const VisualBounds &bounds) {
        const auto &key = resident.geometry.chunk.key;
        auto &caster = casters.emplace_back();
        caster.bounds = bounds;
        caster.content = "terrain mesh";
        AppendShadowValue(caster.content, owner);
        AppendShadowValue(caster.content, shadowSourceRevision);
        AppendShadowValue(caster.content, key.x);
        AppendShadowValue(caster.content, key.z);
        AppendShadowValue(caster.content, shadowMeshRevisions[size_t(key.z) * source->ChunksX() + key.x]);
        appendBounds(bounds);
    });
    if (casters.size() != first) {
        auto &caster = casters.emplace_back();
        caster.bounds = unknownBounds ? VisualBounds{} : ownerBounds;
        caster.content = "terrain owner";
        AppendShadowValue(caster.content, owner);
        AppendShadowValue(caster.content, definition->transform.position);
        const auto &q = definition->transform.rotation;
        AppendShadowValue(caster.content, glm::vec4(q.x, q.y, q.z, q.w));
        caster.content += JudasLighting::ShadowRenderKey(*definition->render);
    }
}

void Runtime::Draw(Renderer &renderer) const {
    const auto *d = DrawDefinition();
    if (!d)
        return;
    renderer.SetRenderLayer(world.RenderLayerOf(owner));
    if (!renderer.AcceptsCurrentLayer())
        return;
    auto *resources = world.Resources();
    renderer.SetMaterialBindings(BuildRenderMaterialBindings(resources, *d->render));
    auto texture = resources ? resources->TryGetTexture(d->render->textureAsset) : TextureHandle{};
    auto materialTexture = [&](const AssetId& id) {
        if (!resources || id.empty())
            return TextureHandle{};
        resources->RequestTexture(id);
        return resources->TryGetTexture(id);
    };
    std::array<TextureHandle, 4> layers{};
    for (unsigned i = 0; i < 4; ++i)
        layers[i] = materialTexture(source->layerTextures[i]);
    const auto normal = materialTexture(source->materialDetail.normalTexture);
    const auto worldToTerrain=glm::inverse(glm::translate(glm::mat4(1), d->transform.position) *
                                           glm::mat4_cast(d->transform.rotation));
    renderer.BeginTerrainMaterial({}, layers, source->layerColours, worldToTerrain,
                                  source->materialDetail, normal);
    RendererProfileScope gpu(renderer, "Terrain V2 geometry");
    bool omitCaveNormal = normal.IsValid() && !renderer.Quality().caveDetailNormals;
    VisitDraws(*d, [&](const GridDraw &grid) {
        if (!renderer.IsVisible(grid.bounds))
            return;
        const auto &page = renderPages.Acquire(renderer, *source, grid.key);
        renderer.SetTerrainWeightPage(page.weights, page.weightRect);
        renderer.DrawTerrainGrid(
            page.heights, grid.cells,
            {float(grid.firstX), float(grid.firstZ), grid.stride, float(grid.cells)},
            grid.key.edges, {float(source->samplesX), float(source->samplesZ)}, source->spacing,
            grid.position, d->transform.rotation, grid.width, grid.minimum, grid.maximum,
            d->render->color, d->render->alpha, grid.coverage, true, grid.cellsZ, grid.depth);
    }, [&](const Resident &r, glm::vec3 position, const VisualBounds &bounds) {
        if (!renderer.IsVisible(bounds))
            return;
        if (omitCaveNormal) {
            renderer.BeginTerrainMaterial({}, layers, source->layerColours, worldToTerrain,
                                          source->materialDetail, {});
            omitCaveNormal = false;
        }
        const auto &c = r.geometry.chunk;
        const auto &page = renderPages.Acquire(
            renderer, *source, {{c.key.x, c.key.z, 0}, glm::vec4(1), false}, [&] {
                auto product = r.geometry.recipe->Acquire();
                return std::shared_ptr<const Chunk>(product, &product->chunk);
            });
        renderer.SetTerrainWeightPage(page.weights, page.weightRect);
        renderer.DrawMesh(page.mesh, position, d->transform.rotation, glm::vec3(1), texture,
                          d->render->color, d->render->alpha);
    });
    gpu.End();
    renderer.EndTerrainMaterial();
}
} // namespace TerrainV2
