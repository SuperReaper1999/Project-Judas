// Compile the identical observer against each revision's OWN headers/library.
// CPU free flight uses the pre-existing VisualParticlePool API, including its
// actual simulation and presentation work. GPU is an explicit candidate path.
#include "../third_party/nlohmann/json.hpp"
#include "BuildIdentity.h"
#include "EngineHost.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "UniformGravity.h"
#include "VisualParticles.h"

#if __has_include("particles/GpuParticles.h")
#define JUDAS_BENCHMARK_GPU_PARTICLES 1
#include "particles/CollisionSnapshot.h"
#include "particles/GpuParticles.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr int width = 1920, height = 1080;
constexpr float dt = 1.f / 60;
constexpr const char* wholePass = "Particles V2 complete simulation and render";
struct Options {
    std::string backend = "cpu", kind = "free";
    std::filesystem::path out;
    unsigned count = 100000, frames = 240, warmup = 60, views = 1;
    bool requireGpuTimers = false;
};
unsigned Number(const std::string& text, unsigned minimum, unsigned maximum) {
    std::size_t used = 0;
    const auto result = std::stoul(text, &used);
    if (used != text.size() || result < minimum || result > maximum)
        throw std::invalid_argument("benchmark argument outside range: " + text);
    return unsigned(result);
}
Options Parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string name = argv[i];
        if (name == "--require-gpu-timers") {
            options.requireGpuTimers = true;
            continue;
        }
        if (name == "--help") {
            std::puts("ParticlesV2Benchmark --backend cpu|gpu --case free|collision --out NEW_DIRECTORY\n"
                "  [--count 1..100000] [--frames 1..3600] [--warmup 0..600] [--views 1|2]\n"
                "  [--require-gpu-timers]\n"
                "Complete fixed-dt particle simulation, CPU presentation/GPU view preparation and rendering.\n"
                "1920x1080 main / 960x540 secondary; cold work and final diagnostic readback separate.\n"
                "Free flight is the matched baseline case. Static CPU/GPU collision is candidate-only.");
            std::exit(0);
        }
        if (++i == argc)
            throw std::invalid_argument("missing argument: " + name);
        const std::string value = argv[i];
        if (name == "--backend") options.backend = value;
        else if (name == "--case") options.kind = value;
        else if (name == "--out") options.out = value;
        else if (name == "--count") options.count = Number(value, 1, 100000);
        else if (name == "--frames") options.frames = Number(value, 1, 3600);
        else if (name == "--warmup") options.warmup = Number(value, 0, 600);
        else if (name == "--views") options.views = Number(value, 1, 2);
        else throw std::invalid_argument("unknown argument: " + name);
    }
    if ((options.backend != "cpu" && options.backend != "gpu") ||
        (options.kind != "free" && options.kind != "collision") || options.out.empty())
        throw std::invalid_argument("explicit valid backend, case and --out required");
#if !defined(JUDAS_BENCHMARK_GPU_PARTICLES)
    if (options.kind == "collision")
        throw std::invalid_argument("collision observer is candidate-only; baseline has no visual-particle collision");
    if (options.backend == "gpu")
        throw std::invalid_argument("this observer was compiled against a CPU-only engine revision");
