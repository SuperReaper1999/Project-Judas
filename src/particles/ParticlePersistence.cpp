#include "ParticleImpacts.h"
#include "../RuntimeWorld.h"
#include "../SaveArchive.h"

#include <algorithm>
#include <map>
#include <set>

namespace {
constexpr std::uint32_t kMaxEmitters = 8192;
constexpr std::uint32_t kMaxRecords = 65536;
constexpr std::uint64_t kMaxExactCount = 9007199254740991ull;
constexpr std::size_t kMaxQueueBytes = 8 * 1024 * 1024;
constexpr std::size_t kImpactBytes = 176; // Explicit fields below, never sizeof(ParticleImpact).

struct QueueSnapshot {
    std::uint64_t nextSequence = 1;
    std::vector<JudasParticles::ParticleImpact> records;
};

QueueSnapshot Capture(const VisualParticlePool &pool) {
    const auto *state = pool.ImpactState();
    return state ? QueueSnapshot{state->NextSequence(), state->Pending()} : QueueSnapshot{};
}

void Impact(SaveArchive &archive, JudasParticles::ParticleImpact &record) {
    archive(record.sequence, record.tick, record.receiver, record.geometryRevision,
            record.childKey, record.feature, record.position, record.localPosition,
            record.normal, record.localNormal, record.velocity, record.surfaceVelocity,
            record.color, record.size, record.age, record.count, record.weight);
    archive.Require(record.count > 0 && record.count <= kMaxExactCount && record.weight >= 0 &&
                        record.size >= 0 && record.age >= 0 &&
                        record.color.a >= 0 && record.color.a <= 1,
                    "invalid saved particle observation");
}

void Queue(SaveArchive &archive, QueueSnapshot &snapshot, unsigned capacity,
           std::uint32_t &total) {
    std::uint32_t count = std::uint32_t(snapshot.records.size());
    archive(snapshot.nextSequence, count);
    archive.Require(snapshot.nextSequence != 0 && count <= capacity &&
                        count <= kMaxRecords - total,
                    "saved particle impact capacity/sequence limit");
    total += count;
    if (archive.reading) {
        archive.Require(count <= (archive.bytes.size() - archive.position) / kImpactBytes,
                        "impossible saved particle observation count");
        snapshot.records.resize(count);
    }
    for (auto &record : snapshot.records) Impact(archive, record);
    // Validate sequence ordering and every native queue invariant before any
    // live queue is published. Restoring a queue never replays its observations.
    JudasParticles::ParticleImpacts validation;
    std::string error;
    archive.Require(validation.Restore(snapshot.records, snapshot.nextSequence, error),
                    "invalid saved particle impact queue: " + error);
}

bool Reliable(const RuntimeWorld &world, SceneObjectId id,
              const ParticleEmitterSettings &settings) {
    return settings.impacts == ParticleImpactDelivery::Reliable &&
           !world.IsTransientEntity(id) && world.RuntimeDefinition(id);
}
} // namespace

bool RuntimeWorld::PrepareParticlePersistence(std::string &error) const {
    for (const auto &emitter : m_particleEmitters)
        if (Reliable(*this, emitter.id, emitter.pool.settings) && emitter.pool.PendingStep()) {
            // SaveService already retries its ordinary preparation boundary.
            // A completed queue is serializable; a partially processed captured
            // particle step is deliberately not a portable simulation snapshot.
            error = "loading";
            return false;
        }
    error.clear();
    return true;
}

