// Matched public-API workloads. Compile this same source with each revision's
// own headers and libraries. No private renderer flags or fixed-step changes.
#include "../third_party/nlohmann/json.hpp"
#include "BuildIdentity.h"
#include "EngineHost.h"
#include "GraphicsApi.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "ShadowTransforms.h"
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
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
using Json = nlohmann::json;
constexpr int Width = 1920, Height = 1080;
constexpr const char *WholeScene = "Renderer V2 whole scene";
struct Options {
    std::string kind = "mixed";
    std::filesystem::path out;
    unsigned objects = 4096, frames = 600, warmup = 60, views = 1, lights = 8, shadows = 0;
    bool objectsExplicit = false, lightsExplicit = false, requireGPU = false, visible = false;
};
unsigned Number(const std::string &text, unsigned minimum, unsigned maximum) {
    std::size_t used = 0;
    const auto value = std::stoul(text, &used);
    if (used != text.size() || value < minimum || value > maximum)
        throw std::runtime_error("Numeric option outside its documented range: " + text);
    return unsigned(value);
}
Options Parse(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--require-gpu") {
            o.requireGPU = true;
            continue;
        }
        if (key == "--visible") {
            o.visible = true;
            continue;
        }
        if (key == "--help") {
            std::puts(
                "RendererV2Benchmark --case mixed|repeated|sprites|particles|lights --out "
                "NEW_DIRECTORY\n"
                "  [--objects 1..262144] [--frames 1..36000] [--warmup 0..3600]\n"
                "  [--views 1..4] [--lights 0..1024] [--shadows 0..8] [--visible] [--require-gpu]\n"
                "1920x1080 main, 960x540 additional views; swap interval zero.\n"
                "CPU submission excludes swap, collection, readback and file IO. GPU timestamps are "
                "asynchronous.\n"
                "Cold host/resource/first-completed-frame costs are reported separately. No physics is "
                "simulated.");
            std::exit(0);
        }
        if (++i == argc)
            throw std::runtime_error("Missing value for " + key);
        const std::string value = argv[i];
        if (key == "--case")
            o.kind = value;
        else if (key == "--out")
            o.out = value;
        else if (key == "--objects") {
            o.objects = Number(value, 1, 262144);
            o.objectsExplicit = true;
        } else if (key == "--frames")
            o.frames = Number(value, 1, 36000);
        else if (key == "--warmup")
            o.warmup = Number(value, 0, 3600);
        else if (key == "--views")
            o.views = Number(value, 1, 4);
        else if (key == "--lights") {
            o.lights = Number(value, 0, 1024);
            o.lightsExplicit = true;
        } else if (key == "--shadows")
            o.shadows = Number(value, 0, 8);
        else
            throw std::runtime_error("Unknown option " + key);
    }
    if (o.kind != "mixed" && o.kind != "repeated" && o.kind != "sprites" && o.kind != "particles" &&
        o.kind != "lights")
        throw std::runtime_error("Unknown case " + o.kind);
    if (!o.objectsExplicit && (o.kind == "sprites" || o.kind == "particles"))
        o.objects = 10000;
    if (!o.lightsExplicit)
        o.lights = o.kind == "lights" ? 128 : (o.kind == "sprites" || o.kind == "particles") ? 0 : 8;
    if (o.shadows > o.lights)
        throw std::runtime_error("--shadows requires at least as many --lights");
    if ((o.kind == "sprites" || o.kind == "particles") && (o.lights || o.shadows))
        throw std::runtime_error("Sprite/particle isolates do not shade their billboards with dynamic "
                                 "lights; use zero lights/shadows");
    if (o.out.empty())
        throw std::runtime_error("--out is required");
    o.out = std::filesystem::absolute(o.out);
    if (std::filesystem::exists(o.out))
        throw std::runtime_error("Output already exists; preserve it and choose a fresh directory");
    std::filesystem::create_directories(o.out);
    return o;
}
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
Json Distribution(std::vector<double> samples) {
    if (samples.empty())
        return nullptr;
    std::sort(samples.begin(), samples.end());
    const auto q = [&](double fraction) {
        return samples[std::max(std::size_t(1), std::size_t(std::ceil(fraction * samples.size()))) - 1];
    };
    return {{"samples", samples.size()},
            {"median", q(.5)},
            {"p95", q(.95)},
            {"p99", q(.99)},
            {"max", samples.back()}};
}
Json ProcessPeakRSS() {
#if defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0)
        return std::uint64_t(usage.ru_maxrss) * 1024;
#endif
    return nullptr;
}
std::string GLString(unsigned name) {
    const auto *value = glGetString(name);
    return value ? reinterpret_cast<const char *>(value) : "unavailable";
}
void WriteJson(const std::filesystem::path &path, const Json &value) {
    std::ofstream stream(path);
    stream << value.dump(2) << '\n';
    if (!stream)
        throw std::runtime_error("Cannot write " + path.string());
}
TextureData Pattern(bool alpha, bool normal = false) {
    TextureData result;
    result.width = result.height = 16;
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x) {
            const bool alternate = ((x / 4) + (y / 4)) % 2;
            const float dx = float(x) - 7.5f, dy = float(y) - 7.5f;
            result.pixels.insert(
                result.pixels.end(),
                {std::uint8_t(normal ? (alternate ? 136 : 120) : (alternate ? 220 : 140)),
                 std::uint8_t(normal ? 128 : (alternate ? 180 : 210)),
                 std::uint8_t(normal ? 254 : (alternate ? 150 : 235)),
                 std::uint8_t(alpha && dx * dx + dy * dy > 49 ? 0 : 255)});
        }
    return result;
}
struct Workload {
    Renderer &r;
    Options o;
    std::vector<MaterialHandle> materials;
    TextureHandle spriteTexture;
    std::vector<RenderTargetHandle> targets;
    std::vector<DynamicLight> lights;
    std::vector<ParticleBillboard> particles;
    VisualBounds particleBounds;
    unsigned side;
    float extent;
    Workload(Renderer &renderer, const Options &options)
        : r(renderer), o(options), side(unsigned(std::ceil(std::sqrt(double(o.objects))))),
          extent(float(side) * .65f + 2) {
        r.SetShadowBudget(o.shadows);
        if (o.kind == "mixed" || o.kind == "repeated" || o.kind == "lights") {
            const unsigned count = o.kind == "mixed" ? std::min(64u, o.objects) : 1;
            for (unsigned i = 0; i < count; ++i) {
                MaterialDefinition material;
                material.model = MaterialModel::PBR;
                material.baseColor = {.25f + float(i % 7) * .08f, .35f + float(i % 3) * .12f, .55f, 1};
                material.metallic = float(i % 4) * .2f;
                material.roughness = .15f + float(i % 6) * .13f;
                material.maps[0].embedded = Pattern(false);
                material.maps[2].embedded = Pattern(false, true);
                const auto handle = r.CreateMaterial(material);
                if (!handle.IsValid())
                    throw std::runtime_error("Material creation failed");
                materials.push_back(handle);
            }
        }
        if (o.kind == "sprites" || o.kind == "particles") {
            spriteTexture = r.CreateTexture(Pattern(true), true);
            if (!spriteTexture.IsValid())
                throw std::runtime_error("Billboard texture creation failed");
        }
        for (unsigned i = 1; i < o.views; ++i) {
            std::string error;
            auto target = r.CreateRenderTarget(Width / 2, Height / 2, error);
            if (!target.IsValid())
                throw std::runtime_error(error);
            targets.push_back(target);
        }
        for (unsigned i = 0; i < o.lights; ++i) {
            DynamicLight light;
            light.identity = i + 1;
            const float angle = float(i) * 2.39996323f;
            const float distance = o.kind == "lights" ? extent * .18f : extent * .65f;
            light.position = {std::cos(angle) * distance, extent * .55f + 5,
                              std::sin(angle) * distance};
            light.direction = glm::normalize(-light.position);
            light.range = extent * (o.kind == "lights" ? 4.f : 2.f);
            light.color = glm::vec3{2.4f, 2.1f, 1.8f} * (8.f / std::max(8u, o.lights));
            if (i < o.shadows) {
                light.kind = LightKind::Spot;
                light.innerConeDegrees = 30;
                light.outerConeDegrees = 55;
                light.castsShadow = true;
                light.shadowMapIndex = int(i + 1);
            }
            lights.push_back(light);
        }
        if (o.kind == "particles") {
            particles.reserve(o.objects);
            for (unsigned i = 0; i < o.objects; ++i) {
                ParticleBillboard particle;
                particle.position = {float(int(i % side) - int(side / 2)) * .35f,
                                     .5f + float((i * 17) % 97) * .035f,
                                     float(int(i / side) - int(side / 2)) * .35f};
                particle.size = .22f;
                particle.color = {.35f + float(i % 7) * .08f, .65f, .95f, .35f};
                particles.push_back(particle);
                particleBounds.Include(particle.position - glm::vec3(particle.size));
                particleBounds.Include(particle.position + glm::vec3(particle.size));
            }
            extent = float(side) * .2f + 3;
        }
    }
    ~Workload() {
        for (auto target : targets)
            r.DestroyRenderTarget(target);
        for (auto material : materials)
            r.DestroyMaterial(material);
        if (spriteTexture.IsValid())
            r.DestroyTexture(spriteTexture);
    }
    void Camera(unsigned view, unsigned frame) {
        const float phase = float(frame) * .003f;
        if (o.kind == "sprites") {
            CameraLens lens;
            lens.projection = CameraProjectionKind::Orthographic;
            lens.height = float(side) * .75f + 2;
            r.SetCameraFrame(BuildCameraFrame({float(view) * .3f + std::sin(phase) * .2f, 0, 10},
                                              {1, 0, 0, 0}, 65, .1f, 100, lens,
                                              view ? Width / 2 : Width, view ? Height / 2 : Height));
        } else {
            const glm::vec3 eye{extent * (.15f * std::sin(phase) + float(view) * .25f), extent * .9f,
                                extent * 1.75f};
            r.SetCamera(glm::lookAt(eye, glm::vec3(0, .5f, 0), {0, 1, 0}),
                        glm::perspective(glm::radians(65.f), float(Width) / Height, .1f, extent * 12));
        }
    }
    void Opaque() {
        r.SetMaterialBindings({});
        r.BeginMeshBatch();
        r.DrawBox({0, -.3f, 0}, {1, 0, 0, 0}, {extent, .2f, extent}, {.19f, .21f, .25f});
        for (unsigned i = 0; i < o.objects; ++i) {
            if (o.kind == "mixed" || i == 0) {
                MaterialBinding binding;
                binding.handle = materials[i % materials.size()];
                r.SetMaterialBindings({binding});
            }
            const auto position =
                glm::vec3{float(int(i % side) - int(side / 2)) * 1.2f, .5f + float(i % 5) * .04f,
                          float(int(i / side) - int(side / 2)) * 1.2f};
            const float mirror = i % 13 == 0 ? -1.f : 1.f;
            r.DrawBox(position, glm::angleAxis(float(i % 17) * .07f, glm::vec3(0, 1, 0)),
                      {mirror * .4f, .45f + float(i % 5) * .04f, .4f}, {.85f, .9f, 1});
        }
        r.EndMeshBatch();
        r.SetMaterialBindings({});
    }
    void Sprites() {
        for (unsigned i = 0; i < o.objects; ++i) {
            SpriteSettings sprite;
            sprite.size = {.9f, .9f};
            sprite.sortingLayer = int(i % 3);
            sprite.order = int(i % 7);
            sprite.ySort = true;
            sprite.flipX = i % 5 == 0;
            sprite.nearest = i % 11 != 0;
            sprite.pivot = {i % 9 == 0 ? .2f : .5f, .5f};
            r.QueueSprite(sprite,
                          {float(int(i % side) - int(side / 2)) * .7f,
                           float(int(i / side) - int(side / 2)) * .7f, 0},
                          {1, 0, 0, 0}, {1, 1, 1}, spriteTexture,
                          {.55f + float(i % 5) * .09f, .8f, .9f, .8f}, i + 1);
        }
        r.FlushSprites();
    }
    void Draw(unsigned frame) {
        r.ResetStats();
        r.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {.025f, .03f, .045f});
        r.ClearWeather();
        r.SetRenderMask(kAllCategories);
        r.SetRenderLayer(0);
        // Complete auxiliary views before opening the main HDR pass, matching
        // production WorldPresentation. No pass borrows an unfinished parent.
        for (unsigned step = 1; step <= o.views; ++step) {
            const unsigned view=step%o.views;
            if(!view)r.BeginFrame(Width,Height);
            if (view && !r.BeginRenderTarget(targets[view - 1]))
                throw std::runtime_error("Secondary target failed");
            Camera(view, frame);
            for (unsigned i = 0; i < o.shadows; ++i) {
                const auto &light = lights[i];
                r.BeginShadowPass(int(i + 1),
                                  ComputeSpotShadowMatrix(light.position, light.direction,
                                                          light.outerConeDegrees, light.range));
                Opaque();
                r.EndShadowPass();
            }
            Camera(view, frame);
            r.SetLighting(glm::normalize(glm::vec3(.3f, 1, .2f)), {.65f, .62f, .58f},
                          {.12f, .14f, .18f});
            r.SetDynamicLights(lights);
            if (o.kind == "sprites")
                Sprites();
            else if (o.kind == "particles")
                r.DrawParticles(particles, particleBounds, spriteTexture);
            else
                Opaque();
            if (view)
                r.EndRenderTarget();
        }
        r.EndFrame();
    }
};
struct Sample {
    std::uint64_t frame;
    double cpuSubmission, processCPU, outer;
    RenderStats stats;
};
Json StatsSummary(const std::vector<Sample> &samples) {
    std::vector<double> draws, triangles, instances, submission, process, outer;
    for (const auto &s : samples) {
        draws.push_back(s.stats.drawCalls);
        triangles.push_back(s.stats.triangles);
        instances.push_back(s.stats.instances);
        submission.push_back(s.cpuSubmission);
        process.push_back(s.processCPU);
        outer.push_back(s.outer);
    }
    return {{"cpu_submission_ms", Distribution(submission)},
            {"process_cpu_submission_ms", Distribution(process)},
            {"outer_frame_ms_including_swap", Distribution(outer)},
            {"draws", Distribution(draws)},
            {"triangles", Distribution(triangles)},
            {"instances", Distribution(instances)}};
}
} // namespace

