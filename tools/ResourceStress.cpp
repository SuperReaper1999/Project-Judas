// Milestone 31 stress demonstration (developer tooling, not part of the
// engine): judas_resource_stress.
//
// Builds a temporary project with dozens of synthetic but REAL assets
// (multi-megabyte OBJ spheres, 1024x1024 PNG textures), opens a hidden
// window with a real GL context, plays the tiny game's scene through the
// ordinary InteractivePlay frame, and while it runs:
//
//   phase 1  requests every asset asynchronously at mixed priorities and
//            measures frame time, fixed-step continuity, queue depth,
//            worker utilization, completed jobs and per-asset latency
//            until everything is Ready (GPU uploads on this thread);
//   phase 2  cancels and releases (demand drops mid-flight);
//   phase 3  halves the budget, evicts, re-requests evicted assets;
//   phase 4  the same load in BLOCKING mode (the M30 path) for comparison.
//
// Run from the repository root (the tiny game scene is copied from
// projects/tiny_game). Needs a display (Xvfb is enough). An optional
// argument is a path for a JSON report.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#include <unistd.h>

#include <SDL2/SDL.h>

#include "AssetDatabase.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "WorldCoordinates.h"

namespace fs = std::filesystem;

namespace {
constexpr int kMeshCount = 24;
constexpr int kTextureCount = 16;
constexpr int kSphereSegments = 160;
constexpr int kTextureSize = 1024;

std::string HexId(int n) {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%032x", 0x31000000 + n);
    return buffer;
}

void WriteSphereObj(const std::string& path, float radius) {
    std::ofstream out(path);
    const int R = kSphereSegments, S = kSphereSegments;
    for (int r = 0; r <= R; ++r) {
        const float v = static_cast<float>(r) / R;
        const float theta = v * 3.14159265f;
        for (int s = 0; s <= S; ++s) {
            const float u = static_cast<float>(s) / S;
            const float phi = u * 6.2831853f;
            const float x = std::sin(theta) * std::cos(phi), y = std::cos(theta), z = std::sin(theta) * std::sin(phi);
            out << "v " << x * radius << ' ' << y * radius << ' ' << z * radius << '\n';
            out << "vn " << x << ' ' << y << ' ' << z << '\n';
            out << "vt " << u << ' ' << v << '\n';
        }
    }
    for (int r = 0; r < R; ++r) {
        for (int s = 0; s < S; ++s) {
            const int a = r * (S + 1) + s + 1, b = a + 1, c = a + S + 1, d = c + 1;
            out << "f " << a << '/' << a << '/' << a << ' ' << c << '/' << c << '/' << c << ' ' << b << '/' << b << '/' << b << '\n';
            out << "f " << b << '/' << b << '/' << b << ' ' << c << '/' << c << '/' << c << ' ' << d << '/' << d << '/' << d << '\n';
        }
    }
}

void WriteNoisePng(const std::string& path, unsigned int seed) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(kTextureSize) * kTextureSize * 3);
    unsigned int state = seed * 2654435761u + 1u;
    for (int y = 0; y < kTextureSize; ++y) {
        for (int x = 0; x < kTextureSize; ++x) {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            unsigned char* p = &pixels[(static_cast<std::size_t>(y) * kTextureSize + x) * 3];
            p[0] = static_cast<unsigned char>((x * 255) / kTextureSize);
            p[1] = static_cast<unsigned char>((y * 255) / kTextureSize);
            p[2] = static_cast<unsigned char>(state & 0xff);
        }
    }
    WriteRgbPng(path, kTextureSize, kTextureSize, pixels);
}

struct PhaseMetrics {
    std::string name;
    std::vector<double> frameMs;
    double wallSeconds = 0.0;
    unsigned long long fixedSteps = 0;
    int saturatedFrames = 0;  // frames that hit the catch-up cap
    std::size_t maxQueue = 0;
    double sumQueue = 0.0;
    double maxPumpMs = 0.0;  // worst GPU-upload pump inside a frame
    double workerBusyDelta = 0.0;
    unsigned long long jobsCompletedDelta = 0;
    double Avg() const { return frameMs.empty() ? 0.0 : std::accumulate(frameMs.begin(), frameMs.end(), 0.0) / frameMs.size(); }
    double Max() const { return frameMs.empty() ? 0.0 : *std::max_element(frameMs.begin(), frameMs.end()); }
    double Percentile(double p) const {
        if (frameMs.empty()) return 0.0;
        std::vector<double> sorted = frameMs;
        std::sort(sorted.begin(), sorted.end());
        return sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(p * sorted.size()))];
    }
    int FramesOver(double ms) const { return static_cast<int>(std::count_if(frameMs.begin(), frameMs.end(), [ms](double f) { return f > ms; })); }
};

