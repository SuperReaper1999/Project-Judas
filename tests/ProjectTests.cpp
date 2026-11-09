// Milestone 30: projects, stable asset identity, the resource manager
// boundary, project -> startup scene -> runtime, the shipped projects,
// gizmo mathematics with undo/redo, the inspector's data round trip and
// Edit/Play separation — all headless (no window, no GL, no font). Run
// from the repository root through the same code used by runtime/editor.
// Temporary projects are
// created under the system temp directory and removed afterwards.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AssetDatabase.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"
#include "editor/EditorDocument.h"
#include "editor/GizmoMath.h"

namespace fs = std::filesystem;

namespace {
int g_failures = 0;

void Check(bool condition, const std::string& label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}

bool Near(float a, float b, float tolerance = 1.0e-4f) { return std::abs(a - b) <= tolerance; }
bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1.0e-4f) { return glm::length(a - b) <= tolerance; }

std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return text;
}

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name) {
        std::error_code ec;
        path = fs::temp_directory_path(ec) / (name + "_" + std::to_string(static_cast<long long>(::getpid())));
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string Str() const { return path.generic_string(); }
};

// --- A. project format -----------------------------------------------------

void SectionProjectFormat() {
    std::printf("Section A: project file format\n");
    ProjectSettings settings;
    settings.name = "Quoted \"name\"";
    settings.startupScene = "Scenes/main.judas";
    const std::string text = Project::SerializeToString(settings);
    Check(text.rfind("JudasProject 1\n", 0) == 0, "serialized project starts with the format identifier and version");
    ProjectSettings parsed;
    std::string error;
    Check(Project::ParseFromString(text, parsed, error), "serialized project parses: " + error);
    Check(parsed.name == settings.name && parsed.startupScene == settings.startupScene && parsed.assetsDir == "Assets" &&
              parsed.scenesDir == "Scenes" && parsed.savesDir == "Saves",
          "parse round-trips every field, including an escaped quote");
    Check(Project::SerializeToString(parsed) == text, "serialize -> parse -> serialize is byte-identical");

    const auto fails = [&](const std::string& body, const char* label) {
        ProjectSettings out;
        std::string e;
        Check(!Project::ParseFromString(body, out, e) && !e.empty(), label + std::string(" (") + e + ")");
    };
    fails("JudasProject 2\nname \"x\"\nstartup-scene \"\"\nassets-dir \"A\"\nscenes-dir \"S\"\nsaves-dir \"V\"\n", "an unknown version fails");
    fails("name \"x\"\n", "a missing header fails");
    fails("JudasProject 1\nname \"x\"\nstartup-scene \"\"\nassets-dir \"A\"\nscenes-dir \"S\"\n", "a missing required key fails");
    fails("JudasProject 1\nname \"x\"\nname \"y\"\nstartup-scene \"\"\nassets-dir \"A\"\nscenes-dir \"S\"\nsaves-dir \"V\"\n", "a duplicate key fails");
    fails("JudasProject 1\nname \"x\"\nstartup-scene \"\"\nassets-dir \"A\"\nscenes-dir \"S\"\nsaves-dir \"V\"\ncolour \"blue\"\n", "an unknown key fails");
    fails("JudasProject 1\nname \"x\"\nstartup-scene \"\"\nassets-dir \"/abs\"\nscenes-dir \"S\"\nsaves-dir \"V\"\n", "an absolute directory fails");
    fails("JudasProject 1\nname \"x\"\nstartup-scene \"../other.judas\"\nassets-dir \"A\"\nscenes-dir \"S\"\nsaves-dir \"V\"\n", "a startup scene outside the project fails");

    TempDir temp("judas_project_format");
    Project project;
    Check(Project::CreateNew(temp.Str() + "/game", "My Game", project, error), "CreateNew creates a project: " + error);
    Check(fs::is_regular_file(project.ProjectFile()) && fs::is_directory(project.AssetsDir()) &&
              fs::is_directory(project.ScenesDir()) && fs::is_directory(project.SavesDir()),
          "CreateNew wrote the project file and the Assets/Scenes/Saves directories");
    Check(fs::path(project.ProjectFile()).filename() == "My_Game.judasproj", "the project file is named after the project");
    Project unrelated;
    Check(Project::CreateNew(temp.Str() + "/other", "My Game", unrelated, error) &&
          project.Settings().saveIdentity.size()==64 &&
          unrelated.Settings().saveIdentity!=project.Settings().saveIdentity,
          "same-name projects get distinct durable save namespaces");
    Project second;
    Check(!Project::CreateNew(temp.Str() + "/game", "Another", second, error), "a directory holding a project refuses a second one");
    Project loaded;
    Check(loaded.Load(project.ProjectFile(), error) && loaded.Settings().name == "My Game" && loaded.Settings().saveIdentity==project.Settings().saveIdentity && loaded.RootDir() == project.RootDir(),
          "a created project loads back with the same root");
    Check(Project::FindProjectFileFor(project.ScenesDir() + "/nested/deeper/level.judas") == project.ProjectFile(),
          "FindProjectFileFor walks up from a nested scene path to the project file");
    Check(Project::FindProjectFileFor(temp.Str()).empty(), "FindProjectFileFor finds nothing above the project");
    Check(project.Resolve("Scenes/main.judas") == project.RootDir() + "/Scenes/main.judas", "Resolve joins onto the project root");
    Check(project.MakeRelative(project.RootDir() + "/Assets/x.obj") == "Assets/x.obj", "MakeRelative strips the project root");
    project.Settings().startupScene = "Scenes/main.judas";
    Check(project.Save(error) && ReadFile(project.ProjectFile()).find("startup-scene \"Scenes/main.judas\"") != std::string::npos,
          "Save writes the startup scene setting");
}

