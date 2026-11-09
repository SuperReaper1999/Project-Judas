#include "Project.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "Simulation.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "ToolProcess.h"
#include "BuildIdentity.h"
#include "../third_party/nlohmann/json.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <chrono>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
unsigned checks = 0;
void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}
bool Near(glm::vec3 a, glm::vec3 b, double tolerance = .0003) {
    return glm::length(glm::dvec3(a) - glm::dvec3(b)) < tolerance;
}
void AtmosphereContract() {
    Scene scene;
    auto& planet = scene.CreateObject("prescribed atmosphere");
    planet.body = SceneBodyComponent{};
    planet.celestial = SceneCelestialComponent{};
    planet.celestial->gravitationalParameter = 62784;
    planet.atmosphere = SceneAtmosphereComponent{};
    auto& fuel = scene.CreateObject("ordinary thermal carrier");
    fuel.transform.position = {0, 90, 0};
    fuel.body = SceneBodyComponent{};
    fuel.body->motion = SceneBodyMotion::Dynamic;
    fuel.combustible = SceneCombustibleComponent{};
    RuntimeWorld world;

    std::string error;
    Check(world.Build(scene, nullptr, error), "ordinary gas and combustible world builds: " + error);
    const auto body = world.RuntimeBody(2);
    const auto before = world.GetAtmosphere()->field.Sample({0, 90, 0}, world.GetAtmosphere()->frame);
    const auto thermal = *world.Combustion().State(body);
    const glm::vec3 delta(512, -512, 512);
    Check(world.RequestOrigin(glm::dvec3(delta), error) && world.CommitOrigin(), "atmosphere and thermal carriers accept chart changes: " + error);
    const auto after = world.GetAtmosphere()->field.Sample(glm::vec3(0, 90, 0) - delta, world.GetAtmosphere()->frame);
    Check(before.density > 0 && after.density == before.density && after.pressure == before.pressure &&
          after.temperatureKelvin == before.temperatureKelvin && after.velocity == before.velocity,
          "analytic gas density, pressure, temperature and airflow are frame invariant");
    Check(world.RuntimeBody(2).id == body.id && Near(world.Physics().GetTransform(body).position + delta, {0, 90, 0}) &&
          world.Combustion().State(body)->remainingFuelMassKg == thermal.remainingFuelMassKg &&
          world.Combustion().State(body)->temperatureKelvin == thermal.temperatureKelvin,
          "actual carrier and thermal state keep their original physical ownership");
    RadiantHeater heater;
    heater.worldPosition = glm::vec3(0, 90.5f, 0) - delta;
    heater.powerWatts = 18000;
    world.Combustion().Step(1.f / 60, world.Physics(), world.GetAtmosphere()->field,
                            world.GetAtmosphere()->frame, &heater);
    Check(world.Combustion().State(body)->temperatureKelvin > thermal.temperatureKelvin &&
          world.Combustion().State(body)->heaterWattsReceived > 0,
          "translated heater/gas/carrier continue real thermal exchange");
    std::map<std::string, SaveChunk> previous{{"sentinel", {1, "previous snapshot"}}};
    Check(!WorldPersistence::Capture(world, previous, error) && error.find("no modern slot participant") != std::string::npos &&
          previous.size() == 1 && previous.at("sentinel").data == "previous snapshot",
          "unsupported thermal save fails early while preserving the caller's previous snapshot");
}
void TransientThermalContract() {
    Scene scene;
    auto& actor = scene.CreateObject("eligible ephemeral dynamic carrier");
    actor.body = SceneBodyComponent{};
    actor.body->motion = SceneBodyMotion::Dynamic;
    actor.combustible = SceneCombustibleComponent{};
    RuntimeWorld thermal;

    std::string error;
    Check(thermal.Build(scene, nullptr, error) && thermal.SetTransientEntity(1, true, error),
          "ordinary dynamic thermal carrier is independently eligible for the transient flag");
    std::map<std::string, SaveChunk> previous{{"sentinel", {1, "previous snapshot"}}};
    const bool captured = WorldPersistence::Capture(thermal, previous, error);
    Check(!captured && error.find("component combustion") != std::string::npos &&
          previous.size() == 1 && previous.at("sentinel").data == "previous snapshot",
          "transient flags cannot bypass unsupported global thermal ownership");
    scene.Find(1)->combustible.reset();
    RuntimeWorld ordinary;

    Check(ordinary.Build(scene, nullptr, error) && ordinary.SetTransientEntity(1, true, error),
          "independent otherwise identical nonthermal control is eligible");
    Scene excluded;
    Check(WorldPersistence::Capture(ordinary, previous, error) &&
          WorldPersistence::SceneFrom(previous, excluded, error) && excluded.Objects().empty(),
          "ordinary exclusion still succeeds and does not resurrect the omitted body");
}
void CaptureFailureContract() {
    Scene scene;
    scene.CreateObject("motor").characterMotor = CharacterMotorSettings{};
    RuntimeWorld world;

    std::string error;
    Check(world.Build(scene, nullptr, error), "normal motor world builds for late-participant failure control");
    auto* motor = world.RuntimeCharacter(1);
    motor->filter.ignoredBodies.push_back({});
    std::map<std::string, SaveChunk> previous{{"sentinel", {7, "last good snapshot"}}};
    const auto captured = WorldPersistence::Capture(world, previous, error);
    Check(!captured && error.find("unowned ignored body") != std::string::npos && previous.size() == 1 &&
          previous.at("sentinel").version == 7 && previous.at("sentinel").data == "last good snapshot",
          "late serializer failure preserves every byte/version of the caller's prior snapshot");
    motor->filter.ignoredBodies.clear();
    Check(WorldPersistence::Capture(world, previous, error) && !previous.count("sentinel") && previous.count("motors"),
          "positive repaired participant replaces the complete snapshot only after success");
}

}

int main() {
    try { AtmosphereContract();TransientThermalContract();CaptureFailureContract();
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";return 0;
    }catch(const std::exception& e){std::cerr << "FAIL " << e.what() << '\n';return 1;}
}