#endif
    options.out = std::filesystem::absolute(options.out);
    if (std::filesystem::exists(options.out))
        throw std::invalid_argument("output exists; choose a fresh directory to preserve prior evidence");
    std::filesystem::create_directories(options.out);
    return options;
}
double Milliseconds(Clock::time_point time) {
    return std::chrono::duration<double, std::milli>(Clock::now() - time).count();
}
Json Distribution(std::vector<double> values) {
    if (values.empty()) return nullptr;
    std::sort(values.begin(), values.end());
    const auto q = [&](double fraction) {
        return values[std::max(std::size_t(1), std::size_t(std::ceil(values.size() * fraction))) - 1];
    };
    return {{"samples", values.size()}, {"median", q(.5)}, {"p95", q(.95)},
            {"p99", q(.99)}, {"max", values.back()}};
}
void Write(const std::filesystem::path& path, const Json& value) {
    std::ofstream stream(path);
    stream << value.dump(2) << '\n';
    if (!stream) throw std::runtime_error("cannot write benchmark result");
}
std::string GlString(GLenum name) {
    const auto* text = glGetString(name);
    return text ? reinterpret_cast<const char*>(text) : "unavailable";
}
std::string UtcNow() {
    const auto now = std::time(nullptr);
    char text[32]{};
    if (const auto* utc = std::gmtime(&now))
        std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", utc);
    return text;
}
Json PeakRss() {
#if defined(__linux__)
    rusage usage{};
    if (!getrusage(RUSAGE_SELF, &usage)) return std::uint64_t(usage.ru_maxrss) * 1024;
#endif
    return nullptr;
}
struct Sample {
    std::uint64_t frame;
    double simulation, render, cpu, process;
    RenderStats stats;
};
} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        const auto options = Parse(argc, argv);
        const auto startedUtc = UtcNow();
        std::string error;
        EngineHost host;
        auto clock = Clock::now();
        if (!host.Init("Particles V2 matched observer", width, height, false, error))
            throw std::runtime_error(error);
        const double hostInit = Milliseconds(clock);
        SDL_GL_SetSwapInterval(0);
        auto& renderer = host.GetRenderer();
        const auto setupStart = Clock::now();
        ParticleEmitterSettings settings;
        settings.loop = false;
        settings.rate = 0;
        settings.burst = int(options.count);
        settings.maxParticles = int(options.count);
        settings.lifetime = 3600;
        settings.size = settings.endSize = .045f;
        settings.spread = {14, 5, 10};
        settings.velocity = {0, .05f, 0};
        settings.velocityVariation = {.1f, .05f, .1f};
        settings.acceleration = {0, 0, 0};
        settings.color = settings.endColor = {.1f, .65f, 1, .85f};
        settings.seed = 8510;
        const glm::vec3 acceleration = options.kind == "collision" ? glm::vec3(0, -9.81f, 0) : glm::vec3(0);
        UniformGravity gravity(acceleration);
        std::unique_ptr<VisualParticlePool> pool;
        if (options.backend == "cpu") pool = std::make_unique<VisualParticlePool>(settings);
        TextureData image;
        image.width = image.height = 16;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const unsigned char alpha = (x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f) < 49 ? 255 : 0;
                image.pixels.insert(image.pixels.end(), {255, 255, 255, alpha});
            }
        auto texture = renderer.CreateTexture(image);
        if (!texture.IsValid()) throw std::runtime_error("benchmark texture creation failed");
        RenderTargetHandle secondary;
        if (options.views == 2) {
            secondary = renderer.CreateRenderTarget(width / 2, height / 2, error);
            if (!secondary.IsValid()) throw std::runtime_error(error);
        }
        double snapshotCapture = 0;
#if defined(JUDAS_BENCHMARK_GPU_PARTICLES)
        JudasParticles::GpuParticles gpu;
        std::shared_ptr<JudasParticles::CollisionSnapshot> snapshot;
        PhysicsWorld physics;
        if (options.kind == "collision") {
            physics.Init();
            physics.CreateStaticBox({0, -.3f, 0}, {18, .3f, 13}, 0, 0);
            for (int i = 0; i < 12; ++i)
                physics.CreateStaticBox({float(i % 4 - 1.5) * 6, 1, float(i / 4 - 1) * 7},
                                         {1.25f, 1, 1.25f}, 0, 0);
            snapshot = std::make_shared<JudasParticles::CollisionSnapshot>();
            clock = Clock::now();
            if (!snapshot->Capture(physics, {glm::vec3(-256), glm::vec3(256)}, dt, error))
                throw std::runtime_error(error);
            snapshotCapture = Milliseconds(clock);
            settings.collision = true;
            settings.collisionRadius = .025f;
            settings.restitution = .65f;
            settings.velocity = {0, -2, 0};
            settings.useGravity = true;
            if (pool) pool->SetSettings(settings);
        }
        if (options.backend == "gpu") {
            settings.backend = ParticleBackend::Gpu;
            clock = Clock::now();
            if (!gpu.Initialize(renderer.Capabilities(), settings, {}, error))
                throw std::runtime_error(error);
        }