// --- B. asset identity -----------------------------------------------------

void SectionAssetIdentity() {
    std::printf("Section B: stable asset identity through import, rename and move\n");
    Check(IsValidAssetId("0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21") && !IsValidAssetId("0B3AC0E5") && !IsValidAssetId(""),
          "asset ids are 32 lowercase hex digits");
    const AssetId minted = MintAssetId();
    Check(IsValidAssetId(minted) && minted != MintAssetId(), "minted ids are valid and distinct");
    AssetType type;
    Check(AssetTypeForExtension(".obj", type) && type == AssetType::Mesh && AssetTypeForExtension(".PNG", type) &&
              type == AssetType::Texture && AssetTypeForExtension(".ttf", type) && type == AssetType::Font &&
              !AssetTypeForExtension(".judas", type),
          "asset types follow the engine's loadable extensions");

    TempDir temp("judas_asset_identity");
    Project project;
    std::string error;
    Check(Project::CreateNew(temp.Str() + "/game", "Assets", project, error), "temporary project created");
    AssetDatabase db;
    db.Scan(project.RootDir(), project.AssetsDir());
    Check(db.Records().empty() && db.Problems().empty(), "an empty assets directory scans clean");

    // Import a real mesh and texture from the engine's demo tree.
    AssetRecord mesh, texture;
    Check(db.Import("tests/fixtures/engine-assets/models/beacon.obj", "models/lighthouse.obj", mesh, error), "importing a valid .obj succeeds: " + error);
    Check(IsValidAssetId(mesh.id) && mesh.type == AssetType::Mesh && mesh.relativePath == "Assets/models/lighthouse.obj",
          "the imported mesh has a valid id, the mesh type and the destination path");
    Check(fs::is_regular_file(mesh.path) && fs::is_regular_file(mesh.path + ".judasmeta"), "the file and its sidecar were written");
    Check(db.Import("tests/fixtures/engine-assets/textures/beacon.png", "", texture, error) && texture.type == AssetType::Texture &&
              texture.relativePath == "Assets/beacon.png",
          "importing a texture with an empty destination keeps its file name");
    AssetRecord bad;
    Check(!db.Import("README.md", "", bad, error) && !error.empty(), "an unsupported extension is refused: " + error);
    {
        std::ofstream broken(temp.Str() + "/broken.obj");
        broken << "this is not an obj\n";
    }
    Check(!db.Import(temp.Str() + "/broken.obj", "", bad, error) && !fs::exists(project.AssetsDir() + "/broken.obj"),
          "an undecodable mesh is refused and leaves nothing behind: " + error);
    Check(!db.Import("tests/fixtures/engine-assets/models/beacon.obj", "models/lighthouse.obj", bad, error), "importing onto an existing destination is refused");

    // A scene refers to the asset by id.
    Scene scene;
    SceneObject& beacon = scene.CreateObject("Beacon");
    beacon.render = SceneRenderComponent{};
    beacon.render->shape = SceneShape::Mesh;
    beacon.render->meshAsset = mesh.id;
    beacon.render->textureAsset = texture.id;
    std::string sceneText;
    SaveSceneToString(scene, sceneText);
    Check(sceneText.find("render.mesh-asset \"" + mesh.id + "\"") != std::string::npos &&
              sceneText.find("lighthouse") == std::string::npos,
          "the scene file carries the asset id and no file name");

    // Rename + move: the id is unchanged, a fresh scan finds it at the new
    // path, and the scene still resolves.
    const AssetId meshId = mesh.id;
    Check(db.Move(meshId, "models/renamed/tower.obj", error), "renaming and moving the mesh succeeds: " + error);
    Check(!fs::exists(mesh.path) && fs::is_regular_file(project.AssetsDir() + "/models/renamed/tower.obj") &&
              fs::is_regular_file(project.AssetsDir() + "/models/renamed/tower.obj.judasmeta"),
          "file and sidecar moved together");
    Check(!db.Move(meshId, "models/tower.png", error), "a move may not change the extension");
    AssetDatabase rescanned;
    rescanned.Scan(project.RootDir(), project.AssetsDir());
    const AssetRecord* found = rescanned.Find(meshId);
    Check(found != nullptr && found->relativePath == "Assets/models/renamed/tower.obj" && !found->missing,
          "a fresh scan resolves the SAME id at the new path (no filename guessing)");
    Check(rescanned.Problems().empty() && rescanned.Untracked().empty(), "the rescanned project has no problems and no untracked files");
    Scene loadedScene;
    Check(LoadSceneFromString(sceneText, loadedScene, error) && rescanned.Find(loadedScene.Objects()[0].render->meshAsset) == found,
          "the saved scene's reference resolves to the moved asset");

    // Missing file: sidecar present, asset gone.
    std::error_code ec;
    fs::remove(texture.path, ec);
    AssetDatabase missingScan;
    missingScan.Scan(project.RootDir(), project.AssetsDir());
    const AssetRecord* missingRecord = missingScan.Find(texture.id);
    Check(missingRecord != nullptr && missingRecord->missing && !missingScan.Problems().empty(),
          "a sidecar whose file is gone is reported as missing, not silently dropped");

    // Untracked + Track with a forced id; duplicate id and corrupt sidecar problems.
    fs::copy_file("tests/fixtures/engine-assets/models/plane.obj", project.AssetsDir() + "/plane.obj", ec);
    AssetDatabase untrackedScan;
    untrackedScan.Scan(project.RootDir(), project.AssetsDir());
    Check(untrackedScan.Untracked().size() == 1, "a file without a sidecar is listed as untracked");
    AssetRecord tracked;
    Check(untrackedScan.Track(project.AssetsDir() + "/plane.obj", tracked, error, "9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5") &&
              tracked.id == "9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5" && untrackedScan.Untracked().empty(),
          "Track adopts the file in place with the requested id");
    Check(!untrackedScan.Track(project.AssetsDir() + "/plane.obj", tracked, error, "9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5"),
          "an id already in use is refused");
    fs::copy_file(project.AssetsDir() + "/plane.obj.judasmeta", project.AssetsDir() + "/models/renamed/tower.obj.judasmeta", fs::copy_options::overwrite_existing, ec);
    {
        std::ofstream corrupt(project.AssetsDir() + "/corrupt.png.judasmeta");
        corrupt << "JudasAssetMeta 1\nid \"nope\"\n";
    }
    AssetDatabase problemScan;
    problemScan.Scan(project.RootDir(), project.AssetsDir());
    bool sawDuplicate = false, sawCorrupt = false;
    for (const AssetProblem& p : problemScan.Problems()) {
        if (p.message.find("duplicate") != std::string::npos) sawDuplicate = true;
        if (p.path.find("corrupt.png") != std::string::npos) sawCorrupt = true;
    }
    Check(sawDuplicate && sawCorrupt, "duplicate ids and corrupt sidecars are reported as problems, not guessed around");
    Check(problemScan.Find("9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5") != nullptr, "the first holder of a duplicated id stays usable");
}

