#include "AssetDatabase.h"
#include "LiquidTypes.h"
#include "Prefab.h"
#include "RuntimeWorld.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
unsigned checks = 0, failures = 0;
double worstUp = 0, worstMagnitude = 0;

void Check(bool value, const std::string& description) {
    ++checks;
    failures += !value;
    std::cout << (value ? "PASS " : "FAIL ") << description << '\n';
}

void Compare(const Scene& scene, const AssetDatabase& assets, bool expected,
             const std::string& label, bool regional = false) {
    Scene flat;
    std::string error;
    if (!FlattenHierarchy(scene, flat, error)) throw std::runtime_error(error);
    const auto* basin = flat.Find(2);
    LiquidGeometry geometry;
    GravityEquilibrium baked;
    std::string fingerprint;
    const bool prepared = PrepareLiquidBake(flat, *basin, assets, geometry, baked, fingerprint, error);
    RuntimeWorld world;
    world.legacyGameplay = false;
    if (regional) {
        if (!world.Build(Scene{}, nullptr, error)) throw std::runtime_error(error);
        std::vector<EntityId> ids;
        for (const auto& object : flat.Objects()) {
            if (!world.StageRegionObject(object, object, error)) throw std::runtime_error(error);
            ids.push_back(object.id);
        }
        world.PublishRegion(ids);
        world.RebuildRegionGravity({});
    } else if (!world.Build(scene, nullptr, error)) throw std::runtime_error(error);
    const auto pose = world.PresentedTransform(2, basin->transform, 1);
    GravityEquilibrium actual;
    const bool selected = world.Gravity().Equilibrium(pose.position, actual);
    Check(selected == expected && prepared == expected, label + " selects/rejects the same actual region");
    if (selected && prepared) {
        actual.up = glm::inverse(glm::dquat(pose.rotation)) * actual.up;
        actual.center = glm::inverse(glm::dquat(pose.rotation)) *
            (actual.center - glm::dvec3(pose.position));
        const double up = glm::length(actual.up - baked.up);
        const double magnitude = std::abs(actual.magnitude - baked.magnitude);
        worstUp = std::max(worstUp, up);
        worstMagnitude = std::max(worstMagnitude, magnitude);
        Check(actual.Equivalent(baked), label + " bake matches actual published owner/gravity frame");
        world.RebuildRegionGravity({});
        GravityEquilibrium rebuilt;
        Check(world.Gravity().Equilibrium(pose.position, rebuilt), label + " ordinary regional gravity rebuild remains defined");
        rebuilt.up = glm::inverse(glm::dquat(pose.rotation)) * rebuilt.up;
        rebuilt.center = glm::inverse(glm::dquat(pose.rotation)) *
            (rebuilt.center - glm::dvec3(pose.position));
        Check(actual.Equivalent(rebuilt) && baked.Equivalent(rebuilt),
              label + " rebuilt regional field retains the same physical equilibrium");
    }
}

void OriginAfterRebuild(const Scene& scene) {
    Scene independent = scene;
    independent.Find(2)->liquidBasin.reset();
    independent.Find(2)->body = SceneBodyComponent{};
    RuntimeWorld world;
    world.legacyGameplay = false;
    std::string error;
    if (!world.Build(independent, nullptr, error)) throw std::runtime_error(error);
    world.RebuildRegionGravity({});
    const auto point = independent.Find(2)->transform.position;
    GravityEquilibrium before, after;
    Check(world.Gravity().Equilibrium(point, before), "independent rebuilt gravity has an actual equilibrium");
    const glm::vec3 delta(128, -64, 32);
    Check(world.RequestOrigin(glm::dvec3(delta), error) && world.CommitOrigin(),
          "rebuilt field participates in the ordinary origin transaction");
    const bool found = world.Gravity().Equilibrium(point - delta, after);
    std::cout << std::setprecision(17) << "REBUILD_ORIGIN found=" << found
              << " up_error=" << glm::length(before.up - after.up)
              << " magnitude_error=" << std::abs(before.magnitude - after.magnitude) << '\n';
    Check(found && glm::length(before.up - after.up) < 1e-7 &&
          std::abs(before.magnitude - after.magnitude) < 1e-6,
          "regional rebuild then origin shift retains actual gravity, not a raw rotation cache");
}