#endif
        const double resourceSetup = Milliseconds(setupStart);
        std::uint64_t tick = 0;
        const auto simulation = [&] {
            ++tick;
#if defined(JUDAS_BENCHMARK_GPU_PARTICLES)
            if (options.backend == "gpu") {
                JudasParticles::GpuParticleStepInputs inputs;
                inputs.dt = dt;
                inputs.spawnCount = tick == 1 ? options.count : 0;
                inputs.tick = tick;
                inputs.position = {0, 7, 0};
                if (options.kind == "collision") {
                    inputs.gravityKind = JudasParticles::GpuGravityKind::Uniform;
                    inputs.gravity = acceleration;
                }
                if (!gpu.Step(settings, inputs, snapshot.get(), error))
                    throw std::runtime_error(error);
                return;
            }
            if (options.kind == "collision") {
                pool->UpdateWorld(dt, {0, 7, 0}, {1, 0, 0, 0}, {1, 1, 1}, gravity, snapshot, {}, tick);
                if (pool->PendingStep()) throw std::runtime_error("collision benchmark unexpectedly backpressured");
                return;
            }
#endif
            pool->Update(dt, {0, 7, 0}, {1, 0, 0, 0}, {1, 1, 1}, gravity);
        };
        const auto draw = [&] {
            const std::vector<ParticleBillboard>* billboards = nullptr;
            if (options.backend == "cpu") billboards = &pool->Presentation({0, 7, 0}, {1, 0, 0, 0}, {1, 1, 1});
            renderer.ResetStats();
            renderer.SetSceneAppearance(false, 1, {}, 1, {.015f, .02f, .035f, 1}, false);
            renderer.BeginFrame(width, height);
            const auto view = [&](unsigned index) {
                if (index && !renderer.BeginRenderTarget(secondary)) throw std::runtime_error("secondary begin failed");
                renderer.SetCamera(glm::lookAt(index ? glm::vec3(-35, 14, 25) : glm::vec3(0, 14, 40),
                                              glm::vec3(0, 7, 0), glm::vec3(0, 1, 0)),
                    glm::perspective(glm::radians(65.f), float(width) / height, .1f, 500.f));
                renderer.SetLighting({0, 1, 0}, {.6f, .6f, .6f}, {.25f, .25f, .25f});
                renderer.SetDynamicLights({});
                renderer.SetMaterialBindings({});
                if (options.kind == "collision") {
                    renderer.BeginMeshBatch();
                    renderer.DrawBox({0, -.3f, 0}, {1, 0, 0, 0}, {18, .3f, 13}, {.2f, .25f, .3f});
                    for (int i = 0; i < 12; ++i)
                        renderer.DrawBox({float(i % 4 - 1.5) * 6, 1, float(i / 4 - 1) * 7},
                                         {1, 0, 0, 0}, {1.25f, 1, 1.25f}, {.25f, .4f, .55f});
                    renderer.EndMeshBatch();
                }
#if defined(JUDAS_BENCHMARK_GPU_PARTICLES)
                if (options.backend == "gpu") renderer.DrawGpuParticles(gpu, {{-200, -200, -200}, {200, 200, 200}}, texture);
                else
#endif
                    renderer.DrawParticles(*billboards, pool->Bounds(), texture);
                if (index) renderer.EndRenderTarget();
            };
            if (options.views == 2) view(1);
            view(0);
            renderer.EndFrame();
        };
        clock = Clock::now();
        simulation();
        const double firstSimulation = Milliseconds(clock);
        clock = Clock::now();
        draw();
        const double firstSubmission = Milliseconds(clock);
        renderer.FinishForDiagnostics();
        const double firstCompleted = Milliseconds(clock);
        host.GetWindow().SwapBuffers();
        for (unsigned i = 0; i < options.warmup; ++i) {
            host.GetWindow().PollEvents(); simulation(); draw(); host.GetWindow().SwapBuffers();
        }
        renderer.FinishForDiagnostics();
        auto& profiler = PerformanceProfiler::Get();
        profiler.Enable(true);
        profiler.Clear();
        std::map<std::uint64_t, ProfileGPURecord> gpuRecords;
        const auto collect = [&] {
            renderer.PollProfileGPU();
            for (const auto& frame : profiler.History())
                for (const auto& query : frame.gpu)
                    if (!query.pending) gpuRecords[query.token] = query;
        };
        std::vector<Sample> samples;
        for (unsigned i = 0; i < options.frames; ++i) {
            host.GetWindow().PollEvents();
            ProfileFrame frame("Particles V2 measured frame");
            const auto id = profiler.FrameId();
            const auto cpu = std::clock();
            clock = Clock::now();
            const auto token = renderer.BeginProfilePass(wholePass);
            auto phase = Clock::now(); simulation(); const double simulated = Milliseconds(phase);
            phase = Clock::now(); draw(); const double rendered = Milliseconds(phase);
            renderer.EndProfilePass(token);
            const double elapsed = Milliseconds(clock);
            samples.push_back({id, simulated, rendered, elapsed,
                1000. * double(std::clock() - cpu) / CLOCKS_PER_SEC, renderer.Stats()});
            host.GetWindow().SwapBuffers();
            frame.End();
            collect();
        }
        renderer.FinishForDiagnostics();
        { ProfileFrame drain("Particle benchmark timestamp drain"); collect(); }
        const auto diagnostics = profiler.Diagnostics();
        std::map<std::uint64_t, double> whole;
        std::ofstream rawGpu(options.out / "gpu.csv");
        rawGpu.precision(12);
        rawGpu << "frame,token,camera,pass,milliseconds\n";
        for (const auto& [token, record] : gpuRecords) {
            rawGpu << record.frame << ',' << token << ',' << record.camera << ',' << record.pass.c_str() << ',' << record.milliseconds << '\n';
            if (record.pass == wholePass) whole[record.frame] = record.milliseconds;
        }
        std::ofstream raw(options.out / "frames.csv");
        raw.precision(12);
        raw << "sample,frame,simulation_ms,presentation_draw_ms,cpu_ms,process_cpu_ms,gpu_ms,draws,triangles,particles\n";
        std::vector<double> simulationTimes, drawTimes, cpuTimes, processTimes, gpuTimes;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto& s = samples[i];
            raw << i << ',' << s.frame << ',' << s.simulation << ',' << s.render << ',' << s.cpu << ',' << s.process << ',';
            if (const auto found = whole.find(s.frame); found != whole.end()) {
                raw << found->second;
                gpuTimes.push_back(found->second);
            } else raw << "NA";
            raw << ',' << s.stats.drawCalls << ',' << s.stats.triangles << ',' << s.stats.particlesSubmitted << '\n';
            simulationTimes.push_back(s.simulation); drawTimes.push_back(s.render);
            cpuTimes.push_back(s.cpu); processTimes.push_back(s.process);
        }
        const bool gpuValid = gpuTimes.size() == samples.size() && diagnostics.gpuDropped == 0;
        Json proof;
        proof["expected_live"] = options.count;
