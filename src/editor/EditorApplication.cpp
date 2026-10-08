#include "Ragdoll.h"
#include "ModelCook.h"
#include <glm/gtc/matrix_transform.hpp>
#include "WorldStreaming.h"
#include "Prefab.h"
#include "PerformanceProfiler.h"
#include "ComponentEditors.h"
#include "EditorApplication.h"
#include "Prefab.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include <glm/gtc/quaternion.hpp>

#include "EngineHost.h"
#include "EnginePaths.h"
#include "PlatformServices.h"
#include "ProjectExporter.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "TerrainLibrary.h"
#include "WorldCoordinates.h"
#include "WorldDebugView.h"
#include "WorldPresentation.h"
#include "WorldState.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

namespace fs = std::filesystem;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr const char* kProjectFileHint = "Relative to the working directory or absolute, e.g. projects/tiny_game/tiny_game.judasproj";

// Viewport picking uses a bounding SPHERE per object — generous for
// empties and lights so they can still be clicked, and deliberately
// approximate: a long thin plank picks as a big ball. Precise mesh/box
// picking is a documented M30 limitation (docs/ARCHITECTURE.md).
float PickRadius(const SceneObject& o) {
    float radius = 0.75f;
    if (o.render) {
        if (o.render->shape == SceneShape::Sphere) radius = std::max(radius, o.render->radius);
        else if (o.render->shape == SceneShape::Box) radius = std::max(radius, glm::length(o.render->halfExtents));
        else if (o.render->shape == SceneShape::Mesh) radius = std::max(radius, 2.0f * std::max(o.transform.scale.x, std::max(o.transform.scale.y, o.transform.scale.z)));
    }
    if (o.body) {
        if (o.body->shape == SceneShape::Sphere) radius = std::max(radius, o.body->radius);
        else if (o.body->shape == SceneShape::Box) radius = std::max(radius, glm::length(o.body->halfExtents));
        else if (o.body->shape == SceneShape::Terrain) radius = std::max(radius, 90.0f);
    }
    if (o.gravity) {
        radius = std::max(radius, o.gravity->regionShape == SceneRegionShape::Sphere ? o.gravity->regionRadius * 0.1f
                                                                                    : glm::length(o.gravity->regionHalfExtents) * 0.1f);
    }
    return radius;
}

bool RaySphere(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& centre, float radius,
               float& outDistance) {
    const glm::vec3 oc = origin - centre;
    const float b = glm::dot(oc, direction);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return false;
    const float t = -b - std::sqrt(discriminant);
    if (t < 0.0f) return false;
    outDistance = t;
    return true;
}

// The selection outline: the object's own shape where it has one.
void BuildSelectionLines(const SceneObject& o, DebugLineList& out) {
    const glm::vec3 color(1.0f, 0.85f, 0.2f);
    const glm::vec3 position = o.transform.position;
    const glm::quat rotation = glm::normalize(o.transform.rotation);
    bool drawn = false;
    if (o.render) {
        if (o.render->shape == SceneShape::Box) { out.Box(position, rotation, o.render->halfExtents * 1.02f, color); drawn = true; }
        else if (o.render->shape == SceneShape::Sphere) { out.Sphere(position, o.render->radius * 1.02f, color); drawn = true; }
    }
    if (!drawn && o.body) {
        if (o.body->shape == SceneShape::Box) { out.Box(position, rotation, o.body->halfExtents * 1.02f, color); drawn = true; }
        else if (o.body->shape == SceneShape::Sphere) { out.Sphere(position, o.body->radius * 1.02f, color); drawn = true; }
        else if (o.body->shape == SceneShape::Compound) {
            for (const CompoundBox& box : o.body->compoundBoxes) out.Box(position + rotation * box.localCenter, rotation, box.halfExtents * 1.02f, color);
            drawn = true;
        }
    }
    if (!drawn) out.Sphere(position, PickRadius(o), color, 16);
}

Scene MakeStarterScene() {
    Scene scene;
    scene.Settings().name = "Main";
    SceneObject& ground = scene.CreateObject("Ground");
    ground.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
    ground.render = SceneRenderComponent{};
    ground.render->shape = SceneShape::Box;
    ground.render->halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    ground.render->color = glm::vec3(0.4f, 0.44f, 0.36f);
    ground.body = SceneBodyComponent{};
    ground.body->shape = SceneShape::Box;
    ground.body->halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    ground.body->friction = 0.8f;
    ground.body->restitution = 0.05f;
    ground.gravity = SceneGravityComponent{};
    ground.gravity->kind = SceneGravityKind::Uniform;
    ground.gravity->regionShape = SceneRegionShape::Box;
    ground.gravity->regionHalfExtents = glm::vec3(30.0f);
    SceneObject& start = scene.CreateObject("Player start");
    start.transform.position = glm::vec3(0.0f, 1.0f, 5.0f);
    start.playerStart = ScenePlayerStartComponent{};
    SceneObject& lamp = scene.CreateObject("Lamp");
    lamp.transform.position = glm::vec3(0.0f, 4.0f, 0.0f);
    lamp.light = SceneLightComponent{};
    lamp.light->color = glm::vec3(2.5f, 2.3f, 2.0f);
    lamp.light->range = 18.0f;
    return scene;
}
}  // namespace

// Opt-in application integration fixture. Requests use the editor's ordinary
// authoring operations; Play advances only through InteractivePlay::Frame in
// the normal loop below. No test-specific simulation or resource path exists.
struct EditorApplication::StabilizationAutomation {
    std::string output;
    std::string baseline;
    std::string fingerprint;
    std::string assetId;
    std::string assetPath;
    std::string saveBytes;
    SceneObjectId moving = 0, removed = 0, coarse = 0, mesh = 0;
    EntityId created = 0;
    EntityPhysicalState dormantState, coarseBefore;
    std::size_t playSteps = 0, phaseStep = 0;
    unsigned int phase = 0, frames = 0, checks = 0, failures = 0;

    bool Check(bool ok, const char* name) {
        ++checks;
        failures += ok ? 0u : 1u;
        std::fprintf(stderr, "FTFT6 CHECK %s %s\n", name, ok ? "PASS" : "FAIL");
        return ok;
    }
};