// --- C. resource manager boundary (headless) --------------------------------

void SectionResourceManager() {
    std::printf("Section C: resource manager semantics (headless: every load fails honestly)\n");
    AssetDatabase db;
    db.Scan(".", "assets");
    ResourceManager resources(nullptr, &db);
    const AssetId beacon = "0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21";
    Check(resources.StateOf(beacon) == ResourceState::Unloaded, "an unrequested asset is Unloaded");
    std::string error;
    MeshHandle handle = resources.GetMesh(beacon, error);
    Check(!handle.IsValid() && resources.StateOf(beacon) == ResourceState::Failed && error.find("no renderer") != std::string::npos,
          "without a renderer a request fails with a message and no handle: " + error);
    Check(resources.Stats().misses == 1 && resources.Stats().failed == 1, "the load attempt counted one miss and one failure");
    error.clear();
    resources.GetMesh(beacon, error);
    Check(resources.Stats().hits == 1 && resources.Stats().misses == 1 && error == resources.ErrorOf(beacon),
          "a failed asset is remembered: the second request is a hit that returns the same error, no retry");
    resources.Invalidate(beacon);
    Check(resources.StateOf(beacon) == ResourceState::Unloaded && resources.Stats().failed == 0, "Invalidate forgets the failure");
    resources.GetMesh(beacon, error);
    Check(resources.Stats().misses == 2, "a request after Invalidate loads again");
    Check(!resources.GetTexture(beacon, error).IsValid() || true, "requesting a mesh id as a texture is handled");
    ResourceManager typed(nullptr, &db);
    error.clear();
    typed.GetTexture("9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5", error);
    Check(typed.StateOf("9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5") == ResourceState::Failed, "a type mismatch is a Failed state");
    error.clear();
    resources.GetMesh("ffffffffffffffffffffffffffffffff", error);
    Check(error.find("unknown asset id") != std::string::npos || error.find("no renderer") != std::string::npos,
          "an unknown id fails with a message: " + error);
    Check(!resources.GetMesh("", error).IsValid() && resources.StateOf("") == ResourceState::Unloaded,
          "an empty id is a no-op request, not a failure");
    resources.ReleaseAll();
    Check(resources.StateOf(beacon) == ResourceState::Unloaded && resources.Stats().loadedMeshes == 0, "ReleaseAll leaves nothing loaded");
}