Scene Fixture(glm::quat rotation, glm::vec3 position, bool physical, bool box) {
    Scene scene;
    auto& field = scene.CreateObject("field");
    field.gravity = SceneGravityComponent{};
    field.gravity->regionShape = box ? SceneRegionShape::Box : SceneRegionShape::Sphere;
    field.gravity->regionHalfExtents = {2, .25f, 2};
    field.gravity->regionRadius = 100;
    field.transform.rotation = rotation;
    auto& basin = scene.CreateObject("basin");
    basin.transform.position = position;
    basin.transform.rotation = rotation;
    basin.liquidBasin = LiquidBasinSettings{};
    basin.liquidBasin->geometry = "e3000000000000000000000000000001";
    if (physical) {
        basin.body = SceneBodyComponent{};
        basin.body->motion = SceneBodyMotion::Static;
    }
    return scene;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: judas_liquid_bake_frame_tests NEW-output");
        const auto output = fs::absolute(argv[1]);
        if (fs::exists(output)) throw std::runtime_error("fresh output required");
        fs::create_directories(output / "Assets");
        const auto path = output / "Assets/cavity.judascavity";
        std::ofstream file(path);
        const auto geometry = LiquidBox({-.05, -.05, -.05}, {.05, .05, .05});
        file << "JudasCavity1 " << geometry.cells.size() << '\n' << std::setprecision(17);
        for (const auto& tet : geometry.cells) {
            for (const auto& point : tet) file << point.x << ' ' << point.y << ' ' << point.z << ' ';
            file << '\n';
        }
        file.close();
        std::string error;
        if (!AssetDatabase::WriteMeta(path.string() + kAssetMetaExtension,
            "e3000000000000000000000000000001", AssetType::Liquid, "frame regression", error))
            throw std::runtime_error(error);
        AssetDatabase assets;
        assets.Scan(output.string(), (output / "Assets").string());
        const auto original = glm::angleAxis(.8f, glm::normalize(glm::vec3(1, 2, 3)));
        for (bool body : {false, true}) {
            Compare(Fixture(original, {}, body, false), assets, true,
                    body ? "original rotation with rigid owner" : "original rotation with liquid-only owner");
            Compare(Fixture(glm::quat(0, .6f, .8f, 0), {}, body, false), assets, true,
                    body ? "half-turn with rigid owner" : "half-turn with liquid-only owner");
            Compare(Fixture(original, {}, body, false), assets, true,
                    body ? "streamed original rigid owner" : "streamed original liquid-only owner", true);
        }
        auto nested = Fixture(original, {}, false, false);
        auto& parent = nested.CreateObject("frame parent");
        parent.transform.rotation = glm::angleAxis(.41f, glm::normalize(glm::vec3(2, -1, 3)));
        parent.transform.position = {2, 3, -1};
        nested.Find(1)->parent = nested.Find(2)->parent = parent.id;
        Compare(nested, assets, true, "nested gravity and liquid owner");
        OriginAfterRebuild(Fixture(original, {}, true, false));
        const auto quarterTurn = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 0, 1));
        Compare(Fixture(quarterTurn, {0, 1, 0}, false, true), assets, true,
                "rotated noncubic gravity zone positive");
        Compare(Fixture(quarterTurn, {1, 0, 0}, false, true), assets, false,
                "rotated noncubic gravity zone independent outside control");
        auto radial = Fixture(original, {0, 10, 0}, false, false);
        radial.Find(1)->gravity->kind = SceneGravityKind::Radial;
        Compare(radial, assets, true, "radial owner-frame control");
        std::cout << std::setprecision(17) << "FRAME_ERRORS up=" << worstUp
                  << " magnitude=" << worstMagnitude << '\n';
        std::cout << "SUMMARY " << checks << " checks " << failures << " failures\n";
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