void EditorApplication::AdvanceStabilizationAutomation() {
    if (!m_stabilization) return;
    StabilizationAutomation& a = *m_stabilization;
    const auto check = [&](bool ok, const char* label) {
        if (!a.Check(ok, label)) m_quit = true;
        return ok;
    };
    const auto request = [&](EditorRequests r) { HandleRequests(r); };
    const auto sceneText = [&]() { std::string text; SaveSceneToString(m_document.GetScene(), text); return text; };
    const auto read = [](const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto handle = [&](EntityId id) { return m_world->DynamicBodies()[m_world->FindEntity(id)->slot].Handle(); };
    const auto sameState = [](const EntityPhysicalState& x, const EntityPhysicalState& y) {
        return x.position == y.position && x.rotation == y.rotation &&
               x.linearVelocity == y.linearVelocity && x.angularVelocity == y.angularVelocity;
    };
    std::string error;
    std::error_code ec;
    if (++a.frames > 30000) { check(false, "progress_watchdog"); return; }
    if (a.phase == 0) {
        if (!check(!m_host->Resources().BlockingMode(), "normal_async_mode")) return;
        const fs::path root = fs::path(a.output) / "project";
        if (!check(!fs::exists(root), "fresh_generated_project")) return;
        fs::create_directories(a.output, ec);
        if (!check(!ec && CreateProject(root.string(), "FTFT6 Tiny", error), "create_project")) return;
        if (!check(!m_document.GetScene().Objects().empty() && m_project.Settings().startupScene == "Scenes/main.judas",
                   "starter_scene_and_project_startup")) return;
        bool ordinary = true;
        for (const SceneObject& o : m_document.GetScene().Objects())
            ordinary = ordinary && !o.celestial && !o.atmosphere && !o.fluidVolume;
        if (!check(ordinary, "non_planetary_project")) return;
        for (int k = 0; k < 3; ++k) {
            EditorRequests r; r.createKind = "dynamic-box"; r.createPosition = glm::vec3(-6.0f + 4.0f * k, 8.0f, 0.0f);
            request(r);
            if (k == 0) a.moving = m_document.Selected();
            if (k == 1) a.removed = m_document.Selected();
            if (k == 2) a.coarse = m_document.Selected();
        }
        if (!check(a.moving && a.removed && a.coarse, "create_authored_entities")) return;
        const std::string before = sceneText();
        m_document.BeginEdit();
        m_document.GetScene().Find(a.moving)->body->initialLinearVelocity = glm::vec3(0.75f, 0.0f, 0.0f);
        m_document.GetScene().Find(a.coarse)->body->initialLinearVelocity = glm::vec3(-0.5f, 0.0f, 0.0f);
        m_document.CommitEdit();
        const std::string edited = sceneText();
        EditorRequests r; r.undo = true; request(r);
        if (!check(sceneText() == before, "undo_component_edit")) return;
        r = {}; r.redo = true; request(r);
        if (!check(sceneText() == edited && edited != before, "redo_component_edit")) return;
        r = {}; r.duplicateId = a.moving; request(r);
        const SceneObjectId duplicate = m_document.Selected();
        if (!check(duplicate != a.moving && m_document.GetScene().Find(duplicate), "duplicate_entity")) return;
        // Same document transaction as the editor's Delete key.
        m_document.BeginEdit(); m_document.GetScene().DestroyObject(duplicate); m_document.CommitEdit();
        if (!check(!m_document.GetScene().Find(duplicate), "delete_authored_entity")) return;
        r = {}; r.undo = true; request(r);
        if (!check(m_document.GetScene().Find(duplicate) != nullptr, "undo_delete")) return;
        r = {}; r.redo = true; request(r);
        if (!check(!m_document.GetScene().Find(duplicate), "redo_delete")) return;
        const fs::path source = fs::path(a.output) / "import.obj";
        { std::ofstream file(source); file << "v -1 0 0\nv 1 0 0\nv 0 2 0\nf 1 2 3\n"; }
        r = {}; r.importSource = source.string(); r.importDestination = "models/original.obj"; request(r);
        a.assetId = m_panels.browserSelection;
        const AssetRecord* imported = m_host->Assets().Find(a.assetId);
        if (!check(imported && !imported->missing, "import_real_obj")) return;
        a.assetPath = imported->path;
        r = {}; r.dropMeshAssetId = a.assetId; r.createPosition = glm::vec3(4.0f, 0.0f, -3.0f); request(r);
        a.mesh = m_document.Selected();
        if (!check(m_document.GetScene().Find(a.mesh)->render->meshAsset == a.assetId, "authored_stable_asset_reference")) return;
        a.phase = 1;
    } else if (a.phase == 1) {
        if (m_host->Resources().StateOf(a.assetId) != ResourceState::Ready) return;
        if (!check(m_host->Resources().TryGetMesh(a.assetId).IsValid() &&
                   m_host->Resources().DecodeThreadOf(a.assetId) != m_host->Resources().OwnerThread(), "async_decode_and_real_gpu_mesh")) return;
        EditorRequests r; r.moveAssetId = a.assetId; r.moveAssetTo = "renamed/moved.obj"; request(r);
        const AssetRecord* moved = m_host->Assets().Find(a.assetId);
        if (!check(moved && moved->path != a.assetPath && fs::exists(moved->path) && !fs::exists(a.assetPath) &&
                   m_document.GetScene().Find(a.mesh)->render->meshAsset == a.assetId, "move_preserves_identity_and_reference")) return;
        a.assetPath = moved->path;
        fs::rename(a.assetPath, a.assetPath + ".missing", ec);
        if (!check(!ec, "prepare_missing_asset")) return;
        r = {}; r.rescanAssets = true; request(r);
        const AssetRecord* missing = m_host->Assets().Find(a.assetId);
        if (!check(missing && missing->missing && !m_host->Assets().Problems().empty(), "missing_asset_database_diagnostic")) return;
        a.phase = 2;
    } else if (a.phase == 2) {
        if (m_host->Resources().StateOf(a.assetId) != ResourceState::Failed) return;
        if (!check(!m_host->Resources().ErrorOf(a.assetId).empty() && !m_host->Resources().TryGetMesh(a.assetId).IsValid(),
                   "missing_asset_resource_failure")) return;
        fs::rename(a.assetPath + ".missing", a.assetPath, ec);
        if (!check(!ec, "restore_missing_asset")) return;
        const fs::path duplicate = fs::path(m_project.AssetsDir()) / "duplicate.obj";
        fs::copy_file(a.assetPath, duplicate, fs::copy_options::none, ec);
        if (!check(!ec, "prepare_duplicate_asset")) return;
        fs::copy_file(a.assetPath + kAssetMetaExtension, duplicate.string() + kAssetMetaExtension, fs::copy_options::none, ec);
        if (!check(!ec, "prepare_duplicate_metadata")) return;
        EditorRequests r; r.rescanAssets = true; request(r);
        bool reported = false;
        for (const AssetProblem& problem : m_host->Assets().Problems())
            reported = reported || problem.message.find("duplicate asset id") != std::string::npos;
        if (!check(reported && m_host->Assets().Records().size() == 1, "duplicate_metadata_reported_without_second_identity")) return;
        fs::remove(duplicate, ec); fs::remove(duplicate.string() + kAssetMetaExtension, ec);
        r = {}; r.rescanAssets = true; request(r);
        if (!check(m_host->Assets().Problems().empty() && m_host->Assets().Find(a.assetId)->path == a.assetPath,
                   "asset_repair_recovers_original_identity")) return;
        a.phase = 3;
    } else if (a.phase == 3) {
        if (m_host->Resources().StateOf(a.assetId) != ResourceState::Ready) return;
        if (!check(m_host->Resources().TryGetMesh(a.assetId).IsValid(), "resource_recovers_after_rescan")) return;
        if (!check(m_document.SaveAs(m_project.Resolve("Scenes/edited.judas"), error), "save_authored_scene_as")) return;
        m_project.Settings().startupScene = "Scenes/edited.judas";
        EditorRequests r; r.saveProject = true; request(r);
        a.baseline = sceneText();
        if (!check(ComputeSceneFingerprint(m_document.GetScene(), a.fingerprint, error), "authored_fingerprint")) return;
        const std::string projectFile = m_project.ProjectFile();
        if (!check(OpenProject(projectFile, error) && OpenScene(m_project.Settings().startupScene, error), "reopen_project_startup_scene")) return;
        if (!check(sceneText() == a.baseline && !m_document.IsDirty() && m_project.Settings().startupScene == "Scenes/edited.judas",
                   "save_reopen_preserves_authored_data")) return;
        r = {}; r.play = true; request(r);
        if (!check(m_play && m_world && m_panels.mode == EditorMode::Play, "play_builds_real_runtime")) return;
        if (!check(!m_world->HasFluid() && m_world->CelestialParticipants().empty() && !m_world->GetAtmosphere(),
                   "play_requires_no_planetary_systems")) return;
        a.phase = 4;
    } else if (a.phase == 4) {
        if (m_play->FixedStepsSinceReset() < 24) return;
        a.playSteps = m_play->FixedStepsSinceReset();
        EntityPhysicalState moved;
        if (!check(m_world->GetEntityState(a.moving, moved) && moved.position.x > -6.0f && moved.position.y < 8.0f &&
                   m_panels.profiler.drawCalls > 0, "ordinary_loop_advances_and_renders_scene")) return;
        const BodyHandle old = handle(a.moving);
        if (!check(m_world->SetEntityFidelity(a.moving, SimulationFidelity::Dormant, &error) &&
                   m_world->FindEntity(a.moving)->lifecycle == EntityLifecycle::Unloaded && !m_world->Physics().IsDynamicBody(old),
                   "unload_releases_physics_preserves_identity")) return;
        if (!check(m_world->SetEntityFidelity(a.moving, SimulationFidelity::Full, &error) && handle(a.moving).id != old.id &&
                   !m_world->Physics().IsDynamicBody(old), "reconstruction_invalidates_stale_handle")) return;
        EntityPhysicalState reconstructed; m_world->GetEntityState(a.moving, reconstructed);
        if (!check(sameState(moved, reconstructed), "reconstruction_preserves_moving_state")) return;
        if (!check(m_world->SetEntityFidelity(a.moving, SimulationFidelity::Dormant, &error) &&
                   m_world->GetEntityState(a.moving, a.dormantState), "return_to_dormant")) return;
        if (!check(m_world->SetEntityFidelity(a.coarse, SimulationFidelity::Coarse, &error) &&
                   m_world->FindEntity(a.coarse)->lifecycle == EntityLifecycle::Active &&
                   m_world->GetEntityState(a.coarse, a.coarseBefore), "coarse_remains_active")) return;
        if (!check(m_world->DestroyEntity(a.removed, &error) &&
                   m_world->FindEntity(a.removed)->lifecycle == EntityLifecycle::Destroyed &&
                   !m_world->SetEntityFidelity(a.removed, SimulationFidelity::Full, &error), "destroy_is_permanent")) return;
        SceneObject definition = *m_document.GetScene().Find(a.removed);
        definition.id = 0; definition.name = "Runtime created"; definition.transform.position = glm::vec3(10.0f, 6.0f, 0.0f);
        a.created = m_world->CreateEntity(definition, nullptr, &error);
        if (!check(a.created >= kRuntimeEntityIdBase && m_world->FindEntity(a.created), "create_runtime_entity")) return;
        a.phaseStep = m_play->FixedStepsSinceReset();
        a.phase = 5;
    } else if (a.phase == 5) {
        if (m_play->FixedStepsSinceReset() < a.phaseStep + 12) return;
        a.playSteps = m_play->FixedStepsSinceReset();
        EntityPhysicalState dormant, coarse;
        if (!check(m_world->GetEntityState(a.moving, dormant) && sameState(dormant, a.dormantState), "dormant_does_not_simulate")) return;
        if (!check(m_world->GetEntityState(a.coarse, coarse) && coarse.position != a.coarseBefore.position &&
                   m_world->FindEntity(a.coarse)->coarseStepsSimulated >= 12, "coarse_simulates_in_real_loop")) return;
        EditorRequests r; r.saveWorldState = true; request(r);
        a.saveBytes = read(m_play->WorldStatePath());
        if (!check(!a.saveBytes.empty(), "save_mixed_runtime_deltas")) return;
        r = {}; r.stop = true; request(r);
        if (!check(!m_world && !m_play && sceneText() == a.baseline, "stop_restores_exact_authored_scene")) return;
        r = {}; r.play = true; request(r);
        if (!check(m_world && m_play, "play_reloads_saved_world")) return;
        EntityPhysicalState restored;
        if (!check(m_world->GetEntityState(a.moving, restored) && sameState(restored, a.dormantState) &&
                   m_world->FindEntity(a.moving)->id == a.moving && m_world->FindEntity(a.moving)->fidelity == SimulationFidelity::Full,
                   "delta_reconstructs_identity_and_pose_not_fidelity")) return;
        if (!check(m_world->FindEntity(a.removed)->lifecycle == EntityLifecycle::Destroyed &&
                   !m_world->SetEntityFidelity(a.removed, SimulationFidelity::Full, &error) && m_world->FindEntity(a.created),
                   "destroyed_and_created_deltas_survive_play_restart")) return;
        r = {}; r.stop = true; request(r);
        m_document.BeginEdit(); m_document.GetScene().Find(a.moving)->transform.position.x += 0.25f; m_document.CommitEdit();
        const std::string incompatible = sceneText();
        const std::string statePath = WorldStatePathFor(m_document.Path());
        r = {}; r.play = true; request(r);
        if (!check(!m_world && !m_play && m_panels.mode == EditorMode::Edit &&
                   m_panels.status.find("fingerprint") != std::string::npos && sceneText() == incompatible &&
                   read(statePath) == a.saveBytes, "incompatible_play_rejected_without_authoring_or_save_mutation")) return;
        r = {}; r.undo = true; request(r);
        if (!check(sceneText() == a.baseline && m_document.Save(error), "restore_compatible_authored_baseline")) return;
        std::ofstream result(fs::path(a.output) / "result.json");
        result << "{\n  \"overall_pass\": true,\n  \"checks\": " << a.checks + 1 << ",\n  \"frames\": " << a.frames
               << ",\n  \"fixed_steps\": " << a.playSteps << ",\n  \"project\": \"project/FTFT6_Tiny.judasproj\",\n"
               << "  \"startup_scene\": \"project/Scenes/edited.judas\",\n  \"authored_fingerprint\": \"" << a.fingerprint
               << "\",\n  \"asset_id\": \"" << a.assetId << "\",\n  \"moved_entity\": " << a.moving
               << ",\n  \"destroyed_entity\": " << a.removed << ",\n  \"created_entity\": " << a.created << "\n}\n";
        result.close();
        if (!check(static_cast<bool>(result), "write_result_metadata")) return;
        std::fprintf(stderr, "FTFT6 SUMMARY PASS checks=%u fixed_steps=%zu frames=%u\n", a.checks, a.playSteps, a.frames);
        m_quit = true;
    }
}

EditorApplication::EditorApplication() = default;
EditorApplication::~EditorApplication() = default;

void EditorApplication::RefreshProjectLists() {
    m_panels.terrainSurfaces = KnownTerrainSurfaces();
    m_panels.sceneFiles.clear();
    m_panels.project = &m_project;
    m_panels.assets = m_host ? &m_host->Assets() : nullptr;
    m_panels.resources = m_host ? &m_host->Resources() : nullptr;
    m_panels.importJobs=m_host?&m_host->Jobs():nullptr;
    if (!m_project.IsLoaded()) return;
    std::error_code ec;
    if (!fs::is_directory(m_project.ScenesDir(), ec)) return;
    for (const auto& entry : fs::recursive_directory_iterator(m_project.ScenesDir(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".judas") {
            m_panels.sceneFiles.push_back(m_project.MakeRelative(entry.path().generic_string()));
        }
    }
    std::sort(m_panels.sceneFiles.begin(), m_panels.sceneFiles.end());
}

bool EditorApplication::OpenProject(const std::string& projectFile, std::string& outError) {
    Project project;
    if (!project.Load(projectFile, outError)) return false;
    m_project = project;
    m_host->OpenProjectAssets(m_project.RootDir(), m_project.AssetsDir());
    if(!m_host->GetWindow().Input().SetMap(m_project.Settings().input,outError))return false;
    RefreshProjectLists();
    m_panels.showProjectSettings = false;
    m_panels.browserSelection.clear();
    return true;
}

bool EditorApplication::CreateProject(const std::string& directory, const std::string& name, std::string& outError) {
    Project project;
    if (!Project::CreateNew(directory, name, project, outError)) return false;
    // A starter scene so the new project runs immediately.
    const std::string sceneRelative = project.Settings().scenesDir + "/main.judas";
    if (!SaveSceneToFile(MakeStarterScene(), project.Resolve(sceneRelative), outError)) return false;
    project.Settings().startupScene = sceneRelative;
    if (!project.Save(outError)) return false;
    if (!OpenProject(project.ProjectFile(), outError)) return false;
    return OpenScene(sceneRelative, outError);
}

std::string EditorApplication::ResolveScenePath(const std::string& input) const {
    if (input.empty() || fs::path(input).is_absolute()) return input;
    std::error_code ec;
    if (m_project.IsLoaded() && fs::exists(m_project.Resolve(input), ec)) return m_project.Resolve(input);
    if (fs::exists(input, ec)) return fs::absolute(input, ec).lexically_normal().generic_string();
    return m_project.IsLoaded() ? m_project.Resolve(input) : input;
}

std::string EditorApplication::WorldStatePathFor(const std::string& scenePath) const {
    if (scenePath.empty()) return std::string();
    return m_project.IsLoaded() ? m_project.WorldStatePathForScene(scenePath) : DefaultWorldStatePath(scenePath);
}

bool EditorApplication::OpenScene(const std::string& path, std::string& outError) {
    m_panels.liquidPreview.Clear();m_panels.deformablePreview.Clear();m_panels.collisionPreview.Clear();
    const std::string resolved = ResolveScenePath(path);
    Scene authored, resolvedPrefabs;
    if(!LoadSceneFromFile(resolved,authored,outError)||!ResolvePrefabs(authored,&m_host->Assets(),resolvedPrefabs,outError))return false;
    if (!m_document.Load(resolved, outError)) return false;
    m_document.GetScene()=std::move(resolvedPrefabs);
    m_panels.pathInput = m_project.IsLoaded() ? m_project.MakeRelative(resolved) : resolved;
    if (const SceneObject* first = m_document.GetScene().Objects().empty() ? nullptr : &m_document.GetScene().Objects().front()) {
        m_camera.LookAt(first->transform.position, 40.0f);
    }
    return true;
}

bool EditorApplication::RunProject(std::string& outMessage) {
    if (!m_project.IsLoaded()) { outMessage = "no project open"; return false; }
    if (m_project.Settings().startupScene.empty()) { outMessage = "the project has no startup scene (Project settings)"; return false; }
    std::string saveError;
    if (!m_project.Save(saveError)) { outMessage = "could not save the project before running: " + saveError; return false; }
#ifdef _WIN32
    const auto runtime = fs::u8path(EngineExecutableDir()) / RuntimeExecutableName();
    if (!fs::is_regular_file(runtime)) { outMessage = "runtime not found beside editor: " + runtime.u8string(); return false; }
    if (!WindowsLaunchProcess(runtime, {m_project.ProjectFile()}, outMessage)) return false;
    outMessage = "Launched " + runtime.u8string(); return true;
#elif defined(__unix__) || defined(__APPLE__)
    // The runtime binary sits beside the editor binary in every build tree.
    const std::string runtime = EngineExecutableDir() + "/judas";
    std::error_code ec;
    if (!fs::is_regular_file(runtime, ec)) { outMessage = "runtime not found beside the editor: " + runtime; return false; }
    // The child resolves engine data (the UI font) the way this editor
    // did; pass that root on explicitly so it never depends on the CWD.
    const std::string fontPath = ResolveEngineDataPath("assets/fonts/DejaVuSans.ttf");
    const std::string engineRoot = fs::path(fontPath).parent_path().parent_path().parent_path().generic_string();
    std::vector<std::string> envStrings;
    for (char** e = environ; e && *e; ++e) {
        if (std::strncmp(*e, "JUDAS_ENGINE_ROOT=", 18) == 0) continue;
        // The editor's own automation hook must not leak into the game.
        if (std::strncmp(*e, "JUDAS_EDITOR_AUTOTEST=", 22) == 0) continue;
        envStrings.push_back(*e);
    }
    envStrings.push_back("JUDAS_ENGINE_ROOT=" + engineRoot);
    std::vector<char*> envp;
    for (std::string& s : envStrings) envp.push_back(s.data());
    envp.push_back(nullptr);
    std::string argv0 = runtime, argv1 = m_project.ProjectFile();
    char* argv[] = {argv0.data(), argv1.data(), nullptr};
    pid_t pid = 0;
    const int result = posix_spawn(&pid, runtime.c_str(), nullptr, nullptr, argv, envp.data());
    if (result != 0) { outMessage = "could not launch the runtime: " + std::string(std::strerror(result)); return false; }
    outMessage = "Launched " + runtime + " " + m_project.ProjectFile() + " (pid " + std::to_string(pid) + ") on " +
                 m_project.Settings().startupScene;
    return true;
#else
    outMessage = "Run project launches ./judas as a separate process; not implemented on this platform. Run: judas " +
                 m_project.ProjectFile();
    return false;
#endif
}

bool EditorApplication::StartPlay(std::string& outError) {
    m_world = std::make_unique<RuntimeWorld>();
    m_world->legacyGameplay = m_project.Settings().legacyGameplay;
    m_world->audioGroups=m_project.Settings().audio;
    m_world->SetSceneControl(std::make_shared<SceneSession>(m_project,m_document.Path()));
    m_world->SceneControl()->SetEditorSaveIsolation(true);
    if (!m_world->Build(m_document.GetScene(), &m_host->Resources(), outError, &m_project.Settings().classification, &m_project.Settings().navigation)) {
        m_world.reset();
        return false;
    }
    // Milestone 29: a saved world-state delta for this scene file layers
    // over the freshly instantiated baseline, exactly as the runtime does.
    m_panels.worldStatePath = WorldStatePathFor(m_document.Path());
    bool stateApplied = false;
    if(m_world->IsComposed())m_panels.worldStatePath.clear();
    if (!ApplyWorldStateFileIfPresent(*m_world, m_panels.worldStatePath, stateApplied, outError)) {
        m_world.reset();
        return false;
    }
    if(m_project.IsLoaded()&&!m_host->GetWindow().Input().SetMap(m_project.Settings().input,outError))return false;
    m_play = std::make_unique<InteractivePlay>();
    if (!m_play->Begin(*m_world, WorldCoordinates(m_document.GetScene().Settings().worldOrigin), outError)) {
        m_play.reset();
        m_world.reset();
        return false;
    }
    m_play->SetWorldStatePath(m_panels.worldStatePath, stateApplied);
    // The debug view over the played world, inside the 3D frame.
    m_play->SetWorldOverlay([this](Renderer& renderer, float alpha) {
        if (!m_panels.debug.AnyEnabled() || !m_world) return;
        m_debugLines.Clear();
        BuildWorldDebugLines(*m_world, &m_play->Session(), alpha, m_panels.debug, m_debugLines);
        renderer.DrawDebugLines(m_debugLines.Lines());
    });
    m_panels.runtime = m_world.get();
    m_panels.mode = EditorMode::Play;
    m_panels.status = m_world->legacyGameplay
        ? "Playing historical controls: Escape pauses; Stop restores authored state"
        : "Playing project scripts; Stop restores authored state";
    // Keys pressed while editing (F to focus, R, Space, ...) must not fire
    // as gameplay requests on the first played frame, and a gizmo drag in
    // progress is abandoned.
    m_drag = GizmoDrag{};
    m_hoverAxis = GizmoAxis::None;
    if (m_document.EditInProgress()) m_document.CancelEdit();
    m_host->GetWindow().ClearPendingRequests();
    m_host->GetWindow().SetMouseCaptured(m_world->legacyGameplay);
    return true;
}

void EditorApplication::StopPlay() {
    if (m_play) m_play->End();
    m_play.reset();
    m_world.reset();  // the authored Scene was never written; nothing to revert
    m_panels.runtime = nullptr;
    m_panels.mode = EditorMode::Edit;
    m_panels.runtimeInfo.clear();
    m_panels.status = "Stopped: authored scene restored";
    m_host->GetWindow().SetMouseCaptured(false);
    m_host->GetWindow().Input().DiscardStickHistory();
}

void EditorApplication::HandleRequests(EditorRequests& r) {
    std::string error;
    if (r.quit) m_quit = true;
    if (r.play && m_panels.mode == EditorMode::Edit) {
        if (!StartPlay(error)) m_panels.status = "Play failed: " + error;
    }
    if (r.stop && m_panels.mode == EditorMode::Play) StopPlay();
    if (m_panels.mode == EditorMode::Play && m_play) {
        std::string message;
        if (r.saveWorldState) { m_play->SaveWorldStateNow(message); m_panels.status = message; }
        if (r.deleteWorldState) { m_play->DeleteWorldStateNow(message); m_panels.status = message; }
    }
    if (m_panels.mode != EditorMode::Edit) return;

    // --- Project ---
    if (r.newProject) ImGui::OpenPopup("New project");
    if (r.openProject) ImGui::OpenPopup("Open project");
    if (r.saveProject) m_panels.status = m_project.Save(error) ? "Saved project " + m_project.ProjectFile() : "Project save failed: " + error;
    if (r.exportProject) {
        ProjectExportOptions options; options.destination = m_panels.exportDestination;
        ProjectExportResult result;
        const bool exported = m_project.Save(error) && ExportProject(m_project, options, result, error);
        m_panels.runProjectInfo = exported ? "Exported " + result.packageDirectory + " (" +
            std::to_string(result.assetCount) + " assets)" : error;
        m_panels.status = m_panels.runProjectInfo;
    }
    if (r.runProject) {
        std::string message;
        RunProject(message);
        m_panels.status = message;
        m_panels.runProjectInfo = message;
    }
    if (!r.openSceneRelative.empty()) {
        m_panels.status = OpenScene(r.openSceneRelative, error) ? "Opened " + r.openSceneRelative : "Open failed: " + error;
    }

    // --- Assets ---
    bool assetsChanged = false;
    if (!r.importSource.empty()) {
        AssetRecord record;
        if (m_host->Assets().Import(r.importSource, r.importDestination, record, error)) {
            m_panels.status = "Imported " + record.relativePath + " as " + AssetTypeName(record.type) + " " + record.id;
            m_panels.browserSelection = record.id;
            m_panels.importSourceInput.clear();
            m_panels.importDestinationInput.clear();
        } else {
            m_panels.status = "Import failed: " + error;
        }
        assetsChanged = true;
    }
    if (!r.trackAssetPath.empty()) {
        AssetRecord record;
        m_panels.status = m_host->Assets().Track(r.trackAssetPath, record, error) ? "Tracked " + record.relativePath + " as " + record.id
                                                                                   : "Track failed: " + error;
        assetsChanged = true;
    }
    if (!r.moveAssetId.empty()) {
        // The GPU copy keyed by this id stays valid: the id did not change.
        m_panels.status = m_host->Assets().Move(r.moveAssetId, r.moveAssetTo, error) ? "Moved asset to " + r.moveAssetTo
                                                                                     : "Move failed: " + error;
        assetsChanged = true;
    }
    if (!r.removeAssetId.empty()) {
        m_host->Resources().Invalidate(r.removeAssetId);
        m_panels.status = m_host->Assets().Remove(r.removeAssetId, error) ? "Removed asset" : "Remove failed: " + error;
        m_panels.browserSelection.clear();
        assetsChanged = true;
    }
    if (r.rescanAssets || assetsChanged) {
        if (r.rescanAssets) m_host->OpenProjectAssets(m_project.RootDir(), m_project.AssetsDir());
        RefreshProjectLists();
    }

    // --- Scene ---
    if (r.newScene) {
        m_document.NewScene();
        m_panels.status = "New scene";
    }
    if (r.open) ImGui::OpenPopup("Open scene");
    if (r.saveAs) ImGui::OpenPopup("Save scene as");
    if (r.save) {
        if (m_document.Path().empty()) ImGui::OpenPopup("Save scene as");
        else {
            m_panels.status = m_document.Save(error) ? "Saved " + m_document.Path() : "Save failed: " + error;
            RefreshProjectLists();
        }
    }
    if (r.undo) m_document.Undo();
    if (r.redo) m_document.Redo();
    if (!r.createKind.empty()) {
        std::string defaultMesh;
        for (const auto& [id, record] : m_host->Assets().Records()) {
            if (record.type == AssetType::Mesh && !record.missing) { defaultMesh = id; break; }
        }
        CreateObjectOfKind(m_document, r.createKind, r.createPosition, defaultMesh);
        m_panels.status = "Created " + r.createKind;
    }
    if (!r.dropMeshAssetId.empty()) {
        std::optional<SceneAnimationComponent> animation;
        if(auto rig=m_host->Resources().TryGetSkeletal(r.dropMeshAssetId)){animation.emplace();animation->clip=rig->clips.empty()?"":rig->clips.front().name;}
        CreateObjectOfKind(m_document, "mesh", r.createPosition, r.dropMeshAssetId, animation?&*animation:nullptr);
        m_panels.status = "Placed mesh asset from the Asset Browser";
    }
    if (r.duplicateId != kInvalidSceneObjectId) {
        if (DuplicateObject(m_document, r.duplicateId) != kInvalidSceneObjectId) m_panels.status = "Duplicated object";
    }
    if (r.focusSelection) {
        if (const SceneObject* o = m_document.SelectedObject()) {
            m_camera.LookAt(o->transform.position, std::max(4.0f, PickRadius(*o) * 3.0f));
        }
    }

    // Path popups (the editor has no OS file dialog; a path field is enough
    // — see docs/ARCHITECTURE.md, "Milestone 30, Limitations").
    const auto pathPopup = [&](const char* title, const char* button, const char* label, const char* hint, auto&& action) {
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        char buffer[512];
        std::strncpy(buffer, m_panels.pathInput.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText(label, buffer, sizeof(buffer))) m_panels.pathInput = buffer;
        ImGui::TextDisabled("%s", hint);
        if (ImGui::Button(button) && !m_panels.pathInput.empty()) {
            action(m_panels.pathInput);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    };
    pathPopup("Open scene", "Open", "Path (.judas)", "Relative to the project root, e.g. Scenes/main.judas",
              [&](const std::string& path) {
                  std::string e;
                  m_panels.status = OpenScene(path, e) ? "Opened " + path : "Open failed: " + e;
              });
    pathPopup("Save scene as", "Save", "Path (.judas)", "Relative to the project root, e.g. Scenes/level2.judas",
              [&](const std::string& path) {
                  std::string e;
                  const std::string resolved = m_project.IsLoaded() && !fs::path(path).is_absolute() ? m_project.Resolve(path) : path;
                  m_panels.status = m_document.SaveAs(resolved, e) ? "Saved " + resolved : "Save failed: " + e;
                  RefreshProjectLists();
              });
    pathPopup("Open project", "Open", "Project file (.judasproj)", kProjectFileHint, [&](const std::string& path) {
        std::string e;
        if (!OpenProject(path, e)) { m_panels.status = "Open project failed: " + e; return; }
        m_panels.status = "Opened project " + m_project.Settings().name;
        if (!m_project.Settings().startupScene.empty()) {
            if (!OpenScene(m_project.Settings().startupScene, e)) m_panels.status = "Project opened; startup scene failed: " + e;
        } else {
            m_document.NewScene();
        }
    });
    if (ImGui::BeginPopupModal("New project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char directory[512] = "";
        static char name[128] = "";
        ImGui::InputTextWithHint("Directory", "projects/my_game (created if missing)", directory, sizeof(directory));
        ImGui::InputTextWithHint("Name", "My Game", name, sizeof(name));
        ImGui::TextDisabled("Creates <dir>/<name>.judasproj with Assets/, Scenes/main.judas and Saves/.");
        if (ImGui::Button("Create") && directory[0] && name[0]) {
            std::string e;
            m_panels.status = CreateProject(directory, name, e) ? "Created project " + m_project.ProjectFile()
                                                                 : "New project failed: " + e;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void EditorApplication::PickAtPixel(int x, int y) {
    glm::vec3 origin, direction;
    m_camera.PixelRay(x, y, m_host->GetWindow().Width(), m_host->GetWindow().Height(), origin, direction);
    SceneObjectId best = kInvalidSceneObjectId;
    float bestDistance = 1.0e30f;
    Scene pickScene;std::string error;
    if(!FlattenHierarchy(m_document.GetScene(),pickScene,error))return;
    for (const SceneObject& o : pickScene.Objects()) {
        float distance = 0.0f;
        if (RaySphere(origin, direction, o.transform.position, PickRadius(o), distance) && distance < bestDistance) {
            bestDistance = distance;
            best = o.id;
        }
    }
    m_document.Select(best, ImGui::GetIO().KeyCtrl);
}

void EditorApplication::UpdateGizmo(bool allowInteraction) {
    m_gizmoLines.Clear();
    SceneObject* selected = m_document.SelectedObject();
    if (!selected) {
        if (m_drag.Active()) { m_drag = GizmoDrag{}; m_document.CancelEdit(); }
        m_hoverAxis = GizmoAxis::None;
        return;
    }
    Window& window = m_host->GetWindow();
    int mx = 0, my = 0;
    window.GetMousePosition(mx, my);
    glm::vec3 rayOrigin, rayDirection;
    m_camera.PixelRay(mx, my, window.Width(), window.Height(), rayOrigin, rayDirection);
    Scene flattened;std::string error;
    if(!FlattenHierarchy(m_document.GetScene(),flattened,error))return;
    auto presented=flattened.Find(selected->id)->transform;
    if(m_document.Selection().size()>1){presented.position={0,0,0};auto roots=m_document.SelectionRoots();for(auto id:roots)presented.position+=flattened.Find(id)->transform.position;presented.position/=float(roots.size());}
    const glm::vec3 origin = presented.position;
    // Handles keep a roughly constant on-screen size.
    m_gizmoHandleLength = std::max(0.2f, glm::length(origin - m_camera.Position()) * 0.14f);
    const bool snap = m_panels.gizmoSnap || ImGui::GetIO().KeyCtrl;

    if (m_drag.Active()) {
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)){m_document.CancelEdit();m_drag=GizmoDrag{};return;}
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            auto value=UpdateGizmoDrag(m_drag, rayOrigin, rayDirection, snap);
            Scene baseline;std::string error;
            if(FlattenHierarchy(m_document.EditBaseline(),baseline,error)){
                auto delta=value.position-m_drag.startTransform.position;
                auto rotation=glm::normalize(value.rotation*glm::inverse(m_drag.startTransform.rotation));
                auto scale=value.scale/m_drag.startTransform.scale;
                auto candidate=m_document.EditBaseline();
                for(auto id:m_document.SelectionRoots()){
                    auto t=baseline.Find(id)->transform;t.position=(m_panels.gizmoIndividualOrigins?t.position:m_drag.origin+rotation*((t.position-m_drag.origin)*scale))+delta;t.rotation=glm::normalize(rotation*t.rotation);t.scale*=scale;
                    glm::mat4 matrix=glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);
                    auto* o=candidate.Find(id);
                    if(o->parent){auto p=baseline.Find(o->parent)->transform;matrix=glm::inverse(glm::translate(glm::mat4(1),p.position)*glm::mat4_cast(p.rotation)*glm::scale(glm::mat4(1),p.scale))*matrix;}
                    JointTransform local;if(!DecomposeRigidPose(matrix,local,error))break;o->transform={local.translation,local.rotation,local.scale};
                }
                if(error.empty())m_document.GetScene()=std::move(candidate);else m_panels.status=error;
            }
        } else {
            // Release: one undo step for the whole drag.
            m_document.CommitEdit();
            m_drag = GizmoDrag{};
        }
    } else if (allowInteraction) {
        m_hoverAxis = PickGizmoAxis(m_panels.gizmoMode, rayOrigin, rayDirection, origin, glm::normalize(presented.rotation),
                                    m_panels.gizmoSpace, m_gizmoHandleLength, m_gizmoHandleLength * 0.12f);
        if (m_hoverAxis != GizmoAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            GizmoDrag drag;
            if (BeginGizmoDrag(m_panels.gizmoMode, m_hoverAxis, rayOrigin, rayDirection, presented,
                               m_panels.gizmoSpace, m_gizmoHandleLength, drag)) {
                m_drag = drag;
                m_document.BeginEdit();
            }
        }
    } else {
        m_hoverAxis = GizmoAxis::None;
    }
    BuildGizmoLines(m_panels.gizmoMode, presented.position, glm::normalize(presented.rotation),
                    m_panels.gizmoSpace, m_gizmoHandleLength, m_drag.Active() ? m_drag.axis : m_hoverAxis, m_gizmoLines);
}

void EditorApplication::DrawEditOverlay(Renderer& renderer, const Scene& scene) {
    m_debugLines.Clear();
    BuildAuthoredDebugLines(scene, m_panels.debug, m_debugLines);
    if(m_panels.debug.navigation)m_debugLines.Append(m_panels.navigationPreview);
    m_debugLines.Append(m_panels.recipePreview);m_debugLines.Append(m_panels.liquidPreview);m_debugLines.Append(m_panels.deformablePreview);m_debugLines.Append(m_panels.collisionPreview);
    for(auto id:m_document.Selection())if(const SceneObject* selected=scene.Find(id))BuildSelectionLines(*selected,m_debugLines);
    if(const auto* selected=scene.Find(m_document.Selected())){
        const auto* owner=selected->socket?scene.Find(selected->socket->target):selected;
        if(owner&&owner->render){auto asset=m_host->Resources().TryGetSkeletal(owner->render->meshAsset);if(asset){auto global=PoseGlobalMatrices(asset->skeleton,asset->skeleton.rest);auto t=owner->transform;auto model=glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);for(auto& matrix:global){JointTransform joint;std::string error;if(DecomposeRigidPose(model*matrix,joint,error))m_debugLines.Axes(joint.translation,joint.rotation,.18f);}}}
    }
    if(m_panels.skeletonFitPreview&&m_panels.skeletonFitGeneration==m_document.Generation()) {
        if(const auto* o=scene.Find(m_panels.skeletonFitOwner);o&&o->render) {
            if(auto asset=m_host->Resources().TryGetSkeletal(o->render->meshAsset)) {
                auto global=PoseGlobalMatrices(asset->skeleton,asset->skeleton.rest);
                auto model=glm::translate(glm::mat4(1),o->transform.position)*glm::mat4_cast(o->transform.rotation)*glm::scale(glm::mat4(1),o->transform.scale);
                for(const auto& bone:m_panels.skeletonFitPreview->bones) {
                    int index=FindSkeletonJoint(asset->skeleton,bone.joint);
                    if(index<0)continue;
                    JointTransform joint;std::string error;
                    if(!DecomposeRigidPose(model*global[size_t(index)],joint,error))continue;
                    auto center=joint.translation+joint.rotation*(joint.scale*bone.offset);
                    auto orientation=glm::normalize(joint.rotation*bone.orientation);
                    if(bone.shape==RagdollShape::Sphere)m_debugLines.Sphere(center,bone.radius,{1,.5f,.1f});
                    else m_debugLines.Box(center,orientation,bone.halfExtents*joint.scale,{1,.5f,.1f});
                }
            }
        }
    }
    renderer.DrawDebugLines(m_debugLines.Lines(), /*depthTest=*/true);
    renderer.DrawDebugLines(m_gizmoLines.Lines(), /*depthTest=*/false);
}

void EditorApplication::FrameEditMode(float deltaSeconds) {
    JUDAS_PROFILE_SCOPE("Editor viewport");
    Window& window = m_host->GetWindow();
    Renderer& renderer = m_host->GetRenderer();
    const ImGuiIO& io = ImGui::GetIO();

    // Right mouse held over the viewport: capture the mouse and fly.
    const bool rightHeld = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    if (rightHeld && !m_lookActive && !io.WantCaptureMouse) {
        m_lookActive = true;
        window.SetMouseCaptured(true);
    } else if (!rightHeld && m_lookActive) {
        m_lookActive = false;
        window.SetMouseCaptured(false);
    }
    int dx = 0, dy = 0;
    window.GetMouseDelta(dx, dy);
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool fast = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    m_camera.Update(window, deltaSeconds, m_lookActive && !io.WantCaptureKeyboard, dx, dy, fast);
    m_panels.cameraFocus = m_camera.Position() + m_camera.Forward() * 8.0f;

    // Gizmo first: a click on a handle is a drag, not a pick. The viewport
    // owns the mouse only when no panel does and the camera is not flying.
    const bool viewportOwnsMouse = !io.WantCaptureMouse && !m_lookActive;
    UpdateGizmo(viewportOwnsMouse);
    if (viewportOwnsMouse && !m_drag.Active() && m_hoverAxis == GizmoAxis::None &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int mx = 0, my = 0;
        window.GetMousePosition(mx, my);
        PickAtPixel(mx, my);
    }

    // Asset Browser drag released over the viewport: place the mesh where
    // the mouse ray crosses the plane through the camera focus.
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
        if (payload->IsDataType(kAssetDragPayload)) {
            m_pendingDropAsset.assign(static_cast<const char*>(payload->Data), payload->DataSize);
        }
    } else if (!m_pendingDropAsset.empty()) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            const AssetRecord* record = m_host->Assets().Find(m_pendingDropAsset);
            if (record && record->type == AssetType::Mesh) {
                int mx = 0, my = 0;
                window.GetMousePosition(mx, my);
                glm::vec3 rayOrigin, rayDirection;
                m_camera.PixelRay(mx, my, window.Width(), window.Height(), rayOrigin, rayDirection);
                glm::vec3 hit = m_panels.cameraFocus;
                RayPlaneIntersection(rayOrigin, rayDirection, m_panels.cameraFocus, -m_camera.Forward(), hit);
                CreateObjectOfKind(m_document, "mesh", hit, m_pendingDropAsset);
                m_panels.status = "Placed " + record->relativePath;
            } else {
                m_panels.status = "Only mesh assets can be dropped into the viewport";
            }
        }
        m_pendingDropAsset.clear();
    }

    Scene scene;std::string hierarchyError;
    if(!FlattenHierarchy(m_document.GetScene(),scene,hierarchyError)){m_panels.status=hierarchyError;return;}
    RefreshAssetDemand();
    const int height = std::max(window.Height(), 1);
    const float aspect = static_cast<float>(window.Width()) / static_cast<float>(height);
    renderer.SetLighting(glm::normalize(scene.Settings().sunDirection), scene.Settings().sunColor,
                         scene.Settings().ambientColor);
    const auto& appearance=scene.Settings();m_host->Resources().RequestEnvironment(appearance.environmentAsset);
    renderer.SetSceneAppearance(appearance.linearRendering,appearance.exposure,m_host->Resources().TryGetEnvironment(appearance.environmentAsset),appearance.environmentIntensity,appearance.environmentRotation,appearance.environmentBackground,appearance.backgroundColor);
    RendererProfileScope editorCameraGPU(renderer, "Editor camera");
    renderer.BeginFrame(window.Width(), window.Height());
    renderer.SetCamera(m_camera.ViewMatrix(), m_camera.ProjectionMatrix(aspect));
    renderer.SetDynamicLights(BuildAuthoredLights(scene));
    DrawAuthoredScene(renderer, scene, m_host->Resources());
    if(m_panels.worldPreview&&!m_project.Settings().worldManifest.empty()){
        auto key=m_project.ProjectFile()+m_document.Path()+std::to_string(m_panels.worldPreviewRevision);
        if(key!=m_regionPreviewKey){m_regionPreviewKey=key;m_regionPreview.clear();
            auto* asset=m_host->Assets().Find(m_project.Settings().worldManifest);WorldManifest manifest;std::string error;
            if(asset&&LoadWorldManifest(asset->path,manifest,error)){
                glm::dvec3 origin=scene.Settings().worldOrigin;glm::quat orientation{1,0,0,0};
                for(auto& [_,r]:manifest.regions)if(m_project.Resolve(r.scene)==m_document.Path()){origin=r.origin;orientation=r.rotation;}
                for(auto& [_,r]:manifest.regions){if(m_project.Resolve(r.scene)==m_document.Path())continue;PreparedWorldRegion prepared;Scene flat;
                    if(!PrepareWorldRegion(r,m_project,m_host->Assets(),prepared,error)||!FlattenHierarchy(prepared.source,flat,error)){m_panels.status="Preview: "+error;continue;}
                    auto rotation=glm::inverse(orientation)*r.rotation;auto translation=glm::vec3(glm::inverse(glm::dquat(orientation))*(r.origin-origin));
                    for(auto& o:flat.Objects()){o.transform.position=translation+rotation*o.transform.position;o.transform.rotation=rotation*o.transform.rotation;}
                    m_regionPreview.push_back(std::move(flat));
                }
            }
        }
        for(auto& region:m_regionPreview)DrawAuthoredScene(renderer,region,m_host->Resources());
    }else {m_regionPreview.clear();m_regionPreviewKey.clear();}
    DrawEditOverlay(renderer, scene);
    renderer.EndFrame();
}