#if defined(JUDAS_BENCHMARK_GPU_PARTICLES)
        if (options.backend == "gpu") {
            const auto before = gpu.Diagnostics();
            proof["dispatches"] = before.dispatches;
            proof["draws"] = before.draws;
            proof["asynchronous_readback_bytes"] = before.readbackBytes;
            proof["gpu_owned_bytes"] = gpu.Stats().gpuBytes;
            proof["geometry_uploads"] = gpu.Stats().geometryUploads;
            proof["state_upload_bytes"] = gpu.Stats().stateTransferBytes;
            std::vector<JudasParticles::GpuParticleState> state;
            if (!gpu.ReadStateForDiagnostics(state, error)) throw std::runtime_error(error);
            proof["contacts"] = gpu.Stats().contacts;
            proof["query_failures"] = gpu.Stats().queryFailures;
            if (gpu.Stats().queryFailures)
                throw std::runtime_error("GPU benchmark reached a collision/numerical safety limit");
            std::size_t live = 0;
            for (const auto& p : state) live += p.identity[0] != 0;
            proof["actual_live"] = live;
            proof["final_blocking_diagnostic_state_bytes"] = state.size() * sizeof(state[0]);
            if (live != options.count) throw std::runtime_error("GPU final live population differs from requested matched population");
        } else
