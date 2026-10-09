// FTFT1: independent SHA-256 vectors and canonical authored-field coverage.
#include <cstdio>
#include <functional>
#include <limits>
#include <string>

#include "Scene.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"

namespace {
int g_checks = 0, g_failures = 0;
void Check(bool condition, const std::string& label) {
    ++g_checks;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}
std::string Fingerprint(const Scene& scene) {
    std::string result, error;
    if (!ComputeSceneFingerprint(scene, result, error)) {
        Check(false, "unexpected fingerprint failure: " + error);
    }
    return result;
}
Scene AllComponents() {
    Scene s;
    s.Settings().name = "Canonical baseline";
    s.Settings().fidelityPolicy = SceneFidelityPolicy::Distance;
    auto& o = s.CreateObject("All serialized components");
    o.render = SceneRenderComponent{};
    o.body = SceneBodyComponent{};
    o.body->motion = SceneBodyMotion::Dynamic;
    o.body->compoundBoxes.push_back({glm::vec3(0.0f), glm::vec3(0.5f)});
    o.body->fluidCavities.push_back({glm::vec3(0.0f), glm::vec3(0.25f)});
    o.gravity = SceneGravityComponent{};
    o.light = SceneLightComponent{};
    o.door = SceneDoorComponent{};
    o.lightSwitch = SceneLightSwitchComponent{};
    o.vehicle = SceneVehicleComponent{};
    o.celestial = SceneCelestialComponent{};
    o.celestial->gravitationalParameter = 2.0f;
    o.atmosphere = SceneAtmosphereComponent{};
    o.combustible = SceneCombustibleComponent{};
    o.fluidVolume = SceneFluidVolumeComponent{};
    o.playerStart = ScenePlayerStartComponent{};
    s.CreateObject("Second object");
    return s;
}
void CheckSceneChange(const Scene& scene, const char* label, const std::function<void(Scene&)>& change) {
    const auto expected = Fingerprint(scene);
    auto changed = scene;
    change(changed);
    Check(Fingerprint(changed) != expected, std::string("fingerprint covers ") + label);
}
void CheckObjectChange(const Scene& scene, const char* label, const std::function<void(SceneObject&)>& change) {
    CheckSceneChange(scene, label, [&](Scene& s) { change(s.Objects()[0]); });
}
void Reject(const Scene& scene, const char* label) {
    std::string result = "unchanged", error;
    Check(!ComputeSceneFingerprint(scene, result, error) && result == "unchanged" && !error.empty(), label);
}
void RoundTrip(const Scene& scene, const std::string& label) {
    std::string text, error;
    Scene loaded;
    const bool loadedOk = SaveSceneToString(scene, text) && LoadSceneFromString(text, loaded, error);
    Check(loadedOk, label + " serializes/reloads: " + error);
    if (!loadedOk) return;
    Check(Fingerprint(scene) == Fingerprint(loaded), label + " keeps canonical identity");
}
}  // namespace

