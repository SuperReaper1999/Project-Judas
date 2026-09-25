// Milestone 28: scene data, serialization, stable identity, mutation,
// runtime instantiation and the authored/runtime boundary — headless (a
// RuntimeWorld built without a RenderAssetCache creates no GPU state), no
// window, no GL, no font. Run from the repository root: the demo scene
// files under assets/scenes are loaded through the same path the runtime
// uses.
#include <cmath>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GameSession.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"

namespace {
int g_failures = 0;

void Check(bool condition, const char* label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label);
    if (!condition) ++g_failures;
}

bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1.0e-5f) {
    return glm::length(a - b) <= tolerance;
}

Scene MakeAuthoredScene() {
    Scene scene;
    scene.Settings().name = "Test scene";
    scene.Settings().worldOrigin = glm::dvec3(1.0e9, -2.0e9, 3.0e9);
    scene.Settings().fluidScale = 1.0f;

    SceneObject& ground = scene.CreateObject("Ground");
    ground.transform.position = glm::vec3(0.0f, -1.0f, 0.0f);
    ground.render = SceneRenderComponent{};
    ground.render->shape = SceneShape::Box;
    ground.render->halfExtents = glm::vec3(10.0f, 1.0f, 10.0f);
    ground.render->color = glm::vec3(0.4f, 0.5f, 0.3f);
    ground.body = SceneBodyComponent{};
    ground.body->motion = SceneBodyMotion::Static;
    ground.body->shape = SceneShape::Box;
    ground.body->halfExtents = glm::vec3(10.0f, 1.0f, 10.0f);
    ground.body->friction = 0.8f;
    ground.body->restitution = 0.1f;
    ground.gravity = SceneGravityComponent{};
    ground.gravity->kind = SceneGravityKind::Uniform;
    ground.gravity->magnitude = 9.81f;
    ground.gravity->regionShape = SceneRegionShape::Box;
    ground.gravity->regionHalfExtents = glm::vec3(20.0f, 30.0f, 20.0f);

    SceneObject& crate = scene.CreateObject("Crate");
    crate.transform.position = glm::vec3(1.5f, 4.0f, -0.25f);
    crate.transform.rotation = glm::angleAxis(0.3f, glm::normalize(glm::vec3(0.2f, 1.0f, 0.1f)));
    crate.render = SceneRenderComponent{};
    crate.render->shape = SceneShape::Box;
    crate.render->halfExtents = glm::vec3(0.5f);
    crate.render->color = glm::vec3(0.85f, 0.35f, 0.2f);
    crate.body = SceneBodyComponent{};
    crate.body->motion = SceneBodyMotion::Dynamic;
    crate.body->shape = SceneShape::Box;
    crate.body->halfExtents = glm::vec3(0.5f);
    crate.body->mass = 5.0f;
    crate.body->friction = 0.6f;
    crate.body->restitution = 0.15f;
    crate.body->initialLinearVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
    crate.body->pickable = true;

    SceneObject& lamp = scene.CreateObject("Lamp");
    lamp.transform.position = glm::vec3(-2.0f, 3.0f, 2.0f);
    lamp.light = SceneLightComponent{};
    lamp.light->kind = SceneLightKind::Spot;
    lamp.light->color = glm::vec3(3.0f, 2.5f, 2.0f);
    lamp.light->range = 12.0f;
    lamp.light->innerConeDegrees = 12.0f;
    lamp.light->outerConeDegrees = 24.0f;

    SceneObject& start = scene.CreateObject("Player start");
    start.transform.position = glm::vec3(0.0f, 2.0f, 4.0f);
    start.playerStart = ScenePlayerStartComponent{};
    start.playerStart->yawDegrees = 180.0f;
    start.playerStart->view = ScenePlayerView::FirstPerson;
    return scene;
}

