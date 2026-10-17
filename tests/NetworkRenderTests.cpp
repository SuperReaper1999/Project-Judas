#include "EngineHost.h"
#include "NetworkSession.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "WorldPresentation.h"

#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr int Width = 320, Height = 192;
unsigned checks = 0;
void Check(bool pass, const std::string& description) {
    ++checks;
    std::printf("%s %s\n", pass ? "PASS" : "FAIL", description.c_str());
    std::fflush(stdout);
    if (!pass) throw std::runtime_error(description);
}

glm::mat4 View(float offset = 0) {
    return glm::lookAt(glm::vec3(offset, 0, 8), glm::vec3(offset, 0, 0), glm::vec3(0, 1, 0));
}
glm::mat4 Projection() { return glm::ortho(-8.f, 8.f, -4.8f, 4.8f, .1f, 20.f); }
double ProjectX(glm::vec3 point, int width = Width, float offset = 0) {
    const auto clip = Projection() * View(offset) * glm::vec4(point, 1);
    return (double(clip.x / clip.w) * .5 + .5) * width - .5;
}
struct Centroid { double x = 0; size_t count = 0; };
Centroid RedPixels(const std::vector<unsigned char>& pixels) {
    Centroid result;
    for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x) {
        const size_t index = size_t(y * Width + x) * 3;
        const unsigned red = pixels.at(index), green = pixels.at(index + 1), blue = pixels.at(index + 2);
        if (red > 60 && red > green * 3 && red > blue * 3) {
            result.x += x;
            ++result.count;
        }
    }
    if (result.count) result.x /= double(result.count);
    return result;
}
std::vector<unsigned char> Draw(Renderer& renderer, RuntimeWorld& world, float offset = 0) {
    renderer.SetSceneAppearance(false, 1, {}, 1, {1, 0, 0, 0}, false, {0, 0, 0});
    renderer.BeginFrame(Width, Height);
    renderer.SetCamera(View(offset), Projection());
    renderer.SetLighting({0, 0, 1}, {0, 0, 0}, {1, 1, 1});
    renderer.SetDynamicLights({});
    renderer.InvalidateShadowSlot(0);
    DrawWorldGeometry(renderer, world, nullptr, 1, {});
    renderer.EndFrame();
    std::vector<unsigned char> pixels;
    renderer.CaptureFrame(Width, Height, pixels);
    return pixels;
}

struct Instance {
    RuntimeWorld world;
    explicit Instance(EngineHost& engine, const Project& project, const Scene& scene) {
        std::string error;
        world.legacyGameplay = false;
        world.SetSceneControl(std::make_shared<SceneSession>(project, project.StartupScenePath()));
        Check(world.Build(scene, &engine.Resources(), error, &project.Settings().classification,
                          &project.Settings().navigation), "ordinary render/network world builds: " + error);
    }
    ~Instance() { world.Destroy(); }
};
}