void EditorApplication::DrawModelImportPreview(float deltaSeconds) {
 JUDAS_PROFILE_SCOPE("Model import preview");
 if(!m_panels.showAssetBrowser||m_panels.mode!=EditorMode::Edit||!m_panels.importAccepted)return;
 auto& task=*m_panels.importAccepted;auto& resources=m_host->Resources();auto& renderer=m_host->GetRenderer();
 if(m_modelPreviewAsset!=task.assetId){if(!m_modelPreviewAsset.empty())resources.ReleaseRef(m_modelPreviewAsset);m_modelPreviewAsset=task.assetId;resources.AddRef(m_modelPreviewAsset);m_panels.modelPreviewTime=0;m_panels.modelPreviewHidden.clear();}
 std::string error;auto mesh=resources.GetMesh(task.assetId,error);if(!mesh.IsValid()||!task.preview)return;
 auto asset=resources.TryGetSkeletal(task.assetId);SkeletalPose pose;std::vector<glm::mat4> palette,global;
 if(asset){pose=asset->skeleton.rest;if(!asset->clips.empty()){auto& clip=asset->clips[std::min(size_t(m_panels.modelPreviewClip),asset->clips.size()-1)];if(m_panels.modelPreviewPlaying&&clip.duration>0)m_panels.modelPreviewTime=std::fmod(m_panels.modelPreviewTime+deltaSeconds,clip.duration);pose=SampleClip(asset->skeleton,clip,m_panels.modelPreviewTime);}palette=ResolveSkinMatrices(asset->skeleton,pose);global=ResolveJointMatrices(asset->skeleton,pose);}
 // Fit from the immutable rest mesh once per accepted generation, not by
 // skinning every source vertex on the editor thread every preview frame.
 if(m_modelPreviewData.lock()!=task.preview){auto& cpu=*task.preview;glm::vec3 lo(INFINITY),hi(-INFINITY);auto rest=cpu.skeletal?ResolveSkinMatrices(cpu.skeletal->skeleton,cpu.skeletal->skeleton.rest):std::vector<glm::mat4>{};for(size_t i=0;i<cpu.vertices.size();++i){auto p=cpu.vertices[i].position;if(!rest.empty()){auto& w=cpu.skinVertices[i];glm::mat4 m(0);for(int k=0;k<4;++k)m+=rest[w.joints[k]]*w.weights[k]+rest[w.joints1[k]]*w.weights1[k];p=glm::vec3(m*glm::vec4(p,1));}lo=glm::min(lo,p);hi=glm::max(hi,p);}m_modelPreviewMin=lo;m_modelPreviewMax=hi;m_modelPreviewData=task.preview;}
 auto lo=m_modelPreviewMin,hi=m_modelPreviewMax;
 auto center=(lo+hi)*.5f;float radius=std::max(glm::length(hi-lo)*.6f,.5f);glm::vec3 eye=center+glm::vec3(std::sin(m_panels.modelPreviewYaw)*radius,radius*.25f,std::cos(m_panels.modelPreviewYaw)*radius*1.5f);
 if(!m_modelPreviewTarget.IsValid()){renderer.SetSceneAppearance(false,1,{},1,{1,0,0,0},false);m_modelPreviewTarget=renderer.CreateRenderTarget(600,360,error);}renderer.SetSceneAppearance(true,1,{},1,{1,0,0,0},false,{.035f,.045f,.065f});
 if(!m_modelPreviewTarget.IsValid()||!renderer.BeginRenderTarget(m_modelPreviewTarget))return;
 renderer.SetCamera(glm::lookAt(eye,center,glm::vec3(0,1,0)),glm::perspective(glm::radians(55.f),600.f/360,.01f,std::max(100.f,radius*10)));
 renderer.SetLighting(glm::normalize(glm::vec3(1,2,3)),{1.8f,1.8f,1.8f},{.35f,.35f,.35f});renderer.SetSceneAppearance(true,1,{},1,{1,0,0,0},false,{.035f,.045f,.065f});renderer.SetDynamicLights({});renderer.SetMaterialBindings({});renderer.DrawMesh(mesh,{0,0,0},{1,0,0,0},{1,1,1},{},{1,1,1},1,palette.empty()?nullptr:&palette,&m_panels.modelPreviewHidden);
 DebugLineList lines;if(m_panels.modelDiagnosticVisible&&m_panels.collisionDiagnosticAsset==task.assetId){auto p=glm::vec3(m_panels.collisionDiagnostic.point);float size=std::max(.05f,radius*.02f);lines.Line(p-glm::vec3(size,0,0),p+glm::vec3(size,0,0),{1,0,0});lines.Line(p-glm::vec3(0,size,0),p+glm::vec3(0,size,0),{1,0,0});lines.Line(p-glm::vec3(0,0,size),p+glm::vec3(0,0,size),{1,0,0});}lines.Line({lo.x,lo.y,lo.z},{lo.x+1,lo.y,lo.z},{1,1,1});lines.Axes({0,0,0},{1,0,0,0},.3f);
 if(m_panels.modelPreviewSkeleton&&asset)for(size_t i=0;i<global.size();++i)if(asset->skeleton.parents[i]>=0)lines.Line(glm::vec3(global[i][3]),glm::vec3(global[asset->skeleton.parents[i]][3]),{.2f,1,.6f});
 if(asset)for(auto& key:m_panels.skeletonPicked){int joint=FindSkeletonJoint(asset->skeleton,key);if(joint>=0&&size_t(joint)<global.size()){JointTransform t;std::string error;if(DecomposeRigidPose(global[joint],t,error))lines.Axes(t.translation,t.rotation,.15f);}}
 if(asset&&!asset->clips.empty()){auto& clip=asset->clips[std::min(size_t(m_panels.modelPreviewClip),asset->clips.size()-1)];glm::vec3 previous(0);for(unsigned i=0;i<=60;++i){auto p=SampleRootMotion(clip,double(clip.duration)*i/60,false).translation;if(i)lines.Line(previous,p,{1,.7f,.1f});previous=p;}}
 renderer.DrawDebugLines(lines.Lines(),false);renderer.EndRenderTarget();m_panels.modelPreviewToken=renderer.EditorImageToken(renderer.RenderTargetTexture(m_modelPreviewTarget));
}