#endif
        {
            proof["actual_live"] = pool->Particles().size();
            if (pool->Particles().size() != options.count) throw std::runtime_error("CPU matched population changed");
#if defined(JUDAS_BENCHMARK_GPU_PARTICLES)
            if (options.kind == "collision") {
                const auto stats = pool->RuntimeStats();
                proof["contacts"] = stats.contacts;
                proof["query_failures"] = stats.queryFailures;
                if (stats.queryFailures)
                    throw std::runtime_error("CPU benchmark reached a collision/numerical safety limit");
                proof["snapshot_bodies"] = snapshot->Stats().retainedBodies;
                proof["snapshot_colliders"] = snapshot->Stats().colliders;
            }
#endif
        }
        profiler.Enable(false);
        draw();
        renderer.FinishForDiagnostics();
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(width, height, pixels);
        if (!WriteRgbPng((options.out / "main.png").string(), width, height, pixels))
            throw std::runtime_error("final benchmark capture failed");
        if (options.views == 2) {
            if (!renderer.BeginRenderTarget(secondary, true)) throw std::runtime_error("secondary capture failed");
            renderer.CaptureFrame(width / 2, height / 2, pixels);
            if (!WriteRgbPng((options.out / "secondary.png").string(), width / 2, height / 2, pixels))
                throw std::runtime_error("secondary PNG failed");
            renderer.EndRenderTarget();
            renderer.DestroyRenderTarget(secondary);
        }
        const auto* videoDriver = SDL_GetCurrentVideoDriver();
        const auto* sessionType = std::getenv("XDG_SESSION_TYPE");
        Json result = {{"schema", 1}, {"build", Json::parse(BuildIdentityJson())},
            {"run", {{"started_utc", startedUtc}, {"completed_utc", UtcNow()},
                {"sdl_video_driver", videoDriver ? videoDriver : "unavailable"},
                {"xdg_session_type", sessionType ? sessionType : "unset"}}},
            {"workload", {{"backend", options.backend}, {"case", options.kind}, {"count", options.count},
                {"frames", options.frames}, {"warmup", options.warmup}, {"views", options.views},
                {"width", width}, {"height", height}, {"dt", dt}}},
            {"graphics", {{"vendor", GlString(GL_VENDOR)}, {"renderer", GlString(GL_RENDERER)}, {"version", GlString(GL_VERSION)}}},
            {"cold", {{"host_init_ms", hostInit}, {"resource_setup_ms", resourceSetup},
                {"snapshot_capture_ms_included_in_resource_setup", snapshotCapture},
                {"first_simulation_ms", firstSimulation},
                {"first_draw_submission_ms", firstSubmission}, {"first_draw_completed_ms", firstCompleted}}},
            {"summary", {{"simulation_ms", Distribution(simulationTimes)}, {"presentation_draw_ms", Distribution(drawTimes)},
                {"complete_cpu_ms", Distribution(cpuTimes)}, {"process_cpu_ms", Distribution(processTimes)},
                {"gpu_ms", gpuValid ? Distribution(gpuTimes) : Json(nullptr)}}},
            {"gpu_timing_valid", gpuValid}, {"gpu_dropped", diagnostics.gpuDropped},
            {"gpu_samples", gpuTimes.size()}, {"truncated_frames", diagnostics.truncatedFrames},
            {"process_peak_rss_bytes", PeakRss()}, {"proof", proof},
            {"notes", "Complete real simulation/presentation/draw; no rigid solver stepping. Free is the identical baseline/candidate workload; collision is a separate CPU/GPU candidate capability case with gravity and 13 static boxes. Collision capture occurs once outside per-frame timing. CPU/GPU floating-point arithmetic and birth initialization require independent trajectory controls, not an assumed exact image match. GPU timing includes submission bubbles. Final blocking diagnostic read and screenshots are outside all samples. No per-frame full-state readback or GPU finish."}};
        Write(options.out / "RESULTS.json", result);
#if defined(JUDAS_BENCHMARK_GPU_PARTICLES)
        if (gpu.Initialized() && !gpu.Shutdown(error)) throw std::runtime_error(error);
#endif
        renderer.DestroyTexture(texture);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("GL error after particle benchmark");
        std::printf("ParticlesV2Benchmark backend=%s case=%s count=%u frames=%u gpu_valid=%d\n",
            options.backend.c_str(), options.kind.c_str(), options.count, options.frames, gpuValid);
        return options.requireGpuTimers && !gpuValid ? 2 : 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Particles V2 benchmark: %s\n", error.what());
        return 1;
    }
}