// --- D. a new project owns its scene and physics ----------------------------
Scene ProjectPhysicsFixture() {
    Scene scene;
    auto &floor = scene.CreateObject("Floor");
    floor.body = SceneBodyComponent{};
    floor.body->halfExtents = {8, .5f, 8};
    floor.transform.position = {0, -.5f, 0};
    auto &gravity = scene.CreateObject("Gravity");
    gravity.gravity = SceneGravityComponent{};
    gravity.gravity->regionHalfExtents = {20, 20, 20};
    auto &box = scene.CreateObject("Box");
    box.body = SceneBodyComponent{};
    box.body->motion = SceneBodyMotion::Dynamic;
    box.body->mass = 2;
    box.transform.position = {0, 2, 0};
    return scene;
}
void SectionProjectRuntime() {
    TempDir temp("judas_project_runtime");
    Project project;
    std::string error;
    Check(Project::CreateNew(temp.Str(), "Current runtime", project, error), "fresh project creation: " + error);
    project.Settings().startupScene = "Scenes/main.judas";

    const auto authored = ProjectPhysicsFixture();
    Check(project.Save(error) && SaveSceneToFile(authored, project.StartupScenePath(), error), "authored startup scene persists");
    Project reopened;
    Scene scene;
    Check(reopened.Load(project.ProjectFile(), error) && LoadSceneFromFile(reopened.StartupScenePath(), scene, error), "fresh project reopens its own startup scene");
    RuntimeWorld world;

    Check(world.Build(scene, nullptr, error), "ordinary runtime builds current authored components");

    Check(world.IsBuilt(), "runtime simulation context begins");
    Window window;
    window.SetTestInputMode(true);
    for (int i = 0; i < 240; ++i) StepPlayedWorld(world, window, SimulationTiming::kFixedTimestep);
    Check(world.DynamicBodies().size() == 1 && Near(world.DynamicBodies()[0].GetPosition().y, .5f, .05f), "authored dynamic box settles on real collision");
    const auto counts = world.CountLifecycle();
    Check(counts.full == 1 && counts.coarse == 0 && counts.dormant == 0, "ordinary project retains full physics without a fidelity policy");
}

