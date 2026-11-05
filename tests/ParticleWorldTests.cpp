#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "particles/GpuParticles.h"

#include <algorithm>
#include <cmath>
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

int main() {
    try {
        EngineHost host;
        std::string error;
        Require(host.Init("Particle world lifecycle", 128, 128, false, error), "actual renderer/compute context: " + error);
        if (host.GraphicsCaps().modernTier) { Lifecycle(host); Fallback(host); SelectedGravity(host); }
        else Portable(host);
        host.Shutdown();
        std::printf("Particle world: %u checks PASS\n", checks);
        return 0;
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "FAIL %s\n", failure.what()); return 1;
    }
}