void SectionSerialization() {
    std::printf("Section A: create -> save -> load -> equivalent authored state\n");
    const Scene authored = MakeAuthoredScene();
    std::string text;
    Check(SaveSceneToString(authored, text), "scene serializes to text");
    Scene loaded;
    std::string error;
    Check(LoadSceneFromString(text, loaded, error), ("scene text loads: " + error).c_str());
    Check(ScenesEqual(authored, loaded), "loaded scene equals the authored scene exactly");
    std::string again;
    SaveSceneToString(loaded, again);
    Check(again == text, "save -> load -> save is byte-identical (deterministic format)");
    Check(text.rfind("JudasScene 2\n", 0) == 0, "file starts with the format identifier and version");
    Check(text.find("object 2 \"Crate\"") != std::string::npos, "object ids and names are written explicitly");
    Check(text.find("9.81") != std::string::npos && text.find("9.81000") == std::string::npos,
          "floats use the shortest exact decimal");
}

void SectionIdentity() {
    std::printf("Section B: stable identity across deletion and reload\n");
    Scene scene = MakeAuthoredScene();
    const SceneObjectId crateId = scene.Objects()[1].id;
    const SceneObjectId lampId = scene.Objects()[2].id;
    Check(scene.DestroyObject(crateId), "an object can be deleted by id");
    Check(scene.Find(crateId) == nullptr, "deleted id no longer resolves");
    SceneObject& fresh = scene.CreateObject("Fresh");
    Check(fresh.id != crateId && fresh.id > lampId, "a deleted id is never reused");
    std::string text;
    SaveSceneToString(scene, text);
    Scene loaded;
    std::string error;
    LoadSceneFromString(text, loaded, error);
    Check(loaded.Find(lampId) != nullptr && loaded.Find(lampId)->name == "Lamp", "ids survive save/load");
    Check(loaded.NextId() == scene.NextId(), "the id counter survives save/load");
    SceneObject& afterReload = loaded.CreateObject("After reload");
    Check(afterReload.id == fresh.id + 1, "ids continue from the saved counter after reload");
}

void SectionMutation() {
    std::printf("Section C: create/delete/modify generic objects\n");
    Scene scene;
    SceneObject& a = scene.CreateObject("A");
    const SceneObjectId aId = a.id;
    SceneObject& b = scene.CreateObject("B");
    const SceneObjectId bId = b.id;
    Check(scene.Objects().size() == 2 && aId != bId, "two created objects get distinct ids");
    scene.Find(bId)->transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    scene.Find(bId)->render = SceneRenderComponent{};
    Check(scene.Find(bId)->render.has_value(), "a component can be added to an existing object");
    scene.Find(bId)->render.reset();
    Check(!scene.Find(bId)->render.has_value(), "a component can be removed again");
    Check(scene.MoveObject(bId, -1) && scene.Objects()[0].id == bId, "objects can be reordered");
    Check(!scene.MoveObject(bId, -1), "reordering past the front is rejected");
    Check(!scene.DestroyObject(9999), "deleting an unknown id is rejected");
    SceneObject dup;
    dup.id = aId;
    Check(!scene.InsertObject(dup), "inserting a duplicate id is rejected");
    scene.Clear();
    Check(scene.Objects().empty() && scene.NextId() == 1, "clear resets objects and the id counter");
}

void SectionInvalid() {
    std::printf("Section D: malformed/incompatible scene data fails clearly\n");
    const Scene authored = MakeAuthoredScene();
    std::string text;
    SaveSceneToString(authored, text);
    Scene out;
    std::string error;

    const auto fails = [&](const std::string& mutated, const char* label) {
        out = Scene();
        out.CreateObject("sentinel");
        const bool ok = LoadSceneFromString(mutated, out, error);
        Check(!ok && !error.empty(), label);
        Check(out.Objects().size() == 1 && out.Objects()[0].name == "sentinel",
              "  ...and the output scene is left untouched");
    };
    fails("", "empty input fails");
    fails("JudasScene 99\nsettings\nend\n", "an unsupported version fails");
    fails("NotAScene 1\n", "a wrong identifier fails");
    {
        std::string t = text;
        t.replace(t.find("body.mass 5"), 11, "body.mass nan");
        fails(t, "a non-finite number fails");
    }
    {
        std::string t = text;
        t.replace(t.find("body.friction 0.6"), 17, "body.frictoin 0.6");
        fails(t, "an unknown key fails (no silent default for the missing one)");
    }
    {
        std::string t = text;
        const std::size_t pos = t.find("  position 1.5 4 -0.25\n");
        t.erase(pos, std::string("  position 1.5 4 -0.25\n").size());
        fails(t, "a missing required transform field fails");
    }
    {
        std::string t = text;
        t.replace(t.find("object 2 \"Crate\""), 16, "object 1 \"Crate\"");
        fails(t, "a duplicate object id fails");
    }
    {
        std::string t = text;
        t.replace(t.find("object 4 \"Player start\""), 23, "object 4 \"Player start\"\n  vehicle local\n  vehicle.headlight true\n  vehicle.navigation-lights true\n  vehicle.drag-coefficient 1\n  vehicle.initial-pilot-attached false");
        fails(t, "a component whose prerequisites are absent (vehicle without a body) fails");
    }
    {
        std::string t = text;
        const std::size_t pos = t.rfind("end\n");
        t.erase(pos);
        fails(t, "a truncated file fails");
    }
    Check(!LoadSceneFromFile("assets/scenes/does_not_exist.judas", out, error) && !error.empty(),
          "a missing file fails with its path in the message");
}