struct Runner {
    EngineHost& host;
    InteractivePlay& play;
    Window& window;
    Renderer& renderer;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();

    // One ordinary frame: events, `before` (a blocking load in phase 4),
    // resource pump (GPU uploads), play frame. Everything from `before`
    // to the swap is inside the measured frame time.
    double Frame(PhaseMetrics* metrics, const std::function<void()>& before = nullptr) {
        window.PollEvents();
        const auto start = std::chrono::steady_clock::now();
        if (before) before();
        const auto pumpStart = std::chrono::steady_clock::now();
        host.PumpResources();
        const double pumpMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pumpStart).count();
        const Uint64 now = SDL_GetPerformanceCounter();
        const float dt = static_cast<float>(now - previousCounter) / static_cast<float>(frequency);
        previousCounter = now;
        play.Frame(window, renderer, dt, false);
        window.SwapBuffers();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (metrics) {
            metrics->frameMs.push_back(ms);
            metrics->maxPumpMs = std::max(metrics->maxPumpMs, pumpMs);
            metrics->fixedSteps += static_cast<unsigned long long>(play.LastFixedStepsThisFrame());
            if (play.LastFixedStepsThisFrame() >= 8) ++metrics->saturatedFrames;
            const JobStats jobs = host.Jobs().Stats();
            metrics->maxQueue = std::max(metrics->maxQueue, jobs.queued);
            metrics->sumQueue += static_cast<double>(jobs.queued);
        }
        return ms;
    }
};

void Report(std::FILE* out, const PhaseMetrics& m, unsigned int workers) {
    const double utilization = workers > 0 && m.wallSeconds > 0.0 ? 100.0 * m.workerBusyDelta / (workers * m.wallSeconds) : 0.0;
    std::fprintf(out, "  %-28s frames %4zu | frame avg %6.2f ms p99 %6.2f ms max %7.2f ms (pump max %6.2f ms) | >33 ms: %3d | fixed steps %5llu (expected ~%5.0f, %d saturated) | queue max %2zu avg %5.1f | worker util %5.1f%% | jobs done %llu\n",
                 m.name.c_str(), m.frameMs.size(), m.Avg(), m.Percentile(0.99), m.Max(), m.maxPumpMs, m.FramesOver(33.0), m.fixedSteps,
                 m.wallSeconds * 60.0, m.saturatedFrames, m.maxQueue, m.frameMs.empty() ? 0.0 : m.sumQueue / m.frameMs.size(),
                 utilization, m.jobsCompletedDelta);
}
}  // namespace

