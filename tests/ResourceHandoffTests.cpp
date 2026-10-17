#include "EngineHost.h"
#include "ModelArchive.h"
#include "PerformanceProfiler.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "SkeletalAnimation.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using Json = nlohmann::json;

namespace {
constexpr unsigned JointCount = 109;
constexpr unsigned MaterialCount = 8;
constexpr unsigned MapDimension = 512;
constexpr char FixtureId[] = "e781cd7828284fea9dfe37f73fac8819";

struct Fixture {
    fs::path root, assets, model;
    AssetId id;
    std::string mode;
    unsigned parts = 0, maps = 0;
};

// The regression exercises the normalized archive and real upload path, not
// the optional source-import consumer project. Embedded maps deliberately give
// the owner's normal 2 ms soft budget multiple indivisible driver units.
Fixture GenerateFixture(const fs::path& output, std::string& error) {
    Fixture fixture{output / "fixture", output / "fixture/Assets", {}, FixtureId,
                    "generated", MaterialCount, MaterialCount * 5};
    fs::create_directories(fixture.assets);
    fixture.model = fixture.assets / "staged.judasmodel";
    MeshData mesh;
    auto skeletal = std::make_shared<SkeletalAsset>();
    auto& skeleton = skeletal->skeleton;
    for (unsigned i = 0; i < JointCount; ++i) {
        skeleton.names.push_back("Joint" + std::to_string(i));
        skeleton.parents.push_back(i ? int(i - 1) : -1);
        skeleton.order.push_back(int(i));
        skeleton.rest.local.emplace_back();
        skeleton.skinNodes.push_back(int(i));
        skeleton.inverseBind.emplace_back(1.0f);
    }
    AnimationClip clip;
    clip.name = "Translate";
    clip.duration = 1;
    AnimationTrack track;
    track.times = {0, 1};
    track.values = {{0, 0, 0, 0}, {.25f, 0, 0, 0}};
    clip.tracks.push_back(track);
    skeletal->clips.push_back(clip);
    mesh.skeletal = skeletal;
    for (unsigned part = 0; part < MaterialCount; ++part) {
        MaterialDefinition material;
        for (unsigned role = 0; role < material.maps.size(); ++role) {
            auto& image = material.maps[role].embedded;
            image.width = image.height = MapDimension;
            image.pixels.resize(std::size_t(MapDimension) * MapDimension * 4);
            for (unsigned y = 0; y < MapDimension; ++y) {
                for (unsigned x = 0; x < MapDimension; ++x) {
                    const auto offset = (std::size_t(y) * MapDimension + x) * 4;
                    image.pixels[offset] = uint8_t((x + part * 17) % 256);
                    image.pixels[offset + 1] = uint8_t((y + role * 29) % 256);
                    image.pixels[offset + 2] = uint8_t((x ^ y ^ (part + role)) % 256);
                    image.pixels[offset + 3] = 255;
                }
            }
        }
        mesh.materials.push_back(std::move(material));
        mesh.materialKeys.push_back("Material" + std::to_string(part));
        const unsigned first = unsigned(mesh.indices.size());
        for (unsigned joint = 0; joint < JointCount; ++joint) {
            const unsigned base = unsigned(mesh.vertices.size());
            for (auto corner : {glm::vec2(0, 0), glm::vec2(1, 0),
                                glm::vec2(1, 1), glm::vec2(0, 1)}) {
                MeshVertex vertex;
                vertex.position = {float(part) + corner.x, float(joint) + corner.y, 0};
                vertex.normal = {0, 0, 1};
                vertex.uv = vertex.uv1 = corner;
                mesh.vertices.push_back(vertex);
                MeshSkinVertex skin;
                for (unsigned influence = 0; influence < 4; ++influence) {
                    skin.joints[influence] = (joint + influence) % JointCount;
                    skin.joints1[influence] = (joint + influence + 4) % JointCount;
                }
                skin.weights = skin.weights1 = glm::vec4(.125f);
                mesh.skinVertices.push_back(skin);
            }
            for (unsigned index : {0u, 1u, 2u, 0u, 2u, 3u}) mesh.indices.push_back(base + index);
        }
        mesh.primitives.emplace_back(first, unsigned(mesh.indices.size()) - first,
                                     int(part), "Part" + std::to_string(part));
    }
    std::vector<uint8_t> bytes;
    if (!EncodeModelArchive(mesh, bytes, error)) throw std::runtime_error(error);
    {
        std::ofstream file(fixture.model, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        if (!file) throw std::runtime_error("cannot write owned model archive");
    }
    if (!AssetDatabase::WriteMeta(fixture.model.string() + kAssetMetaExtension, fixture.id,
                                  AssetType::Mesh, "generated resource handoff regression", error))
        throw std::runtime_error(error);
    std::string hash;
    if (!SceneFingerprintSha256File(fixture.model.string(), hash, error)) throw std::runtime_error(error);
    std::ofstream(output / "fixture.json") << Json{
        {"mode", fixture.mode}, {"modelSha256", hash}, {"archiveBytes", bytes.size()},
        {"skinJoints", JointCount}, {"vertices", mesh.vertices.size()},
        {"indices", mesh.indices.size()}, {"parts", fixture.parts}, {"maps", fixture.maps},
        {"mapWidth", MapDimension}, {"mapHeight", MapDimension},
        {"rgbaPayloadBytes", std::uint64_t(fixture.maps) * MapDimension * MapDimension * 4}
    }.dump(2) << '\n';
    return fixture;
}

Fixture OriginalFixture(std::string& error) {
    Project project;
    if (!project.Load("projects/import_lab/import_lab.judasproj", error)) throw std::runtime_error(error);
    Scene scene;
    if (!LoadSceneFromFile(project.StartupScenePath(), scene, error)) throw std::runtime_error(error);
    const auto* object = scene.Find(10);
    if (!object || !object->render) throw std::runtime_error("original fixture entity 10 has no render model");
    Fixture fixture{project.RootDir(), project.AssetsDir(), {}, object->render->meshAsset, "original"};
    AssetDatabase assets;
    assets.Scan(fixture.root.string(), fixture.assets.string());
    const auto* record = assets.Find(fixture.id);
    if (!record || record->missing)
        throw std::runtime_error("optional original import-lab model " + fixture.id +
            " is unavailable; restore the author's local imported assets, or omit --original for the self-contained gate");
    fixture.model = record->path;
    return fixture;
}

struct UploadTrace {
    std::atomic<unsigned> meshesCreated{0}, meshesDestroyed{0}, texturesCreated{0}, texturesDestroyed{0};
    std::atomic<bool> ownerContext{true};
    std::atomic<bool> pauseNextMap{false};
    std::atomic<std::uint64_t> injectedPauseNanoseconds{0};
    const std::thread::id owner = std::this_thread::get_id();
    void Observe(const ResourceTraceEvent& event) {
        bool gpu = true;
        switch (event.point) {
            case ResourceTracePoint::MeshCreated: ++meshesCreated; break;
            case ResourceTracePoint::MeshDestroyed: ++meshesDestroyed; break;
            case ResourceTracePoint::TextureCreated:
                ++texturesCreated;
                if (pauseNextMap.exchange(false)) {
                    // Cooperative observation seam, armed only after Init and
                    // CPU preparation. No reentry or resource mutation. A fast
                    // GPU must also yield under the normal 2 ms soft budget,
                    // so cancellation observes a real partially uploaded map.
                    const auto start = Clock::now();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    injectedPauseNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - start).count();
                }
                break;
            case ResourceTracePoint::TextureDestroyed: ++texturesDestroyed; break;
            default: gpu = false; break;
        }
        if (gpu && (event.thread != owner || !event.contextCurrent)) ownerContext = false;
    }
};
} // namespace

