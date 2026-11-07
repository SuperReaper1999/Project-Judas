#include "TerrainRuntime.h"
#include "CollisionAsset.h"
#include "PerformanceProfiler.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "TerrainAsset.h"
#include "TerrainMesher.h"
#include "WorldPresentation.h"
#include <algorithm>
#include <chrono>

namespace TerrainV2 {
namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
} // namespace
Runtime::Runtime(RuntimeWorld &w, uint64_t id) : world(w), owner(id) {}
Runtime::~Runtime() {
    ReleaseGraphics();
    if (job.IsValid() && world.Resources() && world.Resources()->Jobs()) {
        world.Resources()->Jobs()->Cancel(job);
        world.Resources()->Jobs()->Forget(job);
    }
    for (auto &[_, r] : chunks) {
        if (r.body.IsValid())
            world.Physics().DestroyBody(r.body);
    }
}
std::vector<BodyHandle> Runtime::Bodies() const {
    std::vector<BodyHandle> out;
    for (const auto &[_, r] : chunks)
        if (r.body.IsValid())
            out.push_back(r.body);
    return out;
}
bool Runtime::Owns(BodyHandle body) const {
    return body.IsValid() && body.world == Seed().world && bodyIds.count(body.id);
}
SolidSnapshot Runtime::Solid() const {
    const auto *d = Definition();
    if (!source || !d || !d->body || !d->body->enabled || d->body->sensor || !world.IsPublished(owner))
        return {};
    return {owner, geometryRevision, source, d->transform.position, d->transform.rotation, geometryEpoch};
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
    std::string material;
    float friction = d->body->friction, restitution = d->body->restitution;
    physics.GetPhysicalMaterial(Seed(), material, friction, restitution);
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
            physics.SetBodySensor(r.body, d->body->sensor);
            physics.SetBodyTags(r.body, world.TagsOf(owner));
            physics.SetPhysicalMaterial(r.body, material, friction, restitution);
        }
}
bool Runtime::Create(Source value, std::string &error) {
    if (source || pending) {
        error = "terrain already initialized";
        return false;
    }
    if (!Validate(value, error))
        return false;
    std::vector<ChunkKey> keys;
    for (unsigned z = 0; z < value.ChunksZ(); ++z)
        for (unsigned x = 0; x < value.ChunksX(); ++x)
            keys.push_back({x, z});
    return Submit(std::make_shared<const Source>(std::move(value)), std::move(keys), error);
}
bool Runtime::Submit(std::shared_ptr<const Source> value, std::vector<ChunkKey> keys,
                     std::string &error, bool replaceAll) {
    if (!ValidateSubmission(*value, error))
        return false;
    auto *resources = world.Resources();
    auto work = std::make_shared<Work>();
    work->baseAsset = baseAsset;
    work->baseFingerprint = baseFingerprint;
    work->replaceAll = replaceAll;
    if (!replaceAll)
        work->raster = raster;
    work->source = std::move(value);
    if (source && source->samplesX == work->source->samplesX &&
        source->samplesZ == work->source->samplesZ &&
        source->chunkCells == work->source->chunkCells &&
        source->spacing == work->source->spacing && source->heights == work->source->heights)
        work->lod = lod;
    const auto pendingCount = keys.size();
    auto handle = resources->Jobs()->Submit(
        [work, keys = std::move(keys)](JobContext &context) {
            if (!PrepareGeometry(*work, keys, [&] { return context.CancelRequested(); })) {
                if (context.CancelRequested())
                    context.ReportCancelled();
                else
                    context.SetError(work->error);
            }
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
bool Runtime::ValidateSubmission(const Source &value, std::string &error) const {
    // The same resident budget applies to creation, restoration and edits.
    // A valid file is not permission to bypass the current runtime budget.
    if (size_t(value.ChunksX()) * value.ChunksZ() > 1024) {
        error = "terrain active chunk budget exceeds 1024";
        return false;
    }
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
        if (value.samplesX > limit || value.samplesZ > limit) {
            error = "landscape sample page exceeds this device's texture limit; split the "
                    "source into smaller streamed regions";
            return false;
        }
    }
    return true;
}
bool Runtime::Install(PreparedAsset &asset, std::string &error) {
    if (source || pending || asset.consumed || !asset.geometry.source || !asset.geometry.lod ||
        !asset.geometry.error.empty() || asset.geometry.chunks.size() !=
            size_t(asset.geometry.source->ChunksX()) * asset.geometry.source->ChunksZ()) {
        error = "prepared terrain is incomplete, already consumed or owner is initialized";
        return false;
    }
    if (!ValidateSubmission(*asset.geometry.source, error))
        return false;
    std::set<ChunkKey> keys;
    for (const auto &part : asset.geometry.chunks)
        if (part.chunk.key.x >= asset.geometry.source->ChunksX() ||
            part.chunk.key.z >= asset.geometry.source->ChunksZ() || !keys.insert(part.chunk.key).second) {
            error = "prepared terrain chunk coverage is invalid";
            return false;
        }
    auto work = std::make_shared<Work>();
    static_cast<Preparation &>(*work) = std::move(asset.geometry);
    work->baseAsset = asset.asset;
    work->baseFingerprint = asset.fingerprint;
    asset.consumed = true;
    pending = std::move(work);
    const bool ok = Publish(error);
    pending.reset();
    if (ok)
        error.clear();
    status.error = error;
    return ok;
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
        if (r.body.IsValid()) {
            bodyIds.erase(r.body.id);
            physics.DestroyBody(r.body);
        }
        r.body = staged[i];
        if (r.body.IsValid())
            bodyIds.insert(r.body.id);
        r.geometry = std::move(p);
    }
    if (pending->replaceAll) {
        std::set<ChunkKey> retained;
        for (const auto &p : pending->chunks)
            retained.insert(p.chunk.key);
        for (auto it = chunks.begin(); it != chunks.end();)
            if (!retained.count(it->first)) {
                ReleaseMesh(it->second);
                if (it->second.body.IsValid()) {
                    bodyIds.erase(it->second.body.id);
                    physics.DestroyBody(it->second.body);
                }
                it = chunks.erase(it);
            } else
                ++it;
    }
    // The ordinary owner is only an identity/placement anchor once the first
    // terrain revision exists. Never leave an uncut heightfield underneath it.
    physics.SetBodyEnabled(seed, false);
    if (!pending->chunks.empty() || !source) {
        ++geometryRevision;
        geometryEpoch = std::make_shared<const unsigned char>(0);
    }
    CommitPresentation();
    source = pending->source;
    baseAsset = pending->baseAsset;
    baseFingerprint = pending->baseFingerprint;
    raster = std::move(pending->raster);
    lod = pending->lod;
    status.ready = true;
    ++status.revision;
    status.prepareMs = pending->milliseconds;
    status.samples = source->heights.size();
    status.bytes = status.samples * (sizeof(float) + sizeof(glm::vec4));
    status.triangles = 0;
    status.chunks = chunks.size();
    status.volumeChunks = 0;
    residentPayloadBytes = SourcePayloadBytes(*source, lod.get()) +
                           raster.size() * sizeof(RasterChanges::value_type);
    for (const auto &[_, r] : chunks) {
        const auto &c = r.geometry.chunk;
        status.triangles += c.mesh.indices.size() / 3;
        status.bytes +=
            c.mesh.vertices.size() * sizeof(MeshVertex) + c.mesh.indices.size() * sizeof(uint32_t);
        status.volumeChunks += c.volumetric;
        residentPayloadBytes += r.geometry.PayloadBytes();
    }
    Synchronize();
    status.publishMs = Milliseconds(start);
    return true;
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
} // namespace TerrainV2