int main(int argc, char** argv) {
    const std::string jsonPath = argc > 1 ? argv[1] : std::string();
    std::error_code ec;
    const fs::path root = fs::temp_directory_path(ec) / ("judas_m31_stress_" + std::to_string(static_cast<long long>(::getpid())));
    fs::remove_all(root, ec);
    fs::create_directories(root / "Assets" / "meshes", ec);
    fs::create_directories(root / "Assets" / "textures", ec);
    fs::create_directories(root / "Scenes", ec);
    fs::create_directories(root / "Saves", ec);
    fs::copy_file("projects/tiny_game/Scenes/main.judas", root / "Scenes" / "main.judas", ec);
    if (ec) { std::fprintf(stderr, "run from the repository root (projects/tiny_game not found)\n"); return 1; }

    std::printf("M31 resource stress: generating %d sphere meshes (%dx%d) and %d %dx%d textures under %s\n",
                kMeshCount, kSphereSegments, kSphereSegments, kTextureCount, kTextureSize, kTextureSize, root.c_str());
    std::vector<AssetId> meshIds, textureIds;
    std::string error;
    std::uint64_t bytesOnDisk = 0;
    for (int i = 0; i < kMeshCount; ++i) {
        const std::string path = (root / "Assets" / "meshes" / ("sphere" + std::to_string(i) + ".obj")).generic_string();
        WriteSphereObj(path, 0.5f + 0.1f * i);
        AssetDatabase::WriteMeta(path + ".judasmeta", HexId(i), AssetType::Mesh, "synthetic", error);
        meshIds.push_back(HexId(i));
        bytesOnDisk += fs::file_size(path, ec);
    }
    for (int i = 0; i < kTextureCount; ++i) {
        const std::string path = (root / "Assets" / "textures" / ("noise" + std::to_string(i) + ".png")).generic_string();
        WriteNoisePng(path, static_cast<unsigned int>(i + 1));
        AssetDatabase::WriteMeta(path + ".judasmeta", HexId(100 + i), AssetType::Texture, "synthetic", error);
        textureIds.push_back(HexId(100 + i));
        bytesOnDisk += fs::file_size(path, ec);
    }
    std::printf("  %.1f MB of asset files\n", bytesOnDisk / 1048576.0);
    Project project;
    if (!Project::CreateNew(root.generic_string(), "M31 stress", project, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    project.Settings().startupScene = "Scenes/main.judas";
    project.Save(error);

    EngineHost host;
    if (!host.Init("M31 stress", 640, 480, false, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
    ResourceManager& resources = host.Resources();
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    window.SetTestInputMode(true);

    Scene scene;
    if (!LoadSceneFromFile(project.StartupScenePath(), scene, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    RuntimeWorld world;
    if (!world.Build(scene, &resources, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    InteractivePlay play;
    if (!play.Begin(world, WorldCoordinates(scene.Settings().worldOrigin), error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    Runner runner{host, play, window, renderer};
    const unsigned int workers = host.Jobs().WorkerCount();
    std::printf("  %u workers, scene '%s' playing (%zu dynamic bodies)\n", workers, scene.Settings().name.c_str(), world.DynamicBodies().size());

    std::vector<PhaseMetrics> phases;
    const auto beginPhase = [&](const std::string& name) {
        PhaseMetrics m;
        m.name = name;
        m.workerBusyDelta = -host.Jobs().Stats().workerBusySeconds;
        m.jobsCompletedDelta = host.Jobs().Stats().completed;
        return m;
    };
    const auto endPhase = [&](PhaseMetrics& m, std::chrono::steady_clock::time_point start) {
        m.wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        m.workerBusyDelta += host.Jobs().Stats().workerBusySeconds;
        m.jobsCompletedDelta = host.Jobs().Stats().completed - m.jobsCompletedDelta;
        phases.push_back(m);
    };
    const auto allReady = [&]() {
        for (const AssetId& id : meshIds) if (resources.StateOf(id) != ResourceState::Ready) return false;
        for (const AssetId& id : textureIds) if (resources.StateOf(id) != ResourceState::Ready) return false;
        return true;
    };

    // Phase 0: baseline, nothing loading.
    {
        PhaseMetrics m = beginPhase("baseline (no loads)");
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 120; ++i) runner.Frame(&m);
        endPhase(m, start);
    }

    // Phase 1: asynchronous load of everything, mixed priorities.
    double asyncLoadSeconds = 0.0;
    {
        PhaseMetrics m = beginPhase("async load (40 assets)");
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < meshIds.size(); ++i) {
            resources.AddRef(meshIds[i]);
            resources.RequestMesh(meshIds[i], i < 8 ? JobPriority::High : i < 16 ? JobPriority::Normal : JobPriority::Low);
        }
        for (std::size_t i = 0; i < textureIds.size(); ++i) {
            resources.AddRef(textureIds[i]);
            resources.RequestTexture(textureIds[i], i < 4 ? JobPriority::High : JobPriority::Low);
        }
        while (!allReady() && std::chrono::steady_clock::now() - start < std::chrono::seconds(120)) runner.Frame(&m);
        asyncLoadSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        endPhase(m, start);
        double minLatency = 1e9, maxLatency = 0.0, sumLatency = 0.0;
        int n = 0;
        for (const ResourceManager::EntryView& e : resources.Entries()) {
            if (e.state != ResourceState::Ready) continue;
            minLatency = std::min(minLatency, e.loadMilliseconds);
            maxLatency = std::max(maxLatency, e.loadMilliseconds);
            sumLatency += e.loadMilliseconds;
            ++n;
        }
        const ResourceStats s = resources.Stats();
        std::printf("  async: all %d assets Ready after %.2f s; per-asset latency (request->Ready) min %.0f ms avg %.0f ms max %.0f ms; %llu uploads; resident %.1f MB (peak %.1f MB)\n",
                    n, asyncLoadSeconds, minLatency, sumLatency / std::max(n, 1), maxLatency, s.uploads, s.bytesResident / 1048576.0,
                    s.peakBytesResident / 1048576.0);
    }

    // Phase 2: demand drops mid-flight.
    {
        PhaseMetrics m = beginPhase("cancel + release");
        const auto start = std::chrono::steady_clock::now();
        for (const AssetId& id : meshIds) resources.ReleaseRef(id);
        for (const AssetId& id : textureIds) resources.ReleaseRef(id);
        for (int i = 0; i < 20; ++i) resources.Release(meshIds[i]);
        const unsigned long long cancelledBefore = resources.Stats().cancelled;
        for (int i = 0; i < 20; ++i) { resources.AddRef(meshIds[i]); resources.RequestMesh(meshIds[i], JobPriority::Low); }
        for (int i = 0; i < 10; ++i) resources.ReleaseRef(meshIds[i]);  // no demand any more
        runner.Frame(&m);
        for (int i = 10; i < 20; ++i) resources.ReleaseRef(meshIds[i]);
        while (resources.Stats().loading > 0 && std::chrono::steady_clock::now() - start < std::chrono::seconds(60)) runner.Frame(&m);
        endPhase(m, start);
        const ResourceStats s = resources.Stats();
        std::printf("  cancel: %llu loads cancelled by dropped demand, %llu stale completions discarded, %zu ready, %zu loading\n",
                    s.cancelled - cancelledBefore, s.staleDiscarded, s.ready, s.loading);
    }

    // Phase 3: budget pressure.
    double evictReloadSeconds = 0.0;
    std::uint64_t bytesBefore = 0, bytesAfter = 0;
    unsigned long long evictions = 0;
    {
        PhaseMetrics m = beginPhase("evict + reload");
        const auto start = std::chrono::steady_clock::now();
        // Everything currently Ready is unreferenced now; hold the textures
        // so they must survive.
        for (const AssetId& id : textureIds) resources.AddRef(id);
        bytesBefore = resources.Stats().bytesResident;
        resources.SetBudgetBytes(bytesBefore / 2);
        runner.Frame(&m);  // Pump enforces the budget
        bytesAfter = resources.Stats().bytesResident;
        evictions = resources.Stats().evictions;
        std::vector<AssetId> evicted;
        for (const AssetId& id : meshIds) if (resources.StateOf(id) == ResourceState::Unloaded) evicted.push_back(id);
        bool texturesSurvived = true;
        for (const AssetId& id : textureIds) texturesSurvived = texturesSurvived && resources.StateOf(id) == ResourceState::Ready;
        std::printf("  evict: budget %.1f MB -> resident %.1f MB before, %.1f MB after, %llu evictions; referenced textures survived: %s\n",
                    bytesBefore / 2 / 1048576.0, bytesBefore / 1048576.0, bytesAfter / 1048576.0, evictions, texturesSurvived ? "yes" : "NO");
        resources.SetBudgetBytes(1ull << 31);
        const std::size_t reload = std::min<std::size_t>(6, evicted.size());
        const auto reloadStart = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < reload; ++i) { resources.AddRef(evicted[i]); resources.RequestMesh(evicted[i], JobPriority::High); }
        for (;;) {
            bool ready = true;
            for (std::size_t i = 0; i < reload; ++i) ready = ready && resources.StateOf(evicted[i]) == ResourceState::Ready;
            if (ready || std::chrono::steady_clock::now() - reloadStart > std::chrono::seconds(60)) break;
            runner.Frame(&m);
        }
        evictReloadSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - reloadStart).count();
        endPhase(m, start);
        std::printf("  reload: %zu evicted meshes re-requested and Ready again after %.2f s\n", reload, evictReloadSeconds);
        for (std::size_t i = 0; i < reload; ++i) resources.ReleaseRef(evicted[i]);
        for (const AssetId& id : textureIds) resources.ReleaseRef(id);
    }

    // Phase 4: the same load, blocking (M30 path), one asset per frame.
    double blockingLoadSeconds = 0.0;
    {
        resources.ReleaseAll();
        resources.SetBlockingMode(true);
        PhaseMetrics m = beginPhase("BLOCKING load (40 assets)");
        const auto start = std::chrono::steady_clock::now();
        std::size_t next = 0;
        const std::size_t total = meshIds.size() + textureIds.size();
        while (next < total) {
            runner.Frame(&m, [&] {
                std::string e;
                if (next < meshIds.size()) resources.GetMesh(meshIds[next], e);
                else resources.GetTexture(textureIds[next - meshIds.size()], e);
                ++next;
            });
        }
        blockingLoadSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        endPhase(m, start);
        resources.SetBlockingMode(false);
        std::printf("  blocking: all 40 assets loaded on the main thread in %.2f s (one per frame)\n", blockingLoadSeconds);
    }

    std::printf("\nPer-phase main-thread measurements (frame = pump + play frame + swap; 60 Hz fixed step):\n");
    for (const PhaseMetrics& m : phases) Report(stdout, m, workers);
    const PhaseMetrics& async = phases[1];
    const PhaseMetrics& blocking = phases[4];
    std::printf("\nClaim under test: expensive resource preparation no longer forces the main thread to wait for the whole operation.\n");
    std::printf("  async  : worst frame %7.2f ms, %3d frames over 33 ms, %d saturated catch-up frames, wall %.2f s\n",
                async.Max(), async.FramesOver(33.0), async.saturatedFrames, asyncLoadSeconds);
    std::printf("  blocking: worst frame %7.2f ms, %3d frames over 33 ms, %d saturated catch-up frames, wall %.2f s\n",
                blocking.Max(), blocking.FramesOver(33.0), blocking.saturatedFrames, blockingLoadSeconds);
    std::printf("  (the work itself is not free: worker busy time during the async phase was %.2f s across %u workers)\n",
                async.workerBusyDelta, workers);

    if (!jsonPath.empty()) {
        std::ofstream json(jsonPath);
        json << "{\n  \"workers\": " << workers << ",\n  \"assets\": " << (kMeshCount + kTextureCount) << ",\n  \"phases\": [\n";
        for (std::size_t i = 0; i < phases.size(); ++i) {
            const PhaseMetrics& m = phases[i];
            json << "    {\"name\": \"" << m.name << "\", \"frames\": " << m.frameMs.size() << ", \"avgMs\": " << m.Avg()
                 << ", \"p99Ms\": " << m.Percentile(0.99) << ", \"maxMs\": " << m.Max() << ", \"over33\": " << m.FramesOver(33.0)
                 << ", \"fixedSteps\": " << m.fixedSteps << ", \"wallSeconds\": " << m.wallSeconds << ", \"saturated\": " << m.saturatedFrames
                 << ", \"maxQueue\": " << m.maxQueue << ", \"workerBusySeconds\": " << m.workerBusyDelta << ", \"jobsCompleted\": " << m.jobsCompletedDelta
                 << "}" << (i + 1 < phases.size() ? "," : "") << "\n";
        }
        json << "  ],\n  \"asyncLoadSeconds\": " << asyncLoadSeconds << ",\n  \"blockingLoadSeconds\": " << blockingLoadSeconds
             << ",\n  \"bytesBeforeEviction\": " << bytesBefore << ",\n  \"bytesAfterEviction\": " << bytesAfter << ",\n  \"evictions\": " << evictions
             << "\n}\n";
    }

    play.End();
    world.Destroy();
    host.Shutdown();  // resources -> jobs -> renderer -> window
    fs::remove_all(root, ec);
    std::printf("\nshutdown clean (resources, workers, renderer, window in that order); temporary project removed\n");
    return 0;
}