void EditorApplication::RefreshAssetDemand() {
    std::vector<std::string> wanted;
    if(!m_document.GetScene().Settings().environmentAsset.empty())wanted.push_back(m_document.GetScene().Settings().environmentAsset);
    for (const SceneObject& o : m_document.GetScene().Objects()) {
        if (!o.render) continue;
        for(const auto& slot:o.render->materials)if(!slot.asset.empty())wanted.push_back(slot.asset);
        if (!o.render->meshAsset.empty()) wanted.push_back(o.render->meshAsset);
        if (!o.render->textureAsset.empty()) wanted.push_back(o.render->textureAsset);
    }
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    if (wanted == m_heldAssets) return;
    ResourceManager& resources = m_host->Resources();
    for (const std::string& id : wanted) {
        if (!std::binary_search(m_heldAssets.begin(), m_heldAssets.end(), id)) resources.AddRef(id);
    }
    for (const std::string& id : m_heldAssets) {
        if (!std::binary_search(wanted.begin(), wanted.end(), id)) resources.ReleaseRef(id);
    }
    m_heldAssets = wanted;
}

void EditorApplication::CollectProfilerData(float frameDeltaSeconds) {
    ProfilerData& p = m_panels.profiler;
    m_frameAverageMilliseconds = glm::mix(m_frameAverageMilliseconds, frameDeltaSeconds * 1000.0f, 0.1f);
    p.frameMilliseconds = m_frameAverageMilliseconds;
    p.framesPerSecond = m_frameAverageMilliseconds > 0.0f ? 1000.0f / m_frameAverageMilliseconds : 0.0f;
    const RenderStats& stats = m_host->GetRenderer().Stats();
    p.meshesVisible=stats.renderablesVisible;p.meshesCulled=stats.renderablesCulled;
    p.emittersVisible=stats.particleEmittersVisible;p.emittersCulled=stats.particleEmittersCulled;p.particlesSubmitted=stats.particlesSubmitted;
    p.drawCalls = stats.drawCalls;
    p.triangles = stats.triangles;
    p.shadowPasses = stats.shadowPasses;
    p.dynamicLights = stats.dynamicLights;
    p.debugLines = stats.debugLines;
    p.resources = m_host->Resources().Stats();
    p.jobs = m_host->Jobs().Stats();
    p.playing = m_panels.mode == EditorMode::Play && m_play && m_world;
    if (!p.playing) {
        p.fixedStepsThisFrame = 0;
        p.fixedStepMilliseconds = p.physicsMilliseconds = p.fluidMilliseconds = p.surfaceMilliseconds = p.sceneMilliseconds = 0.0f;
        p.physicsBodies = p.dynamicBodies = p.contacts = 0;
        p.entitiesFull = p.entitiesCoarse = p.entitiesDormant = p.entitiesDestroyed = 0;
        p.fluidParticles = 0;
        p.physics = PhysicsWorld::StepStats{};
        return;
    }
    p.fixedStepsThisFrame = m_play->LastFixedStepsThisFrame();
    p.fixedStepMilliseconds = static_cast<float>(m_play->LastFixedStepMilliseconds());
    p.physicsBodies = m_world->Physics().AliveBodyCount();
    p.dynamicBodies = m_world->Physics().DynamicBodyCount();
    p.contacts = m_world->Physics().LastStepContactCount();
    const RuntimeWorld::LifecycleCounts counts = m_world->CountLifecycle();
    p.entitiesFull = counts.full;
    p.entitiesCoarse = counts.coarse;
    p.entitiesDormant = counts.dormant;
    p.entitiesDestroyed = counts.destroyed;
    p.fluidParticles = m_world->HasFluid() ? m_world->Fluid().Particles().size() : 0;
    const FixedStepMeasurements& m = m_play->LastMeasurements();
    p.fluidMilliseconds = m.fluidMeasured ? static_cast<float>(m.fluidMilliseconds) : 0.0f;
    p.surfaceMilliseconds = static_cast<float>(m_play->LastSurfaceMilliseconds());
    p.sceneMilliseconds = static_cast<float>(m_play->LastSceneMilliseconds());
    p.physics = m_world->Physics().LastStepStats();
}