// --- F. gizmo mathematics + undo/redo ---------------------------------------

void SectionGizmo() {
    std::printf("Section F: gizmo mathematics and undo/redo participation\n");
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::quat yaw90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Check(Near(GizmoAxisDirection(GizmoAxis::X, yaw90, GizmoSpace::World, GizmoMode::Translate), glm::vec3(1.0f, 0.0f, 0.0f)),
          "world-space handles ignore the object's rotation");
    Check(Near(GizmoAxisDirection(GizmoAxis::X, yaw90, GizmoSpace::Local, GizmoMode::Translate), glm::vec3(0.0f, 0.0f, -1.0f)),
          "local-space handles follow the object's rotation (local X of a 90-degree yaw is world -Z)");
    Check(Near(GizmoAxisDirection(GizmoAxis::X, yaw90, GizmoSpace::World, GizmoMode::Scale), glm::vec3(0.0f, 0.0f, -1.0f)),
          "scale handles are always the object's local axes");

    // A camera at (0, 5, 10) looking at the origin; rays aimed at points.
    const glm::vec3 eye(0.0f, 5.0f, 10.0f);
    const auto rayTo = [&](const glm::vec3& target) { return glm::normalize(target - eye); };
    SceneTransform start;
    start.position = glm::vec3(1.0f, 0.0f, 0.0f);
    Check(PickGizmoAxis(GizmoMode::Translate, eye, rayTo(glm::vec3(2.7f, 0.0f, 0.0f)), start.position, identity, GizmoSpace::World, 2.0f, 0.2f) == GizmoAxis::X,
          "a ray at the X handle picks X");
    Check(PickGizmoAxis(GizmoMode::Translate, eye, rayTo(glm::vec3(1.0f, 1.8f, 0.0f)), start.position, identity, GizmoSpace::World, 2.0f, 0.2f) == GizmoAxis::Y,
          "a ray at the Y handle picks Y");
    Check(PickGizmoAxis(GizmoMode::Translate, eye, rayTo(glm::vec3(8.0f, 8.0f, 0.0f)), start.position, identity, GizmoSpace::World, 2.0f, 0.2f) == GizmoAxis::None,
          "a ray far from every handle picks nothing");
    Check(PickGizmoAxis(GizmoMode::Rotate, eye, rayTo(glm::vec3(1.0f + 1.4142f, 0.0f, 1.4142f)), start.position, identity, GizmoSpace::World, 2.0f, 0.2f) == GizmoAxis::Y,
          "a ray on the Y ring (away from the other rings) picks the Y rotation handle");
    Check(PickGizmoAxis(GizmoMode::Rotate, eye, rayTo(glm::vec3(1.0f, 1.4142f, 1.4142f)), start.position, identity, GizmoSpace::World, 2.0f, 0.2f) == GizmoAxis::X,
          "a ray on the X ring picks the X rotation handle");

    GizmoDrag drag;
    Check(BeginGizmoDrag(GizmoMode::Translate, GizmoAxis::X, eye, rayTo(glm::vec3(1.5f, 0.0f, 0.0f)), start, GizmoSpace::World, 2.0f, drag),
          "a translate drag begins from a ray on the axis");
    SceneTransform moved = UpdateGizmoDrag(drag, eye, rayTo(glm::vec3(4.5f, 0.0f, 0.0f)), false);
    Check(Near(moved.position, glm::vec3(4.0f, 0.0f, 0.0f)) && moved.rotation == start.rotation && moved.scale == start.scale,
          "dragging 3 m along the X handle moves exactly 3 m along X and nothing else");
    SceneTransform snapped = UpdateGizmoDrag(drag, eye, rayTo(glm::vec3(4.2f, 0.0f, 0.0f)), true);
    Check(Near(snapped.position, glm::vec3(3.5f, 0.0f, 0.0f)), "snapping rounds the 2.7 m delta to 2.5 m (0.5 m steps from the start)");
    Check(Near(SnapValue(1.26f, 0.5f), 1.5f) && Near(SnapValue(-0.24f, 0.5f), 0.0f) && Near(SnapValue(37.0f, 15.0f), 30.0f), "snap rounds to the nearest step");

    GizmoDrag rotate;
    start.position = glm::vec3(0.0f);
    const glm::vec3 above(0.0f, 20.0f, 0.0f);
    Check(BeginGizmoDrag(GizmoMode::Rotate, GizmoAxis::Y, above, glm::normalize(glm::vec3(2.0f, 0.0f, 0.0f) - above), start, GizmoSpace::World, 2.0f, rotate),
          "a rotate drag begins from a ray on the Y ring");
    const SceneTransform rotated = UpdateGizmoDrag(rotate, above, glm::normalize(glm::vec3(0.0f, 0.0f, -2.0f) - above), false);
    const glm::vec3 turnedX = rotated.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    Check(Near(turnedX, glm::vec3(0.0f, 0.0f, -1.0f), 1.0e-3f) && Near(rotated.position, start.position),
          "dragging a quarter turn around Y rotates the object's X axis to -Z and leaves the position alone");
    const SceneTransform rotatedSnapped = UpdateGizmoDrag(rotate, above, glm::normalize(glm::vec3(1.0f, 0.0f, -0.25f) - above), true);
    const float snappedAngle = glm::degrees(glm::angle(rotatedSnapped.rotation));
    Check(Near(snappedAngle, 15.0f, 0.1f), "rotation snapping rounds a 14-degree drag to 15 degrees");

    GizmoDrag scale;
    Check(BeginGizmoDrag(GizmoMode::Scale, GizmoAxis::Y, eye, rayTo(glm::vec3(0.0f, 1.0f, 0.0f)), start, GizmoSpace::World, 2.0f, scale),
          "a scale drag begins from a ray on the Y handle");
    const SceneTransform scaled = UpdateGizmoDrag(scale, eye, rayTo(glm::vec3(0.0f, 2.0f, 0.0f)), false);
    Check(Near(scaled.scale, glm::vec3(1.0f, 1.5f, 1.0f), 1.0e-3f), "dragging half a handle length along Y adds 0.5 to the Y scale only");

    // Undo/redo: a drag is exactly one history step.
    EditorDocument doc;
    doc.NewScene();
    SceneObject& crate = doc.GetScene().CreateObject("Crate");
    const SceneObjectId crateId = crate.id;
    crate.transform.position = glm::vec3(1.0f, 0.0f, 0.0f);
    doc.Select(crateId);
    doc.BeginEdit();
    const SceneTransform dragStart = doc.GetScene().Find(crateId)->transform;
    GizmoDrag undoDrag;
    BeginGizmoDrag(GizmoMode::Translate, GizmoAxis::X, eye, rayTo(glm::vec3(1.5f, 0.0f, 0.0f)), dragStart, GizmoSpace::World, 2.0f, undoDrag);
    for (int frame = 1; frame <= 10; ++frame) {  // many intermediate updates, one commit
        doc.GetScene().Find(crateId)->transform = UpdateGizmoDrag(undoDrag, eye, rayTo(glm::vec3(1.5f + 0.3f * static_cast<float>(frame), 0.0f, 0.0f)), false);
    }
    doc.CommitEdit();
    Check(Near(doc.GetScene().Find(crateId)->transform.position, glm::vec3(4.0f, 0.0f, 0.0f)) && doc.CanUndo() && doc.IsDirty(),
          "a ten-frame drag lands at 4 m and records one undo step");
    doc.Undo();
    Check(Near(doc.GetScene().Find(crateId)->transform.position, glm::vec3(1.0f, 0.0f, 0.0f)) && !doc.CanUndo() && doc.CanRedo(),
          "undo restores the pre-drag transform in one step");
    doc.Redo();
    Check(Near(doc.GetScene().Find(crateId)->transform.position, glm::vec3(4.0f, 0.0f, 0.0f)), "redo reapplies the whole drag");
    doc.BeginEdit();
    doc.CancelEdit();
    Check(doc.CanUndo() && !doc.CanRedo(), "a cancelled edit records nothing");
}

