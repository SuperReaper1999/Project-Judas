#include "BuildIdentity.h"
#include "CrashRig.h"
#include "EngineHost.h"
#include "ModelLoader.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SkeletalAnimation.h"
#include "WorldPersistence.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
double Elapsed(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
void Require(bool ok, const std::string &error) {
    if (!ok)
        throw std::runtime_error(error);
}
struct VehicleModel {
    MeshData body;
    std::array<MeshData, 4> wheels;
    std::array<glm::vec3, 4> wheelCenters{};
};
VehicleModel LoadVehicle(const std::string &path) {
    MeshData source;
    std::string error;
    Require(LoadModelMesh(path, source, error), error);
    if (source.skeletal) {
        auto palette = ResolveSkinMatrices(source.skeletal->skeleton, source.skeletal->skeleton.rest);
        std::vector<float> orientation(source.vertices.size(), 1);
        Require(source.skinVertices.size() == source.vertices.size(), "source rest-pose skin is incomplete");
        for (size_t i = 0; i < source.vertices.size(); ++i) {
            auto &v = source.vertices[i];
            auto skin = source.skinVertices[i];
            glm::mat4 transform(0);
            for (unsigned k = 0; k < 4; ++k) {
                transform += palette.at(skin.joints[k]) * skin.weights[k];
                transform += palette.at(skin.joints1[k]) * skin.weights1[k];
            }
            v.position = glm::vec3(transform * glm::vec4(v.position, 1));
            v.normal = glm::normalize(glm::transpose(glm::inverse(glm::mat3(transform))) * v.normal);
            orientation[i] = glm::determinant(glm::mat3(transform)) < 0 ? -1.f : 1.f;
            v.tangent = glm::vec4(glm::normalize(glm::mat3(transform) * glm::vec3(v.tangent)),
                                  v.tangent.w * orientation[i]);
        }
        for (size_t t = 0; t < source.indices.size(); t += 3)
            if (orientation[source.indices[t]] + orientation[source.indices[t + 1]] +
                    orientation[source.indices[t + 2]] <
                0)
                std::swap(source.indices[t + 1], source.indices[t + 2]);
    }
    glm::vec3 low(1e20), high(-1e20);
    for (auto &v : source.vertices) {
        low = glm::min(low, v.position);
        high = glm::max(high, v.position);
    }
    // Local profiles: FBX car sources face +Z; the engine's wheels face -Z.
    const float scale = (high.z - low.z) < .1f ? 100.f : 1.f;
    std::cout << "MODEL_WORLD_SIZE " << high.x - low.x << "," << high.y - low.y << "," << high.z - low.z
              << " fixture_scale=" << scale << " vertices=" << source.vertices.size()
              << " triangles=" << source.indices.size() / 3 << '\n';
    const glm::vec3 offset{(low.x + high.x) * .5f, .75f / scale, (low.z + high.z) * .5f};
    for (auto &v : source.vertices) {
        v.position = (v.position - offset) * scale;
        v.position.x = -v.position.x;
        v.position.z = -v.position.z;
        v.normal.x = -v.normal.x;
        v.normal.z = -v.normal.z;
        v.tangent.x = -v.tangent.x;
        v.tangent.z = -v.tangent.z;
    }
    VehicleModel result;
    std::array<std::map<int, int>, 5> slots;
    for (const auto &p : source.primitives) {
        auto name = p.part;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        bool wheel = name.find("wheel") != std::string::npos || name.find("tyre") != std::string::npos ||
                     name.find("tire") != std::string::npos || name.find("/rim") != std::string::npos ||
                     name.find("_rim") != std::string::npos || name.find("spoke") != std::string::npos;
        std::array<unsigned, 5> first{};
        for (unsigned i = 0; i < 5; ++i)
            first[i] = unsigned(i ? result.wheels[i - 1].indices.size() : result.body.indices.size());
        for (unsigned t = p.first; t < p.first + p.count; t += 3) {
            glm::vec3 mean(0);
            for (unsigned k = 0; k < 3; ++k)
                mean += source.vertices[source.indices[t + k]].position / 3.f;
            unsigned destination = wheel ? 1 + (mean.z > 0 ? 2u : 0u) + (mean.x > 0 ? 1u : 0u) : 0;
            auto &mesh = destination ? result.wheels[destination - 1] : result.body;
            for (unsigned k = 0; k < 3; ++k) {
                mesh.indices.push_back(unsigned(mesh.vertices.size()));
                mesh.vertices.push_back(source.vertices[source.indices[t + k]]);
            }
        }
        for (unsigned i = 0; i < 5; ++i) {
            auto &mesh = i ? result.wheels[i - 1] : result.body;
            auto n = unsigned(mesh.indices.size()) - first[i];
            if (!n)
                continue;
            int material = -1;
            if (p.material >= 0) {
                auto [it, added] = slots[i].emplace(p.material, int(mesh.materials.size()));
                material = it->second;
                if (added) {
                    mesh.materials.push_back(source.materials.at(p.material));
                    mesh.materialKeys.push_back(source.materialKeys.at(p.material));
                    auto &material = mesh.materials.back();
                    if (i) {
                        material.alpha = MaterialAlpha::Opaque;
                        material.baseColor.a = 1;
                    } else if (name.find("carpaint") != std::string::npos ||
                               name.find("paintwork") != std::string::npos) {
                        material.baseColor = {.18f, .4f, .7f, 1};
                        material.metallic = .25f;
                        material.roughness = .35f;
                    }
                }
            }
            mesh.primitives.push_back({first[i], n, material, p.part, p.node});
        }
    }
    for (unsigned i = 0; i < 4; ++i) {
        auto &m = result.wheels[i];
        low = glm::vec3(1e20);
        high = -low;
        for (auto &v : m.vertices) {
            low = glm::min(low, v.position);
            high = glm::max(high, v.position);
        }
        result.wheelCenters[i] = m.indices.empty() ? glm::vec3(0) : (low + high) * .5f;
        for (auto &v : m.vertices)
            v.position -= result.wheelCenters[i];
    }
    return result;
}
void Snapshot(Renderer &renderer, const std::filesystem::path &path, int w, int h) {
    std::vector<unsigned char> pixels;
    renderer.CaptureFrame(w, h, pixels);
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << w << ' ' << h << "\n255\n";
    for (int y = 0; y < h; ++y)
        out.write(reinterpret_cast<const char *>(pixels.data() + size_t(y) * w * 3), w * 3);
}
void Summary(const char *name, std::vector<double> values) {
    if (values.empty())
        return;
    std::sort(values.begin(), values.end());
    auto q = [&](double p) {
        return values[std::min(values.size() - 1, size_t(std::ceil(p * values.size())) - 1)];
    };
    std::cout << name << " count=" << values.size()
              << " mean_ms=" << std::accumulate(values.begin(), values.end(), 0.) / values.size()
              << " p50_ms=" << q(.5) << " p95_ms=" << q(.95) << " p99_ms=" << q(.99)
              << " worst_ms=" << values.back() << '\n';
}
std::string DurableShapeHash(RuntimeWorld &world, const std::vector<CrashRig> &rigs) {
    SaveArchive evidence;
    for (const auto &rig : rigs) {
        std::string error;
        auto *state = world.RuntimeDeformable(rig.owner, error);
        Require(state != nullptr, error);
        evidence(state->positions, state->velocities, state->fracture.plasticTranslation,
                 state->fracture.plasticRotation, state->fracture.plasticWork, state->fracture.broken,
                 state->fracture.removed);
        for (auto wheelId : rig.wheels) {
            auto *wheel = world.RuntimeWheel(wheelId);
            evidence(wheel->state.angularVelocity, wheel->state.angle);
        }
        auto *train = world.RuntimePowertrain(rig.owner);
        evidence(train->absoluteSpin);
        for (bool initialized : train->internalInitialized)
            evidence(initialized);
    }
    return SceneFingerprintSha256(evidence.bytes);
}
} // namespace
int main(int argc, char **argv) {
    if (HandleBuildIdentityArguments(argc, argv, "crash_proof"))
        return 0;
    try {
        if (argc < 3) {
            std::cerr << "usage: judas_crash_proof LOCAL_MODEL LOCAL_WORK [--headless] [--frames=N] "
                         "[--case=frontal|offset|side|rear|cars] [--count=N] [--load] [--save-at=N]\n";
            return 64;
        }
        std::string modelPath = argv[1], kind = "frontal";
        std::filesystem::path root = std::filesystem::absolute(argv[2]);
        unsigned frames = 0, count = 1, saveAt = 0, layers = 2, columns = 2, segments = 8;
        float speed = 12;
        bool headless = false, load = false, profile = false, drive = false, cpuSkin = false, repeat = false,
             postDrive = false, xray = false;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--segments=", 0) == 0)
                segments = unsigned(std::stoul(arg.substr(11)));
            else if (arg == "--xray")
                xray = true;
            else if (arg.rfind("--columns=", 0) == 0)
                columns = unsigned(std::stoul(arg.substr(10)));
            else if (arg == "--repeat")
                repeat = true;
            else if (arg == "--post-drive")
                postDrive = true;
            else if (arg == "--cpu-skin")
                cpuSkin = true;
            else if (arg.rfind("--layers=", 0) == 0)
                layers = unsigned(std::stoul(arg.substr(9)));
            else if (arg == "--profile")
                profile = true;
            else if (arg == "--drive")
                drive = true;
            else if (arg.rfind("--speed=", 0) == 0)
                speed = std::stof(arg.substr(8));
            else if (arg == "--headless")
                headless = true;
            else if (arg == "--load")
                load = true;
            else if (arg.rfind("--frames=", 0) == 0)
                frames = unsigned(std::stoul(arg.substr(9)));
            else if (arg.rfind("--count=", 0) == 0)
                count = unsigned(std::stoul(arg.substr(8)));
            else if (arg.rfind("--save-at=", 0) == 0)
                saveAt = unsigned(std::stoul(arg.substr(10)));
            else if (arg.rfind("--case=", 0) == 0)
                kind = arg.substr(7);
            else
                throw std::runtime_error("unknown argument " + arg);
        }
        if (headless && !frames)
            frames = 600;
        Require(kind == "frontal" || kind == "offset" || kind == "side" || kind == "rear" || kind == "cars",
                "unknown crash case");
        Require(count >= 1 && count <= 8, "vehicle count must be 1..8");
        Require(std::isfinite(speed) && speed >= 0 && speed <= 30, "crash speed must be 0..30 m/s");
        if (kind == "cars")
            count = std::max(count, 2u);
        std::filesystem::create_directories(root / "Assets");
        auto preparation = Clock::now();
        auto model = LoadVehicle(modelPath);
        Require(layers == 1 || layers == 2, "layers must be 1 or 2");
        Require(columns == 2 || columns == 3, "columns must be 2 or 3");
        Require(segments == 8 || segments == 12, "segments must be 8 or 12");
        auto asset = MakeCrashStructure(segments, layers, columns);
        {
            std::ofstream out(root / "Assets/structure.judasdeform", std::ios::binary);
            out << EncodeDeformableAsset(asset);
        }
        std::unique_ptr<EngineHost> host;
        if (!headless) {
            host = std::make_unique<EngineHost>();
            std::string error;
            Require(host->Init("Judas Deformables V2 | Physical Crash Proof", 1440, 900, true, error), error);
        }
        AssetDatabase db;
        db.Scan(root.string(), (root / "Assets").string());
        AssetRecord record;
        std::string error;
        if (auto *existing = db.FindByRelativePath("Assets/structure.judasdeform"))
            record = *existing;
        else
            Require(db.Track((root / "Assets/structure.judasdeform").string(), record, error), error);
        ResourceManager resources(nullptr, &db);
        resources.RequestDeformable(record.id);
        resources.WaitForAll();
        Scene scene;
        auto box = [&](const std::string &name, glm::vec3 position, glm::vec3 half) {
            auto &o = scene.CreateObject(name);
            o.transform.position = position;
            o.body = SceneBodyComponent{};
            o.body->halfExtents = half;
            o.body->friction = .8f;
            return o.id;
        };
        box("Ground", {0, -.5f, 0}, {1000, .5f, 1000});
        auto &gravity = scene.CreateObject("Gravity");
        gravity.gravity = SceneGravityComponent{};
        gravity.gravity->kind = SceneGravityKind::Uniform;
        gravity.gravity->regionRadius = 10000;
        std::vector<CrashRig> rigs;
        for (unsigned i = 0; i < count; ++i) {
            glm::vec3 p{float(i) * 7, 1.02f, 0};
            if (kind == "cars" && i == 1)
                p = {0, 1.02f, -12};
            rigs.push_back(AddCrashRig(scene, asset, record.id, p, model.body));
            if (kind == "cars" && i == 1)
                scene.Find(rigs.back().owner)->transform.rotation =
                    glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0));
        }
        if (kind != "cars")
            for (unsigned i = 0; i < count; ++i) {
                float x = float(i) * 7;
                if (kind == "side")
                    box("Side pole", {x + 4, 1, 0}, {.18f, 1, .18f});
                else if (kind == "rear")
                    box("Rear wall", {x, 1, 7}, {3, 1, .4f});
                else
                    box("Front wall", {x + (kind == "offset" ? 1.2f : 0), 1, -7},
                        {kind == "offset" ? .7f : 3.f, 1, .4f});
            }
        Require(SaveSceneToFile(scene, (root / "crash-proof.judas").string(), error), error);
        RuntimeWorld world;
        Require(world.Build(scene, &resources, error), error);
        world.UpdateDeformables(1. / 60);
        std::string modelHash;
        Require(SceneFingerprintSha256File(modelPath, modelHash, error), error);
        const auto content =
            SceneFingerprintSha256(EncodeDeformableAsset(asset) + kind + std::to_string(count) + modelHash);
        SaveStorage storage((root / "Saves").string());
        if (load) {
            GameSnapshot snapshot;
            bool recovered = false;
            Require(storage.Read("crash", snapshot, recovered, error), error);
            Require(snapshot.content == content, "saved structural fixture differs from the selected "
                                                 "rig/case; retain it and use its matching configuration");
            Scene restored;
            Require(WorldPersistence::SceneFrom(snapshot.participants, restored, error), error);
            Require(world.Build(restored, &resources, error, nullptr, nullptr, true), error);
            Require(WorldPersistence::Prepare(world, error), error);
            Require(WorldPersistence::Restore(world, snapshot.participants, error), error);
            WorldPersistence::Published(world);
            const auto hash = DurableShapeHash(world, rigs);
            Require(snapshot.metadata == "{\"shape\":\"" + hash + "\"}",
                    "cold restoration changed durable deformation/wheel/rotor state");
            std::cout << "COLD_SHAPE_VERIFIED " << hash << '\n';
        }
        std::cout << std::setprecision(10) << "PREPARATION ms=" << Elapsed(preparation)
                  << " vehicles=" << count << " sections=" << asset.fracture->parts.size()
                  << " bonds=" << asset.fracture->bonds.size()
                  << " body_triangles=" << model.body.indices.size() / 3
                  << " skin_numeric_payload_bytes=" << rigs[0].skin.Bytes() << '\n';
        std::array<MeshHandle, 4> wheelMeshes{};
        std::vector<MeshHandle> bodyMeshes;
        std::vector<uint64_t> meshRevisions;
        std::vector<glm::mat4> palette;
        auto initialGpuResources = Clock::now();
        if (host) {
            auto &r = host->GetRenderer();
            r.SetLighting({.4f, -1, -.3f}, {2, 1.9f, 1.8f}, {.7f, .75f, .8f});
            r.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {.15f, .19f, .24f});
            for (auto &rig : rigs) {
                auto *sim = world.RuntimeDeformable(rig.owner, error);
                if (cpuSkin)
                    rig.skin.Map(*sim, 1, rig.mapped);
                else
                    rig.skin.BuildGpuMesh(*sim, rig.mapped);
                bodyMeshes.push_back(r.CreateMesh(rig.mapped));
                Require(bodyMeshes.back().IsValid(), "GPU structural skin upload failed");
                meshRevisions.push_back(sim->fracture.revision);
            }
            for (unsigned i = 0; i < 4; ++i)
                if (!model.wheels[i].indices.empty())
                    wheelMeshes[i] = r.CreateMesh(model.wheels[i]);
            std::cout << "INITIAL_GPU_RESOURCES_CPU_MS " << Elapsed(initialGpuResources) << '\n';
        }
        std::vector<double> stepTimes, mapTimes, renderTimes, gpuTimes;
        std::deque<uint64_t> gpuFrames;
        unsigned minimumVisible = count, maximumVisible = 0;
        std::map<std::string, std::vector<double>> phaseTimes;
        if (profile)
            PerformanceProfiler::Get().Enable(true);
        InputSystem input;
        unsigned tick = 0;
        bool paused = false, launchRequested = false;
        float orbit = .45f, cameraDistance = 6;
        if (host)
            host->GetWindow().SetEventHook([&](const SDL_Event &event) {
                if (event.type == SDL_MOUSEWHEEL)
                    cameraDistance = std::clamp(cameraDistance - float(event.wheel.y) * .5f, 3.f, 18.f);
            });
        double accumulator = 0;
        auto previousTime = Clock::now();
        auto save = [&]() {
            GameSnapshot snapshot;
            snapshot.project = "local-crash-proof";
            snapshot.scene = "crash-proof.judas";
            snapshot.content = content;
            snapshot.metadata = "{\"shape\":\"" + DurableShapeHash(world, rigs) + "\"}";
            Require(WorldPersistence::Capture(world, snapshot.participants, error), error);
            Require(storage.Write("crash", snapshot, error), error);
            std::cout << "SAVE tick=" << tick << '\n';
        };
        while (!frames || tick < frames) {
            double dt = 1. / 60;
            if (host) {
                host->GetWindow().PollEvents();
                if (host->GetWindow().ShouldClose())
                    break;
                auto *keys = SDL_GetKeyboardState(nullptr);
                if (keys[SDL_SCANCODE_ESCAPE])
                    break;
                dt = std::min(.1, std::chrono::duration<double>(Clock::now() - previousTime).count());
                previousTime = Clock::now();
                static bool lastP = false, lastF = false, lastR = false, lastT = false;
                bool reset = keys[SDL_SCANCODE_R], launch = keys[SDL_SCANCODE_RETURN];
                if (reset && !lastR) {
                    Require(world.Build(scene, &resources, error), error);
                    world.UpdateDeformables(1. / 60);
                    tick = 0;
                    meshRevisions.assign(rigs.size(), ~uint64_t(0));
                    paused = false;
                }
                if (launch && !lastT)
                    launchRequested = true;
                lastR = reset;
                lastT = launch;
                orbit += float(dt) * (int(keys[SDL_SCANCODE_Q]) - int(keys[SDL_SCANCODE_E]));
                bool p = keys[SDL_SCANCODE_P], f = keys[SDL_SCANCODE_F6];
                if (p && !lastP)
                    paused = !paused;
                if (f && !lastF)
                    save();
                lastP = p;
                lastF = f;
            }
            accumulator += dt;
            while (!paused && accumulator >= 1. / 60 && (!frames || tick < frames)) {
                const auto *keys = host ? SDL_GetKeyboardState(nullptr) : nullptr;
                for (auto &rig : rigs) {
                    if (auto *train = world.RuntimePowertrain(rig.owner)) {
                        train->command.sources[0].input = (drive && tick > 60) ||
                                                                  (postDrive && tick >= 300) ||
                                                                  (keys && keys[SDL_SCANCODE_W])
                                                              ? 1
                                                          : keys && keys[SDL_SCANCODE_S] ? -1
                                                                                         : 0;
                        for (auto &brake : train->command.brakes)
                            brake.engagement = keys && keys[SDL_SCANCODE_SPACE] ? 1 : 0;
                    }
                    for (unsigned i = 0; i < rig.wheels.size(); ++i)
                        if (auto *wheel = world.RuntimeWheel(rig.wheels[i]))
                            wheel->command.steeringAngle =
                                i < 2 && postDrive && tick >= 300 ? .3f
                                : i < 2 && keys
                                    ? .4f * (int(keys[SDL_SCANCODE_A]) - int(keys[SDL_SCANCODE_D]))
                                    : 0;
                }
                // Controlled crash launch is a declared initial condition, never a
                // contact-triggered animation/velocity reset. Manual W/S uses wheels.
                if (launchRequested || ((tick == 120 || (repeat && tick > 120 && (tick - 120) % 300 == 0)) &&
                                        !load && !drive && (frames || headless)))
                    for (unsigned i = 0; i < rigs.size(); ++i)
                        for (unsigned p = 0; p < asset.fracture->parts.size(); ++p) {
                            auto body = world.RuntimeBody(world.FracturePartEntity(rigs[i].owner, p));
                            auto velocity = kind == "side" ? glm::vec3(speed, 0, 0)
                                            : kind == "rear"
                                                ? glm::vec3(0, 0, speed)
                                                : glm::vec3(0, 0, kind == "cars" && i == 1 ? speed : -speed);
                            world.Physics().SetLinearVelocity(body, velocity);
                        }
                launchRequested = false;
                auto t = Clock::now();
                {
                    ProfileFrame frame("Crash proof fixed step");
                    StepAuthoritativeWorld(world, input, 1.f / 60);
                }
                stepTimes.push_back(Elapsed(t));
                if (profile) {
                    auto history = PerformanceProfiler::Get().History();
                    if (!history.empty()) {
                        std::map<std::string, double> totals;
                        for (const auto &scope : history.back().scopes)
                            totals[scope.name.c_str()] += double(scope.end - scope.start) * 1e-6;
                        for (auto &[name, ms] : totals)
                            phaseTimes[name].push_back(ms);
                    }
                }
                ++tick;
                accumulator -= 1. / 60;
                if (tick == saveAt)
                    save();
                if (tick % 60 == 0) {
                    for (auto &rig : rigs) {
                        for (auto wid : rig.wheels) {
                            auto *wheel = world.RuntimeWheel(wid);
                            std::cout << "WHEEL id=" << wid << " available=" << wheel->state.available
                                      << " center=" << wheel->state.center.x << "," << wheel->state.center.y
                                      << "," << wheel->state.center.z << " contact=" << wheel->state.contact
                                      << " diagnostic=" << wheel->state.diagnostic << '\n';
                        }
                        auto *s = world.RuntimeDeformable(rig.owner, error);
                        Require(s && s->error.empty(), s ? s->error : error);
                        double work = std::accumulate(s->fracture.plasticWork.begin(),
                                                      s->fracture.plasticWork.end(), 0.);
                        double travel = 0, angle = 0;
                        unsigned failed = 0;
                        for (auto v : s->fracture.plasticTranslation)
                            travel = std::max(travel, double(glm::length(v)));
                        for (auto v : s->fracture.plasticRotation)
                            angle = std::max(angle, double(glm::length(v)));
                        for (auto b : s->fracture.broken)
                            failed += b;
                        auto reference = world.Physics().GetTransform(
                            world.RuntimeBody(world.FracturePartEntity(rig.owner, rig.referencePart)));
                        double mountAngle = 0;
                        unsigned connected = 0;
                        for (auto wid : rig.wheels) {
                            auto *wheel = world.RuntimeWheel(wid);
                            auto carrier = world.RuntimeBody(world.WheelChassisEntity(wheel->settings));
                            if (carrier.IsValid()) {
                                auto relative = glm::inverse(reference.rotation) *
                                                world.Physics().GetTransform(carrier).rotation;
                                mountAngle = std::max(
                                    mountAngle,
                                    2 * std::acos(double(std::clamp(std::abs(relative.w), 0.f, 1.f))));
                            }
                        }
                        if (auto *train = world.RuntimePowertrain(rig.owner))
                            for (auto &link : train->state.links)
                                connected += link.connected;
                        std::set<unsigned> components;
                        for (size_t part = 0; part < s->fracture.component.size(); ++part)
                            if (!s->fracture.removed[part])
                                components.insert(s->fracture.component[part]);
                        std::cout << "COMPONENTS " << components.size() << '\n';
                        std::cout << "MOUNTS max_angle_rad=" << mountAngle
                                  << " connected_drive_links=" << connected << '\n';
                        std::cout << "STATE x=" << reference.position.x << " y=" << reference.position.y
                                  << " z=" << reference.position.z << " tick=" << tick
                                  << " owner=" << rig.owner << " plastic_m=" << travel
                                  << " plastic_rad=" << angle << " plastic_work_j=" << work
                                  << " broken=" << failed << " error=" << s->error << '\n';
                    }
                }
            }
            if (paused)
                accumulator = 0;
            if (host) {
                auto &r = host->GetRenderer();
                auto &window = host->GetWindow();
                ProfileFrame renderProfile("Crash proof rendering");
                const auto renderFrameId = PerformanceProfiler::Get().FrameId();
                r.PollProfileGPU();
                const auto gpuPass = r.BeginProfilePass("Crash proof complete render");
                auto target = world.Physics()
                                  .GetTransform(world.RuntimeBody(
                                      world.FracturePartEntity(rigs[0].owner, rigs[0].referencePart)))
                                  .position;
                if (count > 1) {
                    target = glm::vec3(0);
                    for (const auto &rig : rigs)
                        target += world.Physics()
                                      .GetTransform(world.RuntimeBody(
                                          world.FracturePartEntity(rig.owner, rig.referencePart)))
                                      .position /
                                  float(count);
                }
                const float distance = cameraDistance * float(count);
                auto eye = target + glm::vec3(distance * std::sin(.78f + orbit), 3 + float(count - 1) * 1.5f,
                                              -distance * std::cos(.78f + orbit));
                auto t = Clock::now();
                r.ResetStats();
                r.BeginFrame(window.Width(), window.Height());
                r.SetCamera(glm::lookAt(eye, target, glm::vec3(0, 1, 0)),
                            glm::perspective(glm::radians(55.f), float(window.Width()) / window.Height(), .1f,
                                             300.f));
                for (auto o : scene.Objects())
                    if (o.body)
                        r.DrawBox(o.transform.position, o.transform.rotation, o.body->halfExtents,
                                  o.name == "Ground" ? glm::vec3(.25f, .28f, .29f)
                                                     : glm::vec3(.65f, .68f, .68f),
                                  xray && o.name != "Ground" ? .22f : 1.f);
                unsigned visibleModels = 0;
                for (unsigned i = 0; i < rigs.size(); ++i) {
                    auto &rig = rigs[i];
                    auto mapping = Clock::now();
                    auto *sim = world.RuntimeDeformable(rig.owner, error);
                    if (meshRevisions[i] != sim->fracture.revision) {
                        if (cpuSkin)
                            rig.skin.Map(*sim, 1, rig.mapped);
                        else
                            rig.skin.BuildGpuMesh(*sim, rig.mapped);
                        r.DestroyMesh(bodyMeshes[i]);
                        bodyMeshes[i] = r.CreateMesh(rig.mapped);
                        meshRevisions[i] = sim->fracture.revision;
                    }
                    if (cpuSkin) {
                        rig.skin.Map(*sim, 1, rig.mapped);
                        r.UpdateMeshVertices(bodyMeshes[i], rig.mapped.vertices);
                        palette.clear();
                    } else
                        rig.skin.Palette(*sim, 1, palette);
                    mapTimes.push_back(Elapsed(mapping));
                    const auto beforeVisible = r.Stats().renderablesVisible;
                    r.DrawMesh(bodyMeshes[i], {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 1, 1}, 1, &palette);
                    if (r.Stats().renderablesVisible > beforeVisible)
                        ++visibleModels;
                    for (unsigned w = 0; w < 4; ++w)
                        if (auto *wheel = world.RuntimeWheel(rig.wheels[w]);
                            wheel && wheel->state.available && wheelMeshes[w].IsValid())
                            r.DrawMesh(wheelMeshes[w], wheel->state.center, wheel->state.rotation, {1, 1, 1},
                                       {}, {1, 1, 1});
                }
                minimumVisible = std::min(minimumVisible, visibleModels);
                maximumVisible = std::max(maximumVisible, visibleModels);
                r.EndFrame();
                r.BeginUIFrame(window.Width(), window.Height());
                r.DrawUIRect({15, 15}, {1200, 110}, {.02f, .03f, .04f, .9f});
                r.DrawUIText("DEFORMABLES V2 | LOCAL CRASH PROOF | " + kind, {30, 25}, .8f, {1, 1, 1, 1});
                r.DrawUIText("W/S drive | A/D steer | Space brake | Enter test impact | P pause | F6 save",
                             {30, 60}, .65f, {.5f, 1, 1, 1});
                r.DrawUIText(
                    "Q/E orbit | Wheel zoom | R restart | Esc quit | Physical damage, no scripted animation",
                    {30, 90}, .6f, {1, 1, 1, 1});
                r.EndUIFrame();
                r.EndProfilePass(gpuPass);
                renderTimes.push_back(Elapsed(t));
                if (frames && tick >= frames)
                    Snapshot(r, root / "final.ppm", window.Width(), window.Height());
                window.SwapBuffers();
                renderProfile.End();
                if (profile) {
                    gpuFrames.push_back(renderFrameId);
                    while (gpuFrames.size() > 16)
                        gpuFrames.pop_front();
                    for (auto it = gpuFrames.begin(); it != gpuFrames.end();) {
                        const auto snapshot = PerformanceProfiler::Get().Snapshot(*it);
                        bool complete = false;
                        for (const auto &sample : snapshot.gpu) {
                            if (!sample.pending &&
                                sample.pass == std::string_view("Crash proof complete render")) {
                                gpuTimes.push_back(sample.milliseconds);
                                complete = true;
                            }
                        }
                        if (complete)
                            it = gpuFrames.erase(it);
                        else
                            ++it;
                    }
                }
            }
        }
        for (auto &[name, values] : phaseTimes)
            Summary(("PHASE " + name).c_str(), values);
        Summary("SIMULATION", stepTimes);
        Summary("SKIN_MAPPING", mapTimes);
        Summary("RENDER_CPU", renderTimes);
        Summary("RENDER_GPU", gpuTimes);
        if (host)
            std::cout << "FRUSTUM_VISIBLE_VEHICLES min=" << minimumVisible << " max=" << maximumVisible
                      << '\n';
#if defined(__linux__)
        rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
        std::cout << "PEAK_RSS_KIB " << usage.ru_maxrss << '\n';
#endif

        if (host) {
            host->GetWindow().SetEventHook({});
            for (auto mesh : bodyMeshes)
                host->GetRenderer().DestroyMesh(mesh);
            for (auto mesh : wheelMeshes)
                if (mesh.IsValid())
                    host->GetRenderer().DestroyMesh(mesh);
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "CRASH_PROOF_FAILED " << e.what() << '\n';
        return 1;
    }
}
