#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "particles/GpuParticles.h"
#include "particles/CollisionSnapshot.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace {
unsigned checks = 0;
constexpr float dt = .05f;
void Require(bool condition, const std::string &message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
    std::printf("PASS %s\n", message.c_str());
}
Scene Fixture(ParticleBackend backend, bool collision = false) {
    Scene scene;
    scene.Settings().worldOrigin = {1e12 - 4096, 0, 0};
    auto &emitter = scene.CreateObject("World-owned effect");
    emitter.transform.position = {.125f, 2, 0};
    emitter.particleEmitter = ParticleEmitterSettings{};
    auto &settings = *emitter.particleEmitter;
    settings.backend = backend; settings.collision = collision;
    settings.rate = 0; settings.burst = 0; settings.lifetime = 10; settings.maxParticles = 16;
    settings.velocity = {1, 0, 0}; settings.velocityVariation = settings.spread = glm::vec3(0);
    if (collision) {
        auto &floor = scene.CreateObject("Collidable owner");
        floor.body = SceneBodyComponent{}; floor.body->halfExtents = {5, .5f, 5};
    }
    return scene;
}
// Isolates world-owned snapshot preparation from rendering and fixed-step physics.
int IdleBenchmark(bool active = false, ParticleBackend backend = ParticleBackend::Cpu, ResourceManager *resources = nullptr) {
    for (unsigned emitters : {0u, 1u, 12u}) {
        Scene scene;
        for (unsigned i = 0; i < 250; ++i) {
            auto &body = scene.CreateObject("Static prop");
            body.transform.position = {float(i % 25) * 3, 0, float(i / 25) * 3};
            body.body = SceneBodyComponent{};
        }
        for (unsigned i = 0; i < emitters; ++i) {
            auto &object = scene.CreateObject("Parked CPU effect");
            object.transform.position = {0, -20, 0};
            object.particleEmitter = ParticleEmitterSettings{};
            auto &settings = *object.particleEmitter;
            settings.backend = backend; settings.collision = true;
            settings.rate = 0; settings.burst = active ? 1 : 0; settings.loop = false;
            settings.lifetime = 3600; settings.velocity = settings.velocityVariation = glm::vec3(0);
            settings.spread = glm::vec3(0); settings.useGravity = false;
            settings.maxParticles = 192; settings.collisionRadius = .01f;
        }
        RuntimeWorld world; world.legacyGameplay = false;
        std::string error;
        Require(world.Build(scene, resources, error), "collision benchmark fixture: " + error);
        std::vector<double> rows;
        for (unsigned tick = 0; tick < 330; ++tick) {
            const auto start = std::chrono::steady_clock::now();
            world.UpdateVisualParticles(1.f / 60);
            const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (tick >= 30) rows.push_back(ms);
        }
        std::uint64_t uploadBytes = 0, sharedBytes = 0;
        double captureMs = 0;
        for (const auto &emitter : world.VisualEmitters()) {
            if (emitter.gpu) { uploadBytes += emitter.gpu->Diagnostics().geometryUploadBytes; sharedBytes += emitter.gpu->Diagnostics().geometrySharedBytes; }
            captureMs += emitter.runtime.snapshotMs;
        }
        std::printf("CAPTURE active=%d backend=%u emitters=%u last_capture_ms=%.9f geometry_upload_bytes=%llu shared_bytes=%llu\n",
                    active, unsigned(backend), emitters, captureMs, (unsigned long long)uploadBytes, (unsigned long long)sharedBytes);
        std::sort(rows.begin(), rows.end());
        double sum = 0; for (double ms : rows) sum += ms;
        std::printf("IDLE emitters=%u bodies=250 samples=300 mean_ms=%.9f median_ms=%.9f p95_ms=%.9f worst_ms=%.9f\n",
                    emitters, sum / rows.size(), rows[150], rows[284], rows.back());
        world.Destroy();
    }
    return 0;
}
void SharedSnapshots() {
    auto scene = Fixture(ParticleBackend::Cpu, true);
    scene.Objects()[1].body->motion = SceneBodyMotion::Kinematic;
    scene.Objects()[0].particleEmitter->response = ParticleResponse::Stick;
    scene.Objects()[0].particleEmitter->velocity = {0,-40,0};
    scene.Objects()[0].particleEmitter->useGravity = false;
    const auto original = *scene.Objects()[0].particleEmitter;
    auto &second = scene.CreateObject("Compatible emitter");
    second.transform = scene.Objects()[0].transform; second.particleEmitter = original;
    RuntimeWorld world; world.legacyGameplay = false; std::string error;
    Require(world.Build(scene, nullptr, error) && world.EmitParticleBurst(1,1) && world.EmitParticleBurst(3,1),
            "compatible live CPU emitters build and emit");
    world.UpdateVisualParticles(dt);
    const auto first = world.VisualEmitters()[0].collision;
    Require(first && first == world.VisualEmitters()[1].collision &&
                world.GetParticleRuntimeStats(3).snapshotMs == 0,
            "compatible emitters share one captured snapshot, not two measured captures");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision != first &&
                world.VisualEmitters()[0].collision == world.VisualEmitters()[1].collision &&
                world.VisualEmitters()[0].collision->Colliders()[0].geometryRevision == first->Colliders()[0].geometryRevision,
            "next boundary refreshes the shared snapshot and preserves geometry history");
    Require(world.Physics().SetKinematicVelocity(world.RuntimeBody(2),{1,0,0},{0,0,0}),
            "shared stick receiver starts native prescribed movement");
    world.Physics().Step(dt);
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision == world.VisualEmitters()[1].collision &&
                std::abs(world.VisualEmitters()[0].pool.Particles()[0].position.x-.175f)<1e-5f &&
                world.VisualEmitters()[0].pool.Particles()[0].position == world.VisualEmitters()[1].pool.Particles()[0].position,
            "shared fresh motion preserves actual stick anchors on a moving native body");
    world.ControlParticles(3,"pause"); world.UpdateVisualParticles(dt);
    world.ControlParticles(3,"play"); world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision != world.VisualEmitters()[1].collision,
            "divergent capture histories do not share or reassign stick-anchor revisions");
    world.Destroy();
    scene.Objects()[2].particleEmitter->collisionLayers = 0;
    Require(world.Build(scene, nullptr, error) && world.EmitParticleBurst(1,1) && world.EmitParticleBurst(3,1),
            "different-filter control builds");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision != world.VisualEmitters()[1].collision &&
                !world.VisualEmitters()[0].collision->Bodies().empty() && world.VisualEmitters()[1].collision->Bodies().empty(),
            "different layers never borrow a more permissive or restrictive snapshot");
    world.Destroy();
    scene.Objects()[2].particleEmitter->collisionLayers = original.collisionLayers;
    scene.Objects()[0].body = SceneBodyComponent{};
    scene.Objects()[2].body = SceneBodyComponent{};
    Require(world.Build(scene, nullptr, error) && world.EmitParticleBurst(1,1) && world.EmitParticleBurst(3,1),
            "own-body exclusion control builds");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision != world.VisualEmitters()[1].collision,
            "different emitting body exclusions do not share even with matching layers");
    world.Destroy();

    scene = Fixture(ParticleBackend::Cpu, true);
    const auto base = *scene.Objects()[0].particleEmitter;
    for (unsigned i=1;i<65;++i) {
        auto &effect = scene.CreateObject("Bounded cache overflow control");
        effect.transform = scene.Objects()[0].transform; effect.particleEmitter = base;
        effect.particleEmitter->collisionLayers = std::uint64_t(i+1);
    }
    scene.Objects()[0].particleEmitter->collisionLayers = 1;
    Require(world.Build(scene,nullptr,error), "65 incompatible capture groups build");
    bool emitted=true;
    for(const auto &effect:world.VisualEmitters()) emitted &= world.EmitParticleBurst(effect.id,1);
    Require(emitted,"all 65 overflow controls emit");
    world.UpdateVisualParticles(dt);
    const auto overflow=world.VisualEmitters().back().collision;
    Require(overflow && overflow->Bodies().size()==1 &&
                overflow!=world.VisualEmitters().front().collision,
            "cache overflow captures independently with full collision coverage");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters().back().collision!=overflow &&
                world.VisualEmitters().back().collision->Colliders()[0].geometryRevision==overflow->Colliders()[0].geometryRevision,
            "overflow retains its own geometry history across fixed boundaries");
    world.Destroy();
}
void IdleCpuLifecycle() {
    auto scene = Fixture(ParticleBackend::Cpu, true);
    auto &settings = *scene.Objects()[0].particleEmitter;
    settings.loop = false; settings.velocity = {0, -40, 0};
    settings.useGravity = false; settings.restitution = 1; settings.collisionRadius = .1f;
    RuntimeWorld world; world.legacyGameplay = false;
    std::string error;
    Require(world.Build(scene, nullptr, error), "CPU collision lifecycle builds without graphics: " + error);
    world.UpdateVisualParticles(dt);
    Require(!world.VisualEmitters()[0].collision && world.GetParticleRuntimeStats(1).snapshotBodies == 0 &&
                world.GetParticleRuntimeStats(1).snapshotMs == 0,
            "idle CPU emitter performs no collision capture");
    Require(world.EmitParticleBurst(1, 1), "idle colliding CPU emitter accepts immediate burst");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision && world.GetParticleRuntimeStats(1).contacts == 1 &&
                world.VisualEmitters()[0].pool.Particles()[0].velocity.y > 0,
            "first live step captures geometry and bounces against the actual floor");
    const auto snapshot = world.VisualEmitters()[0].collision;
    Require(world.ControlParticles(1, "clear"), "clear retires the live CPU population");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision == snapshot && world.GetParticleRuntimeStats(1).snapshotMs == 0 &&
                world.GetParticleRuntimeStats(1).snapshotBodies == 0,
            "return to idle retains geometry history but reports no current capture work");
    auto changed = world.VisualEmitters()[0].pool.settings;
    changed.loop = true; changed.rate = 20;
    Require(world.SetParticleSettings(1, changed), "enable ordinary rate emission on idle CPU emitter");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].pool.Particles().size() == 1 &&
                world.VisualEmitters()[0].collision == snapshot,
            "empty tick advances emission credit and creates a birth without moving it");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision != snapshot && world.GetParticleRuntimeStats(1).contacts >= 2,
            "automatic birth receives fresh collision before its first movement");
    world.Destroy();

    scene = Fixture(ParticleBackend::Cpu, true);
    scene.Objects()[0].particleEmitter->burst = 1;
    Require(world.Build(scene, nullptr, error), "authored initial-burst fixture builds");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].pool.Particles().size() == 1 && !world.VisualEmitters()[0].collision,
            "authored initial burst is not lost when its initial empty step skips capture");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].collision != nullptr,
            "authored initial birth captures on the next step before movement");
    world.Destroy();

    scene = Fixture(ParticleBackend::Auto, true);
    scene.Objects()[0].particleEmitter->velocity = {0, -40, 0};
    scene.Objects()[0].particleEmitter->useGravity = false;
    scene.Objects()[0].particleEmitter->collisionRadius = .1f;
    Require(world.Build(scene, nullptr, error) && world.EmitParticleBurst(1, 1),
            "Auto queues an explicit burst before headless fallback");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].cpuPinned && world.VisualEmitters()[0].collision &&
                world.GetParticleRuntimeStats(1).contacts == 1,
            "queued Auto fallback burst captures before moving and collides on that tick");
    world.Destroy();

    scene = Fixture(ParticleBackend::Cpu, true);
    scene.Objects()[0].particleEmitter->burst = 1;
    for (unsigned i = 0; i < 512; ++i) {
        auto &body = scene.CreateObject("Snapshot limit control");
        body.transform.position = {float(i) * 2, -10, 0};
        body.body = SceneBodyComponent{};
    }
    Require(world.Build(scene, nullptr, error), "over-limit collision fixture builds");
    world.UpdateVisualParticles(dt);
    Require(world.VisualEmitters()[0].pool.Particles().size() == 1 &&
                !world.VisualEmitters()[0].collision,
            "idle emitter does not need geometry even in an over-limit world");
    const auto before = world.VisualEmitters()[0].pool.Particles()[0];
    world.UpdateVisualParticles(dt);
    Require(world.GetParticleRuntimeStats(1).queryFailures == 1 &&
                world.GetParticleRuntimeStats(1).diagnostic.find("snapshot unavailable") != std::string::npos &&
                world.VisualEmitters()[0].pool.Particles()[0].position == before.position &&
                world.VisualEmitters()[0].pool.Particles()[0].age == before.age,
            "first live step still refuses over-limit geometry without silently disabling collision");
    world.Destroy();
}
std::vector<JudasParticles::GpuParticleState> Read(JudasParticles::GpuParticles &gpu) {
    std::vector<JudasParticles::GpuParticleState> result;
    std::string error;
    Require(gpu.ReadStateForDiagnostics(result, error), "explicit diagnostic GPU readback: " + error);
    return result;
}
unsigned Live(const std::vector<JudasParticles::GpuParticleState> &state) {
    return unsigned(std::count_if(state.begin(), state.end(), [](const auto &particle) {
        return particle.identity[0] != 0;
    }));
}
void Lifecycle(EngineHost &host) {
    RuntimeWorld world;
    world.legacyGameplay = false;
    std::string error;
    Require(world.Build(Fixture(ParticleBackend::Gpu), &host.Resources(), error),
            "ordinary world construction selects authored required GPU backend");
    Require(world.EmitParticleBurst(1, 3) && world.VisualEmitters()[0].pool.Particles().empty(),
            "start-time burst queues without creating CPU particles before backend initialization");
    world.UpdateVisualParticles(dt);
    auto &emitter = world.VisualEmitters()[0];
    Require(emitter.gpu && emitter.gpu->Diagnostics().emitted == 3,
            "first actual world tick initializes GPU and consumes queued burst exactly once");
    const auto first = Read(*emitter.gpu);
    Require(Live(first) == 3, "resident first-tick particle count matches requested burst");
    world.UpdateVisualParticles(dt);
    const auto second = Read(*emitter.gpu);
    bool advanced = true;
    for (std::size_t index = 0; index < first.size(); ++index) if (first[index].identity[0])
        advanced &= second[index].identity == first[index].identity &&
                    std::abs(second[index].positionAge[0] - first[index].positionAge[0] - dt) < 1e-6 &&
                    second[index].positionAge[3] > first[index].positionAge[3];
    Require(advanced, "subsequent ordinary world tick advances resident positions and ages");
    auto settings = emitter.pool.settings;
    settings.enabled = false;
    Require(world.SetParticleSettings(1, settings) && !world.EmitParticleBurst(1, 2),
            "disabled required-GPU emitter rejects explicit bursts like CPU");
    const auto emitted = emitter.gpu->Diagnostics().emitted;
    world.UpdateVisualParticles(dt);
    const auto disabled = Read(*emitter.gpu);
    bool aged = false;
    for (std::size_t index = 0; index < second.size(); ++index) if (second[index].identity[0])
        aged |= disabled[index].positionAge[3] > second[index].positionAge[3];
    Require(aged && Live(disabled) == 3 && emitter.gpu->Diagnostics().emitted == emitted,
            "disabled visual particles age without hidden emission");
    settings.enabled = true;
    Require(world.SetParticleSettings(1, settings) && world.ControlParticles(1, "pause"),
            "runtime settings and pause act on the existing world-owned emitter");
    const auto paused = Read(*emitter.gpu);
    world.UpdateVisualParticles(dt);
    const auto frozen = Read(*emitter.gpu);
    bool same = paused.size() == frozen.size();
    for (std::size_t index = 0; index < paused.size() && same; ++index)
        same &= paused[index].identity == frozen[index].identity &&
                paused[index].positionAge == frozen[index].positionAge &&
                paused[index].velocityLife == frozen[index].velocityLife;
    Require(same && world.GetParticleRuntimeStats(1).paused, "pause freezes actual resident state, not just diagnostics");
    Require(world.ControlParticles(1, "play"), "play resumes the same emitter");
    const glm::dvec3 shift(512, 0, -512);
    const bool requested = world.RequestOrigin(world.Settings().worldOrigin + shift, error);
    Require(requested, "ordinary origin request remains inside the native absolute range: " + error);
    Require(world.CommitOrigin(), "ordinary live-origin transaction includes resident particles");
    const auto translated = Read(*emitter.gpu);
    bool equivalent = true;
    for (std::size_t index = 0; index < paused.size(); ++index) if (paused[index].identity[0])
        equivalent &= translated[index].identity == paused[index].identity &&
                      translated[index].positionAge[3] == paused[index].positionAge[3] &&
                      glm::length(glm::dvec3(translated[index].positionAge[0], translated[index].positionAge[1], translated[index].positionAge[2]) +
                                  shift - glm::dvec3(paused[index].positionAge[0], paused[index].positionAge[1], paused[index].positionAge[2])) < 5e-5;
    Require(equivalent, "origin shift preserves absolute resident pose, age and birth identity");
    Require(world.ControlParticles(1, "stop") && !world.EmitParticleBurst(1, 1),
            "stop blocks new emission while retaining existing particles");
    world.UpdateVisualParticles(dt);
    Require(Live(Read(*emitter.gpu)) == 3, "stopped emitter continues existing live visuals");
    Require(world.ControlParticles(1, "restart"), "explicit restart clears and resets resident effect");
    Require(Live(Read(*emitter.gpu)) == 0 && world.EmitParticleBurst(1, 2),
            "restarted effect accepts a fresh bounded burst");
    world.UpdateVisualParticles(dt);
    Require(Live(Read(*emitter.gpu)) == 2, "restarted effect remains GPU-backed and usable");
    settings = emitter.pool.settings;
    settings.backend = ParticleBackend::Cpu;
    Require(world.SetParticleSettings(1, settings), "explicit backend switch retains live particles");
    const auto switched = world.GetParticleRuntimeStats(1);
    Require(!emitter.gpu && emitter.pool.Particles().size() == 2 &&
                switched.active == ParticleBackend::Cpu && switched.gpuBytes == 0,
            "immediate backend readback reports the actual CPU owner and released GPU memory before another tick");
    world.Destroy();
    Require(world.VisualEmitters().empty(), "world teardown retires emitter owners before graphics context shutdown");
}
void Fallback(EngineHost &host) {
    RuntimeWorld world;
    world.legacyGameplay = false;
    std::string error;
    Require(world.Build(Fixture(ParticleBackend::Auto, true), &host.Resources(), error) &&
                world.EmitParticleBurst(1, 2), "ordinary Auto collision effect queues start-time burst");
    world.UpdateVisualParticles(dt);
    auto &emitter = world.VisualEmitters()[0];
    Require(emitter.gpu && Live(Read(*emitter.gpu)) == 2, "supported stationary scene initially uses resident GPU state");
    Require(world.ControlParticles(1, "restart") && world.EmitParticleBurst(1, 2),
            "restart before backend transfer accepts a fresh generation");
    world.UpdateVisualParticles(dt);
    Require(emitter.gpu->Diagnostics().emitted == 2,
            "restart resets birth continuation metadata before a later CPU transfer");
    const auto moving = world.RuntimeBody(2);
    Require(world.Physics().SetMotionType(moving, BodyMotionType::Kinematic) &&
                world.Physics().SetKinematicVelocity(moving, {1, 0, 0}, {0, 0, 0}),
            "change real collidable owner to prescribed motion");
    world.Physics().Step(dt);
    world.UpdateVisualParticles(dt);
    const auto stats = world.GetParticleRuntimeStats(1);
    Require(!emitter.gpu && emitter.pool.Particles().size() == 2 &&
                stats.active == ParticleBackend::Cpu && stats.stateTransferBytes > 0 && !stats.diagnostic.empty(),
            "unsupported moving collision transfers live state once to explicit CPU fallback");
    Require(world.EmitParticleBurst(1, 1), "fallback emitter continues accepting new effects");
    world.UpdateVisualParticles(dt);
    Require(emitter.pool.Particles().size() == 3 && !emitter.gpu,
            "CPU fallback remains active and does not repeatedly transfer or lose particles");
    world.Destroy();
}
void SelectedGravity(EngineHost &host) {
    Scene scene = Fixture(ParticleBackend::Gpu);
    auto &source = scene.CreateObject("Selected radial gravity");
    source.transform.position = {.125f, 0, 0};
    source.gravity = SceneGravityComponent{};
    source.gravity->kind = SceneGravityKind::Radial;
    source.gravity->magnitude = 10;
    const auto sourceId = source.id;
    auto *definition = scene.Find(1);
    definition->particleEmitter->useGravity = true;
    definition->particleEmitter->velocity = glm::vec3(0);
    GravitySelection selected;
    selected.mode = GravitySelection::Mode::Field;
    selected.source = sourceId;
    definition->gravitySelection = selected;
    RuntimeWorld world;
    world.legacyGameplay = false;
    std::string error;
    Require(world.Build(scene, &host.Resources(), error) && world.EmitParticleBurst(1, 1),
            "actual world publishes an explicitly selected radial particle source");
    world.UpdateVisualParticles(dt);
    world.UpdateVisualParticles(dt);
    auto &emitter = world.VisualEmitters()[0];
    const auto first = Read(*emitter.gpu);
    const auto live = std::find_if(first.begin(), first.end(), [](const auto &p) { return p.identity[0]; });
    Require(live != first.end() && std::abs(live->velocityLife[1] + .5f) < 1e-6 &&
                std::abs(live->positionAge[1] - 1.975f) < 1e-6,
            "radial descriptor follows the independent constant-magnitude inward oracle");
    Require(world.RequestOrigin(world.Settings().worldOrigin + glm::dvec3(512, 0, -512), error) &&
                world.CommitOrigin(), "selected radial source and GPU particles share the origin transaction");
    world.UpdateVisualParticles(dt);
    const auto shifted = Read(*emitter.gpu);
    const auto after = std::find_if(shifted.begin(), shifted.end(), [](const auto &p) { return p.identity[0]; });
    Require(after != shifted.end() && std::abs(after->velocityLife[1] + 1.f) < 1e-5 &&
                std::abs(after->positionAge[1] - 1.925f) < 1e-5 &&
                std::abs(after->velocityLife[0]) < 1e-6 && std::abs(after->velocityLife[2]) < 1e-6,
            "rebasing preserves the actual radial acceleration, not merely the cached source label");
    selected.mode = GravitySelection::Mode::Uniform;
    selected.source = 0;
    selected.acceleration = {10, 0, 0};
    Require(world.SetGravitySelection(1, selected, error), "runtime gravity selection can rotate particle acceleration");
    world.UpdateVisualParticles(dt);
    const auto uniform = Read(*emitter.gpu);
    const auto rotated = std::find_if(uniform.begin(), uniform.end(), [](const auto &p) { return p.identity[0]; });
    Require(rotated != uniform.end() && std::abs(rotated->velocityLife[0] - .5f) < 1e-5 &&
                std::abs(rotated->velocityLife[1] + 1.f) < 1e-5,
            "rotated uniform gravity changes the real resident velocity without resetting prior motion");
    world.Destroy();
}
void SharedGpu(EngineHost &host) {
    auto scene = Fixture(ParticleBackend::Auto, true);
    scene.Objects()[0].particleEmitter->velocity = {0,-40,0};
    scene.Objects()[0].particleEmitter->useGravity = false;
    auto settings = *scene.Objects()[0].particleEmitter;
    auto &second = scene.CreateObject("Shared GPU emitter");
    second.transform = scene.Objects()[0].transform; second.particleEmitter = settings;
    RuntimeWorld shared, control; shared.legacyGameplay = control.legacyGameplay = false;
    std::string error;
    Require(shared.Build(scene,&host.Resources(),error) && control.Build(scene,&host.Resources(),error),
            "shared and independent GPU worlds build from identical authored geometry");
    // Independent histories force the control to capture/upload separately.
    for (auto &effect : control.VisualEmitters())
        effect.collision = std::make_shared<JudasParticles::CollisionSnapshot>();
    for (auto *world : {&shared,&control})
        Require(world->EmitParticleBurst(1,3) && world->EmitParticleBurst(3,3), "both GPU fixtures queue identical births");
    for (unsigned tick=0;tick<4;++tick) {
        shared.UpdateVisualParticles(dt); control.UpdateVisualParticles(dt);
        for (unsigned i=0;i<2;++i) {
            const auto a=Read(*shared.VisualEmitters()[i].gpu), b=Read(*control.VisualEmitters()[i].gpu);
            bool same=a.size()==b.size();
            for(std::size_t j=0;j<a.size()&&same;++j)
                same=a[j].positionAge==b[j].positionAge&&a[j].velocityLife==b[j].velocityLife&&a[j].identity==b[j].identity;
            Require(same,"shared collision buffers preserve exact resident particle trajectories");
        }
    }
    Require(shared.VisualEmitters()[1].gpu->Diagnostics().geometryUploadBytes==0 &&
                shared.VisualEmitters()[1].gpu->Diagnostics().geometrySharedBytes>0 &&
                control.VisualEmitters()[1].gpu->Diagnostics().geometryUploadBytes>0,
            "compatible GPU emitter aliases actual immutable storage; independent control uploads");
    auto changed=shared.VisualEmitters()[0].pool.settings; changed.backend=ParticleBackend::Cpu;
    Require(shared.SetParticleSettings(1,changed),"retire the original GPU geometry owner into CPU");
    shared.UpdateVisualParticles(dt);
    Require(!shared.VisualEmitters()[0].gpu && shared.VisualEmitters()[1].gpu &&
                Live(Read(*shared.VisualEmitters()[1].gpu))==3,
            "remaining GPU consumer survives source-owner teardown with real live particle state");
    auto transform=scene.Objects()[1].transform; transform.position.y=.3f;
    Require(shared.SetRuntimeTransform(2,transform),"edit collision geometry placement after sharing");
    const auto bytes=shared.VisualEmitters()[1].gpu->Diagnostics().geometryUploadBytes;
    shared.UpdateVisualParticles(dt);
    Require(shared.VisualEmitters()[1].gpu && shared.VisualEmitters()[1].gpu->Diagnostics().geometryUploadBytes>bytes,
            "changed geometry invalidates the retained shared upload and publishes a new allocation");
    shared.Destroy(); control.Destroy();
}
void Portable(EngineHost &host) {
    RuntimeWorld automatic, required;
    automatic.legacyGameplay = required.legacyGameplay = false;
    std::string error;
    Require(automatic.Build(Fixture(ParticleBackend::Auto), &host.Resources(), error) &&
                automatic.EmitParticleBurst(1, 3), "portable Auto emitter queues its pre-init burst");
    automatic.UpdateVisualParticles(dt);
    Require(!automatic.VisualEmitters()[0].gpu && automatic.VisualEmitters()[0].pool.Particles().size() == 3 &&
                automatic.GetParticleRuntimeStats(1).active == ParticleBackend::Cpu &&
                !automatic.GetParticleRuntimeStats(1).diagnostic.empty(),
            "portable Auto fallback consumes queued burst through real CPU simulation");
    Require(required.Build(Fixture(ParticleBackend::Gpu), &host.Resources(), error) && required.EmitParticleBurst(1, 3),
            "portable required-GPU authoring remains inspectable");
    required.UpdateVisualParticles(dt);
    Require(!required.VisualEmitters()[0].gpu && required.VisualEmitters()[0].pool.Particles().empty() &&
                required.GetParticleRuntimeStats(1).diagnostic.find("required GPU") != std::string::npos,
            "unsupported required GPU reports refusal instead of silently substituting CPU");
    automatic.Destroy(); required.Destroy();
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "--idle-benchmark") == 0) return IdleBenchmark();
        if (argc == 2 && std::strcmp(argv[1], "--active-benchmark") == 0) return IdleBenchmark(true);
        if (argc == 2 && std::strcmp(argv[1], "--gpu-benchmark") == 0) {
            EngineHost host; std::string error;
            Require(host.Init("Shared collision benchmark", 128, 128, false, error), error);
            Require(host.GraphicsCaps().modernTier, "GPU benchmark requires modern graphics");
            return IdleBenchmark(true, ParticleBackend::Gpu, &host.Resources());
        }
        if (argc == 2 && std::strcmp(argv[1], "--cpu-only") == 0) {
            SharedSnapshots(); IdleCpuLifecycle();
            std::printf("CPU world: %u checks PASS\n", checks);
            return 0;
        }
        SharedSnapshots(); IdleCpuLifecycle();
        EngineHost host;
        std::string error;
        Require(host.Init("Particle world lifecycle", 128, 128, false, error), "actual renderer/compute context: " + error);
        if (host.GraphicsCaps().modernTier) { Lifecycle(host); Fallback(host); SelectedGravity(host); SharedGpu(host); }
        else Portable(host);
        host.Shutdown();
        std::printf("Particle world: %u checks PASS\n", checks);
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "FAIL %s\n", failure.what()); return 1;
    }
}