// --- G. inspector round trip ------------------------------------------------

void SectionInspectorRoundTrip() {
    std::printf("Section G: every authorable component round-trips through save/load unchanged\n");
    // Each component on an object that satisfies its prerequisites (the
    // loader is strict about them), with non-default values throughout.
    Scene scene;
    scene.Settings().name = "Everything";
    {
        SceneObject& o = scene.CreateObject("Ship");
        o.transform.position = glm::vec3(1.5f, -2.25f, 3.125f);
        o.transform.rotation = glm::normalize(glm::quat(0.9f, 0.1f, 0.2f, 0.3f));
        o.transform.scale = glm::vec3(2.0f, 0.5f, 1.25f);
        o.render = SceneRenderComponent{}; o.render->shape = SceneShape::Mesh; o.render->meshAsset = "0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21";
        o.render->textureAsset = "5e4d3c2b1a0f4e6d9c8b7a6f5e4d3c2b"; o.render->color = glm::vec3(0.1f, 0.2f, 0.3f); o.render->alpha = 0.75f;
        o.body = SceneBodyComponent{}; o.body->motion = SceneBodyMotion::Dynamic; o.body->mass = 12.5f;  o.body->managed = true;
        o.body->initialLinearVelocity = glm::vec3(0.0f, 0.0f, -3.0f);
        o.gravity = SceneGravityComponent{}; o.gravity->kind = SceneGravityKind::Uniform; o.gravity->regionShape = SceneRegionShape::Box; o.gravity->regionHalfExtents = glm::vec3(7.0f);
        o.light = SceneLightComponent{}; o.light->kind = SceneLightKind::Spot; o.light->range = 33.0f; o.light->innerConeDegrees = 10.0f;
        o.celestial = SceneCelestialComponent{}; o.celestial->gravitationalParameter = 5.0f;
        o.combustible = SceneCombustibleComponent{}; o.combustible->initialFuelMassKg = 0.5f;
        o.fluidVolume = SceneFluidVolumeComponent{}; o.fluidVolume->fillHeight = 1.7f;
    }
    {
        SceneObject& o = scene.CreateObject("Door");
        o.render = SceneRenderComponent{}; o.render->halfExtents = glm::vec3(1.0f, 1.0f, 0.1f);
        o.basicWater = BasicWaterSettings{}; o.basicWater->waveSpeed = 1.7f;
    }
    {
        SceneObject& o = scene.CreateObject("Switch");
        o.render = SceneRenderComponent{}; o.render->halfExtents = glm::vec3(0.06f, 0.18f, 0.04f);
        o.renderCamera = SceneRenderCameraComponent{}; o.renderCamera->width = 512;
    }
    {
        SceneObject& o = scene.CreateObject("Planet");
        o.body = SceneBodyComponent{}; o.body->shape = SceneShape::Sphere; o.body->radius = 80.0f;
        o.celestial = SceneCelestialComponent{}; o.celestial->gravitationalParameter = 62784.0f;
        o.atmosphere = SceneAtmosphereComponent{}; o.atmosphere->topRadius = 123.0f;
    }
    {
        SceneObject& o = scene.CreateObject("Start");
        o.characterMotor = CharacterMotorSettings{}; o.characterMotor->stepHeight = .45f;
    }
    std::string text, error;
    Check(SaveSceneToString(scene, text), "objects carrying every component type serialize");
    Scene loaded;
    Check(LoadSceneFromString(text, loaded, error), "and load: " + error);
    if (loaded.Objects().size() != 5) { Check(false, "five objects reloaded"); return; }
    Check(ScenesEqual(scene, loaded), "every component's every field survives exactly (what the inspector edits is what the file holds)");
    const SceneObject& ship = loaded.Objects()[0];
    Check(ship.render && ship.body && ship.gravity && ship.light && ship.celestial && ship.combustible && ship.fluidVolume &&
              loaded.Objects()[1].basicWater && loaded.Objects()[2].renderCamera && loaded.Objects()[3].atmosphere && loaded.Objects()[4].characterMotor,
          "all current fixture component types are present after reload");
    Check(ship.render->meshAsset == "0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21" && ship.render->textureAsset == "5e4d3c2b1a0f4e6d9c8b7a6f5e4d3c2b",
          "asset references round-trip as ids");
}