// The play loop's fixed step needs a Window for held-key state; a Window
// in test-input mode never touches SDL (tests/SpacecraftControlTests.cpp
// established this pattern).
void SectionRuntimeSeparation() {
    std::printf("Section E: edit -> play -> mutate runtime -> stop -> authored state restored\n");
    Scene scene = MakeAuthoredScene();
    std::string text;
    SaveSceneToString(scene, text);

    RuntimeWorld world;
    std::string error;
    Check(world.Build(scene, nullptr, error), ("scene instantiates headlessly: " + error).c_str());
    Check(world.DynamicBodies().size() == 1 && world.StaticBodies().size() == 1,
          "one dynamic and one static body were created");
    Check(world.GetPlayerStart().has_value() && Near(world.GetPlayerStart()->position, glm::vec3(0.0f, 2.0f, 4.0f)),
          "the player start was read from the scene");
    Check(world.StaticLights().size() == 1, "the standalone light was instantiated");

    GameSession session;
    Check(session.Begin(world, error), "a game session begins over the world");
    Window window;
    window.SetTestInputMode(true);
    const SceneObjectId crateId = scene.Objects()[1].id;
    const glm::vec3 authoredCratePosition = scene.Find(crateId)->transform.position;
    for (int i = 0; i < 180; ++i) StepPlayedWorld(session, window, SimulationTiming::kFixedTimestep);
    const glm::vec3 fallen = world.DynamicBodies()[0].GetPosition();
    Check(fallen.y < authoredCratePosition.y - 2.0f, "the crate fell under the scene's gravity during play");
    Check(Near(scene.Find(crateId)->transform.position, authoredCratePosition),
          "playing did not touch the authored scene object");
    world.Physics().SetLinearVelocity(world.DynamicBodies()[0].Handle(), glm::vec3(3.0f, 0.0f, 0.0f));
    for (int i = 0; i < 30; ++i) StepPlayedWorld(session, window, SimulationTiming::kFixedTimestep);
    Check(std::abs(world.DynamicBodies()[0].GetPosition().x - authoredCratePosition.x) > 0.1f,
          "runtime mutation moved the body sideways");

    // Stop: the runtime instance is discarded; the Scene is what it was.
    session.End();
    world.Destroy();
    std::string afterPlay;
    SaveSceneToString(scene, afterPlay);
    Check(afterPlay == text, "after stopping, the scene serializes byte-identically to before play");

    // Play again from the same Scene: every body starts from authored state.
    Check(world.Build(scene, nullptr, error), "the same scene instantiates again after stop");
    Check(Near(world.DynamicBodies()[0].GetPosition(), authoredCratePosition),
          "the second run starts from the authored pose, not the mutated one");
    Check(session.Begin(world, error), "a second session begins");
    for (int i = 0; i < 60; ++i) StepPlayedWorld(session, window, SimulationTiming::kFixedTimestep);
    session.ResetToAuthoredState();
    Check(Near(world.DynamicBodies()[0].GetPosition(), authoredCratePosition),
          "an in-run reset restores the authored pose as well");
}

