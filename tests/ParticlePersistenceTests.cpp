#include "JobSystem.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveEvolution.h"
#include "SaveStorage.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "ToolProcess.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "../third_party/nlohmann/json.hpp"

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace {
using Json = nlohmann::json;
unsigned checks = 0;
void Require(bool condition, const std::string &message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
    std::printf("PASS %s\n", message.c_str());
}
Scene Fixture() {
    Scene scene;
    scene.Settings().worldOrigin = {1e12, 0, 0};
    auto &emitter = scene.CreateObject("Reliable effect");
    emitter.transform.position = {.125f, 2, 0};
    emitter.particleEmitter = ParticleEmitterSettings{};
    auto &settings = *emitter.particleEmitter;
    settings.rate = 0; settings.burst = 0; settings.maxParticles = 16;
    settings.lifetime = 10;
    settings.velocity = {0, -4, 0}; settings.velocityVariation = settings.spread = glm::vec3(0);
    settings.collision = true; settings.impacts = ParticleImpactDelivery::Reliable;
    settings.maxImpactCells = 4;
    auto &floor = scene.CreateObject("Receiver");
    floor.body = SceneBodyComponent{};
    floor.body->halfExtents = {5, .5f, 5};
    return scene;
}
std::string Queue(RuntimeWorld &world, EntityId id) {
    std::string bytes, error;
    Require(world.CaptureParticleImpacts(id, bytes, error), "capture bounded owner continuation: " + error);
    return bytes;
}
void CreateImpact(RuntimeWorld &world, EntityId id) {
    Require(world.EmitParticleBurst(id, 1), "emit actual colliding visual particle");
    world.UpdateVisualParticles(.5f);
    const auto *pending = world.PeekParticleImpacts(id);
    Require(pending && pending->size() == 1 && pending->front().count == 1,
            "real snapshot collision produces one reliable aggregate");
    std::string error;
    Require(!world.ValidateEntityDestruction(pending->front().receiver, error) &&
                error.find("consume reliable") != std::string::npos,
            "generic destruction preflight preserves pending receiver observations");
}
bool Pump(WorldStreaming &stream, const std::function<bool()> &ready) {
    for (unsigned frame = 0; frame < 3000; ++frame) {
        stream.Advance(false);
        if (ready()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

void TransactionalQueues() {
    std::string error;
    Scene scene;
    for (unsigned index = 0; index < 2; ++index) {
        auto &object = scene.CreateObject("Reliable owner");
        object.particleEmitter = ParticleEmitterSettings{};
        object.particleEmitter->impacts = ParticleImpactDelivery::Reliable;
        object.particleEmitter->maxImpactCells = 4;
    }
    RuntimeWorld world;
    Require(world.Build(scene, nullptr, error), "build independent reliable queue owners");
    JudasParticles::ParticleImpact observation;
    observation.count = 1; observation.weight = 1;
    observation.normal = observation.localNormal = {0, 1, 0};
    for (auto &source : world.VisualEmitters())
        Require(source.pool.ImpactState().Add(observation, source.pool.settings), "seed first durable observation");
    SaveArchive old;
    world.PersistParticleImpacts(old);
    observation.localPosition.x = 3;
    for (auto &source : world.VisualEmitters())
        Require(source.pool.ImpactState().Add(observation, source.pool.settings), "change live queues independently of saved bytes");
    const auto first = Queue(world, 1), second = Queue(world, 2);
    bool rejected = false;
    try { SaveArchive corrupt(old.bytes + 'x'); world.PersistParticleImpacts(corrupt); }
    catch (const std::exception &) { rejected = true; }
    Require(rejected && Queue(world, 1) == first && Queue(world, 2) == second,
            "late corruption cannot partially replace either reliable owner queue");

    auto fixture = Fixture();
    fixture.Find(1)->particleEmitter->maxImpactCells = 1;
    RuntimeWorld blocked;
    Require(blocked.Build(fixture, nullptr, error), "build bounded backpressure persistence control");
    auto &pool = blocked.VisualEmitters()[0].pool;
    observation.receiver = 2; observation.localPosition.x = 100;
    Require(pool.ImpactState().Add(observation, pool.settings), "fill one-cell reliable delivery budget");
    Require(blocked.EmitParticleBurst(1, 1), "emit contact behind full reliable delivery queue");
    blocked.UpdateVisualParticles(.5f);
    Require(pool.PendingStep() && !blocked.PrepareParticlePersistence(error) && error == "loading",
            "partial captured simulation step defers persistence instead of saving incomplete motion");
    blocked.TakeParticleImpacts(1);
    blocked.UpdateVisualParticles(.5f);
    Require(!pool.PendingStep() && blocked.PrepareParticlePersistence(error) &&
                blocked.PeekParticleImpacts(1) && blocked.PeekParticleImpacts(1)->size() == 1,
            "drain resumes retained step and completed observations become serializable");
}

void Regional(const std::filesystem::path &directory) {
    std::string error;
    Project project;
    Require(Project::CreateNew(directory.string(), "particle-regions", project, error),
            "create ordinary project for regional continuation: " + error);
    auto fixture = Fixture();
    const auto emitterLocal = fixture.Objects()[0].id, receiverLocal = fixture.Objects()[1].id;
    auto producer = fixture;
    producer.Objects().erase(producer.Objects().begin() + 1);
    auto receiver = fixture;
    receiver.Objects().erase(receiver.Objects().begin());
    Require(SaveSceneToFile(producer, project.Resolve("Scenes/producer.judas"), error) &&
                SaveSceneToFile(receiver, project.Resolve("Scenes/receiver.judas"), error),
            "author independent producer and receiver regions");
    AssetDatabase assets;
    assets.Scan(project.RootDir(), project.AssetsDir());
    JobSystem jobs(1);
    ResourceManager resources(nullptr, &assets, &jobs);
    RuntimeWorld world;
    Scene bootstrap;
    bootstrap.Settings().worldOrigin = fixture.Settings().worldOrigin;
    Require(world.Build(bootstrap, &resources, error), "build empty authoritative streaming world");
    WorldRegion a, b;
    a.id = "producer"; a.scene = "Scenes/producer.judas";
    a.origin = fixture.Settings().worldOrigin;
    b = a; b.id = "receiver"; b.scene = "Scenes/receiver.judas";
    WorldManifest manifest;
    manifest.regions = {{a.id, a}, {b.id, b}};
    WorldStreaming stream(world, resources, project, manifest);
    auto first = stream.Request(a.id, false, error), second = stream.Request(b.id, false, error);
    Require(first && second && Pump(stream, [&] {
        return stream.Status(first)->state == "active" && stream.Status(second)->state == "active";
    }), "publish both regions through actual residency coordinator");
    auto emitter = stream.Resolve(a.id, emitterLocal), target = stream.Resolve(b.id, receiverLocal);
    CreateImpact(world, emitter);
    Require(world.PeekParticleImpacts(emitter)->front().receiver == target,
            "regional observation names the actual independent receiver");
    Require(stream.Release(first) && stream.Release(second), "release ordinary residency demands");
    for (unsigned frame = 0; frame < 8; ++frame) stream.Advance(false);
    bool producersPinned = false, receiversPinned = false;
    for (const auto &region : stream.Regions()) {
        const bool pinned = region.state == "active" && std::any_of(region.pins.begin(), region.pins.end(),
            [](const auto &reason) { return reason.find("reliable particle") != std::string::npos; });
        if (region.id == a.id) producersPinned = pinned;
        if (region.id == b.id) receiversPinned = pinned;
    }
    Require(producersPinned && receiversPinned,
            "undelivered reliable observations pin both producer and receiver regions");
    const auto delivered = world.TakeParticleImpacts(emitter);
    const auto next = delivered.back().sequence + 1;
    Require(Pump(stream, [&] { return stream.Stats().active == 0; }),
            "draining events permits actual regional suspension");
    Require(stream.ArchiveVersion() == 10, "only retained particle continuation opts into streaming archive10");
    SaveArchive saved;
    stream.Persist(saved, stream.ArchiveVersion());
    saved.Finish();
    Require(!saved.bytes.empty(), "streaming continuation enters a genuine bounded archive");
    first = stream.Request(a.id, false, error); second = stream.Request(b.id, false, error);
    Require(Pump(stream, [&] {
        return stream.Status(first)->state == "active" && stream.Status(second)->state == "active";
    }), "revisit reconstructs both regions with new runtime ownership");
    const auto returned = stream.Resolve(a.id, emitterLocal);
    Require(returned && returned != emitter, "regional emitter receives a new runtime identity");
    auto bytes = Queue(world, returned);
    SaveArchive counter(bytes);
    std::uint64_t restoredNext = 0; counter(restoredNext);
    Require(restoredNext == next, "empty reliable queue retains its monotonic sequence across unload/reload");
    CreateImpact(world, returned);
    Require(world.PeekParticleImpacts(returned)->front().sequence == next &&
                world.PeekParticleImpacts(returned)->front().receiver == stream.Resolve(b.id, receiverLocal),
            "resumed effect uses next sequence and newly resolved receiver, never stale body identity");
    world.TakeParticleImpacts(returned);
}

void Cold(const std::string &mode, const std::filesystem::path &directory) {
    const bool writing = mode == "write";
    Require(writing || mode == "read", "known cold process mode");
    SaveStorage storage((directory / "slots").string());
    GameSnapshot snapshot;
    RuntimeWorld world;
    Scene scene;
    std::string error;
    if (writing) {
        scene = Fixture();
        Require(world.Build(scene, nullptr, error), "build authored source without graphics dependencies: " + error);
        CreateImpact(world, 1);
        const auto *pending = world.PeekParticleImpacts(1);
        Require(pending->front().receiver == 2 && pending->front().position.x == 1e12 + .125,
                "known-answer impact retains receiver and double absolute position");
        Require(!world.VisualEmitters()[0].pool.Particles().empty(), "source has live visuals before cold save");
        const auto queue = Queue(world, 1);
        Require(WorldPersistence::Capture(world, snapshot.participants, error),
                "capture authoritative optional impact participant: " + error);
        Require(snapshot.participants.count("particle-impacts") == 1 &&
                    snapshot.participants.at("particle-impacts").version == 1,
                "reliable emitter requires version1 participant");
        snapshot.project = "particle-cold"; snapshot.scene = "pool";
        snapshot.content = std::string(64, 'a');
        snapshot.metadata = Json{{"queue", SceneFingerprintSha256(queue)},
                                {"sequence", pending->front().sequence}}.dump();
        Require(storage.Write("effect", snapshot, error), "atomic checksum-protected native save: " + error);
        auto policy = Json{{"format", 1}, {"compatibility", "particle-test"}, {"version", 2},
            {"sources", Json::array({{{"content", snapshot.content}, {"compatibility", ""}, {"version", 0}}})},
            {"steps", Json::array({{{"from", 0}, {"to", 1}, {"operations", Json::array()}},
                                  {{"from", 1}, {"to", 2}, {"operations", Json::array()}}})}};
        GameSnapshot migrated; std::string report;
        Require(EvolveSave(policy.dump(), std::string(64, 'b'), "", snapshot, true, migrated, report, error) &&
                    migrated.participants.at("particle-impacts").data == snapshot.participants.at("particle-impacts").data,
                "ordered project migration preserves opaque reliable observations exactly: " + error);
    } else {
        bool recovered = false;
        Require(storage.Read("effect", snapshot, recovered, error) && !recovered,
                "independent process reads the committed save: " + error);
        Require(WorldPersistence::SceneFrom(snapshot.participants, scene, error) &&
                    world.Build(scene, nullptr, error, nullptr, nullptr, true) &&
                    WorldPersistence::Prepare(world, error) &&
                    WorldPersistence::Restore(world, snapshot.participants, error),
                "cold world reconstructs and restores genuine participants: " + error);
        const auto metadata = Json::parse(snapshot.metadata);
        const auto queue = Queue(world, 1);
        Require(SceneFingerprintSha256(queue) == metadata.at("queue").get<std::string>(),
                "cold observation fields and next sequence exactly match the source");
        Require(world.VisualEmitters()[0].pool.Particles().empty(), "live particle visuals deliberately remain transient");
        const auto *pending = world.PeekParticleImpacts(1);
        Require(pending && pending->size() == 1 && pending->front().receiver == 2 &&
                    pending->front().position.x == 1e12 + .125 && pending->front().normal.y > .99f,
                "independent cold oracle checks actual receiver, witness precision and normal");
        Require(world.RestoreParticleImpacts(1, queue, error) && world.RestoreParticleImpacts(1, queue, error) &&
                    world.PeekParticleImpacts(1)->size() == 1 && Queue(world, 1) == queue,
                "repeated restore replaces one queue without replay or duplication");
        auto malformed = queue + 'x';
        Require(!world.RestoreParticleImpacts(1, malformed, error) && Queue(world, 1) == queue,
                "trailing continuation corruption rejects before queue publication");
        malformed = queue;
        for (std::size_t byte = 12; byte < 20; ++byte) malformed[byte] = 0;
        Require(!world.RestoreParticleImpacts(1, malformed, error) && Queue(world, 1) == queue,
                "invalid observation sequence rejects transactionally");
        auto absent = snapshot.participants;
        absent.erase("particle-impacts");
        Require(!WorldPersistence::Restore(world, absent, error) && Queue(world, 1) == queue,
                "missing required participant rejects without consuming pending observations");
        auto newer = snapshot.participants; newer.at("particle-impacts").version = 99;
        Require(!WorldPersistence::Restore(world, newer, error) && Queue(world, 1) == queue,
                "newer participant rejects explicitly");
        const auto delivered = world.TakeParticleImpacts(1);
        Require(delivered.size() == 1 && world.TakeParticleImpacts(1).empty(),
                "restored observation is delivered exactly once");
        CreateImpact(world, 1);
        Require(world.PeekParticleImpacts(1)->front().sequence == delivered[0].sequence + 1,
                "new collision continues monotonic sequence after cold load");
    }
    RuntimeWorld ordinary; Scene empty;
    empty.CreateObject("Ordinary visual").particleEmitter = ParticleEmitterSettings{};
    std::map<std::string, SaveChunk> chunks;
    Require(ordinary.Build(empty, nullptr, error) && WorldPersistence::Capture(ordinary, chunks, error) &&
                !chunks.count("particle-impacts"),
            "default visual-only worlds gain no new save participant");
}
void PersistentOwnership() {
    Scene scene;
    auto &object = scene.CreateObject("Persistent gameplay effect");
    object.body = SceneBodyComponent{};
    object.particleEmitter = ParticleEmitterSettings{};
    object.particleEmitter->impacts = ParticleImpactDelivery::Reliable;
    RuntimeWorld world;

    std::string error;
    Require(world.Build(scene, nullptr, error) && world.FindEntity(object.id),
            "reliable owner has a genuine entity record for exclusion controls");
    Require(!world.SetTransientEntity(object.id, true, error) &&
                error.find("persistent owner") != std::string::npos &&
                !world.IsTransientEntity(object.id),
            "reliable observations cannot disappear behind a transient-owner flag");
    auto settings = *object.particleEmitter;
    settings.impacts = ParticleImpactDelivery::Off;
    Require(world.SetParticleSettings(object.id, settings) &&
                world.SetTransientEntity(object.id, true, error),
            "ordinary visual-only body retains supported explicit exclusion");
    settings.impacts = ParticleImpactDelivery::Reliable;
    Require(!world.SetParticleSettings(object.id, settings) &&
                world.VisualEmitters()[0].pool.settings.impacts == ParticleImpactDelivery::Off,
            "transient owner cannot acquire reliable delivery through a runtime patch");
    Require(world.SetTransientEntity(object.id, false, error) &&
                world.SetParticleSettings(object.id, settings),
            "restoring persistent ownership permits reliable delivery again");
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 3) { Cold(argv[1], std::filesystem::absolute(argv[2])); }
        else if (argc == 1 || argc == 2) {
            const auto directory = argc == 2 ? std::filesystem::absolute(argv[1]) :
                std::filesystem::temp_directory_path() / ("judas-particle-persistence-v2-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            Require(!std::filesystem::exists(directory), "qualification output must be fresh");
            std::filesystem::create_directories(directory);
            for (const std::string mode : {"write", "read"}) {
                std::string output, error;
                const bool passed = CaptureToolOutput(std::filesystem::absolute(argv[0]),
                    {mode, directory.string()}, output, error, std::chrono::seconds(60), 65536);
                std::printf("%s", output.c_str());
                Require(passed, "cold child " + mode + ": " + error);
            }
            TransactionalQueues();
            PersistentOwnership();
            Regional(directory / "regional-project");
        } else throw std::runtime_error("usage: particle_persistence_tests [fresh-output] | write/read <output>");
        std::printf("Particle persistence: %u checks PASS\n", checks);
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "FAIL %s\n", failure.what()); return 1;
    }
}
