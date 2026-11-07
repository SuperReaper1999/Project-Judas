#include "GameWaterLabScene.h"
#include "CollisionAsset.h"
#include "SaveStorage.h"
#include "SceneFingerprint.h"
#include "authoring/TerrainAuthoring.h"
#include "fluid/GameWaterCoupling.h"
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace WaterLab {
namespace {
// Authored physical walls from the preserved BasinFixture.h, not its exact
// cut-cell implementation. The preview deliberately samples them at 0.2 m.
const Wall originalWalls[] = {{{0, -.1f, 0}, {1.1f, .1f, 1.1f}, {}},
                              {{-1.1f, 1.5f, 0}, {.1f, 1.5f, 1.1f}, {}},
                              {{1.1f, 1.5f, 0}, {.1f, 1.5f, 1.1f}, {}},
                              {{0, 1.5f, -1.1f}, {1, 1.5f, .1f}, {}},
                              {{0, 1.5f, 1.1f}, {1, 1.5f, .1f}, {}}};
} // namespace
Scene::Scene(unsigned selected, bool rotate, bool empty)
    : mode(selected), rotated(rotate), noBodies(empty) {
    settings.cells = {30, 12, 20};
    settings.cellSize = .4f;
    if (mode == 4) {
        settings.cells = {15, 20, 15};
        settings.cellSize = .2f;
    }
    if (rotated) {
        settings.rotation = mode == 4 ? glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3)))
                                      : glm::angleAxis(.7f, glm::normalize(glm::vec3(1, 0, 1)));
        settings.gravity = settings.rotation * settings.gravity;
    }
    std::string error;
    if (!water.Initialize(settings, error) || !physics.Init())
        throw std::runtime_error(error);
    auto wall = [&](glm::vec3 p, glm::vec3 half, glm::quat q = glm::quat(1, 0, 0, 0)) {
        walls.push_back({water.ToWorld(p), half, settings.rotation * q});
        physics.CreateStaticBox(walls.back().center, walls.back().rotation, half, .7f, 0);
    };
    wall({6, -.2f, 4}, {9, .2f, 7});
    if (mode != 4) {
        for (float x : {-.2f, 12.2f})
            wall({x, 1.5f, 4}, {.2f, 1.5f, 4.4f});
        for (float z : {-.2f, 8.2f})
            wall({6, 1.5f, z}, {6, 1.5f, .2f});
    }
    if (mode == 2) {
        for (float x : {6.6f, 9.4f})
            wall({x, 2.1f, 4}, {.2f, .7f, 1.4f});
        for (float z : {2.6f, 5.4f})
            wall({8, 2.1f, z}, {1.2f, .7f, .2f});
        wall({7.25f, 2.8f, 4}, {.65f, .2f, 1.4f});
        wall({8.75f, 2.8f, 4}, {.65f, .2f, 1.4f});
        wall({8, 2.8f, 3.25f}, {.1f, .2f, .65f});
        wall({8, 2.8f, 4.75f}, {.1f, .2f, .65f});
        gateShape = {water.ToWorld({8, 2.8f, 4}), {.1f, .2f, .1f}, settings.rotation};
    } else if (mode == 3) {
        terrain = std::make_shared<TerrainSource>(CreateTerrain(25, 17, 12, 8));
        for (unsigned z = 0; z < terrain->nz; ++z)
            for (unsigned x = 0; x < terrain->nx; ++x) {
                const float px = float(x) * 12 / (terrain->nx - 1) - 5,
                            pz = float(z) * 8 / (terrain->nz - 1) - 4;
                terrain->heights[z * terrain->nx + x] =
                    .12f + 2.4f * std::exp(-px * px / 3.f - pz * pz / 5.f);
            }
        terrainMesh = BuildTerrainMesh(*terrain);
        auto collision = std::make_shared<CollisionAsset>();
        if (!CookCollision(terrainMesh, {}, *collision, error))
            throw std::runtime_error(error);
        physics.CreateShape(Shape::Cooked(collision, "game-water-terrain"),
                            {water.ToWorld({6, 0, 4}), settings.rotation}, false, 0, .7f, 0);
    } else if (mode == 4) {
        for (const auto &box : originalWalls)
            wall(glm::vec3(glm::dvec3(box.center) + glm::dvec3(1.5, .5, 1.5)), box.half);
    }
    water.BakeStatic([&](glm::vec3 p) {
        if (terrain) {
            const auto q = water.ToLocal(p) - glm::vec3(6, 0, 4);
            glm::vec3 surface(0);
            if (TerrainPoint(*terrain, q.x, q.z, surface) && q.y <= surface.y)
                return true;
        }
        for (const auto &b : walls)
            if (glm::all(glm::lessThanEqual(glm::abs(glm::conjugate(b.rotation) * (p - b.center)),
                                            b.half + glm::vec3(settings.cellSize * 1e-5f))))
                return true;
        return false;
    });
    initial = water.Fill([&](glm::vec3 p) {
        p = water.ToLocal(p);
        if (mode == 1)
            return p.y < (p.x < 6 ? 2.f : .4f);
        if (mode == 2 && p.x > 6.8f && p.x < 9.2f && p.z > 2.8f && p.z < 5.2f)
            return p.y < 1.2f;
        if (mode == 4) {
            return p.x >= .5f && p.x < 2.5f && p.z >= .5f && p.z < 2.5f && p.y >= .5f &&
                   p.y < 2.9f &&
                   !glm::all(
                       glm::lessThan(glm::abs(p - glm::vec3(1.5f, 1.2f, 1.5f)), glm::vec3(.39f)));
        }
        return p.y < 2.f;
    });
    if (mode == 1)
        gateShape = {water.ToWorld({6, 2, 4}), {.2f, 2, 4}, settings.rotation};
    SetGate(mode == 1 || mode == 2);
    if (!noBodies) {
        Spawn(mode == 4 ? glm::vec3(1.5f, 1.2f, 1.5f) : glm::vec3(3, 2.3f, 3),
              glm::vec3(mode == 4 ? .3f : .4f), 500, {.25f, .9f, .45f});
        Spawn(mode == 4   ? glm::vec3(2.1f, 3.3f, 1.5f)
              : mode == 2 ? glm::vec3(4, 3, 5)
                          : glm::vec3(9, 3, 4),
              glm::vec3(.3f), 2000, {.45f, .48f, .55f});
    }
}
void Scene::SetGate(bool closed) {
    if (mode != 1 && mode != 2)
        return;
    if (gate == closed)
        return;
    if (closed)
        gateHandle_ =
            physics.CreateStaticBox(gateShape.center, gateShape.rotation, gateShape.half, .7f, 0);
    else {
        physics.DestroyBody(gateHandle_);
        gateHandle_ = {};
    }
    gate = closed;
}
void Scene::Spawn(glm::vec3 p, glm::vec3 half, float density, glm::vec3 color) {
    auto handle = physics.CreateDynamicBox(water.ToWorld(p), half,
                                           8 * half.x * half.y * half.z * density, .6f, .05f);
    physics.ResetBody(handle, water.ToWorld(p), settings.rotation);
    bodies.push_back({handle, half, color, density});
}
void Scene::Advance() {
    const auto start = std::chrono::steady_clock::now();
    constexpr float dt = 1.f / 60;
    if ((mode == 1 && step == 60) || (mode == 2 && step == 600))
        SetGate(false);
    std::vector<JudasWater::SolidBox> solids;
    for (auto b : bodies)
        solids.push_back(JudasWater::ReadBox(physics, b.handle, b.half));
    if (gate)
        solids.push_back({100000, gateShape.center, gateShape.half, {}, {}, gateShape.rotation});
    water.SetSolids(solids);
    std::string error;
    if (!water.Step(dt, error))
        throw std::runtime_error(error);
    for (auto b : bodies) {
        physics.ApplyLinearAcceleration(b.handle, settings.gravity, dt);
        JudasWater::ApplyBoxLoad(water, physics, b.handle, b.half, dt);
    }
    physics.Step(dt);
    ++step;
    lastStepMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
bool Scene::Save(const std::string &directory, std::string &error) try {
    SaveArchive a;
    std::uint32_t version = 1;
    a(version, mode, rotated, noBodies, step, gate, initial);
    water.Persist(a);
    std::uint32_t count = std::uint32_t(bodies.size());
    a(count);
    for (auto b : bodies) {
        auto pose = physics.GetTransform(b.handle);
        auto velocity = physics.GetLinearVelocity(b.handle),
             omega = physics.GetAngularVelocity(b.handle);
        a(b.half, b.color, b.density, pose.position, pose.rotation, velocity, omega);
        physics.PersistBodyMass(a, b.handle);
        physics.PersistSleep(a, b.handle);
    }
    GameSnapshot snapshot;
    snapshot.project = "judas-game-water-lab";
    snapshot.scene = "pool";
    snapshot.content = SceneFingerprintSha256("game-water-lab-v1");
    snapshot.participants["lab"] = {1, std::move(a.bytes)};
    std::filesystem::create_directories(directory);
    SaveStorage storage(directory);
    return storage.Write("pool", snapshot, error);
} catch (const std::exception &e) {
    error = e.what();
    return false;
}
std::unique_ptr<Scene> Scene::Load(const std::string &directory, std::string &error) try {
    SaveStorage storage(directory);
    GameSnapshot snapshot;
    bool recovered = false;
    if (!storage.Read("pool", snapshot, recovered, error))
        return {};
    if (snapshot.project != "judas-game-water-lab" || snapshot.participants.count("lab") != 1)
        throw std::runtime_error("Not a game-water lab save.");
    SaveArchive a(snapshot.participants.at("lab").data);
    std::uint32_t version = 0, mode = 0, step = 0;
    bool rotated = false, noBodies = false, gate = false;
    double initial = 0;
    a(version, mode, rotated, noBodies, step, gate, initial);
    a.Require(version == 1 && mode <= 4, "Unsupported water lab state.");
    auto scene = std::make_unique<Scene>(mode, rotated, true);
    scene->noBodies = noBodies;
    scene->step = step;
    scene->initial = initial;
    scene->SetGate(gate);
    scene->water.Persist(a);
    std::uint32_t count = 0;
    a(count);
    a.Require(count <= 256, "Saved water lab body budget exceeded.");
    for (unsigned i = 0; i < count; ++i) {
        glm::vec3 half(0), color(0), position(0), velocity(0), omega(0);
        glm::quat rotation(1, 0, 0, 0);
        float density = 0;
        a(half, color, density, position, rotation, velocity, omega);
        a.Require(glm::all(glm::greaterThan(half, glm::vec3(0))) && density > 0,
                  "Invalid saved water body.");
        scene->Spawn(scene->water.ToLocal(position), half, density, color);
        auto h = scene->bodies.back().handle;
        scene->physics.ResetBody(h, position, rotation);
        scene->physics.SetLinearVelocity(h, velocity);
        scene->physics.SetAngularVelocity(h, omega);
        scene->physics.PersistBodyMass(a, h);
        scene->physics.PersistSleep(a, h);
    }
    a.Finish();
    error.clear();
    return scene;
} catch (const std::exception &e) {
    error = e.what();
    return {};
}
} // namespace WaterLab