void RuntimeWorld::PersistParticleImpacts(SaveArchive &archive) {
    std::map<SceneObjectId, VisualParticlePool *> emitters;
    for (auto &emitter : m_particleEmitters)
        if (Reliable(*this, emitter.id, emitter.pool.settings)) {
            archive.Require(!emitter.pool.PendingStep(), "particle simulation boundary pending");
            archive.Require(emitters.emplace(emitter.id, &emitter.pool).second,
                            "duplicate reliable particle owner");
        }
    const auto definitions = ScriptObjects();
    const auto required = std::count_if(definitions.begin(), definitions.end(),
        [&](const auto &object) {
            return object.particleEmitter &&
                   Reliable(*this, object.id, *object.particleEmitter);
        });
    archive.Require(std::size_t(required) == emitters.size(),
                    "required reliable particle owner unavailable");
    std::uint32_t count = std::uint32_t(emitters.size()), total = 0;
    archive(count);
    archive.Require(count <= kMaxEmitters && count == emitters.size(),
                    "saved reliable particle owner count mismatch");
    struct Staged {
        JudasParticles::ParticleImpacts *target;
        JudasParticles::ParticleImpacts state;
    };
    std::vector<Staged> staged;
    std::set<SceneObjectId> seen;
    if (archive.reading) staged.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        SceneObjectId id = 0;
        if (!archive.reading) {
            auto source = emitters.begin();
            std::advance(source, index);
            id = source->first;
        }
        archive(id);
        const auto found = emitters.find(id);
        archive.Require(found != emitters.end() && seen.insert(id).second,
                        "unknown/duplicate saved particle owner");
        auto &pool = *found->second;
        auto snapshot = archive.reading ? QueueSnapshot{} : Capture(pool);
        Queue(archive, snapshot, pool.settings.maxImpactCells, total);
        if (archive.reading) {
            Staged next{&pool.ImpactState(), {}};
            std::string error;
            archive.Require(next.state.Restore(std::move(snapshot.records), snapshot.nextSequence, error),
                            "invalid saved particle queue: " + error);
            staged.push_back(std::move(next));
        }
    }
    // This method owns the complete participant. Reject trailing corruption
    // before replacing even the first queue in the private restoration world.
    archive.Finish();
    for (auto &next : staged) *next.target = std::move(next.state);
}

bool RuntimeWorld::CaptureParticleImpacts(EntityId id, std::string &bytes,
                                        std::string &error) const {
    try {
        const auto &emitters = m_particleEmitters;
        const auto source = std::find_if(emitters.begin(), emitters.end(),
                                        [&](const auto &emitter) { return emitter.id == id; });
        if (source == emitters.end() ||
            source->pool.settings.impacts != ParticleImpactDelivery::Reliable) {
            bytes.clear(); error.clear(); return true;
        }
        if (source->pool.PendingStep()) { error = "loading"; return false; }
        SaveArchive archive;
        auto snapshot = Capture(source->pool);
        std::uint32_t total = 0;
        Queue(archive, snapshot, source->pool.settings.maxImpactCells, total);
        archive.Require(archive.bytes.size() <= kMaxQueueBytes, "particle continuation exceeds 8 MiB");
        bytes = std::move(archive.bytes);
        error.clear(); return true;
    } catch (const std::exception &failure) { error = failure.what(); return false; }
}

bool RuntimeWorld::RestoreParticleImpacts(EntityId id, const std::string &bytes,
                                        std::string &error) {
    try {
        const auto source = std::find_if(m_particleEmitters.begin(), m_particleEmitters.end(),
                                        [&](const auto &emitter) { return emitter.id == id; });
        if (source == m_particleEmitters.end() ||
            source->pool.settings.impacts != ParticleImpactDelivery::Reliable) {
            error = "saved reliable particle owner unavailable"; return false;
        }
        SaveArchive archive(bytes);
        archive.Require(bytes.size() <= kMaxQueueBytes && !source->pool.PendingStep(),
                        "invalid/pending particle continuation");
        QueueSnapshot snapshot;
        std::uint32_t total = 0;
        Queue(archive, snapshot, source->pool.settings.maxImpactCells, total);
        archive.Finish();
        JudasParticles::ParticleImpacts candidate;
        archive.Require(candidate.Restore(std::move(snapshot.records), snapshot.nextSequence, error),
                        "invalid saved particle queue: " + error);
        source->pool.ImpactState() = std::move(candidate);
        error.clear(); return true;
    } catch (const std::exception &failure) { error = failure.what(); return false; }
}
