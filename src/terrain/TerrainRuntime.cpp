#include "TerrainRuntime.h"
#include "CollisionAsset.h"
#include "PerformanceProfiler.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneFingerprint.h"
#include "TerrainMesher.h"
#include "WorldPresentation.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

namespace TerrainV2 {
namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
std::string GeometryFingerprint(const Chunk &chunk) {
    // Collision identity depends on this chunk's actual local geometry, not
    // unrelated heights/materials elsewhere in the landscape. Canonical bits
    // keep cold restoration portable and avoid serializing the entire source
    // for every local edit.
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
Runtime::Runtime(RuntimeWorld &w, uint64_t id) : world(w), owner(id) {}
Runtime::~Runtime() {
    if (heightTexture.IsValid() && world.Resources() && world.Resources()->GetRenderer())
        world.Resources()->GetRenderer()->DestroyTexture(heightTexture);
    if (weightTexture.IsValid() && world.Resources() && world.Resources()->GetRenderer())
        world.Resources()->GetRenderer()->DestroyTexture(weightTexture);
    if (job.IsValid() && world.Resources() && world.Resources()->Jobs()) {
        world.Resources()->Jobs()->Cancel(job);
        world.Resources()->Jobs()->Forget(job);
    }
    for (auto &[_, r] : chunks) {
        ReleaseMesh(r);
        if (r.body.IsValid())
            world.Physics().DestroyBody(r.body);
    }
}
void Runtime::ReleaseMesh(Resident &r) const {
    if (r.mesh.IsValid() && world.Resources() && world.Resources()->GetRenderer())
        world.Resources()->GetRenderer()->DestroyMesh(r.mesh);
    r.mesh = {};
}
std::vector<BodyHandle> Runtime::Bodies() const {
    std::vector<BodyHandle> out;
    for (const auto &[_, r] : chunks)
        if (r.body.IsValid())
            out.push_back(r.body);
    return out;
}
bool Runtime::Owns(BodyHandle body) const {
    for (const auto &[_, r] : chunks)
        if (r.body.IsValid() && r.body.id == body.id)
            return true;
    return false;
}
const SceneObject *Runtime::Definition() const {
    auto it = world.m_scriptDefinitions.find(owner);
    return it == world.m_scriptDefinitions.end() ? nullptr : &it->second;
}
BodyHandle Runtime::Seed() const {
    auto it = world.m_entityCategories.find(owner);
    return it == world.m_entityCategories.end() ? BodyHandle{} : it->second.body;
}
void Runtime::Synchronize() {
    if (!source)
        return;
    const auto *d = Definition();
    if (!d || !d->body)
        return;
    auto &physics = world.Physics();
    physics.SetBodyEnabled(Seed(), false);
    for (auto &[_, r] : chunks)
        if (r.body.IsValid()) {
            const auto position =
                d->transform.position + d->transform.rotation * r.geometry.chunk.anchor;
            const auto pose = physics.GetTransform(r.body);
            if (pose.position != position || pose.rotation != d->transform.rotation)
                physics.ResetBody(r.body, position, d->transform.rotation);
            physics.SetBodyEnabled(r.body, world.IsPublished(owner) && d->body->enabled);
            physics.SetCollisionFilter(r.body, d->body->collisionLayer, d->body->collisionMask);
            physics.SetBodyTags(r.body, world.TagsOf(owner));
        }
}
bool Runtime::Create(Source value, std::string &error) {
    if (source || pending) {
        error = "terrain already initialized";
        return false;
    }
    if (!Validate(value, error))
        return false;
    if (size_t(value.ChunksX()) * value.ChunksZ() > 1024) {
        error = "terrain active chunk budget exceeds 1024";
        return false;
    }
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < value.ChunksZ(); ++z)
        for (unsigned x = 0; x < value.ChunksX(); ++x)
            keys.push_back({x, z});
    return Submit(std::make_shared<const Source>(std::move(value)), std::move(keys), error);
}
bool Runtime::Submit(std::shared_ptr<const Source> value, std::vector<ChunkKey> keys,
                     std::string &error) {
    if (pending) {
        error = "terrain edit pending; retry after publication";
        return false;
    }
    auto *d = Definition();
    auto *resources = world.Resources();
    if (!d || !d->body || d->body->motion != SceneBodyMotion::Static || d->parent || d->planet ||
        d->deformable || d->navigationSurface || d->transform.scale != glm::vec3(1) ||
        !Seed().IsValid()) {
        error = "landscape requires a root unit-scale static owner with no other geometry owner";
        return false;
    }
    if (!resources || !resources->Jobs()) {
        error = "terrain requires the project job system";
        return false;
    }
    if (auto *renderer = resources->GetRenderer()) {
        const auto limit = unsigned(std::max(0, renderer->Capabilities().limits.textureSize));
        if (value->samplesX > limit || value->samplesZ > limit) {
            error = "landscape sample page exceeds this device's texture limit; split the "
                    "source into smaller streamed regions";
            return false;
        }
    }
    auto work = std::make_shared<Work>();
    work->source = std::move(value);
    if (source && source->samplesX == work->source->samplesX &&
        source->samplesZ == work->source->samplesZ &&
        source->chunkCells == work->source->chunkCells &&
        source->spacing == work->source->spacing && source->heights == work->source->heights)
        work->lod = lod;
    const auto pendingCount = keys.size();
    auto handle = resources->Jobs()->Submit(
        [work, keys = std::move(keys)](JobContext &context) {
            const auto start = Clock::now();
            if (!work->lod)
                work->lod = std::make_shared<const HeightLod>(*work->source);
            std::vector<Chunk> prepared;
            if (!BuildChunks(*work->source, keys, prepared, work->error,
                             [&] { return context.CancelRequested(); })) {
                if (context.CancelRequested())
                    context.ReportCancelled();
                else
                    context.SetError(work->error);
                return;
            }
            for (auto &chunk : prepared) {
                if (context.CancelRequested()) {
                    context.ReportCancelled();
                    return;
                }
                Prepared p;
                p.chunk = std::move(chunk);
                if (!p.chunk.mesh.indices.empty()) {
                    p.collision = std::make_shared<CollisionAsset>();
                    if (!CookCollision(p.chunk.mesh, {}, *p.collision, work->error)) {
                        context.SetError(work->error);
                        return;
                    }
                    p.collision->sourceFingerprint = GeometryFingerprint(p.chunk);
                }
                work->chunks.push_back(std::move(p));
            }
            work->milliseconds = Milliseconds(start);
        },
        JobPriority::High, "terrain V2 dirty chunks");
    if (!handle.IsValid()) {
        error = "terrain job queue unavailable";
        return false;
    }
    pending = std::move(work);
    job = handle;
    status.busy = true;
    status.pendingChunks = pendingCount;
    status.error.clear();
    return true;
}
bool Runtime::Brush(glm::vec3 point, const Edit &brush, std::string &error) {
    if (!source) {
        error = "terrain not ready";
        return false;
    }
    auto *d = Definition();
    if (!d) {
        error = "terrain owner disappeared";
        return false;
    }
    auto next = std::make_shared<Source>(*source);
    auto edit = brush;
    edit.point = glm::inverse(d->transform.rotation) * (point - d->transform.position);
    next->edits.push_back(edit);
    if (!Validate(*next, error))
        return false;
    auto affected = Affected(*next, edit);
    if (affected.empty()) {
        error = "brush outside terrain";
        return false;
    }
    return Submit(std::move(next), std::move(affected), error);
}
bool Runtime::Publish(std::string &error) {
    JUDAS_PROFILE_SCOPE("Terrain V2 publish");
    const auto start = Clock::now();
    const auto *d = Definition();
    if (!d) {
        error = "terrain owner disappeared before publication";
        return false;
    }
    auto &physics = world.Physics();
    unsigned layer = 0;
    CategoryMask mask = 0;
    const auto seed = Seed();
    if (!physics.GetCollisionFilter(seed, layer, mask)) {
        error = "terrain seed body disappeared";
        return false;
    }
    std::vector<BodyHandle> staged;
    try {
        // Allocate every replacement before retiring any old support. No step
        // can observe a half-excavated boundary or an absent collision patch.
        for (const auto &p : pending->chunks) {
            BodyHandle body;
            if (p.collision) {
                body = physics.CreateShape(
                    Shape::Cooked(p.collision, ""),
                    {d->transform.position + d->transform.rotation * p.chunk.anchor,
                     d->transform.rotation},
                    false, 0, d->body->friction, d->body->restitution);
                physics.SetCollisionFilter(body, layer, mask);
                physics.SetBodyTags(body, world.TagsOf(owner));
            }
            staged.push_back(body);
        }
    } catch (const std::exception &e) {
        for (auto h : staged)
            if (h.IsValid())
                physics.DestroyBody(h);
        error = e.what();
        return false;
    }
    for (size_t i = 0; i < pending->chunks.size(); ++i) {
        auto &p = pending->chunks[i];
        auto &r = chunks[p.chunk.key];
        ReleaseMesh(r);
        if (r.body.IsValid())
            physics.DestroyBody(r.body);
        r.body = staged[i];
        r.geometry = std::move(p);
    }
    // The ordinary owner is only an identity/placement anchor once the first
    // terrain revision exists. Never leave an uncut heightfield underneath it.
    physics.SetBodyEnabled(seed, false);
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
    source = pending->source;
    lod = pending->lod;
    drawPatches.clear();
    status.ready = true;
    ++status.revision;
    status.prepareMs = pending->milliseconds;
    status.samples = source->heights.size();
    status.bytes = status.samples * (sizeof(float) + sizeof(glm::vec4));
    status.triangles = 0;
    status.chunks = chunks.size();
    status.volumeChunks = 0;
    for (const auto &[_, r] : chunks) {
        const auto &c = r.geometry.chunk;
        status.triangles += c.mesh.indices.size() / 3;
        status.bytes +=
            c.mesh.vertices.size() * sizeof(MeshVertex) + c.mesh.indices.size() * sizeof(uint32_t);
        status.volumeChunks += c.volumetric;
    }
    Synchronize();
    status.publishMs = Milliseconds(start);
    return true;
}
void Runtime::PrepareViews(const std::vector<PlanetView> &views, glm::dvec3 origin) {
    if (!source || !lod)
        return;
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
    drawPatches = lod->Select(localViews, volume);
}
void Runtime::Advance() {
    if (!pending)
        return;
    auto *jobs = world.Resources()->Jobs();
    if (!jobs->IsFinished(job))
        return;
    std::string error;
    if (jobs->StateOf(job) == JobState::Completed)
        Publish(error);
    else
        error = jobs->ErrorOf(job);
    jobs->Forget(job);
    job = {};
    pending.reset();
    status.busy = false;
    status.pendingChunks = 0;
    status.error = std::move(error);
}
void Runtime::Wait() {
    if (pending) {
        world.Resources()->Jobs()->Wait(job);
        Advance();
    }
}
void Runtime::Draw(Renderer &renderer) const {
    if (!source || !world.IsPublished(owner) || !world.RenderVisible(owner))
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
    if (!heightTexture.IsValid())
        heightTexture = renderer.CreateTexture(HeightImage(*source));
    std::array<TextureHandle, 4> layers;
    for (unsigned i = 0; i < 4; ++i)
        if (!source->layerTextures[i].empty()) {
            world.Resources()->RequestTexture(source->layerTextures[i]);
            layers[i] = world.Resources()->TryGetTexture(source->layerTextures[i]);
        }
    renderer.BeginTerrainMaterial(weightTexture, layers, source->layerColours,
                                  glm::inverse(glm::translate(glm::mat4(1), d->transform.position) *
                                               glm::mat4_cast(d->transform.rotation)));
    RendererProfileScope gpu(renderer, "Terrain V2 geometry");
    std::vector<HeightPatch> fallback;
    if (drawPatches.empty() && lod) {
        std::set<ChunkKey> volume;
        for (const auto &[k, r] : chunks)
            if (r.geometry.chunk.volumetric)
                volume.insert(k);
        fallback = lod->Select({}, volume);
    }
    const auto &patches = drawPatches.empty() ? fallback : drawPatches;
    for (const auto &patch : patches) {
        const auto k = patch.key;
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
            patch.edgeStrides, {float(source->samplesX), float(source->samplesZ)}, source->spacing,
            d->transform.position + d->transform.rotation * anchor, d->transform.rotation, width,
            patch.minimum, patch.maximum, d->render->color, d->render->alpha);
    }
    for (const auto &[_, r] : chunks) {
        auto &c = r.geometry.chunk;
        if (c.mesh.indices.empty() || !c.volumetric)
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
bool Runtime::Capture(std::string &bytes, std::string &error) const {
    if (pending) {
        error = "terrain edit pending";
        return false;
    }
    bytes = source ? Serialize(*source) : std::string{};
    return true;
}
bool Runtime::Restore(const std::string &bytes, std::string &error, bool asynchronous) {
    Source next;
    if (!Parse(bytes, next, error))
        return false;
    if (pending) {
        error = "terrain restore while edit pending";
        return false;
    }
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < next.ChunksZ(); ++z)
        for (unsigned x = 0; x < next.ChunksX(); ++x)
            keys.push_back({x, z});
    if (!Submit(std::make_shared<const Source>(std::move(next)), std::move(keys), error))
        return false;
    if (!asynchronous) {
        Wait();
        error = status.error;
        return error.empty() && status.ready;
    }
    return true;
}
} // namespace TerrainV2