int main(int argc, char** argv) {
    try {
        const fs::path output = fs::absolute(argc > 1 ? argv[1] : ".cache/m99-network-render");
        const fs::path fixture = output / "fixture";
        fs::create_directories(fixture / "Assets");
        fs::create_directories(fixture / "Scenes");
        ProjectSettings settings;
        settings.name = "M99 actual replica draw fixture";
        settings.legacyGameplay = false;
        settings.startupScene = "Scenes/render.judas";
        { std::ofstream file(fixture / "render.judasproj"); file << Project::SerializeToString(settings); }
        Scene scene;
        scene.Settings().sunEnabled = false;
        scene.Settings().ambientColor = glm::vec3(1);
        scene.Settings().backgroundColor = glm::vec3(0);
        auto& box = scene.CreateObject("Physical red subject");
        const EntityId subject = box.id;
        box.body = SceneBodyComponent{};
        box.body->motion = SceneBodyMotion::Dynamic;
        box.body->mass = 1;
        box.body->halfExtents = glm::vec3(.25f);
        box.render = SceneRenderComponent{};
        box.render->halfExtents = glm::vec3(.25f);
        box.render->color = {1, .015f, .015f};
        std::string error;
        Check(SaveSceneToFile(scene, (fixture / "Scenes/render.judas").string(), error),
              "real project scene writes: " + error);
        Project project;
        Check(project.Load((fixture / "render.judasproj").string(), error), "real project loads: " + error);
        EngineHost engine;
        Check(engine.Init("M99 actual replica rendering", Width, Height, false, error),
              "normal EngineHost initializes real GL renderer: " + error);
        engine.OpenProjectAssets(project.RootDir(), project.AssetsDir());
        {
            Instance host(engine, project, scene), client(engine, project, scene);
            NetworkSession::Options options;
            options.address = "127.0.0.1";
            options.port = 0;
            options.snapshotHz = 30;
            Check(host.world.Network().Host(options, error), "real native TCP host begins: " + error);
            double now = 100;
            auto pump = [&] {
                now += 1. / 60;
                engine.PumpResources();
                host.world.PumpNetwork(now);
                host.world.NetworkAfterFixed(1.f / 60);
                client.world.PumpNetwork(now);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            };
            for (unsigned i = 0; i < 600 && !host.world.Network().Port(); ++i) pump();
            Check(host.world.Network().Port() != 0, "host publishes a real listening port after content verification");
            std::string identity;
            Check(host.world.Network().Replicate(subject, "host", "null", identity, error),
                  "existing physical root registers for ordinary replication: " + error);
            options.port = host.world.Network().Port();
            Check(client.world.Network().Join(options, error), "independent replica world joins through TCP: " + error);
            for (unsigned i = 0; i < 600 && client.world.Network().Stats().snapshots < 8; ++i) pump();
            Check(client.world.Network().Status() == "connected" && client.world.Network().Stats().snapshots >= 8,
                  "actual content handshake and repeated physical snapshots arrive");

            // Supply a known authoritative body trajectory, not a fabricated
            // client interpolation record. Actual sockets deliver every sample.
            // The renderer must choose the buffer rather than the latest pose.
            for (unsigned i = 0; i < 24; ++i) {
                EntityPhysicalState state;
                Check(host.world.GetEntityState(subject, state), "read existing authoritative physical body");
                state.position = {-4.f + .3f * float(i), 0, 0};
                state.linearVelocity = {18, 0, 0};
                Check(host.world.SetEntityState(subject, state), "publish known authoritative trajectory sample");
                pump();
            }
            EntityPhysicalState latest;
            SceneTransform buffered;
            Check(client.world.GetEntityState(subject, latest) && client.world.Network().Presentation(subject, buffered),
                  "client holds distinct authoritative and buffered motion observations");
            const double gap = glm::distance(latest.position, buffered.position);
            Check(gap >= 1 && latest.position.x > buffered.position.x,
                  "test creates at least one metre of real latest-versus-buffered pose separation");
            const double bufferedX = ProjectX(buffered.position), latestX = ProjectX(latest.position);
            auto pixels = Draw(engine.GetRenderer(), client.world);
            Check(WriteRgbPng((output / "replica-buffered.png").string(), Width, Height, pixels),
                  "preserve actual rendered replica pixels");
            auto centroid = RedPixels(pixels);
            std::printf("REPLICA_RENDER latest_x=%.6f buffered_x=%.6f gap_m=%.6f expected_pixel=%.3f latest_pixel=%.3f observed_pixel=%.3f red_pixels=%zu\n",
                        latest.position.x, buffered.position.x, gap, bufferedX, latestX, centroid.x, centroid.count);
            Check(centroid.count >= 50 && std::abs(centroid.x - bufferedX) < 2,
                  "actual dynamic colour submission renders the buffered network pose");
            Check(std::abs(centroid.x - latestX) > std::abs(bufferedX - latestX) * .7,
                  "negative control rejects latest-authoritative drawing despite a correct service buffer");

            // A second view uses the same buffered world pose, not a per-view
            // advance or stale draw submission from the first eye.
            pixels = Draw(engine.GetRenderer(), client.world, 1);
            centroid = RedPixels(pixels);
            Check(centroid.count >= 50 && std::abs(centroid.x - ProjectX(buffered.position, Width, 1)) < 2,
                  "second real camera projects the same buffered replica pose independently");

            // Read actual shadow depth, not the renderer's CPU submissions.
            auto& renderer = engine.GetRenderer();
            renderer.BeginShadowPass(0, Projection() * View());
            DrawWorldGeometry(renderer, client.world, nullptr, 1, {});
            GLint viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, viewport);
            std::vector<float> depth(static_cast<size_t>(viewport[2]));
            glReadPixels(0, viewport[3] / 2, viewport[2], 1, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            Centroid shadow;
            for (int x = 0; x < viewport[2]; ++x) if (depth[size_t(x)] < .99f) {
                shadow.x += x; ++shadow.count;
            }
            if (shadow.count) shadow.x /= double(shadow.count);
            renderer.EndShadowPass();
            Check(shadow.count >= 5 && std::abs(shadow.x - ProjectX(buffered.position, viewport[2])) < 3,
                  "actual shadow depth also follows the buffered physical replica");

            MaterialSlot blend;
            blend.overrides.alpha = MaterialAlpha::Blend;
            blend.overrides.baseColor = glm::vec4(1, 1, 1, .5f);
            Check(client.world.SetMaterialSlot(subject, 0, blend), "ordinary material override enables blended rendering");
            pixels = Draw(renderer, client.world);
            centroid = RedPixels(pixels);
            Check(centroid.count >= 50 && std::abs(centroid.x - bufferedX) < 2,
                  "blended dynamic colour output preserves the buffered replica pose");

            Check(client.world.SetRenderVisible(subject, true, false), "ordinary visibility gate hides the replica");
            pixels = Draw(renderer, client.world);
            Check(RedPixels(pixels).count == 0, "hidden-subject negative control leaves no spurious red-centroid signal");
            client.world.SetRenderVisible(subject, true, true);
            client.world.Network().Close();
            Check(client.world.SetMaterialSlot(subject, 0, {}), "offline control restores ordinary opaque material");
            EntityPhysicalState offline;
            Check(client.world.GetEntityState(subject, offline), "offline control has its original physical body");
            offline.position = {-5, 0, 0};
            client.world.SetEntityState(subject, offline);
            pixels = Draw(renderer, client.world);
            centroid = RedPixels(pixels);
            Check(centroid.count >= 50 && std::abs(centroid.x - ProjectX(offline.position)) < 2,
                  "closed-session renderer retains the ordinary non-network physical presentation path");
            Check(glGetError() == GL_NO_ERROR, "colour, second view, shadow and blend leave coherent GL state");
        }
        engine.Shutdown();
        std::printf("SUMMARY checks=%u failures=0\n", checks);
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "M99 network rendering: %s\nSUMMARY checks=%u failures=1\n", exception.what(), checks);
        return 1;
    }
}