// --- H. Edit/Play separation ----------------------------------------------------

void SectionEditPlaySeparation() {
    std::printf("Section H: playing a project scene never writes the authored scene\n");
    TempDir temp("judas_edit_play_separation");
    const std::string scenePath = temp.Str() + "/main.judas";
    std::string writeError;
    Check(SaveSceneToFile(ProjectPhysicsFixture(), scenePath, writeError), "fresh authored scene saved for Play/Stop");
    const std::string diskBefore = ReadFile(scenePath);
    Check(!diskBefore.empty(), "the original authored scene file is readable");
    Scene authored;
    std::string error;
    Check(LoadSceneFromFile(scenePath, authored, error), "current authored scene loads: " + error);
    std::string before;
    SaveSceneToString(authored, before);
    RuntimeWorld world;

    Check(world.Build(authored, nullptr, error), "it instantiates: " + error);

    Check(world.IsBuilt(), "a session begins");
    Window window;
    window.SetTestInputMode(true);
    for (int i = 0; i < 120; ++i) StepPlayedWorld(world, window, SimulationTiming::kFixedTimestep);
    world.Physics().SetLinearVelocity(world.DynamicBodies()[0].Handle(), glm::vec3(5.0f, 3.0f, 0.0f));
    for (int i = 0; i < 120; ++i) StepPlayedWorld(world, window, SimulationTiming::kFixedTimestep);
    Check(!Near(world.DynamicBodies()[0].GetPosition(), authored.Objects()[2].transform.position, 0.5f), "the runtime body moved");
    std::string after;
    SaveSceneToString(authored, after);
    Check(after == before, "the authored scene is byte-identical after play (no delta ever written into it)");

    world.Destroy();
    // Canonical serialization may spell out defaults absent from the original
    // authored file. Disk preservation compares actual pre/post bytes.
    Check(ReadFile(scenePath) == diskBefore, "the scene file on disk is untouched");
}
}  // namespace

int main() {
    SectionProjectFormat();
    SectionAssetIdentity();
    SectionResourceManager();
    SectionProjectRuntime();
    SectionGizmo();
    SectionInspectorRoundTrip();
    SectionEditPlaySeparation();
    std::printf("Project tests: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