int main(int argc, char **argv) {
    if (HandleBuildIdentityArguments(argc, argv, "Renderer V2 benchmark"))
        return 0;
    try {
        const auto o = Parse(argc, argv);
        const auto initialization = Clock::now();
        EngineHost host;
        std::string error;
        if (!host.Init("Judas Renderer V2 matched benchmark", Width, Height, o.visible, error))
            throw std::runtime_error(error);
        const double hostInit = Milliseconds(initialization);
        host.GetWindow().SetTestInputMode(true);
        SDL_GL_SetSwapInterval(0);
        auto &r = host.GetRenderer();
        auto &profiler = PerformanceProfiler::Get();
        profiler.Enable(false);
        const auto setup = Clock::now();
        Workload workload(r, o);
        const double resources = Milliseconds(setup);
        const auto cold = Clock::now();
        workload.Draw(0);
        const double coldSubmission = Milliseconds(cold);
        r.FinishForDiagnostics();
        const double coldCompleted = Milliseconds(cold);
        host.GetWindow().SwapBuffers();
        for (unsigned frame = 0; frame < o.warmup; ++frame) {
            host.GetWindow().PollEvents();
            workload.Draw(frame);
            host.GetWindow().SwapBuffers();
        }
        r.FinishForDiagnostics();
        profiler.Enable(true);
        profiler.Clear();
        std::map<std::uint64_t, ProfileGPURecord> gpuRecords;
        auto collect = [&]() {
            r.PollProfileGPU();
            for (const auto &frame : profiler.History())
                for (const auto &gpu : frame.gpu)
                    if (!gpu.pending)
                        gpuRecords[gpu.token] = gpu;
        };
        std::vector<Sample> samples;
        samples.reserve(o.frames);
        for (unsigned index = 0; index < o.frames; ++index) {
            host.GetWindow().PollEvents();
            ProfileFrame frame("Renderer V2 measured frame");
            const auto id = profiler.FrameId();
            const auto start = Clock::now();
            const auto cpu = std::clock();
            const auto token = r.BeginProfilePass(WholeScene);
            workload.Draw(index);
            r.EndProfilePass(token);
            const double submission = Milliseconds(start);
            const double process = 1000.0 * double(std::clock() - cpu) / CLOCKS_PER_SEC;
            const auto stats = r.Stats();
            host.GetWindow().SwapBuffers();
            const double outer = Milliseconds(start);
            frame.End();
            samples.push_back({id, submission, process, outer, stats});
            collect();
        }
        // One final completion boundary drains timestamps. It is outside every
        // measured CPU sample; the production loop never finishes per frame.
        r.FinishForDiagnostics();
        {
            // PollProfileGPU deliberately defers the current frame. Advancing
            // an unmeasured profiler frame lets the final measured query retire
            // without rendering an extra scene or fabricating a sample.
            ProfileFrame drain("Renderer V2 timestamp drain");
            collect();
        }
        const auto diagnostics = profiler.Diagnostics();
        std::map<std::uint64_t, double> whole;
        std::ofstream gpuCSV(o.out / "gpu.csv");
        gpuCSV << "frame,token,camera,pass,milliseconds\n";
        for (const auto &[token, record] : gpuRecords) {
            gpuCSV << record.frame << ',' << token << ',' << record.camera << ',' << record.pass.c_str()
                   << ',' << record.milliseconds << '\n';
            if (record.pass == WholeScene)
                whole[record.frame] = record.milliseconds;
        }
        std::ofstream csv(o.out / "frames.csv");
        csv << "sample,profile_frame,cpu_submission_ms,process_cpu_submission_ms,outer_frame_ms,gpu_ms,"
               "draws,triangles,instances,instanced_draws,particles,lights,light_overflow,shadow_"
               "passes,light_allocated_bytes,shadow_allocated_bytes,instance_upload_high_water_bytes,"
               "uploaded_bytes,upload_calls,material_applications\n";
        std::vector<double> gpu;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto &s = samples[i];
            const auto &v = s.stats;
            csv << i << ',' << s.frame << ',' << s.cpuSubmission << ',' << s.processCPU << ','
                << s.outer << ',';
            if (const auto found = whole.find(s.frame); found != whole.end()) {
                csv << found->second;
                gpu.push_back(found->second);
            } else
                csv << "NA";
            csv << ',' << v.drawCalls << ',' << v.triangles << ',' << v.instances << ','
                << v.instancedDraws << ',' << v.particlesSubmitted << ',' << v.dynamicLights << ','
                << v.lightTileOverflow << ',' << v.shadowPasses << ',' << v.lightBytes << ','
                << v.shadowBytes << ',' << v.instanceBytes << ',';
#if defined(JUDAS_RENDER_STATS_VERSION) && JUDAS_RENDER_STATS_VERSION >= 2
            csv << v.uploadedBytes << ',' << v.uploadCalls << ',' << v.materialApplications;
#else
            csv << "NA,NA,NA";
#endif
            csv << '\n';
        }
        const bool validGPU = gpu.size() == samples.size() && diagnostics.gpuDropped == 0;
        Json result = {
            {"format", 1},
            {"build", Json::parse(BuildIdentityJson())},
            {"workload",
             {{"case", o.kind},
              {"objects", o.objects},
              {"frames", o.frames},
              {"warmup", o.warmup},
              {"width", Width},
              {"height", Height},
              {"secondary_width", Width / 2},
              {"secondary_height", Height / 2},
              {"views", o.views},
              {"lights", o.lights},
              {"shadows", o.shadows}}},
            {"graphics",
             {{"vendor", GLString(GL_VENDOR)},
              {"renderer", GLString(GL_RENDERER)},
              {"version", GLString(GL_VERSION)}}},
            {"cold",
             {{"host_init_ms", hostInit},
              {"resource_setup_ms", resources},
              {"first_submission_ms", coldSubmission},
              {"first_completed_ms", coldCompleted}}},
            {"summary", StatsSummary(samples)},
            {"gpu_timing_valid", validGPU},
            {"gpu_samples_observed", gpu.size()},
            {"gpu_dropped", diagnostics.gpuDropped},
            {"profiler_truncated_frames", diagnostics.truncatedFrames},
            {"process_peak_rss_bytes", ProcessPeakRSS()},
            {"appearance_owned_bytes", r.AppearanceBytes()},
            {"notes", "Nearest-rank percentiles. CPU submission excludes swap/collection/readback/IO; "
                      "process CPU uses std::clock and includes process worker activity. GPU "
                      "timestamps may include command starvation. instanceBytes is maximum upload "
                      "size, not total bytes. Raw gpu.csv retains completed records beyond the bounded "
                      "profiler history. Cold work is outside measured frames. No simulated physics."}};
        result["summary"]["gpu_ms"] = validGPU ? Distribution(gpu) : Json(nullptr);
        profiler.Export((o.out / "profile-last-history.json").string(), error);
        profiler.Enable(false);
        // Canonical frame zero is independently re-rendered for image comparison;
        // it is never included in timing samples or captured after swapping.
        workload.Draw(0);
        r.FinishForDiagnostics();
        std::vector<unsigned char> pixels;
        r.CaptureFrame(Width, Height, pixels);
        if (!WriteRgbPng((o.out / "main.png").string(), Width, Height, pixels))
            throw std::runtime_error("Framebuffer capture failed");
        for (std::size_t i = 0; i < workload.targets.size(); ++i) {
            if (!r.BeginRenderTarget(workload.targets[i], true))
                throw std::runtime_error("Secondary readback target failed");
            r.CaptureFrame(Width / 2, Height / 2, pixels);
            if (!WriteRgbPng((o.out / ("view-" + std::to_string(i + 1) + ".png")).string(), Width / 2,
                             Height / 2, pixels))
                throw std::runtime_error("Secondary framebuffer capture failed");
            r.EndRenderTarget();
        }
        if (glGetError() != GL_NO_ERROR)
            throw std::runtime_error("Graphics error after benchmark");
        WriteJson(o.out / "RESULTS.json", result);
        std::printf("Renderer V2 case=%s objects=%u views=%u frames=%u gpu_valid=%d output=%s\n",
                    o.kind.c_str(), o.objects, o.views, o.frames, validGPU, o.out.string().c_str());
        return o.requireGPU && !validGPU ? 2 : 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Renderer V2 benchmark: %s\n", error.what());
        return 1;
    }
}
