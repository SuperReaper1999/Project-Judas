#include "../tools/crash_proof/CrashRig.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "Simulation.h"
#include "WorldPersistence.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>
#include <stdexcept>

unsigned RunStructuralCrashChecks() {
    unsigned checks = 0;
    auto check = [&](bool value, const std::string &label) {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    struct Scratch {
        std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("judas-structural-crash-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ~Scratch() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } scratch;
    std::filesystem::create_directories(scratch.path / "Assets");
    auto asset = MakeCrashStructure();
    {
        std::ofstream out(scratch.path / "Assets/structure.judasdeform", std::ios::binary);
        out << EncodeDeformableAsset(asset);
    }
    AssetDatabase database;
    database.Scan(scratch.path.string(), (scratch.path / "Assets").string());
    AssetRecord record;
    std::string error;
    check(database.Track((scratch.path / "Assets/structure.judasdeform").string(), record, error),
          "original procedural crash asset tracks");
    ResourceManager resources(nullptr, &database);
    resources.RequestDeformable(record.id);
    resources.WaitForAll();
    // Engine-owned procedural exterior; downloaded meshes are never test inputs.
    MeshData exterior = asset.render;
    InputSystem input;
    auto work = [](const DeformableInstance &s) {
        return std::accumulate(s.fracture.plasticWork.begin(), s.fracture.plasticWork.end(), 0.);
    };
    for (const std::string kind : {"frontal", "offset", "side", "rear", "cars"}) {
        Scene scene;
        auto &ground = scene.CreateObject("Ground");
        ground.body = SceneBodyComponent{};
        ground.body->halfExtents = {100, .5f, 100};
        ground.transform.position = {0, -.5f, 0};
        auto &gravity = scene.CreateObject("Gravity");
        gravity.gravity = SceneGravityComponent{};
        gravity.gravity->kind = SceneGravityKind::Uniform;
        gravity.gravity->regionRadius = 10000;
        std::vector<CrashRig> rigs;
        rigs.push_back(AddCrashRig(scene, asset, record.id, {0, 1.02f, 0}, exterior));
        SceneObjectId obstacle = 0;
        if (kind == "cars") {
            rigs.push_back(AddCrashRig(scene, asset, record.id, {0, 1.02f, -12}, exterior));
            scene.Find(rigs[1].owner)->transform.rotation =
                glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0));
        } else {
            auto &wall = scene.CreateObject("Obstacle");
            obstacle = wall.id;
            wall.body = SceneBodyComponent{};
            wall.body->halfExtents =
                kind == "side" ? glm::vec3(.18f, 1, .18f) : glm::vec3(kind == "offset" ? .7f : 3.f, 1, .4f);
            wall.transform.position =
                kind == "side" ? glm::vec3(4, 1, 0)
                               : glm::vec3(kind == "offset" ? 1.2f : 0, 1, kind == "rear" ? 7.f : -7.f);
        }
        RuntimeWorld world;
        check(world.Build(scene, &resources, error), kind + " real MK2 structure builds: " + error);
        world.UpdateDeformables(1. / 60);
        for (unsigned step = 0; step < 120; ++step)
            StepAuthoritativeWorld(world, input, 1.f / 60);
        for (auto &rig : rigs) {
            auto *s = world.RuntimeDeformable(rig.owner, error);
            check(s && s->error.empty() && work(*s) == 0,
                  kind + " quiet support does not create permanent damage");
        }
        double mass = 0;
        for (unsigned i = 0; i < rigs.size(); ++i)
            for (unsigned p = 0; p < asset.fracture->parts.size(); ++p) {
                auto body = world.RuntimeBody(world.FracturePartEntity(rigs[i].owner, p));
                mass += world.Physics().GetMass(body);
                world.Physics().SetLinearVelocity(
                    body, kind == "side"
                              ? glm::vec3(12, 0, 0)
                              : glm::vec3(0, 0, kind == "rear" || (kind == "cars" && i == 1) ? 12 : -12));
            }
        check(std::abs(mass - 1500 * rigs.size()) < .01,
              kind + " physical graph owns the full vehicle mass exactly once");
        std::set<unsigned> obstacleBodies;
        if (obstacle)
            obstacleBodies.insert(world.RuntimeBody(obstacle).id);
        else
            for (unsigned p = 0; p < asset.fracture->parts.size(); ++p)
                obstacleBodies.insert(world.RuntimeBody(world.FracturePartEntity(rigs[1].owner, p)).id);
        bool collision = false;
        double peak = 0;
        for (unsigned step = 0; step < 240; ++step) {
            StepAuthoritativeWorld(world, input, 1.f / 60);
            for (auto &rig : rigs) {
                auto *s = world.RuntimeDeformable(rig.owner, error);
                if (!s || !s->error.empty())
                    throw std::runtime_error(kind + " structural step failed: " + error);
                peak = std::max(peak, work(*s));
            }
            if (!collision) {
                for (unsigned p = 0; p < asset.fracture->parts.size(); ++p) {
                    auto body = world.RuntimeBody(world.FracturePartEntity(rigs[0].owner, p));
                    std::vector<PhysicsWorld::BodyContactSample> contacts;
                    if (world.Physics().ReadBodyContactSample(body, contacts))
                        for (auto &contact : contacts)
                            if (obstacleBodies.count(contact.other.id))
                                collision = true;
                }
            }
        }
        check(collision && peak > 100, kind + " actual collision produces finite irreversible plastic work");
        auto *original = world.RuntimeDeformable(rigs[0].owner, error);
        auto plastic = original->fracture.plasticTranslation;
        auto rotations = original->fracture.plasticRotation;
        auto broken = original->fracture.broken;
        auto reference = original->positions;
        std::map<std::string, SaveChunk> chunks;
        check(WorldPersistence::Capture(world, chunks, error),
              kind + " current snapshot captures damaged vehicle: " + error);
        Scene restoredScene;
        check(WorldPersistence::SceneFrom(chunks, restoredScene, error),
              kind + " snapshot reconstructs its scene");
        RuntimeWorld restored;
        check(restored.Build(restoredScene, &resources, error, nullptr, nullptr, true) &&
                  WorldPersistence::Prepare(restored, error) &&
                  WorldPersistence::Restore(restored, chunks, error),
              kind + " damaged physical family restores: " + error);
        WorldPersistence::Published(restored);
        auto *copy = restored.RuntimeDeformable(rigs[0].owner, error);
        double maximum = 0;
        for (unsigned n = 0; n < reference.size(); ++n)
            maximum = std::max(maximum, glm::length(reference[n] - copy->positions[n]));
        check(maximum < 1e-6 && plastic == copy->fracture.plasticTranslation &&
                  rotations == copy->fracture.plasticRotation && broken == copy->fracture.broken,
              kind + " restoration preserves shape and permanent damage");
        for (unsigned step = 0; step < 30; ++step)
            StepAuthoritativeWorld(restored, input, 1.f / 60);
        check(restored.RuntimeDeformable(rigs[0].owner, error)->error.empty(),
              kind + " restored graph continues through MK2");
        std::cout << "CRASH_CONTROL " << kind << " plastic_work_j=" << peak
                  << " restored_shape_error_m=" << maximum << '\n';
    }
    return checks;
}