int main() {
    Check(SceneFingerprintSha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA-256 empty known vector");
    Check(SceneFingerprintSha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256 abc known vector");
    Check(SceneFingerprintSha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
          "SHA-256 multiple-block padding known vector");
    Check(SceneFingerprintSha256(std::string(1000000, 'a')) ==
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "SHA-256 million-a known vector");
    // Independently packed with Python struct's explicit big-endian formats
    // and hashlib.sha256, not this canonical writer: 146 bytes for schema-5 Scene{} (scene format 3).
    Check(Fingerprint(Scene{}) == "74ff6d15a7ff84dc8ba816b6bab6787a2146bd32c42f66d2e73e00848165dd87",
          "schema 5 default Scene independent binary/hash golden vector");
    const auto s = AllComponents();
    RoundTrip(s, "all-component scene");
    auto zeros = s;
    zeros.Settings().worldOrigin.x = -0.0;
    zeros.Objects()[0].transform.position.x = -0.0f;
    Check(Fingerprint(zeros) == Fingerprint(s), "signed zero is canonical");
    auto inactive = s;
    inactive.Settings().fidelityPolicy = SceneFidelityPolicy::None;
    auto otherInactive = inactive;
    otherInactive.Settings().fidelityFullRadius = 123.0f;
    otherInactive.Settings().fidelityCoarseRadius = 321.0f;
    otherInactive.Objects()[0].gravity->regionHalfExtents = glm::vec3(777.0f);
    Check(Fingerprint(otherInactive) == Fingerprint(inactive), "inactive unserialized settings do not change identity");
    RoundTrip(otherInactive, "inactive-field scene");
    auto boxRegion = s;
    boxRegion.Objects()[0].gravity->regionShape = SceneRegionShape::Box;
    auto otherBoxRegion = boxRegion;
    otherBoxRegion.Objects()[0].gravity->regionRadius = 777.0f;
    Check(Fingerprint(boxRegion) == Fingerprint(otherBoxRegion), "inactive sphere radius excluded for box gravity region");
    RoundTrip(otherBoxRegion, "box-region scene");

#define SETTING(field, value) CheckSceneChange(s, "settings." #field, [](Scene& x) { x.Settings().field = value; })
    SETTING(name, "Renamed"); SETTING(worldOrigin.x, 1.0e12); SETTING(sunDirection.y, 0.2f);
    SETTING(sunColor.z, 0.2f); SETTING(ambientColor.x, 0.2f); SETTING(fluidScale, 2.0f);
    SETTING(fluidUpdateRateHz, 20.0f); SETTING(fluidHydrostaticDragRate, 3.0f);
    SETTING(fidelityPolicy, SceneFidelityPolicy::None); SETTING(fidelityFullRadius, 49.0f); SETTING(fidelityCoarseRadius, 151.0f);
#undef SETTING
    CheckSceneChange(s, "NextId", [](Scene& x) { x.SetNextId(100); });
    CheckSceneChange(s, "stable ID", [](Scene& x) { x.Objects()[0].id = 7; x.SetNextId(8); });
    CheckSceneChange(s, "added object", [](Scene& x) { x.CreateObject("New"); });
    CheckSceneChange(s, "removed object", [](Scene& x) { x.DestroyObject(2); });
    CheckSceneChange(s, "authored object order", [](Scene& x) { x.MoveObject(1, 1); });
#define FIELD(field, value) CheckObjectChange(s, #field, [](SceneObject& o) { o.field = value; })
    FIELD(name, "Rename"); FIELD(transform.position.x, 1.0f); FIELD(transform.rotation.w, 0.9f); FIELD(transform.scale.z, 2.0f);
    FIELD(render->shape, SceneShape::Sphere); FIELD(render->halfExtents.x, 0.7f); FIELD(render->radius, 0.7f);
    FIELD(render->color.y, 0.3f); FIELD(render->alpha, 0.3f); FIELD(render->secondaryColor.y, 0.3f);
    FIELD(render->secondaryAlpha, 0.3f); FIELD(render->meshAsset, "stable-mesh-id"); FIELD(render->textureAsset, "stable-texture-id");
    FIELD(body->motion, SceneBodyMotion::Static); FIELD(body->shape, SceneShape::Sphere);
    FIELD(body->halfExtents.z, 0.7f); FIELD(body->radius, 0.7f); FIELD(body->terrainSurface, "terrain-identifier");
    FIELD(body->mass, 3.0f); FIELD(body->friction, 0.3f); FIELD(body->restitution, 0.3f); FIELD(body->initialLinearVelocity.x, 2.0f);
    FIELD(body->pickable, true); FIELD(body->managed, true); FIELD(body->compoundBoxes[0].localCenter.y, 1.0f);
    FIELD(body->compoundBoxes[0].halfExtents.z, 0.7f);
    CheckObjectChange(s, "compound count", [](SceneObject& o) { o.body->compoundBoxes.clear(); });
    FIELD(gravity->kind, SceneGravityKind::Uniform); FIELD(gravity->magnitude, 1.0f);
    FIELD(gravity->regionShape, SceneRegionShape::Box); FIELD(gravity->regionRadius, 11.0f);
    CheckObjectChange(boxRegion, "box gravity extents", [](SceneObject& o) { o.gravity->regionHalfExtents.y = 7.0f; });
    FIELD(light->kind, SceneLightKind::Spot); FIELD(light->color.x, 2.0f); FIELD(light->range, 11.0f);
    FIELD(light->innerConeDegrees, 14.0f); FIELD(light->outerConeDegrees, 26.0f);
    FIELD(door->localHingeAxis.z, 0.1f); FIELD(door->openAngleDegrees, 91.0f); FIELD(door->angularSpeedDegreesPerSecond, 121.0f);
    FIELD(lightSwitch->localHingeAxis.x, 0.1f); FIELD(lightSwitch->toggleAngleDegrees, 41.0f);
    FIELD(lightSwitch->angularSpeedDegreesPerSecond, 221.0f); FIELD(lightSwitch->lampLocalOffset.z, 1.0f);
    FIELD(lightSwitch->lampColor.z, 2.0f); FIELD(lightSwitch->lampRange, 11.0f);
    FIELD(vehicle->gravity, SceneVehicleGravity::Celestial); FIELD(vehicle->headlight, false); FIELD(vehicle->navigationLights, false);
    FIELD(vehicle->dragCoefficient, 2.0f); FIELD(vehicle->initialPilotAttached, true);
    FIELD(celestial->gravitationalParameter, 1.0f); FIELD(celestial->operatorThrustForce, 1.0f);
    FIELD(atmosphere->referenceRadius, 81.0f); FIELD(atmosphere->topRadius, 111.0f); FIELD(atmosphere->referenceDensity, 0.06f);
    FIELD(atmosphere->polytropicExponent, 1.5f); FIELD(atmosphere->oxidizerMassFraction, 0.2f); FIELD(atmosphere->referenceTemperatureKelvin, 301.0f);
    FIELD(combustible->heatCapacityJPerK, 151.0f); FIELD(combustible->initialFuelMassKg, 0.13f); FIELD(combustible->ignitionTemperatureK, 551.0f);
    FIELD(combustible->maximumFuelRateKgPerSecond, 0.004f); FIELD(combustible->radiativeAreaSquareMeters, 1.6f);
    FIELD(combustible->retainedCombustionHeatFraction, 0.76f);
    FIELD(fluidVolume->spacing, 0.06f); FIELD(fluidVolume->countX, 6); FIELD(fluidVolume->countY, 6); FIELD(fluidVolume->countZ, 6);
    FIELD(fluidVolume->emitter, true); FIELD(fluidVolume->emitterLocalOffset.z, 1.0f); FIELD(fluidVolume->maxParticles, 100);
    FIELD(playerStart->view, ScenePlayerView::FirstPerson); FIELD(playerStart->yawDegrees, 1.0f);
    FIELD(playerStart->density, 1000.0f); FIELD(playerStart->fluidDrag, 3.0f);
    FIELD(playerStart->swimAcceleration, 5.0f);
    FIELD(body->fluidCavities[0].localCenter.x, 0.1f);
    FIELD(body->fluidCavities[0].halfExtents.z, 0.3f);
    CheckObjectChange(s, "fluid cavity presence/count", [](SceneObject& o) { o.body->fluidCavities.clear(); });
#define REMOVE(field) CheckObjectChange(s, #field " presence", [](SceneObject& o) { o.field.reset(); })
    REMOVE(render); REMOVE(body); REMOVE(gravity); REMOVE(light); REMOVE(door); REMOVE(lightSwitch);
    REMOVE(vehicle); REMOVE(celestial); REMOVE(atmosphere); REMOVE(combustible); REMOVE(fluidVolume); REMOVE(playerStart);
#undef REMOVE
#undef FIELD
    auto invalid = s;
    invalid.Objects()[1].id = invalid.Objects()[0].id; Reject(invalid, "duplicate IDs rejected without output mutation");
    invalid = s; invalid.Objects()[0].id = 0; Reject(invalid, "zero ID rejected");
    invalid = s; invalid.Objects()[0].id = invalid.NextId(); Reject(invalid, "ID at/above NextId rejected");
    invalid = s; invalid.Objects()[0].body->mass = std::numeric_limits<float>::quiet_NaN(); Reject(invalid, "non-finite component rejected");
    invalid = s; invalid.Settings().worldOrigin.z = std::numeric_limits<double>::infinity(); Reject(invalid, "non-finite setting rejected");
    invalid = s; invalid.Objects()[0].body->motion = static_cast<SceneBodyMotion>(99); Reject(invalid, "invalid enum rejected");

    for (const char* path : {"assets/scenes/classic.judas", "assets/scenes/terrain.judas", "assets/scenes/flat_playground.judas", "assets/scenes/fidelity_demo.judas"}) {
        Scene authored; std::string error;
        const bool loadedOk = LoadSceneFromFile(path, authored, error);
        Check(loadedOk, std::string("load authored fixture ") + path + ": " + error);
        if (loadedOk) RoundTrip(authored, path);
    }
    std::printf("Scene fingerprint: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