int main(int argc, char** argv) {
    if ((argc != 2 && argc != 3) || (argc == 3 && std::string(argv[2]) != "--original")) {
        std::cerr << "usage: judas_resource_handoff_tests NEW-owned-output [--original]\n";
        return 2;
    }
    unsigned checks = 0;
    auto check = [&](bool ok, const std::string& label) {
        ++checks;
        if (!ok) throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    try {
        const auto output = fs::absolute(argv[1]);
        check(!fs::exists(output), "fresh owned output required");
        fs::create_directories(output);
        std::string error;
        const auto fixture = argc == 3 ? OriginalFixture(error) : GenerateFixture(output, error);
        check(fs::is_regular_file(fixture.model), "real self-contained model archive is available");
        auto& profiler = PerformanceProfiler::Get();
        profiler.Enable(true);
        profiler.RegisterThread("Resource handoff owner");
        UploadTrace trace;
        EngineHost host;
        check(host.Init("Private resource handoff", 640, 480, false, error,
                        [&](const ResourceTraceEvent& event) { trace.Observe(event); }),
              "actual owner-thread GL context: " + error);
        host.OpenProjectAssets(fixture.root.string(), fixture.assets.string());
        std::cout << "GL renderer: " << reinterpret_cast<const char*>(glGetString(GL_RENDERER))
                  << " vendor: " << reinterpret_cast<const char*>(glGetString(GL_VENDOR)) << '\n';
        auto& resources = host.Resources();
        const unsigned initialMeshes = trace.meshesCreated;
        const unsigned initialTextures = trace.texturesCreated;
        const unsigned initialMeshDestroys = trace.meshesDestroyed;
        const unsigned initialTextureDestroys = trace.texturesDestroyed;
        const AssetId absent = "ffffffffffffffffffffffffffffffff";
        check(resources.RequestMesh(absent) == ResourceState::Failed &&
                  !resources.TryGetMesh(absent).IsValid() && !resources.ErrorOf(absent).empty(),
              "unknown asset fails with diagnostic rather than publishing a placeholder");
        resources.Release(absent);
        resources.AddRef(fixture.id);
        resources.RequestMesh(fixture.id);
        const auto start = Clock::now();
        std::vector<double> pumps;
        unsigned privateFrames = 0;
        unsigned cancelledMaps = 0;
        bool cancelled = false, admissionBounded = true, publicationPrivate = true;
        // Explicitly hold the normal upload admission at zero to prove the
        // worker result remains private before allowing any GL work.
        bool prepared = false;
        while (resources.StateOf(fixture.id) != ResourceState::Ready) {
            if (Clock::now() - start >= std::chrono::seconds(30))
                throw std::runtime_error("bounded ready progress");
            {
                ProfileFrame frame("Resource admission/upload");
                const auto before = Clock::now();
                resources.Pump(prepared ? 2 : 0);
                pumps.push_back(std::chrono::duration<double, std::milli>(Clock::now() - before).count());
            }
            admissionBounded &= resources.Stats().admittedRequests <= 3;
            if (resources.StateOf(fixture.id) == ResourceState::CpuReady) {
                ++privateFrames;
                publicationPrivate &= !resources.TryGetMesh(fixture.id).IsValid() &&
                    !resources.TryGetSkeletal(fixture.id) && !resources.TryGetModelParts(fixture.id);
                if (!prepared) {
                    check(trace.meshesCreated == initialMeshes && trace.texturesCreated == initialTextures,
                          "completed CPU preparation publishes no partial GPU object");
                    trace.pauseNextMap = true;
                    prepared = true;
                } else if (!cancelled && trace.meshesCreated > initialMeshes &&
                           trace.texturesCreated > initialTextures) {
                    // Trace observes a real Renderer allocation; CpuReady alone
                    // is not accepted as proof of a staged GPU cancellation.
                    const unsigned createdMeshes = trace.meshesCreated;
                    const unsigned createdTextures = trace.texturesCreated;
                    cancelledMaps = createdTextures - initialTextures;
                    resources.ReleaseRef(fixture.id);
                    resources.Pump();
                    check(!resources.TryGetMesh(fixture.id).IsValid() && !resources.TryGetSkeletal(fixture.id) &&
                              resources.StateOf(fixture.id) != ResourceState::Ready,
                          "cancelled private GPU upload cannot publish");
                    check(trace.meshesDestroyed - initialMeshDestroys == createdMeshes - initialMeshes &&
                              trace.texturesDestroyed - initialTextureDestroys == createdTextures - initialTextures,
                          "cancellation destroys every actually allocated private mesh and map");
                    resources.AddRef(fixture.id);
                    resources.RequestMesh(fixture.id);
                    cancelled = true;
                }
            }
            if (resources.StateOf(fixture.id) == ResourceState::Failed)
                throw std::runtime_error(resources.ErrorOf(fixture.id));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto skeleton = resources.TryGetSkeletal(fixture.id);
        const auto* parts = resources.TryGetModelParts(fixture.id);
        check(resources.TryGetMesh(fixture.id).IsValid() && skeleton &&
                  skeleton->skeleton.skinNodes.size() == JointCount,
              "complete 109-joint skeletal asset publishes normally");
        if (fixture.parts) {
            check(parts && parts->size() == fixture.parts && skeleton->clips.size() == 1 &&
                      trace.texturesCreated - trace.texturesDestroyed == initialTextures - initialTextureDestroys + fixture.maps,
                  "complete multipart geometry, clip and all embedded maps publish together");
        }
        check(admissionBounded && publicationPrivate, "bounded admission and private publication throughout all pumps");
        check(prepared && privateFrames > 1 && cancelled && trace.injectedPauseNanoseconds > 0,
              "real staged GPU cancellation and fresh-generation retry exercised");
        check(resources.DecodeThreadOf(fixture.id) != std::thread::id() &&
                  resources.DecodeThreadOf(fixture.id) != resources.OwnerThread() && trace.ownerContext,
              "decode runs on a worker while all GPU operations retain the owner GL context");
        std::sort(pumps.begin(), pumps.end());
        const auto stats = resources.Stats();
        Json result{{"fixtureMode", fixture.mode}, {"readyIncludingCancellationMs",
            std::chrono::duration<double, std::milli>(Clock::now() - start).count()},
            {"timingScope", "functional diagnostic; includes one injected owner-thread map pause"},
            {"injectedPauseMinimumMs", 5},
            {"injectedPauseActualMs", double(trace.injectedPauseNanoseconds) / 1e6},
            {"pumpMs", {{"median", pumps[pumps.size() / 2]},
                        {"p95", pumps[(pumps.size() - 1) * 95 / 100]}, {"maximum", pumps.back()}}},
            {"privateFrames", privateFrames}, {"cancelledUploadedMaps", cancelledMaps},
            {"residentBytes", stats.bytesResident}, {"samples", pumps.size()}};
        resources.ReleaseRef(fixture.id);
        resources.Release(fixture.id);
        resources.Pump();
        check(resources.Stats().loading == 0 && resources.Stats().bytesResident == 0 &&
                  trace.meshesCreated - initialMeshes == trace.meshesDestroyed - initialMeshDestroys &&
                  trace.texturesCreated - initialTextures == trace.texturesDestroyed - initialTextureDestroys,
              "retirement returns CPU/GPU residency and actual allocated resources to zero");
        check(profiler.Export((output / "m56-profile.json").string(), error), "resource profiler evidence exported");
        result["checks"] = checks;
        result["postUnloadBytes"] = resources.Stats().bytesResident;
        result["failures"] = 0;
        std::ofstream(output / "results.json") << result.dump(2) << '\n';
        host.Shutdown();
        std::cout << "SUMMARY " << checks << " checks\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL " << exception.what() << '\n';
        return 1;
    }
}
