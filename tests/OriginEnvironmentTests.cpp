#include "Project.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "GameSession.h"
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
std::string SurfaceState(const LiquidState& state) {
    SaveArchive archive;
    state.dynamicSurface->Persist(archive);
    return archive.bytes;
}
void Cavity(const fs::path& path, const LiquidGeometry& geometry,
            const std::string& id, std::string& error) {
    std::ofstream file(path);
    file << "JudasCavity1 " << geometry.cells.size() << '\n' << std::setprecision(17);
    for (const auto& tet : geometry.cells) {
        for (const auto& p : tet) file << p.x << ' ' << p.y << ' ' << p.z << ' ';
        file << '\n';
    }
    file.close();
    Check(bool(file) && AssetDatabase::WriteMeta(path.string() + kAssetMetaExtension,
          id, AssetType::Liquid, "owned origin regression", error), "normal cavity file and stable asset metadata");
}
Scene LiquidFixture(const fs::path& output, AssetDatabase& assets, bool radial,
                    std::string& error, std::optional<glm::quat> authoredRotation = {}) {
    const auto directory = output / "Assets";
    fs::create_directories(directory);
    const std::string geometryId = "e2000000000000000000000000000001";
    const std::string cupId = "e2000000000000000000000000000002";
    Cavity(directory / "basin.judascavity", LiquidBox({-1, 0, -1}, {1, 1, 1}), geometryId, error);
    Cavity(directory / "cup.judascavity", LiquidBox({-.2, -.3, -.2}, {.2, .3, .2}), cupId, error);
    assets.Scan(output.string(), directory.string());
    Scene scene;
    auto& field = scene.CreateObject("authored field");
    field.gravity = SceneGravityComponent{};
    field.gravity->kind = radial ? SceneGravityKind::Radial : SceneGravityKind::Uniform;
    field.gravity->regionRadius = 5000;
    // Exactly unit in binary32; the rotated plane is deliberately not world-Y.
    // The separately preserved arbitrary-quaternion bake mismatch is not an
    // origin-shift failure and is not repaired by relaxing field tolerances.
    const auto rotation = authoredRotation.value_or(radial ? glm::quat(1, 0, 0, 0) : glm::quat(.5f, .5f, .5f, .5f));
    field.transform.rotation = rotation;
    const glm::vec3 base = radial ? glm::vec3(0, 80, 0) : glm::vec3(0);
    for (unsigned i = 0; i < 2; ++i) {
        auto& basin = scene.CreateObject("reservoir");
        basin.transform.position = base + rotation * glm::vec3(3.f * i, 0, 0);
        basin.transform.rotation = rotation;
        basin.liquidBasin = LiquidBasinSettings{};
        basin.liquidBasin->geometry = geometryId;
        basin.liquidBasin->asset = i ? "e2000000000000000000000000000004" : "e2000000000000000000000000000003";
        basin.liquidBasin->initialVolume = i ? .5 : 2;
        basin.liquidBasin->heightTolerance = .02;
        basin.liquidBasin->surface.enabled = true;
        basin.liquidBasin->surface.columns = basin.liquidBasin->surface.rows = 4;
    }
    auto& cup = scene.CreateObject("container");
    cup.transform.position = base + rotation * glm::vec3(4, 2, 0);
    cup.transform.rotation = rotation;
    cup.liquidContainer = LiquidContainerSettings{};
    cup.liquidContainer->geometry = cupId;
    cup.liquidContainer->initialVolume = .02;
    auto& spill = scene.CreateObject("paired connection");
    spill.transform.position = base + rotation * glm::vec3(1.5f, .2f, 0);
    spill.liquidConnection = LiquidConnectionSettings{};
    spill.liquidConnection->source = 2;
    spill.liquidConnection->destination = 3;
    for (auto& object : scene.Objects()) if (object.liquidBasin) {
        LiquidGeometry geometry;
        GravityEquilibrium equilibrium;
        LiquidBasinData data;
        std::string fingerprint;
        Check(PrepareLiquidBake(scene, object, assets, geometry, equilibrium, fingerprint, error) &&
              BakeLiquidBasin(geometry, equilibrium, 1e-5, .02, fingerprint, data, error) &&
              BakeLiquidSurface(data, object.liquidBasin->surface, data.dynamicSurface, error, &geometry),
              "ordinary authored capacity and dynamic-surface bake: " + error);
        const auto path = directory / (std::to_string(object.id) + ".judasbasin");
        Check(SaveLiquidBasin(path.string(), data, error) &&
              AssetDatabase::WriteMeta(path.string() + kAssetMetaExtension,
                  object.liquidBasin->asset, AssetType::Liquid, "owned origin bake", error),
              "baked data uses normal resource sidecars");
    }
    assets.Scan(output.string(), directory.string());
    return scene;
}
void Prepare(RuntimeWorld& world, ResourceManager& resources, std::string& error) {
    for (unsigned i = 0; i < 20; ++i) {
        if (world.Liquids().PreparePersistence(world, error)) return;
        resources.WaitForAll();
    }
    throw std::runtime_error("required normal liquid preparation: " + error);
}
void LiquidContract(const fs::path& output, bool radial, std::optional<glm::quat> authoredRotation = {},
                    bool bakeOnly = false, bool queryControls = false) {
    AssetDatabase assets;
    std::string error;
    auto scene = LiquidFixture(output, assets, radial, error, authoredRotation);
    JobSystem jobs(2);
    ResourceManager resources(nullptr, &assets, &jobs);
    RuntimeWorld world;ProjectSettings fluids;fluids.advancedFluidDynamics=true;world.ConfigureProject(fluids);
    world.legacyGameplay = false;
    const bool built = world.Build(scene, &resources, error);
    Check(built, "authored world validates original baked fingerprint: " + error);
    Check(!world.RequestOrigin({16384, 0, 0}, error) && error.find("liquid geometry not published") != std::string::npos &&
          world.Settings().worldOrigin == glm::dvec3(0),
          "unpublished liquid-only actors cannot bypass full-volume chart preflight");
    Check(world.RequestOrigin({0, 0, 0}, error) && world.CommitOrigin(),
          "unpublished geometry does not prevent an actual no-op origin request");
    Prepare(world, resources, error);
    GameSession session;
    Check(session.Begin(world, error), "ordinary authoritative session");
    InputSystem input;
    auto step = [&] { StepAuthoritativeWorld(session, input, 1.f / 60); };
    step();
    auto& liquid = world.Liquids();
    for (const auto& [entity, diagnostic] : liquid.Errors())
        std::cerr << "LIQUID entity=" << entity << " diagnostic=" << diagnostic << '\n';
    Check(liquid.Errors().empty(), "ordinary unshifted liquid fixture is healthy before testing chart changes");
    const auto handle = liquid.Handle(2), cup = liquid.Handle(4);
    auto* owner = liquid.Get(handle);
    Check(owner && owner->dynamicSurface && liquid.Get(cup), "real basin surface and independent cup resolve");
    if (bakeOnly) {
        std::cout << "BAKE_ONLY actual authored bake, strict resource decode and published owners verified; no origin claim\n";
        return;
    }
    const auto pose = owner->pose;
    const auto point = pose.position + pose.rotation * glm::vec3(0, .1f, 0);
    Check(liquid.SurfaceImpulse(handle, point, pose.rotation * glm::vec3(8, 0, 0)), "wave receives an ordinary physical impulse");
    Check(liquid.Emit(handle, .01, pose.position + pose.rotation * glm::vec3(0, 2, 0), {.1f, .2f, .3f}) > 0,
          "detached parcel retains a paired ownership debit");
    const auto parcel = liquid.Parcels().begin()->second;
    const auto sample = liquid.Sample(point);
    Check(bool(sample), "positive pre-shift physical submersion sample");
    auto displaced = LiquidBox({-.1, .05, -.1}, {.1, .15, .1});
    for (auto& tet : displaced.cells) for (auto& p : tet)
        p = glm::dvec3(pose.position) + glm::dquat(pose.rotation) * p;
    const auto loading = liquid.Submerged(displaced);
    Check(loading.volume > 0 && glm::length(loading.buoyancy) > 0,
          "positive real displaced-volume and buoyancy integral");
    const auto eye = pose.position + pose.rotation * glm::vec3(0, 2, 4);
    const auto target = pose.position + pose.rotation * glm::vec3(0, .3f, 0);
    const auto up = pose.rotation * glm::vec3(0, 1, 0);
    const auto projection = glm::perspective(glm::radians(60.f), 1.f, .05f, 30.f);
    const auto paths = liquid.OpticalPaths(glm::lookAt(eye, target, up), projection, 8, 8, .5f);
    bool visibleWater = false;
    for (auto path : paths) visibleWater |= path.x > 0 || path.y > 0;
    Check(visibleWater, "positive optical query primes real world-space caches");
    const auto waves = SurfaceState(*owner);
    const auto energy = owner->dynamicSurface->Energy();
    const auto ledger = liquid.Accounting("water");
    const glm::dvec3 delta(512, -512, 512);
    // The public position is binary32. Compare equal represented points as a
    // separate oracle control; a chart subtraction can move a point across a
    // discontinuous finite-volume cell boundary even when all state is exact.
    const auto roundedPoint = (point - glm::vec3(delta)) + glm::vec3(delta);
    const auto roundedSample = liquid.Sample(roundedPoint);
    const auto stablePoint = pose.position + pose.rotation * glm::vec3(.25f, .125f, .25f);
    const auto stableSample = liquid.Sample(stablePoint);
    const auto shiftStarted = std::chrono::steady_clock::now();
    Check(world.RequestOrigin(delta, error) && world.CommitOrigin(), "conserved owners and parcels accept live translation: " + error);
    const auto shiftMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - shiftStarted).count();
    Check(liquid.Get(handle) == owner && SurfaceState(*owner) == waves && owner->dynamicSurface->Energy() == energy,
          "body-independent handle, complete wave history and energy remain byte-identical");
    const auto shifted = liquid.Sample(point - glm::vec3(delta));
    if (authoredRotation && shifted) {
        const auto inverse = glm::inverse(glm::dquat(pose.rotation));
        const auto beforeLocal = inverse * (glm::dvec3(point) - glm::dvec3(pose.position));
        const auto afterLocal = inverse * (glm::dvec3(point - glm::vec3(delta)) -
            glm::dvec3(owner->pose.position));
        std::cout << std::setprecision(17) << "ROTATED_QUERY before_cell="
                  << owner->dynamicSurface->Cell(beforeLocal) << " after_cell="
                  << owner->dynamicSurface->Cell(afterLocal) << " point_error="
                  << glm::length(beforeLocal - afterLocal) << " surface_error="
                  << glm::length(shifted->surfacePoint + glm::vec3(delta) - sample->surfacePoint)
                  << " velocity_error=" << glm::length(shifted->velocity - sample->velocity) << '\n';
    }
    if (queryControls) {
        const auto inverse = glm::inverse(glm::dquat(pose.rotation));
        const auto beforeCell = owner->dynamicSurface->Cell(inverse *
            (glm::dvec3(point) - glm::dvec3(pose.position)));
        const auto roundedCell = owner->dynamicSurface->Cell(inverse *
            (glm::dvec3(roundedPoint) - glm::dvec3(pose.position)));
        Check(roundedSample && roundedPoint != point && beforeCell != roundedCell &&
              glm::length(roundedSample->surfacePoint - sample->surfacePoint) > .001f,
              "unchanged pre-shift state has distinct samples on opposite sides of its represented cell boundary");
        Check(shifted && Near(shifted->surfacePoint + glm::vec3(delta), roundedSample->surfacePoint) &&
              Near(shifted->normal, roundedSample->normal) &&
              Near(shifted->velocity, roundedSample->velocity),
              "translated query matches the same quantized point in unchanged physical state");
        const auto stableShifted = liquid.Sample(stablePoint - glm::vec3(delta));
        Check(stableSample && stableShifted &&
              Near(stableShifted->surfacePoint + glm::vec3(delta), stableSample->surfacePoint) &&
              Near(stableShifted->normal, stableSample->normal) &&
              Near(stableShifted->velocity, stableSample->velocity),
              "independent interior-cell point retains the original strict sample tolerances");
        std::cout << "QUERY_CONTROLS only: original boundary-point contract remains separately failing; no surface reset or sampling correction\n";
        return;
    }
    Check(shifted && Near(shifted->surfacePoint + glm::vec3(delta), sample->surfacePoint) &&
          Near(shifted->normal, sample->normal) && Near(shifted->velocity, sample->velocity),
          "actual depth/normal/flow sample follows the new chart");
    Check(!liquid.Sample(point), "negative old-chart query cannot hit translated water");
    for (auto& tet : displaced.cells) for (auto& p : tet) p -= delta;
    const auto movedLoading = liquid.Submerged(displaced);
    Check(std::abs(movedLoading.volume - loading.volume) < 1e-7 &&
          Near(movedLoading.buoyancy, loading.buoyancy, .003) &&
          Near(movedLoading.center + glm::vec3(delta), loading.center),
          "actual solid-volume loading preserves displacement, force and centre");
    const auto movedPaths = liquid.OpticalPaths(glm::lookAt(eye - glm::vec3(delta), target - glm::vec3(delta), up),
                                              projection, 8, 8, .5f);
    double pathError = 0;
    for (size_t i = 0; i < paths.size(); ++i) pathError = std::max(pathError, double(glm::length(paths[i] - movedPaths[i])));
    Check(pathError < .003, "derived optical planes rebuild in the translated chart");
    std::cout << std::setprecision(17) << "METRIC radial=" << radial << " shift_ms=" << shiftMilliseconds
              << " optical_path_error=" << pathError << " displaced_volume_error="
              << std::abs(movedLoading.volume - loading.volume) << " buoyancy_error="
              << glm::length(movedLoading.buoyancy - loading.buoyancy) << '\n';
    const auto movedParcel = liquid.Parcels().begin()->second;
    Check(Near(movedParcel.position + glm::vec3(delta), parcel.position) && movedParcel.velocity == parcel.velocity &&
          movedParcel.volume == parcel.volume && liquid.Accounting("water").total == ledger.total,
          "parcels translate once; velocity, quantity and total ledger are unchanged");
    std::map<std::string, SaveChunk> chunks;
    const auto captured = WorldPersistence::Capture(world, chunks, error);
    if (!captured) {
        GravityEquilibrium selected;
        world.Gravity().Equilibrium(world.RuntimeDefinition(5)->transform.position, selected);
        const auto localizedUp = glm::inverse(glm::dquat(owner->pose.rotation)) * selected.up;
        std::cerr << std::setprecision(17) << "EQUILIBRIUM selected=" << selected.magnitude
                  << " baked=" << owner->equilibrium.magnitude << " up_error="
                  << glm::length(localizedUp - owner->equilibrium.up) << '\n';
    }
    Check(captured, "shifted disturbed liquid captures through normal participants: " + error);
    Scene saved;
    RuntimeWorld cold;ProjectSettings coldFluids;coldFluids.advancedFluidDynamics=true;cold.ConfigureProject(coldFluids);
    cold.legacyGameplay = false;
    Check(WorldPersistence::SceneFrom(chunks, saved, error) &&
          cold.Build(saved, &resources, error, nullptr, nullptr, true) &&
          WorldPersistence::Prepare(cold, error) && WorldPersistence::Restore(cold, chunks, error),
          "fresh world reconstructs shifted physical liquid state: " + error);
    const auto* restored = cold.Liquids().Get(cold.Liquids().Handle(2));
    Check(restored && SurfaceState(*restored) == waves && cold.Liquids().Sample(point - glm::vec3(delta)) &&
          cold.Liquids().Accounting("water").total == ledger.total,
          "restored endpoint checks real wave allocation, spatial query and owner ledger");
    GameSnapshot snapshot;
    snapshot.project = "owned-origin-contract";
    snapshot.scene = "shifted.judas";
    snapshot.content = SceneFingerprintSha256("owned-origin-contract");
    snapshot.diagnosticBuild = GetBuildIdentity().buildId;
    snapshot.participants = chunks;
    snapshot.metadata = Json{{"waves", SceneFingerprintSha256(waves)}, {"total", ledger.total},
        {"point", {point.x - delta.x, point.y - delta.y, point.z - delta.z}},
        {"surface", {shifted->surfacePoint.x, shifted->surfacePoint.y, shifted->surfacePoint.z}}}.dump();
    std::ofstream(output / "snapshot.bin", std::ios::binary) << EncodeGameSnapshot(snapshot);
    const auto beforeFlow = owner->volume;
    step();
    Check(liquid.Errors().empty() && liquid.Connections().at(5) && owner->volume != beforeFlow,
          "paired flow resumes under translated selected gravity without a false field mismatch");
    Check(std::abs(liquid.Accounting("water").error) <= ledger.tolerance, "continuation remains conservative");
    Check(world.RequestOrigin({0, 0, 0}, error) && world.CommitOrigin() && liquid.Sample(point),
          "return shift restores the same usable local surface");
    const auto origin = world.Settings().worldOrigin;
    // Atomicity compares against the immediate pre-request state. A legitimate
    // paired flow occurred since `ledger` was captured; summation roundoff from
    // that step is not a mutation caused by a later rejected chart request.
    const auto beforeRejectedTotal = liquid.Accounting("water").total;
    std::map<std::string, SaveChunk> beforeRejected, afterRejected;
    Check(WorldPersistence::Capture(world, beforeRejected, error),
          "capture immediate participant baseline before rejected transaction");
    const bool rejected = !world.RequestOrigin({16384, 0, 0}, error);
    const auto rejectionDiagnostic = error;
    Check(WorldPersistence::Capture(world, afterRejected, error),
          "capture actual participants immediately after rejected transaction");
    bool unchanged = beforeRejected.size() == afterRejected.size();
    for (const auto& [name, chunk] : beforeRejected) {
        const auto it = afterRejected.find(name);
        unchanged &= it != afterRejected.end() && it->second.version == chunk.version &&
                     it->second.data == chunk.data;
    }
    Check(unchanged, "rejected chart request preserves every physical participant byte");
    error = rejectionDiagnostic;
    if (authoredRotation) {
        std::cout << std::setprecision(17) << "ROTATED_REJECTION rejected=" << rejected
                  << " error=" << error << " origin_unchanged="
                  << (world.Settings().worldOrigin == origin) << " total="
                  << liquid.Accounting("water").total << " original_total=" << ledger.total
                  << " total_error=" << liquid.Accounting("water").error << '\n';
    }
    Check(rejected && error.find("liquid owner") != std::string::npos &&
          world.Settings().worldOrigin == origin && liquid.Accounting("water").total == beforeRejectedTotal,
          "out-of-chart liquid is rejected atomically with an owner diagnostic");
    Check(!liquid.CanShiftOrigin({std::numeric_limits<double>::quiet_NaN(), 0, 0}, error),
          "nonfinite liquid translations fail preflight");
}
void ColdContract(const fs::path& output) {
    std::ifstream file(output / "snapshot.bin", std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(file), {}};
    GameSnapshot snapshot;
    std::string error;
    Check(DecodeGameSnapshot(bytes, snapshot, error), "fresh process decodes checked snapshot envelope");
    AssetDatabase assets;
    assets.Scan(output.string(), (output / "Assets").string());
    JobSystem jobs(2);
    ResourceManager resources(nullptr, &assets, &jobs);
    Scene scene;
    RuntimeWorld world;ProjectSettings fluids;fluids.advancedFluidDynamics=true;world.ConfigureProject(fluids);
    world.legacyGameplay = false;
    Check(WorldPersistence::SceneFrom(snapshot.participants, scene, error) &&
          world.Build(scene, &resources, error, nullptr, nullptr, true),
          "fresh process builds shifted authored owners without rebaking");
    Prepare(world, resources, error);
    Check(WorldPersistence::Prepare(world, error), "fresh process prepares normal metadata and physical participants");
    const auto restored = WorldPersistence::Restore(world, snapshot.participants, error);
    Check(restored, "fresh process restores actual liquid participants: " + error);
    const auto expected = Json::parse(snapshot.metadata);
    const glm::vec3 point(expected["point"][0], expected["point"][1], expected["point"][2]);
    const glm::vec3 surface(expected["surface"][0], expected["surface"][1], expected["surface"][2]);
    const auto* owner = world.Liquids().Get(world.Liquids().Handle(2));
    const auto sample = world.Liquids().Sample(point);
    Check(owner && sample && Near(sample->surfacePoint, surface) &&
          SceneFingerprintSha256(SurfaceState(*owner)) == expected["waves"] &&
          world.Liquids().Accounting("water").total == expected["total"].get<double>(),
          "cold endpoint verifies physical surface, exact wave history and conserved ownership");
    GameSession session;
    Check(session.Begin(world, error), "cold endpoint begins ordinary continuation");
    InputSystem input;
    StepAuthoritativeWorld(session, input, 1.f / 60);
    Check(world.Liquids().Errors().empty() && world.Liquids().Connections().at(5),
          "fresh-process continuation uses translated gravity and actual connected owners");
    auto corrupt = bytes;
    corrupt.back() ^= 1;
    Check(!DecodeGameSnapshot(corrupt, snapshot, error), "independent damaged-envelope control fails checksum");
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
    RuntimeWorld world;ProjectSettings fluids;fluids.advancedFluidDynamics=true;world.ConfigureProject(fluids);
    world.legacyGameplay = false;
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
    thermal.legacyGameplay = false;
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
    ordinary.legacyGameplay = false;
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
    RuntimeWorld world;ProjectSettings fluids;fluids.advancedFluidDynamics=true;world.ConfigureProject(fluids);
    world.legacyGameplay = false;
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
void LegacyBoundaryContract() {
    Scene scene;
    scene.CreateObject("legacy particle owner").fluidVolume = SceneFluidVolumeComponent{};
    RuntimeWorld world;ProjectSettings experimental;experimental.advancedFluidDynamics=true;world.ConfigureProject(experimental);
    world.legacyGameplay = false;
    std::string error;
    const bool built = world.Build(scene, nullptr, error);
    Check(built, "ordinary legacy particle owner builds without altering its solver: " + error);
    Check(!world.RequestOrigin({512, 0, 0}, error) && error.find("active legacy fluid") != std::string::npos &&
          world.Settings().worldOrigin == glm::dvec3(0),
          "legacy PBF remains an explicit atomic origin rejection");
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 4 && (std::string(argv[1]) == "--bake-regression" ||
                          std::string(argv[1]) == "--bake-only" ||
                          std::string(argv[1]) == "--query-controls")) {
            const auto output = fs::absolute(argv[3]);
            Check(!fs::exists(output), "fresh failed-reproduction output required");
            const std::string mode = argv[2];
            if (mode != "normalization" && mode != "knots") throw std::runtime_error("unknown bake regression");
            const auto rotation = mode == "normalization" ?
                glm::angleAxis(.8f, glm::normalize(glm::vec3(1, 2, 3))) : glm::quat(0, .6f, .8f, 0);
            LiquidContract(output, false, rotation, std::string(argv[1]) == "--bake-only",
                           std::string(argv[1]) == "--query-controls");
            std::cout << "SUMMARY " << checks << " checks 0 failures\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--cold") {
            ColdContract(fs::absolute(argv[2]));
            std::cout << "SUMMARY " << checks << " checks 0 failures\n";
            return 0;
        }
        if (argc != 2) throw std::runtime_error("usage: judas_origin_environment_tests NEW-owned-output");
        const auto output = fs::absolute(argv[1]);
        Check(!fs::exists(output), "fresh output directory required");
        fs::create_directories(output);
        GravityEquilibrium a, b;
        a.magnitude = b.magnitude = 9.81;
        a.up = b.up = glm::normalize(glm::dvec3(1, 2, 3));
        b.center = {512, -512, 512};
        Check(a.Coordinate({2, 3, 4}) == b.Coordinate({2, 3, 4}) && a.Equivalent(b),
              "plane equality ignores its physically unused centre");
        b.up = {0, 1, 0};
        Check(!a.Equivalent(b), "different actual plane directions remain distinct");
        a.kind = b.kind = GravityEquilibrium::Kind::Radius;
        a.center = b.center;
        Check(a.Equivalent(b), "radial equality ignores its unused preferred axis");
        b.center.x += 1;
        Check(!a.Equivalent(b), "different radial centres remain distinct");
        b = a;
        b.magnitude = std::numeric_limits<double>::quiet_NaN();
        Check(!a.Equivalent(b), "nonfinite equilibrium comparison remains false");
        LiquidContract(output / "uniform", false);
        LiquidContract(output / "radial", true);
        for (const auto* region : {"uniform", "radial"}) {
            std::string response, detail;
            const bool cold = CaptureToolOutput(fs::absolute(argv[0]), {"--cold", (output / region).string()},
                response, detail, std::chrono::seconds(30), 65536);
            std::cout << response;
            Check(cold, std::string("independent process verifies ") + region + " origin save: " + detail);
        }
        AtmosphereContract();
        TransientThermalContract();
        CaptureFailureContract();
        LegacyBoundaryContract();
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