void SectionRuntimeLoading() {
    std::printf("Section F: scenes saved by the editor representation load through the runtime path\n");
    const Scene authored = MakeAuthoredScene();
    const std::string path = ".judas_scene_test_roundtrip.judas";
    std::string error;
    Check(SaveSceneToFile(authored, path, error), ("scene saves to a file: " + error).c_str());
    Scene loaded;
    Check(LoadSceneFromFile(path, loaded, error), ("saved file loads from disk: " + error).c_str());
    RuntimeWorld world;
    Check(world.Build(loaded, nullptr, error), "the reloaded scene instantiates");
    std::remove(path.c_str());

    // The committed demonstrations: every shipped scene loads and builds.
    const char* shipped[] = {
        "assets/scenes/classic.judas", "assets/scenes/classic_fluid_rotated.judas",
        "assets/scenes/classic_fluid_zero.judas", "assets/scenes/terrain.judas",
        "assets/scenes/terrain_rotated.judas", "assets/scenes/terrain_atmospheric_pass.judas",
        "assets/scenes/terrain_atmospheric_pass_rotated.judas",
    };
    for (const char* file : shipped) {
        Scene scene;
        RuntimeWorld shippedWorld;
        const bool ok = LoadSceneFromFile(file, scene, error) && shippedWorld.Build(scene, nullptr, error);
        Check(ok, (std::string("shipped scene loads and instantiates: ") + file + (ok ? "" : " — " + error)).c_str());
        std::string roundTrip;
        SaveSceneToString(scene, roundTrip);
        Scene again;
        Check(LoadSceneFromString(roundTrip, again, error) && ScenesEqual(scene, again),
              "  ...and round-trips through the format unchanged");
    }
    Scene classic;
    LoadSceneFromFile("assets/scenes/classic.judas", classic, error);
    RuntimeWorld classicWorld;
    classicWorld.Build(classic, nullptr, error);
    Check(classicWorld.GetVehicle().has_value() && classicWorld.Doors().size() == 1 &&
              classicWorld.LightSwitches().size() == 1 && classicWorld.CelestialParticipants().size() == 3 &&
              classicWorld.HasFluid() && classicWorld.Fluid().Particles().size() == 125,
          "classic scene: vehicle, door, switch, three Newtonian participants, 125 water particles");
    Scene terrain;
    LoadSceneFromFile("assets/scenes/terrain.judas", terrain, error);
    RuntimeWorld terrainWorld;
    terrainWorld.Build(terrain, nullptr, error);
    Check(terrainWorld.Terrains().size() == 1 && terrainWorld.GetAtmosphere().has_value() &&
              terrainWorld.Combustibles().size() == 3 && terrainWorld.PointMassSources().size() == 1 &&
              terrainWorld.Fluid().Particles().size() == 125,
          "terrain scene: terrain body, atmosphere, three combustibles, point-mass source, lake");
    Check(terrainWorld.EmitFluidParticle() && terrainWorld.Fluid().Particles().size() == 126,
          "the authored emitter adds real particles on demand");
    terrainWorld.RestoreAuthoredState();
    Check(terrainWorld.Fluid().Particles().size() == 125 && terrainWorld.EmittedFluidParticles() == 0,
          "restoring authored state discards emitted particles");
}

void SectionInstantiationErrors() {
    std::printf("Section G: unrealisable scenes fail at instantiation, not silently\n");
    Scene scene = MakeAuthoredScene();
    SceneObject& terrain = scene.CreateObject("Bad terrain");
    terrain.body = SceneBodyComponent{};
    terrain.body->motion = SceneBodyMotion::Static;
    terrain.body->shape = SceneShape::Terrain;
    terrain.body->terrainSurface = "no-such-surface";
    RuntimeWorld world;
    std::string error;
    Check(!world.Build(scene, nullptr, error) && error.find("no-such-surface") != std::string::npos,
          "an unknown terrain surface id fails with the id in the message");
    Check(!world.IsBuilt(), "a failed build leaves no world behind");
}
}  // namespace

int main() {
    SectionSerialization();
    SectionIdentity();
    SectionMutation();
    SectionInvalid();
    SectionRuntimeSeparation();
    SectionRuntimeLoading();
    SectionInstantiationErrors();
    std::printf("Scene tests: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