int EditorApplication::Run(int argc, char** argv) {
    ProfileRun profileRun("editor");
    ProfileFrame startupProfile("editor startup",true);
    if (argc > 2) {
        std::fprintf(stderr, "usage: judas_editor [project.judasproj | scene.judas]\n");
        return 1;
    }
    std::string error;
    EngineHost host;
    if (!host.Init("Project Judas Editor", kWindowWidth, kWindowHeight, true, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    m_host = &host;
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    window.SetMouseCaptured(false);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // fixed layout each launch; no files written beside the scene
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(), window.NativeGLContext());
    ImGui_ImplOpenGL3_Init("#version 330 core");
    window.SetEventHook([](const SDL_Event& event) { ImGui_ImplSDL2_ProcessEvent(&event); });

    // --- Milestone 30: open a project, then a scene ---
    std::string argument = argc == 2 ? argv[1] : std::string();
    std::string projectFile;
    std::string sceneArgument;
    if (!argument.empty() && argument.size() > 10 && argument.compare(argument.size() - 10, 10, ".judasproj") == 0) {
        projectFile = argument;
    } else if (!argument.empty()) {
        sceneArgument = argument;
        projectFile = Project::FindProjectFileFor(argument);
    } else {
        projectFile = Project::FindProjectFileFor(".");
    }
    if (const char* mode = std::getenv("JUDAS_RESOURCE_MODE")) {
        if (std::string(mode) == "blocking") host.Resources().SetBlockingMode(true);
    }
    RefreshProjectLists();
    if (!projectFile.empty()) {
        if (!OpenProject(projectFile, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        m_panels.status = "Opened project " + m_project.Settings().name;
    }
    if (!sceneArgument.empty()) {
        m_panels.status = OpenScene(sceneArgument, error) ? "Opened " + sceneArgument : "Open failed: " + error;
    } else if (m_project.IsLoaded() && !m_project.Settings().startupScene.empty()) {
        if (!OpenScene(m_project.Settings().startupScene, error)) m_panels.status = "Startup scene failed: " + error;
        else m_panels.status = "Opened project " + m_project.Settings().name + " at " + m_project.Settings().startupScene;
    } else {
        m_document.NewScene();
        m_panels.status = m_project.IsLoaded() ? "Project has no startup scene; new scene. Create > objects; right-drag to look."
                                               : "No project. File > New project or Open project. Right-drag to look, WASD/QE to fly.";
    }
    if (m_panels.status.rfind("Opened", 0) == 0) {
        m_panels.status += ". Right-drag looks, WASD/QE fly; click selects; W/E/R move/rotate/scale; X local/world; Ctrl snaps.";
    }

    // Developer/automation hook (see docs/ARCHITECTURE.md, "Milestone 30,
    // Automated evidence"): JUDAS_EDITOR_AUTOTEST=<prefix> selects the first
    // object, enables the debug view, renders the edit view, duplicates and
    // undoes (checking the scene is unchanged), enters Play, runs,
    // screenshots both, stops, verifies the authored scene is untouched,
    // saves it, prints profiler and resource lines, and quits.
    const char* autotest = std::getenv("JUDAS_EDITOR_AUTOTEST");
    int autotestFrame = 0;
    std::uint64_t profilerFrozenFrame = 0;
    std::size_t profilerFrozenSteps = 0;
    std::string autotestBaseline;
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_LIQUID_BAKE")){std::vector<SceneObjectId> ids;for(const auto& o:m_document.GetScene().Objects())if(o.liquidBasin)ids.push_back(o.id);for(auto id:ids){bool ok=BakeEditorLiquid(m_document,id,m_panels);std::fprintf(stderr,"[editor autotest] liquid bake %llu: %s: %s\n",static_cast<unsigned long long>(id),ok?"PASS":"FAIL",m_panels.status.c_str());}}
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_NAV_BAKE")){std::vector<SceneObjectId> surfaces;for(const auto& o:m_document.GetScene().Objects())if(o.navigationSurface)surfaces.push_back(o.id);for(auto id:surfaces){bool ok=BakeEditorNavigation(m_document,id,m_panels);std::fprintf(stderr,"[editor autotest] navigation bake %llu: %s: %s\n",static_cast<unsigned long long>(id),ok?"PASS":"FAIL",m_panels.status.c_str());}}
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_DEFORMABLE")){
        m_panels.deformableColumns=9;m_panels.deformableRows=13;m_panels.deformableSubdivision=2;m_panels.deformableSize={2,3,1};m_panels.deformableDestination="deformables/editor-sheet.judasdeform";
        bool sheet=BakeEditorDeformable(m_document,100,m_panels,1);
        m_panels.deformableColumns=4;m_panels.deformableRows=2;m_panels.deformableSubdivision=2;m_panels.deformableSize={3,1,1};m_panels.deformableDestination="deformables/editor-block.judasdeform";
        bool block=BakeEditorDeformable(m_document,300,m_panels,2);
        std::fprintf(stderr,"[editor autotest] deformable sheet/block authoring: %s / %s: %s\n",sheet?"PASS":"FAIL",block?"PASS":"FAIL",m_panels.status.c_str());
        if(const char* source=std::getenv("JUDAS_EDITOR_AUTOTEST_DEFORMABLE_SOURCE")){m_panels.deformableSource=source;m_panels.deformableDestination="deformables/editor-import.judasdeform";bool imported=BakeEditorDeformable(m_document,501,m_panels,3);std::fprintf(stderr,"[editor autotest] deformable indexed import authoring: %s: %s\n",imported?"PASS":"FAIL",m_panels.status.c_str());}

    }
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_FRACTURE")){
        m_panels.deformableColumns=3;m_panels.deformableRows=1;m_panels.deformableSubdivision=1;m_panels.deformableSize={3,1,1};
        m_panels.deformableDestination="deformables/editor-fracture-rigid.judasdeform";bool rigid=BakeEditorDeformable(m_document,100,m_panels,5);
        m_panels.deformableDestination="deformables/editor-fracture-soft.judasdeform";bool soft=BakeEditorDeformable(m_document,200,m_panels,4);
        m_panels.deformableDestination="deformables/editor-fracture-import.judasdeform";m_panels.deformableSource="deformables/L-partition.source";bool imported=BakeEditorDeformable(m_document,450,m_panels,6);
        std::fprintf(stderr,"[editor autotest] fracture rigid/soft/partition authoring: %s / %s / %s: %s\n",rigid?"PASS":"FAIL",soft?"PASS":"FAIL",imported?"PASS":"FAIL",m_panels.status.c_str());
    }
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_COLLISION")){
        auto* object=m_document.GetScene().Find(100);auto source=object&&object->render?object->render->meshAsset:AssetId{};
        CollisionCookSettings settings;bool ok=BakeEditorCollision(m_document,100,m_panels,source,settings,"collision/editor-check.judascollision");
        std::fprintf(stderr,"[editor autotest] shared collision cook/assign: %s: %s\n",ok?"PASS":"FAIL",m_panels.status.c_str());
    }
    if (autotest) SaveSceneToString(m_document.GetScene(), autotestBaseline);
    const char* importRecipe=autotest?std::getenv("JUDAS_EDITOR_AUTOTEST_IMPORT"):nullptr;
    if(importRecipe){m_panels.showAssetBrowser=true;m_panels.modelRecipe=importRecipe;m_panels.importTask=QueueModelImport(host.Jobs(),importRecipe);m_panels.importPublished=false;}
    unsigned importWaitFrames=0;
    const auto screenshot = [&](const std::string& path) {
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(window.Width(), window.Height(), pixels);
        const bool written = WriteRgbPng(path, window.Width(), window.Height(), pixels);
        std::fprintf(stderr, "[editor autotest] %s: %s\n", written ? "wrote" : "FAILED", path.c_str());
    };

    if (const char* output = std::getenv("JUDAS_EDITOR_STABILIZATION")) {
        m_stabilization = std::make_unique<StabilizationAutomation>();
        m_stabilization->output = output;
    }

    EditorRequests deferredRequests;
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    startupProfile.End();
    while (!window.ShouldClose() && !m_quit) {
        ProfileFrame outerProfile(m_panels.mode == EditorMode::Play ? "editor Play" : "editor edit");
        // Last frame's UI focus decides whether the engine's own key/mouse
        // reading is suppressed this frame (a text field must not walk the
        // player or fly the camera).
        window.SetInputClaimed(io.WantCaptureKeyboard, io.WantCaptureMouse && !window.IsMouseCaptured());
        { JUDAS_PROFILE_SCOPE("Input events"); window.PollEvents(); }
        host.PumpResources();  // M31: GPU upload of finished loads, budget eviction
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        renderer.ResetStats();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        EditorRequests requests;
        // Keyboard shortcuts (edit mode, no text field focused, not flying).
        if (m_panels.mode == EditorMode::Edit && !io.WantCaptureKeyboard) {
            const bool ctrl = io.KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) requests.save = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) requests.undo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) requests.redo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) requests.duplicateId = m_document.Selected();
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) requests.play = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_document.Selected() != kInvalidSceneObjectId) {
                std::string error;
                if(!m_document.DeleteSelection(error))m_panels.status=error;
            }
            if (!m_lookActive && !ctrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_F)) requests.focusSelection = true;
                if (ImGui::IsKeyPressed(ImGuiKey_W)) m_panels.gizmoMode = GizmoMode::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) m_panels.gizmoMode = GizmoMode::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) m_panels.gizmoMode = GizmoMode::Scale;
                if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                    m_panels.gizmoSpace = m_panels.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
                }
            }
        } else if (m_panels.mode == EditorMode::Play && !io.WantTextInput && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyPressed(ImGuiKey_F5)) {
            requests.stop = true;
        }

        if (m_panels.mode == EditorMode::Play) {
            if (ImGui::IsKeyPressed(ImGuiKey_F8)) {
                m_panels.profilerInspect = !m_panels.profilerInspect;
                if (m_panels.profilerInspect) m_panels.showProfiler = true;
            }
            m_play->SetPointerCaptureAllowed(!(m_panels.showProfiler && m_panels.profilerInspect));
            // The identical frame the runtime runs. Escape opens the M13
            // pause menu, which releases the mouse for the editor panels.
            m_play->Frame(window, renderer, deltaSeconds, /*drawHud=*/true);
            if(auto scenes=m_world->SceneControl()) {
                scenes->AdvanceStreaming(*m_world,m_play->IsPaused());
                auto* previousWorld=m_world.get();
        scenes->AdvanceSaves(m_world,*m_play,host.Resources(),error);
                // Clear the Load edge without turning a held confirm button into a new jump.
                if(previousWorld!=m_world.get())window.Input().DiscardPending();
        if(!error.empty()){std::fprintf(stderr,"Save service: %s\n",error.c_str());error.clear();}
                if(!scenes->Apply(m_world,*m_play,host.Resources(),error))std::fprintf(stderr,"Scene transition: %s\n",error.c_str());
                if(previousWorld!=m_world.get())window.Input().DiscardStickHistory();
                m_panels.runtime=m_world.get();
                m_panels.worldStatePath=m_play->WorldStatePath();
            }
            m_panels.playPaused = m_play->IsPaused();
            if (m_play->QuitRequested()) requests.stop = true;
            const GameSession& session = m_play->Session();
            const RuntimeWorld::LifecycleCounts counts = m_world->CountLifecycle();
            char info[200];
            std::snprintf(info, sizeof(info), "entities %zu full / %zu coarse / %zu dormant / %zu destroyed | %zu physics bodies | %zu particles | %s",
                          counts.full, counts.coarse, counts.dormant, counts.destroyed, counts.physicsBodies,
                          m_world->HasFluid() ? m_world->Fluid().Particles().size() : std::size_t{0},
                          session.IsPiloting() ? "piloting" : session.Player().IsGrounded() ? "grounded" : "airborne");
            m_panels.runtimeInfo = info;
        } else {
            FrameEditMode(deltaSeconds);
        }
        CollectProfilerData(deltaSeconds);

        static ProfileLabel editorBuildLabel("Editor UI build");
        ProfileScope editorBuildScope(editorBuildLabel);
        DrawEditorMainMenu(m_document, m_panels, requests);
        // While playing, the engine's own HUD occupies the top-left corner
        // and the authored panels are read-only anyway; only the menu bar
        // (Stop), the inspector, profiler and the status bar stay up.
        // The hierarchy is also shown while Play is paused (Escape), with
        // each entity's live fidelity, so the M29 state can be inspected.
        if (m_panels.mode == EditorMode::Edit || m_panels.playPaused) {
            DrawHierarchyPanel(m_document, m_panels, requests);
        }
        if (m_panels.mode == EditorMode::Edit) {
            DrawWorldBuildingPanel(m_document, m_panels);
            DrawSceneSettingsPanel(m_document, m_panels);
            DrawAssetBrowserPanel(m_document, m_panels, requests);
            DrawProjectSettingsPanel(m_document, m_panels, requests);
        }
        DrawInspectorPanel(m_document, m_panels);
        DrawStreamingPanel(m_document,m_panels,requests);
        { JUDAS_PROFILE_SCOPE("Profiler UI"); DrawProfilerPanel(m_panels); }
        DrawStatusBar(m_document, m_panels);

        // Requests raised by the automation hook on the previous frame,
        // after that frame's UI had already been rendered.
        if (deferredRequests.play) requests.play = true;
        if (deferredRequests.stop) requests.stop = true;
        deferredRequests = EditorRequests{};
        // Export automation submits the same UI request during an active ImGui
        // frame. HandleRequests also owns modal UI and cannot run after Render.
        const char* exportDestination = std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT");
        const bool automatedExport = autotest && autotestFrame == 14 && exportDestination;
        if (automatedExport) {
            m_panels.exportDestination = exportDestination;
            requests.exportProject = true;
        }
        if(importRecipe&&autotestFrame==1)requests.dropMeshAssetId=m_panels.importAccepted->assetId;
        HandleRequests(requests);
        if (automatedExport)
            std::fprintf(stderr, "[editor autotest] export project: %s\n", m_panels.runProjectInfo.c_str());
        AdvanceStabilizationAutomation();

        DrawModelImportPreview(deltaSeconds);
        DrawUIAuthoringPreview();
        editorBuildScope.End();
        ImGui::Render();
        // The 3D frame already sits in the default framebuffer; the UI
        // composites over it through ImGui's own GL backend.
        { JUDAS_PROFILE_SCOPE("Editor UI submission"); RendererProfileScope uiGPU(renderer,"Editor UI"); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }

        if(m_panels.mode==EditorMode::Edit&&m_panels.textPreview&&m_panels.showAssetBrowser){
            auto key=m_project.ProjectFile()+m_project.Settings().localization.Encode();if(key!=m_previewLocalizationKey){m_previewLocalization.reset();m_previewLocalization=std::make_unique<LocalizationSession>(m_project.Settings().localization);m_previewLocalization->Bind(&host.Resources());m_previewLocalizationKey=key;}
            std::string previewError;if(!m_panels.previewLocale.empty()&&m_previewLocalization->Locale()!=m_panels.previewLocale)m_previewLocalization->SetLocale(m_panels.previewLocale,previewError);
            if(m_panels.textReload){m_previewLocalization->Reload();m_panels.textReload=false;}m_previewLocalization->Refresh();
            auto& e=m_panels.textElement;auto text=e.text;if(!e.textKey.empty())text=m_previewLocalization->Format(e.textKey,{},previewError);
            std::vector<std::shared_ptr<const TextFont>> fonts;if(!e.font.empty()){host.Resources().RequestFont(e.font);if(auto f=host.Resources().TryGetFont(e.font))fonts.push_back(f);}if(fonts.empty())if(auto f=renderer.DefaultTextFont())fonts.push_back(f);auto fallback=m_previewLocalization->Fonts();fonts.insert(fonts.end(),fallback.begin(),fallback.end());renderer.SelectTextFonts(std::move(fonts));
            TextOptions o;o.pixels=e.fontSize;o.width=m_panels.textPreviewSize.x;o.wrap=e.wrap;o.locale=m_previewLocalization->Locale();o.direction=e.direction;o.alignment=e.textLogicalAlign<0?TextAlignment::Left:TextAlignment(e.textLogicalAlign);
            renderer.BeginUIFrame(window.Width(),window.Height());renderer.SetUIClip(m_panels.textPreviewPosition,m_panels.textPreviewSize);renderer.DrawUIRect(m_panels.textPreviewPosition,m_panels.textPreviewSize,{.04,.05,.08,1});renderer.DrawTextLayout(*renderer.LayoutText(text,o),m_panels.textPreviewPosition,e.color);renderer.ClearUIClip();renderer.EndUIFrame();
        }

        if (autotest) {
            bool waiting=importRecipe&&!m_panels.modelPreviewToken;
            if(waiting&&((m_panels.importTask->done&&!m_panels.importTask->success)||++importWaitFrames>4000)){std::fprintf(stderr,"[editor autotest] import FAIL: %s\n",m_panels.importTask->error.c_str());m_quit=true;}
            if(!waiting)++autotestFrame;
            if(importRecipe&&autotestFrame==1)std::fprintf(stderr,"[editor autotest] shared import + GPU preview PASS (%zu parts)\n",m_panels.importAccepted->preview->primitives.size());
            if(importRecipe&&autotestFrame==2){auto* placed=m_document.SelectedObject();std::fprintf(stderr,"[editor autotest] ordinary imported placement %s\n",placed&&placed->render&&placed->animation?"PASS":"FAIL");m_document.Undo();std::string restored;SaveSceneToString(m_document.GetScene(),restored);std::fprintf(stderr,"[editor autotest] imported placement one undo %s\n",restored==autotestBaseline?"PASS":"FAIL");}
            if(importRecipe&&autotestFrame==6){m_panels.modelPreviewPlaying=true;m_panels.modelPreviewTime=.5f;}
            if(importRecipe&&autotestFrame==12){auto& parts=m_panels.importAccepted->preview->primitives;if(!parts.empty())m_panels.modelPreviewHidden={parts.front().part};}

            const std::string prefix = autotest;
            // Opt-in M56 diagnostic interaction; ordinary Play timing/state is unchanged.
            if (std::getenv("JUDAS_EDITOR_AUTOTEST_PROFILER")) {
                if (autotestFrame == 23) io.AddKeyEvent(ImGuiKey_F8, true);
                if (autotestFrame == 24) io.AddKeyEvent(ImGuiKey_F8, false);
                if (autotestFrame == 28) {
                    auto frames = PerformanceProfiler::Get().Timeline();
                    profilerFrozenFrame = frames.empty() ? 0 : frames.back().id;
                    profilerFrozenSteps = m_play->FixedStepsSinceReset();
                    PerformanceProfiler::Get().Freeze(true);
                }
                if (autotestFrame == 40) {
                    auto frames = PerformanceProfiler::Get().Timeline();
                    bool ok = profilerFrozenFrame && !frames.empty() && frames.back().id == profilerFrozenFrame &&
                        m_play->FixedStepsSinceReset() > profilerFrozenSteps && !m_play->IsPaused() &&
                        !window.IsMouseCaptured() && m_world->pointerCapture;
                    std::fprintf(stderr, "[editor autotest] live cursor + frozen capture while simulation continues: %s\n", ok ? "PASS" : "FAIL");
                    PerformanceProfiler::Get().Freeze(false);
                    m_panels.showProfiler = false;
                }
                if (autotestFrame == 43) {
                    std::fprintf(stderr, "[editor autotest] closing profiler restores capture: %s\n", window.IsMouseCaptured() ? "PASS" : "FAIL");
                    m_panels.profilerInspect = false;
                    m_panels.showProfiler = true;
                }
            }
            if (autotestFrame == 5) {
                if(std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING")) {
                    m_panels.showWorldBuilding=true;m_panels.showUIDocument=true;m_panels.uiCanvasPreview=true;
                    for(auto& [id,record]:m_host->Assets().Records())if(record.type==AssetType::UI){m_panels.browserSelection=id;break;}
                    if(const char* locale=std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING_LOCALE"))m_panels.previewLocale=locale;
                    auto& objects=m_document.GetScene().Objects();
                    if(objects.size()>2){m_document.Select(objects[0].id);m_document.Select(objects[2].id,true);std::string error;
                        bool edited=m_document.BatchTransform({.25f,0,0},{1,0,0,0},{1,1,1},error,true,true);m_document.Undo();
                        std::string restored;SaveSceneToString(m_document.GetScene(),restored);
                        std::fprintf(stderr,"[editor autotest] shared authoring batch + undo %s: %s\n",edited&&restored==autotestBaseline?"PASS":"FAIL",error.c_str());
                    }
                }
                if(std::getenv("JUDAS_EDITOR_AUTOTEST_STREAMING"))m_panels.worldPreview=true;
                if (!m_document.GetScene().Objects().empty()) m_document.Select(m_document.GetScene().Objects().front().id);
                DebugViewOptions all;
                all.collisionShapes = all.playerCapsule = all.contacts = all.gravity = all.frameAxes = all.lights =
                    all.interactionRanges = all.lifecycle = all.terrainNormals = all.fluidParticles = all.atmosphere = true;
                m_panels.debug = all;
                m_panels.showProfiler = !std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING");
            } else if (autotestFrame == 10) {
                // Duplicate + undo must leave the scene exactly as authored.
                const SceneObjectId selected = m_document.Selected();
                if (selected != kInvalidSceneObjectId) {
                    const SceneObjectId copy = DuplicateObject(m_document, selected);
                    std::string afterDuplicate;
                    SaveSceneToString(m_document.GetScene(), afterDuplicate);
                    m_document.Undo();
                    std::string afterUndo;
                    SaveSceneToString(m_document.GetScene(), afterUndo);
                    std::fprintf(stderr, "[editor autotest] duplicate created id %llu: scene %s; undo restores: %s\n",
                                 static_cast<unsigned long long>(copy), afterDuplicate != autotestBaseline ? "CHANGED" : "unchanged(!)",
                                 afterUndo == autotestBaseline ? "IDENTICAL" : "DIFFERENT");
                    m_document.Select(selected);
                }
            } else if (autotestFrame == 15 && std::getenv("JUDAS_EDITOR_AUTOTEST_RUN")) {
                // Launch the runtime on the project exactly as Run project
                // does; with JUDAS_TEST_SCRIPT inherited, the child runs the
                // harness on the startup scene and exits by itself.
                std::string message;
                const bool launched = RunProject(message);
                std::fprintf(stderr, "[editor autotest] run project: %s: %s\n", launched ? "LAUNCHED" : "FAILED", message.c_str());
            } else if (autotestFrame == 20) {
                screenshot(prefix + ".edit.png");
                if(std::getenv("JUDAS_EDITOR_AUTOTEST_STREAMING"))std::fprintf(stderr,"[editor autotest] additive preview: %s (%zu sources)\n",!m_regionPreview.empty()?"PASS":"FAIL",m_regionPreview.size());
                std::fprintf(stderr, "[editor autotest] edit frame: %u draw calls, %u triangles, %u debug lines\n",
                             m_panels.profiler.drawCalls, m_panels.profiler.triangles, m_panels.profiler.debugLines);
                deferredRequests.play = true;
            } else if (autotestFrame == 140) {
                screenshot(prefix + ".play.png");
                if(std::getenv("JUDAS_EDITOR_AUTOTEST_STREAMING")){std::string error;auto* stream=m_world->SceneControl()->Streaming(*m_world,error);std::fprintf(stderr,"[editor autotest] shared world additive activation: %s\n",stream&&stream->Resolve("gallery-0",100)?"PASS":"FAIL");}
                const ProfilerData& p = m_panels.profiler;
                std::fprintf(stderr, "[editor autotest] play frame: %d steps, step %.3f ms, %zu bodies, %zu contacts, %u draw calls, %u triangles, %u shadow passes, %u lights, %u debug lines, %zu particles\n",
                             p.fixedStepsThisFrame, p.fixedStepMilliseconds, p.physicsBodies, p.contacts, p.drawCalls, p.triangles,
                             p.shadowPasses, p.dynamicLights, p.debugLines, p.fluidParticles);
                std::fprintf(stderr, "[editor autotest] resources: %zu meshes, %zu textures, %zu terrain meshes, hits %llu, misses %llu, failed %zu, uploads %llu, resident %llu bytes\n",
                             p.resources.loadedMeshes, p.resources.loadedTextures, p.resources.loadedTerrainMeshes,
                             p.resources.hits, p.resources.misses, p.resources.failed, p.resources.uploads,
                             static_cast<unsigned long long>(p.resources.bytesResident));
                std::fprintf(stderr, "[editor autotest] jobs: %u workers, %llu completed, %llu failed, %llu cancelled\n",
                             p.jobs.workers, p.jobs.completed, p.jobs.failed, p.jobs.cancelled);
                deferredRequests.stop = true;
            } else if (autotestFrame == 150) {
                std::string after;
                SaveSceneToString(m_document.GetScene(), after);
                std::fprintf(stderr, "[editor autotest] authored scene after play/stop is %s\n",
                             after == autotestBaseline ? "IDENTICAL" : "DIFFERENT");
                std::string e;
                const bool saved = SaveSceneToFile(m_document.GetScene(), prefix + ".saved.judas", e);
                std::fprintf(stderr, "[editor autotest] save %s\n", saved ? "ok" : e.c_str());
                m_quit = true;
            }
        }
        { JUDAS_PROFILE_WAIT("Swap present wait"); window.SwapBuffers(); }
    }

    if (m_panels.mode == EditorMode::Play) StopPlay();
    for (const std::string& id : m_heldAssets) host.Resources().ReleaseRef(id);
    m_heldAssets.clear();
    window.SetEventHook(nullptr);
    if(m_uiPreviewTarget.IsValid())renderer.DestroyRenderTarget(m_uiPreviewTarget);
    m_uiPreview.reset();
    if(m_modelPreviewTarget.IsValid())renderer.DestroyRenderTarget(m_modelPreviewTarget);
    if(!m_modelPreviewAsset.empty())host.Resources().ReleaseRef(m_modelPreviewAsset);
    m_panels.importTask.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_previewLocalization.reset();
    m_host = nullptr;
    return m_stabilization && m_stabilization->failures ? 1 : 0;
}

