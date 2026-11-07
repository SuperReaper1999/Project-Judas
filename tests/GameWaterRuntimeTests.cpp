#include "../third_party/nlohmann/json.hpp"
#include "GameSession.h"
#include "Project.h"
#include "ProjectFeaturePolicy.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SaveStorage.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "WorldPersistence.h"
#include "fluid/GameWaterRuntime.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << message << '\n';
    }
}
Scene Pool() {
    Scene scene;
    scene.Settings().name = "Ordinary project grid-water proof";
    auto &gravity = scene.CreateObject("Gravity");
    gravity.gravity = SceneGravityComponent{};
    gravity.gravity->kind = SceneGravityKind::Uniform;
    gravity.gravity->regionRadius = 10000;
    auto box = [&](const char *name, glm::vec3 at, glm::vec3 half) {
        auto &object = scene.CreateObject(name);
        object.transform.position = at;
        object.body = SceneBodyComponent{};
        object.body->halfExtents = half;
        return object.id;
    };
    box("Floor", {0, -.2f, 0}, {4, .2f, 2.8f});
    box("West", {-4.2f, 1.5f, 0}, {.2f, 1.5f, 3.2f});
    box("East", {4.2f, 1.5f, 0}, {.2f, 1.5f, 3.2f});
    box("South", {0, 1.5f, -3}, {4, 1.5f, .2f});
    box("North", {0, 1.5f, 3}, {4, 1.5f, .2f});
    auto light = box("Float", {-1, 2.3f, 0}, glm::vec3(.35f));
    scene.Find(light)->body->motion = SceneBodyMotion::Dynamic;
    scene.Find(light)->body->mass = 500 * 8 * .35f * .35f * .35f;
    auto dense = box("Sink", {1, 3, 0}, glm::vec3(.3f));
    scene.Find(dense)->body->motion = SceneBodyMotion::Dynamic;
    scene.Find(dense)->body->shape = SceneShape::Sphere;
    scene.Find(dense)->body->radius = .3f;
    scene.Find(dense)->body->mass = 2000 * 4 * 3.14159265f * .3f * .3f * .3f / 3;
    auto &swimmer = scene.CreateObject("Swimmer");
    swimmer.characterMotor = CharacterMotorSettings{};
    swimmer.characterMotor->gravityScale = 0;
    swimmer.transform.position = {0, 1, 0};
    auto &water = scene.CreateObject("Water");
    water.fluidVolume = SceneFluidVolumeComponent{};
    auto &f = *water.fluidVolume;
    f.backend = SceneFluidBackend::Grid;
    f.spacing = .4f;
    f.countX = 20;
    f.countY = 10;
    f.countZ = 14;
    f.fillHeight = 2;
    return scene;
}
EntityId Named(const Scene &scene, const std::string &name) {
    for (const auto &o : scene.Objects())
        if (o.name == name)
            return o.id;
    return 0;
}
double Quantity(const JudasWater::Grid &grid) {
    double v = 0;
    for (const auto &c : grid.Cells())
        v += c.water + c.displaced;
    return v;
}
} // namespace
int main(int argc, char **argv) {
    auto scene = Pool();
    // Separate write/read invocations exercise real process-cold persistence.
    const std::string operation = argc > 1 ? argv[1] : "";
    const std::string directory = argc > 2 ? argv[2] : "";
    const auto waterId = Named(scene, "Water");
    std::string error, text;
    Check(SaveSceneToString(scene, text),
          "grid component writes through normal scene serialization");
    Scene parsed;
    Check(LoadSceneFromString(text, parsed, error) && ScenesEqual(scene, parsed),
          "grid fields roundtrip: " + error);
    RuntimeWorld off;
    ProjectSettings disabled;
    disabled.legacyGameplay = false;
    off.ConfigureProject(disabled);
    Check(off.Build(parsed, nullptr, error) && !off.GridWater() && !off.HasFluid(),
          "default policy never allocates an advanced backend: " + error);
    auto invalid = parsed;
    auto &invalidGrid = *invalid.Find(waterId)->fluidVolume;
    invalidGrid.countX = invalidGrid.countY = invalidGrid.countZ = std::numeric_limits<int>::max();
    std::string fingerprint;
    Check(!ComputeSceneFingerprint(invalid, fingerprint, error),
          "programmatic extreme dimensions reject before signed multiplication");
    auto longGrid = parsed;
    auto &longComponent = *longGrid.Find(waterId)->fluidVolume;
    longComponent.countX = 1024;
    longComponent.countY = longComponent.countZ = 2;
    longComponent.fillHeight = .4f;
    std::string longText;
    Scene longRead;
    Check(SaveSceneToString(longGrid, longText) && LoadSceneFromString(longText, longRead, error),
          "scene dimensions match the native total-cell contract instead of inheriting the "
          "particle lattice limit");
    ProjectSettings enabled = disabled;
    enabled.advancedFluidDynamics = true;
    RuntimeWorld source;
    source.ConfigureProject(enabled);
    if (AdvancedFluidDynamicsRestriction()) {
        Check(!source.Build(scene, nullptr, error),
              "Alpha lock rejects execution of the replacement too");
        std::cout << checks << " checks, " << failures << " failures (Alpha lock path)\n";
        return failures ? 1 : 0;
    }
    if (!source.Build(scene, nullptr, error)) {
        Check(false, "ordinary RuntimeWorld builds grid water: " + error);
        return 1;
    }
    Check(source.GridWater() && source.GridWater()->Owners() == std::vector<std::uint64_t>{waterId},
          "runtime has exactly the scene-owned grid");
    bool legacyAllocated = true;
    try {
        source.Fluid();
    } catch (const std::logic_error &) {
        legacyAllocated = false;
    }
    Check(!legacyAllocated && !source.HasFluid() && source.FluidVolumes().empty(),
          "grid-only projects do not allocate the legacy particle solver");
    if (operation == "--read") {
        SaveStorage storage(directory);
        GameSnapshot snapshot;
        bool recovered = false;
        const bool readOkay = storage.Read("runtime-water", snapshot, recovered, error);
        Check(readOkay, "read cold-process fixture: " + error);
        if (!readOkay)
            return 1;
        Scene saved;
        Check(WorldPersistence::SceneFrom(snapshot.participants, saved, error), "read saved scene");
        RuntimeWorld cold;
        cold.ConfigureProject(enabled);
        Check(cold.Build(saved, nullptr, error, nullptr, nullptr, true) &&
                  WorldPersistence::Prepare(cold, error) &&
                  WorldPersistence::Restore(cold, snapshot.participants, error),
              "cold-process restoration: " + error);
        const double expected =
            nlohmann::json::parse(snapshot.metadata).at("quantity").get<double>();
        Check(cold.GridWater() &&
                  std::abs(cold.GridWater()->Find(waterId)->Quantity() - expected) < 1e-10,
              "cold process retains changed quantity");
        GameSession game;
        Check(game.Begin(cold, error), "cold process resumes");
        InputSystem input;
        for (unsigned i = 0; i < 120; ++i) {
            input.BeginFixedStep();
            StepAuthoritativeWorld(game, input, 1.f / 60);
        }
        Check(cold.GridWater()->Error().empty() &&
                  std::abs(cold.GridWater()->Find(waterId)->Quantity() - expected) < 1e-8,
              "cold process advances the real solver without reseeding");
        std::cout << checks << " checks, " << failures << " failures\n";
        return failures ? 1 : 0;
    }
    GameSession session;
    Check(session.Begin(source, error), "ordinary authoritative session starts: " + error);
    InputSystem input;
    auto tick = [&](RuntimeWorld &world, GameSession &game, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            input.BeginFixedStep();
            StepAuthoritativeWorld(game, input, 1.f / 60);
        }
        Check(world.GridWater()->Error().empty(),
              "ordinary simulation has no grid publication error: " + world.GridWater()->Error());
    };
    tick(source, session, 600);
    Check(source.GridWater()->Stats(waterId).geometryBakes == 1,
          "moving CharacterMotor and rigid props do not trigger static rebakes");
    if (operation == "--write") {
        const auto accepted = source.GridWater()->ChangeWater(waterId, {0, 2.5f, 0}, 1, .2);
        Check(std::abs(accepted - .2) < 1e-9, "cold fixture changes real inventory");
        GameSnapshot snapshot;
        snapshot.project = "grid-runtime";
        snapshot.scene = "pool";
        snapshot.content = SceneFingerprintSha256("grid-runtime-cold-fixture");
        snapshot.metadata =
            nlohmann::json{{"quantity", source.GridWater()->Find(waterId)->Quantity()}}.dump();
        Check(WorldPersistence::Capture(source, snapshot.participants, error),
              "capture cold fixture: " + error);
        std::filesystem::create_directories(directory);
        SaveStorage storage(directory);
        Check(storage.Write("runtime-water", snapshot, error),
              "write cold-process fixture: " + error);
        std::cout << checks << " checks, " << failures << " failures\n";
        return failures ? 1 : 0;
    }
    Check(std::abs(Quantity(*source.GridWater()->Find(waterId)) - 89.6) < 1e-4,
          "normal runtime conserves the authored water quantity");
    Check(source.SampleWater({0, 1, 0}, {0, 1, 0}, .3f, .2f).immersion > .9f &&
              source.SampleWater({0, 5, 0}, {0, 1, 0}, .3f, .2f).immersion == 0,
          "existing swimming query samples actual grid occupancy");
    Check(std::abs(
              source.Physics().GetTransform(source.RuntimeBody(Named(scene, "Float"))).position.y -
              2) < .3f,
          "real MK2 box floats through normal simulation");
    Check(std::abs(
              source.Physics().GetTransform(source.RuntimeBody(Named(scene, "Sink"))).position.y -
              .3f) < .05f,
          "real MK2 sphere sinks to the physical bottom");
    std::map<std::string, SaveChunk> chunks;
    Check(WorldPersistence::Capture(source, chunks, error) && chunks.count("grid-water"),
          "normal save captures its owned grid: " + error);
    Scene saved;
    Check(WorldPersistence::SceneFrom(chunks, saved, error),
          "normal save reconstructs grid authoring: " + error);
    RuntimeWorld restored;
    restored.ConfigureProject(enabled);
    Check(restored.Build(saved, nullptr, error, nullptr, nullptr, true),
          "private cold construction supports grid water: " + error);
    Check(WorldPersistence::Prepare(restored, error) &&
              WorldPersistence::Restore(restored, chunks, error),
          "normal participant restoration succeeds: " + error);
    Check(std::abs(Quantity(*restored.GridWater()->Find(waterId)) -
                   Quantity(*source.GridWater()->Find(waterId))) < 1e-8,
          "restored quantity is preserved before the first resumed step");
    GameSession resumed;
    Check(resumed.Begin(restored, error), "restored session starts");
    tick(restored, resumed, 60);
    Check(restored.RequestOrigin({512, 0, 0}, error) && restored.CommitOrigin(),
          "active grid follows normal origin shifts: " + error);
    Check(restored.SampleWater({-512, 1, 0}, {0, 1, 0}, .3f, .2f).immersion > .9f,
          "water query remains anchored after rebasing");
    Check(std::abs(Quantity(*restored.GridWater()->Find(waterId)) - 89.6) < 1e-4,
          "origin shift does not alter quantity");
    // Real scene streaming can replace colliders around a resident water owner.
    SceneObject wall;
    wall.id = 100;
    wall.name = "Streamed wall";
    wall.body = SceneBodyComponent{};
    wall.body->halfExtents = {.2f, 1, .4f};
    wall.transform.position = {-1, 1, 1};
    Check(source.StageRegionObject(wall, wall, error),
          "stage ordinary streamed collider: " + error);
    source.PublishRegion({wall.id});
    tick(source, session, 1);
    Check(source.GridWater()->Stats(waterId).geometryBakes == 2,
          "published streamed collider invalidates cached capacity");
    source.HideRegion({wall.id});
    source.RemoveRegionObject(wall.id);
    tick(source, session, 1);
    Check(source.GridWater()->Stats(waterId).geometryBakes == 3 &&
              std::abs(Quantity(*source.GridWater()->Find(waterId)) - 89.6) < 1e-4,
          "unloaded collider invalidates geometry without losing displaced water");
    auto moved = scene.Find(waterId)->transform;
    moved.position.x += 1;
    Check(!source.SetRuntimeTransform(waterId, moved),
          "water owner teleport rejects rather than detaching the grid");
    Check(!source.DestroyEntity(waterId, &error),
          "water owner destruction rejects rather than deleting quantity");
    auto duplicate = scene;
    auto copy = *scene.Find(waterId);
    auto &second = duplicate.CreateObject("Second independent water");
    copy.id = second.id;
    second = copy;
    RuntimeWorld overlapping;
    overlapping.ConfigureProject(enabled);
    Check(!overlapping.Build(duplicate, nullptr, error) &&
              error.find("overlap") != std::string::npos,
          "overlapping independent domains reject before double coupling");
    auto missing = chunks;
    missing.erase("grid-water");
    RuntimeWorld reject;
    reject.ConfigureProject(enabled);
    reject.Build(saved, nullptr, error, nullptr, nullptr, true);
    Check(!WorldPersistence::Restore(reject, missing, error),
          "missing grid participant rejects instead of reseeding initial water");
    for (const bool radial : {false, true}) {
        Scene environment = Pool();
        const auto rotation = glm::angleAxis(.6f, glm::normalize(glm::vec3(1, 0, 1)));
        if (radial) {
            environment.Objects().front().gravity->kind = SceneGravityKind::Radial;
            environment.Objects().front().transform.position = {0, -80, 0};
        } else
            for (auto &o : environment.Objects()) {
                o.transform.position = rotation * o.transform.position;
                o.transform.rotation = rotation * o.transform.rotation;
            }
        RuntimeWorld planet;
        planet.ConfigureProject(enabled);
        Check(planet.Build(environment, nullptr, error), "build rotated/radial chart: " + error);
        GameSession live;
        Check(live.Begin(planet, error), "start gravity comparison");
        std::vector<double> times;
        std::ofstream csv;
        if (!directory.empty())
            csv.open(std::filesystem::path(directory) / (radial ? "radial.csv" : "rotated.csv"));
        if (csv)
            csv << "step,totalMs,solverMs,preparationMs,couplingMs,quantity,stored,"
                   "speed,pressureResidual,geometryBakes,bytes\n";
        for (unsigned i = 0; i < 1800; ++i) {
            if (i == 120)
                planet.GridWater()->AddImpulse(
                    waterId, radial ? glm::vec3(0, 1, 0) : rotation * glm::vec3(0, 1, 0),
                    radial ? glm::vec3(30, 10, 0) : rotation * glm::vec3(30, 10, 0), 1);
            input.BeginFixedStep();
            StepAuthoritativeWorld(live, input, 1.f / 60);
            const auto stats = planet.GridWater()->Stats(waterId);
            const auto &v = planet.GridWater()->Find(waterId)->Stats();
            if (i >= 60)
                times.push_back(stats.totalMs);
            if (csv)
                csv << i << ',' << stats.totalMs << ',' << v.milliseconds << ','
                    << stats.preparationMs << ',' << stats.couplingMs << ',' << v.water << ','
                    << v.storedDisplacement << ',' << v.maximumSpeed << ',' << v.pressureResidual
                    << ',' << stats.geometryBakes << ',' << stats.bytes << '\n';
            if (!planet.GridWater()->Error().empty())
                break;
        }
        Check(planet.GridWater()->Error().empty() && times.size() == 1740,
              "sustained disturbed gravity chart advances every requested "
              "interval: " +
                  planet.GridWater()->Error());
        Check(std::abs(Quantity(*planet.GridWater()->Find(waterId)) - 89.6) < 1e-4,
              "sustained gravity comparison conserves quantity");
        std::sort(times.begin(), times.end());
        if (!times.empty())
            std::cout << (radial ? "radial" : "rotated") << " runtime median/p95/worst ms "
                      << times[times.size() / 2] << ' '
                      << times[std::size_t(std::ceil(times.size() * .95)) - 1] << ' '
                      << times.back() << '\n';
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