void EditorApplication::DrawUIAuthoringPreview(){
 if(m_panels.mode!=EditorMode::Edit||!m_panels.showAssetBrowser||!m_panels.uiCanvasPreview||!m_panels.uiPreviewRevision)return;
 JUDAS_PROFILE_SCOPE("UI authoring canvas");auto& renderer=m_host->GetRenderer();auto& resources=m_host->Resources();auto key=m_project.ProjectFile()+m_project.Settings().localization.Encode();if(key!=m_previewLocalizationKey){m_previewLocalization=std::make_unique<LocalizationSession>(m_project.Settings().localization);m_previewLocalization->Bind(&resources);m_previewLocalizationKey=key;}
 std::string error;if(!m_panels.previewLocale.empty())m_previewLocalization->SetLocale(m_panels.previewLocale,error);m_previewLocalization->Refresh();
 if(!m_uiPreview||m_uiPreviewRevision!=m_panels.uiPreviewRevision){auto candidate=std::make_unique<RuntimeUI>(&resources);if(!candidate->Add(m_panels.uiPreviewDocument,"authoring",0,error)){m_panels.status=error;return;}m_uiPreview=std::move(candidate);m_uiPreviewRevision=m_panels.uiPreviewRevision;}
 m_uiPreview->SetLocalization(m_previewLocalization.get());int width=m_panels.uiPreviewResolution.x,height=m_panels.uiPreviewResolution.y;if(!renderer.ResizeRenderTarget(m_uiPreviewTarget,width,height,error)||!renderer.BeginRenderTarget(m_uiPreviewTarget)){m_panels.status=error;return;}
 renderer.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.035f,.045f,.065f});renderer.BeginFrame(width,height);renderer.EndFrame();renderer.BeginUIFrame(width,height);m_uiPreview->Draw(renderer,width,height);renderer.EndUIFrame();renderer.EndRenderTarget();m_panels.uiPreviewToken=renderer.EditorImageToken(renderer.RenderTargetTexture(m_uiPreviewTarget));m_panels.uiPreviewLayout.clear();auto handle=m_uiPreview->Find("authoring");for(auto& e:m_panels.uiPreviewDocument.elements)if(auto* l=m_uiPreview->LayoutOf(handle,e.id))m_panels.uiPreviewLayout[e.id]=*l;
 if(m_panels.uiCanvasPick.x>=0){for(auto it=m_panels.uiPreviewDocument.elements.rbegin();it!=m_panels.uiPreviewDocument.elements.rend();++it){auto* l=m_uiPreview->LayoutOf(handle,it->id);if(l&&l->visible&&l->rect.Contains(m_panels.uiCanvasPick)&&l->clip.Contains(m_panels.uiCanvasPick)){m_panels.uiCanvasSelected=it->id;break;}}m_panels.uiCanvasPick={-1,-1};}
}
