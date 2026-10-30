#include "ScriptTools.h"
#include "ProjectStarter.h"
#include "EditorScriptWorkflow.h"
#include "EditorPrimitiveAssets.h"
#include "EditorPauseExample.h"
#include "BuildIdentity.h"
#include "Ragdoll.h"
#include "ModelCook.h"
#include "RetargetPanel.h"
#include <glm/gtc/matrix_transform.hpp>
#include "WorldStreaming.h"
#include "RuntimeInspector.h"
#include "Prefab.h"
#include "PerformanceProfiler.h"
#include "ComponentEditors.h"
#include "EditorApplication.h"
#include "EditorCreationMenu.h"
#include "EditorAssetActions.h"
#include "TerrainTools.h"
#include "VolumeTerrainTools.h"
#include "ModTools.h"
#include "ModAuthoring.h"
#include "ModelArchive.h"
#include "AsyncFile.h"
#include "Prefab.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
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
#include "AnimationTimeline.h"
#include "SpriteSheetEditor.h"
#include "TilemapEditor.h"
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
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "../../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr const char* kProjectFileHint = "Relative to the working directory or absolute, e.g. projects/tiny_game/tiny_game.judasproj";

// Framing and non-geometric selection outlines use a generous size hint.
// Actual viewport selection is handled by EditorScenePicker's geometry tests.
float PickRadius(const SceneObject& o) {
    float radius = 0.75f;
    if (o.render) {
        if (o.render->shape == SceneShape::Sphere) radius = std::max(radius, o.render->radius);
        else if(o.render->shape==SceneShape::Sprite) radius=glm::length(o.render->sprite.size*glm::vec2(o.transform.scale));
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
    m_planetPreview.clear();
    if(m_terrain){m_terrain->Shutdown(m_host->GetRenderer(),m_panels.importJobs);m_terrain.reset();}
    if(m_volumeTerrain){m_volumeTerrain->Shutdown(m_host->GetRenderer(),m_panels.importJobs);m_volumeTerrain.reset();}
    RetireRetargetProject(m_panels);
    auto retiringContent=m_project.ModSession(); // retain leases until old jobs are joined
    m_project = project;
    m_panels.view2D = m_project.Settings().editor2D;
    m_camera.Set2D(m_panels.view2D);
    m_scenePicker.Clear();
    m_host->OpenProjectAssets(m_project.RootDir(), m_project.AssetsDir());
    m_panels.projectHistory.reset();
    m_host->Resources().ConfigureVisualStreaming(uint64_t(m_project.Settings().visualBudgetMiB)*1024*1024);
    {std::string assetError;if(!JudasMods::OpenAuthorAssets(m_project,m_host->Assets(),assetError))m_panels.status=assetError;}
    if(!m_host->GetWindow().Input().SetMap(m_project.Settings().input,outError))return false;
    RefreshProjectLists();
    m_panels.showProjectSettings = false;
    m_panels.browserSelection.clear();
    return true;
}

bool EditorApplication::CreateProject(const std::string& directory, const std::string& name, std::string& outError, bool visibleStarter, int twoDTemplate) {
    Project project;
    if (!Project::CreateNew(directory, name, project, outError)) return false;
    // A starter scene so the new project runs immediately.
    const std::string sceneRelative = project.Settings().scenesDir + "/main.judas";
    Scene starter=twoDTemplate ? Scene{} : MakeStarterScene();
    if(twoDTemplate&&!PopulatePlanarProjectStarter(project,starter,twoDTemplate==2,outError))return false;
    if(visibleStarter&&!twoDTemplate&&!PopulateVisibleProjectStarter(project,starter,outError))return false;
    if (!SaveSceneToFile(starter, project.Resolve(sceneRelative), outError)) return false;
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
        m_camera.LookAt(first->transform.position, m_camera.Is2D()?5.0f:40.0f);
    }
    return true;
}

bool EditorApplication::RunProject(std::string& outMessage) {
    if (!m_project.IsLoaded()) { outMessage = "no project open"; return false; }
    Project saved;
    if(!saved.Load(m_project.ProjectFile(),outMessage))return false;
    if (saved.Settings().startupScene.empty()) { outMessage = "the saved project has no startup scene (Project settings)"; return false; }
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
    if(m_terrain&&!m_terrain->Guard(m_panels)){outError=m_terrain->error;return false;}
    if(m_volumeTerrain&&!m_volumeTerrain->Guard(m_panels)){outError=m_volumeTerrain->error;return false;}
    // Source freshness is an editor/export check; packaged runtime needs only products.
    for(const auto& o:m_document.GetScene().Objects())if(o.render&&!o.render->meshAsset.empty()){auto* a=m_host->Assets().Find(o.render->meshAsset);if(a&&!a->readOnly&&fs::path(a->path).extension()==".judasmodel"){std::vector<uint8_t> bytes;std::string record;std::vector<MaterialDefinition> materials;if(ReadWholeFile(a->path,bytes,outError)&&ReadModelArchiveMetadata(bytes.data(),bytes.size(),record,materials,outError)&&!record.empty()){auto provenance=nlohmann::json::parse(record);if((provenance.value("kind",std::string())=="terrain"||provenance.value("kind",std::string())=="volume-terrain")&&!VerifyImportedModelFresh(m_project.RootDir(),a->path,outError))return false;}}}
    Project playedProject=m_project;
    if(!JudasMods::PrepareSession(playedProject,false,outError))return false;
    Scene playedScene=m_document.GetScene();
    if(playedProject.ModSession()){
        const auto logical=m_project.MakeRelative(m_document.Path());
        if(playedProject.ModSession()->sceneFiles.count(logical)&&!LoadSceneFromFile(playedProject.Resolve(logical),playedScene,outError))return false;
        m_host->OpenContentView(playedProject.ModSession()->assets);
        m_project.SetModSession(playedProject.ModSession());
    }
    CancelCameraNavigation();
    m_world = std::make_unique<RuntimeWorld>();
    m_world->legacyGameplay = m_project.Settings().legacyGameplay;
    m_world->audioGroups=m_project.Settings().audio;
    m_world->ConfigureProject(m_project.Settings());
    m_world->SetSceneControl(std::make_shared<SceneSession>(m_project,m_document.Path()));
    m_world->SceneControl()->SetEditorSaveIsolation(true);
    if (!m_world->Build(playedScene, &m_host->Resources(), outError, &m_project.Settings().classification, &m_project.Settings().navigation)) {
        StopPlay();
        return false;
    }
    // Milestone 29: a saved world-state delta for this scene file layers
    // over the freshly instantiated baseline, exactly as the runtime does.
    m_panels.worldStatePath = WorldStatePathFor(m_document.Path());
    bool stateApplied = false;
    if(m_world->IsComposed())m_panels.worldStatePath.clear();
    if (!ApplyWorldStateFileIfPresent(*m_world, m_panels.worldStatePath, stateApplied, outError)) {
        StopPlay();
        return false;
    }
    if(m_project.IsLoaded()&&!m_host->GetWindow().Input().SetMap(m_project.Settings().input,outError)){StopPlay();return false;}
    if(!m_scriptTools)m_scriptTools=std::make_unique<ScriptTools>();
    m_scriptTools->Attach(*m_world,*m_host);
    m_play = std::make_unique<InteractivePlay>();
    if (!m_play->Begin(*m_world, WorldCoordinates(playedScene.Settings().worldOrigin), outError)) {
        m_play.reset();
        StopPlay();
        return false;
    }
    m_play->SetScriptFaultNoticeEnabled(true);
    m_play->SetWorldStatePath(m_panels.worldStatePath, stateApplied);
    // The debug view over the played world, inside the 3D frame.
    m_play->SetWorldOverlay([this](Renderer& renderer, float alpha) {
        if (!m_panels.debug.AnyEnabled() || !m_world) return;
        m_debugLines.Clear();
        BuildWorldDebugLines(*m_world, &m_play->Session(), alpha, m_panels.debug, m_debugLines);
        renderer.DrawDebugLines(m_debugLines.Lines());
    });
    m_panels.runtime = m_world.get();
    m_panels.renderPreviewHistory.reset();
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
    CancelCameraNavigation();
    if (m_play) m_play->End();
    m_play.reset();
    m_world.reset();  // the authored Scene was never written; nothing to revert
    if(m_project.ModSession()){
        auto retiringContent=m_project.ModSession(); // ResourceManager release joins late readers
        m_host->OpenProjectAssets(m_project.RootDir(),m_project.AssetsDir());
        m_project.SetModSession({});
        std::string error;if(!JudasMods::OpenAuthorAssets(m_project,m_host->Assets(),error))m_panels.status=error;
    }
    m_panels.runtime = nullptr;
    m_panels.renderPreviewHistory.reset();
    m_panels.mode = EditorMode::Edit;
    m_panels.runtimeInfo.clear();
    m_panels.status = "Stopped: authored scene restored";
    m_host->GetWindow().SetMouseCaptured(false);
    m_host->GetWindow().Input().DiscardStickHistory();
}

void EditorApplication::HandleRequests(EditorRequests& r) {
    if (r.checkMilestones) { m_milestoneNotice.Check(true, m_preferences.updateChannel); m_panels.status = "Checking this build channel for updates..."; }
    std::string error;
    bool approvedDocumentAction = false;
    if (m_documentActionApproved) {
        r = m_pendingDocumentAction;
        m_pendingDocumentAction = {};
        m_documentActionApproved = m_documentActionPending = false;
        approvedDocumentAction = true;
    }
    if (r.quit && (m_document.IsDirty()||m_project.IsDirty()) && m_panels.mode == EditorMode::Play) StopPlay();
    const bool terrainReplaces = r.quit || r.newScene || r.open || r.newProject || r.openProject || r.play || r.runProject || r.exportProject || !r.openSceneRelative.empty();
    if(terrainReplaces&&m_spriteSheetEditor&&m_spriteSheetEditor->Dirty()){
        m_panels.showSpriteSheetEditor=true;
        m_panels.status="Save or discard the sprite-sheet draft before leaving, playing or exporting";
        return;
    }
    if(terrainReplaces&&m_terrain&&!m_terrain->Guard(m_panels))return;
    if(terrainReplaces&&m_volumeTerrain&&!m_volumeTerrain->Guard(m_panels))return;
    const bool replacesDocument = r.quit || r.newScene || r.open || r.newProject || r.openProject ||
                                  !r.openSceneRelative.empty();
    const bool replacesProject=r.quit||r.newProject||r.openProject;
    const bool shipping=r.runProject||r.exportProject;
    const bool sceneDirty=m_document.IsDirty()&&(replacesDocument||shipping);
    const bool projectDirty=m_project.IsDirty()&&(replacesProject||shipping);
    if ((sceneDirty||projectDirty) && !approvedDocumentAction) {
        m_pendingDocumentAction=r;
        m_actionNeedsScene=sceneDirty;m_actionNeedsProject=projectDirty;
        m_confirmationTitle=shipping?"Save before run or export":
            sceneDirty?(projectDirty?"Unsaved changes":"Unsaved scene"):"Unsaved project";
        m_documentActionPending=true;
        r={};
        CancelCameraNavigation();
    }
    auto savePending=[&](){
        if(m_actionNeedsScene&&m_document.IsDirty()){
            if(m_document.Path().empty()){m_documentActionSaveAs=true;return false;}
            if(!m_document.Save(error)){m_panels.status="Save failed: "+error;return false;}
            RefreshProjectLists();
        }
        if(m_actionNeedsProject&&m_project.IsDirty()&&!m_project.Save(error)){
            m_panels.status="Save failed: "+error;return false;
        }
        m_documentActionApproved=true;return true;
    };
    if(m_documentActionPending&&!m_documentActionApproved&&!m_documentActionSaveAs)
        ImGui::OpenPopup(m_confirmationTitle.c_str());
    if (m_documentActionSaveAs && !m_fileBrowser.Active() && !ImGui::IsPopupOpen("Save scene as")) r.saveAs = true;
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
    if (r.openJudasJS || r.openQuickStart)
        r.openExternalPath = ResolveEngineDataPath(r.openQuickStart ? "docs/judasjs/getting-started.md" : "docs/JUDASJS.md");
    if (r.openWorkflowGuide) r.openExternalPath = ResolveEngineDataPath("docs/DEVELOPER_WORKFLOWS.md");
    if (!r.openExternalPath.empty()) {
        std::error_code ec;
        const auto path = fs::absolute(r.openExternalPath, ec);
        if (ec || !fs::is_regular_file(path, ec)) m_panels.status = "File unavailable: " + r.openExternalPath;
        else {
            // SDL hands a local URI to the user's default application. Escape
            // path bytes rather than interpolating a filename into shell code.
            const auto generic = path.generic_string();
            std::string uri = generic.empty() || generic.front() != '/' ? "file:///" : "file://";
            constexpr char hex[] = "0123456789ABCDEF";
            for (const unsigned char c : generic) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') uri += static_cast<char>(c);
                else { uri += '%'; uri += hex[c >> 4]; uri += hex[c & 15]; }
            }
            CancelCameraNavigation();
            m_panels.status = SDL_OpenURL(uri.c_str()) == 0 ? "Requested open: " + path.string() :
                "Could not open file; copy its path instead: " + std::string(SDL_GetError());
        }
    }
    if (m_panels.mode != EditorMode::Edit) return;

    // --- Project ---
    if(r.browseImport){m_fileBrowserParent.clear();m_fileBrowser.Open(EditorFileBrowser::Target::Import,EditorFileBrowser::Mode::OpenFile,m_panels.importSourceInput);}
    if(r.browseExport){m_fileBrowserParent.clear();m_fileBrowser.Open(EditorFileBrowser::Target::Export,EditorFileBrowser::Mode::Directory,m_panels.exportDestination);}
    if (r.newProject) ImGui::OpenPopup("New project");
    if (r.openProject) ImGui::OpenPopup("Open project");
    if (r.saveProject) m_panels.status = m_project.Save(error) ? "Saved project " + m_project.ProjectFile() : "Project save failed: " + error;
    if (r.exportProject) {
        ProjectExportOptions options; options.destination = m_panels.exportDestination;
        ProjectExportResult result;
        Project saved;
        const bool exported = saved.Load(m_project.ProjectFile(),error) && ExportProject(saved, options, result, error);
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
        if (r.rescanAssets){m_host->OpenProjectAssets(m_project.RootDir(), m_project.AssetsDir());std::string assetError;if(!JudasMods::OpenAuthorAssets(m_project,m_host->Assets(),assetError))m_panels.status=assetError;}
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
    if (r.undo) {if(m_volumeTerrain&&m_volumeTerrain->loaded&&m_volumeTerrain->editing)m_volumeTerrain->draft.Undo();else if(m_terrain&&m_terrain->loaded&&m_terrain->editing)m_terrain->draft.Undo();else m_document.Undo();}
    if (r.redo) {if(m_volumeTerrain&&m_volumeTerrain->loaded&&m_volumeTerrain->editing)m_volumeTerrain->draft.Redo();else if(m_terrain&&m_terrain->loaded&&m_terrain->editing)m_terrain->draft.Redo();else m_document.Redo();}
    if (!r.createKind.empty()) {
        std::string defaultMesh;
        for (const auto& [id, record] : m_host->Assets().Records()) {
            if (record.type == AssetType::Mesh && !record.missing) { defaultMesh = id; break; }
        }
        if(r.createKind=="planar-player"||r.createKind=="planar-topdown-player"){
            std::string error;
            CreateEditorPlanarPlayer(m_document,m_project,m_host->Assets(),r.createPosition,r.createParent,error,r.createKind=="planar-player");
            m_panels.status=error.empty()?"Created 2D player. Set scene gravity for platformers; edit its project script in Assets.":error;
        } else if(r.createKind=="first-person-player"||r.createKind=="third-person-player"){
            std::string error;
            const auto id=CreateEditorFirstPersonPlayer(m_document,m_project,m_host->Assets(),r.createPosition,r.createParent,error,r.createKind=="third-person-player");
            m_panels.status=id?"Player created. Save scene, then Play: WASD / mouse / Space; V switches camera. Escape releases mouse. Unedited starter camera disabled.":error;
            if(id)m_panels.revealComponent="Character motor";
        }else if(r.createKind=="pause-menu-example"){
            std::string error;
            const auto id=CreateEditorPauseExample(m_document,m_project,m_host->Assets(),error,r.createParent);
            m_panels.status=id?"Optional pause menu created. Save scene; Escape opens Pause / Settings / Controls during Play.":error;
            if(id)m_panels.revealComponent="Scripts";
        }else if(r.createKind=="planar-circle"||r.createKind=="planar-capsule"){
            std::string error;
            const auto id=CreateEditorPlanarPrimitive(m_document,m_project,m_host->Assets(),r.createKind,r.createPosition,r.createParent,error);
            m_panels.status=id?"Created native 2D collision with a matching editable sprite image":error;
        }else if(r.createKind=="capsule"||r.createKind=="cylinder"){
            std::string error;
            const auto id=CreateEditorPrimitive(m_document,m_project,m_host->Assets(),r.createKind,r.createPosition,r.createParent,error);
            m_panels.status=id?"Created "+r.createKind+" with editable mesh and convex collision. Body > Motion selects static/dynamic.":error;
        }else{
            const auto created = CreateObjectOfKind(m_document, r.createKind, r.createPosition, r.createKind=="sprite"?std::string{}:defaultMesh, nullptr, r.createParent);
            m_panels.status = created ? "Created " + r.createKind : "Creation failed: parent no longer exists";
        }
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
        Scene flattened;
        if (FlattenHierarchy(m_document.GetScene(), flattened, error))
        if (const SceneObject* o = flattened.Find(m_document.Selected())) {
            m_camera.LookAt(o->transform.position, std::max(4.0f, PickRadius(*o) * 3.0f));
        }
    }

    // Path popups (the editor has no OS file dialog; a path field is enough
    // — see docs/ARCHITECTURE.md, "Milestone 30, Limitations").
    const auto pathPopup = [&](const char* title, const char* button, const char* label, const char* hint, auto&& action) {
        ImGui::SetNextWindowSizeConstraints({440, 0}, {std::max(440.0f, ImGui::GetIO().DisplaySize.x - 48), 600});
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        char buffer[512];
        std::strncpy(buffer, m_panels.pathInput.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        ImGui::TextUnformatted(label);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        const bool enter = ImGui::InputText("##path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        m_panels.pathInput = buffer;
        ImGui::PushTextWrapPos(0); ImGui::TextDisabled("%s", hint); ImGui::PopTextWrapPos();
        if ((ImGui::Button(button) || enter) && !m_panels.pathInput.empty()) {
            if (action(m_panels.pathInput)) ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Browse...")){
            m_fileBrowserParent=title;
            const bool saving=std::strcmp(title,"Save scene as")==0;
            const bool project=std::strcmp(title,"Open project")==0;
            auto start=m_panels.pathInput;
            if(start.empty())start=m_project.IsLoaded()?m_project.RootDir():".";
            else if(m_project.IsLoaded()&&!fs::u8path(start).is_absolute())start=m_project.Resolve(start);
            m_fileBrowser.Open(EditorFileBrowser::Target::Document,saving?EditorFileBrowser::Mode::SaveFile:EditorFileBrowser::Mode::OpenFile,start,project?".judasproj":".judas");
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (m_documentActionSaveAs && std::strcmp(title, "Save scene as") == 0) {
                m_documentActionSaveAs = m_documentActionPending = false;
                m_pendingDocumentAction = {};
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PushTextWrapPos(0); ImGui::TextDisabled("%s", m_panels.status.c_str()); ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    };
    pathPopup("Open scene", "Open", "Path (.judas)", "Relative to the project root, e.g. Scenes/main.judas",
              [&](const std::string& path) {
                  std::string e;
                  const bool opened = OpenScene(path, e);
                  m_panels.status = opened ? "Opened " + path : "Open failed: " + e;
                  return opened;
              });
    pathPopup("Save scene as", "Save", "Path (.judas)", "Relative to the project root, e.g. Scenes/level2.judas",
              [&](const std::string& path) {
                  std::string e;
                  const std::string resolved = m_project.IsLoaded() && !fs::path(path).is_absolute() ? m_project.Resolve(path) : path;
                  const bool saved = m_document.SaveAs(resolved, e);
                  m_panels.status = saved ? "Saved " + resolved : "Save failed: " + e;
                  RefreshProjectLists();
                  if (saved && m_documentActionSaveAs) {
                      m_documentActionSaveAs = false;
                      savePending();
                  }
                  return saved;
              });
    pathPopup("Open project", "Open", "Project file (.judasproj)", kProjectFileHint, [&](const std::string& path) {
        std::string e;
        if (!OpenProject(path, e)) { m_panels.status = "Open project failed: " + e; return false; }
        m_panels.status = "Opened project " + m_project.Settings().name;
        if (!m_project.Settings().startupScene.empty()) {
            if (!OpenScene(m_project.Settings().startupScene, e)) m_panels.status = "Project opened; startup scene failed: " + e;
        } else {
            m_document.NewScene();
        }
        return true;
    });
    if (ImGui::BeginPopupModal("New project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        char directory[4096],name[128];
        std::snprintf(directory,sizeof(directory),"%s",m_newProjectDirectory.c_str());
        std::snprintf(name,sizeof(name),"%s",m_newProjectName.c_str());
        if(ImGui::InputTextWithHint("Directory","Choose a new project folder",directory,sizeof(directory)))m_newProjectDirectory=directory;
        ImGui::SameLine();if(ImGui::Button("Browse...")){m_fileBrowserParent="New project";m_fileBrowser.Open(EditorFileBrowser::Target::NewProject,EditorFileBrowser::Mode::Directory,m_newProjectDirectory);}
        if(ImGui::InputTextWithHint("Name","My Game",name,sizeof(name)))m_newProjectName=name;
        ImGui::Combo("Template",&m_newProjectTemplate,"3D script starter\0Empty 3D scene\0New 2D game - top-down\0New 2D game - side-view\0");
        ImGui::TextWrapped(m_newProjectTemplate>=2?"Native 2D collision, an orthographic camera and editable project controller. Side-view adds gravity and jumping; top-down uses planar movement.":m_newProjectTemplate==0?"A rotating object, an ordinary JavaScript camera and an editable script. No hidden gameplay controller.":"A floor, light and gravity. Add your own scripts, player and camera.");
        if (ImGui::Button("Create") && !m_newProjectDirectory.empty() && !m_newProjectName.empty()) {
            std::string e;
            if(CreateProject(m_newProjectDirectory,m_newProjectName,e,m_newProjectTemplate==0,m_newProjectTemplate>=2?m_newProjectTemplate-1:0)){
                m_panels.status="Created project "+m_project.ProjectFile();ImGui::CloseCurrentPopup();
            }else m_panels.status="New project failed: "+e;
        }
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
        ImGui::TextWrapped("%s",m_panels.status.c_str());
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({460, 0}, ImGuiCond_Appearing);
    if (m_documentActionPending&&!m_confirmationTitle.empty()&&ImGui::BeginPopupModal(m_confirmationTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool shipping=m_pendingDocumentAction.runProject||m_pendingDocumentAction.exportProject;
        ImGui::TextUnformatted(shipping?"Run and export use saved project and scene files.":"Save your changes before continuing?");
        if(m_actionNeedsScene)ImGui::BulletText("Scene: %s",m_document.Path().empty()?"Untitled":m_document.Path().c_str());
        if(m_actionNeedsProject)ImGui::BulletText("Project settings: %s",m_project.Settings().name.c_str());
        if(ImGui::Button(shipping?"Save all and continue":"Save and continue")){
            if(savePending()||m_documentActionSaveAs)ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button(shipping?"Use saved version":"Discard")){
            if(shipping||!m_actionNeedsProject||m_project.DiscardSettings(error)){
                m_documentActionApproved=true;ImGui::CloseCurrentPopup();
            }else m_panels.status="Discard failed: "+error;
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")||ImGui::IsKeyPressed(ImGuiKey_Escape)){
            m_documentActionPending=m_documentActionApproved=m_documentActionSaveAs=false;
            m_pendingDocumentAction={};ImGui::CloseCurrentPopup();
        }
        if(m_panels.status.rfind("Save failed:",0)==0)ImGui::TextWrapped("%s",m_panels.status.c_str());
        ImGui::EndPopup();
    }
    if(auto selected=m_fileBrowser.Draw()){
        if(!selected->path.empty())switch(selected->target){
        case EditorFileBrowser::Target::Document:m_panels.pathInput=selected->path;break;
        case EditorFileBrowser::Target::NewProject:m_newProjectDirectory=selected->path;break;
        case EditorFileBrowser::Target::Import:m_panels.importSourceInput=selected->path;m_panels.reviewAssetImport=true;break;
        case EditorFileBrowser::Target::Export:m_panels.exportDestination=selected->path;break;
        }
        if(!m_fileBrowserParent.empty()){ImGui::OpenPopup(m_fileBrowserParent.c_str());m_fileBrowserParent.clear();}
    }
}

void EditorApplication::ViewportRay(int x, int y, glm::vec3& origin, glm::vec3& direction) const {
    const auto& view = m_panels.workspace.viewport;
    m_camera.PixelRay(x - static_cast<int>(view.position.x), y - static_cast<int>(view.position.y),
                      std::max(1, static_cast<int>(view.size.x)), std::max(1, static_cast<int>(view.size.y)),
                      origin, direction);
}

void EditorApplication::PickAtPixel(int x, int y) {
    glm::vec3 origin, direction;
    ViewportRay(x, y, origin, direction);
    std::string error;
    auto hit = m_scenePicker.Pick(m_document.GetScene(), origin, direction,
                                  &m_host->Assets(), &m_host->Resources(), &error);
    if (ImGui::GetIO().KeyAlt && error.empty()) {
        const auto hits=m_scenePicker.PickAll(m_document.GetScene(),origin,direction,&m_host->Assets(),&m_host->Resources(),&error);
        for(std::size_t i=0;i<hits.size();++i)if(hits[i].id==m_document.Selected()){
            hit=hits[(i+1)%hits.size()];break;
        }
    }
    if (!error.empty()) { m_panels.status = "Selection failed: " + error; return; }
    m_document.Select(hit.id, ImGui::GetIO().KeyCtrl);
}

void EditorApplication::UpdateGizmo(bool allowInteraction) {
    m_gizmoLines.Clear();
    SceneObject* selected = m_document.SelectedObject();
    if (!selected) {
        if (m_drag.Active()) { m_drag = GizmoDrag{}; m_document.CancelEdit(); }
        m_hoverAxis = GizmoAxis::None;
        return;
    }
    const auto pointer = ImGui::GetIO().MousePos;
    const int mx = static_cast<int>(pointer.x), my = static_cast<int>(pointer.y);
    glm::vec3 rayOrigin, rayDirection;
    ViewportRay(mx, my, rayOrigin, rayDirection);
    Scene flattened;std::string error;
    if(!FlattenHierarchy(m_document.GetScene(),flattened,error))return;
    auto presented=flattened.Find(selected->id)->transform;
    if(m_document.Selection().size()>1){presented.position={0,0,0};auto roots=m_document.SelectionRoots();for(auto id:roots)presented.position+=flattened.Find(id)->transform.position;presented.position/=float(roots.size());}
    const glm::vec3 origin = presented.position;
    // Handles keep a roughly constant on-screen size.
    m_gizmoHandleLength = m_camera.Is2D()?m_camera.OrthographicHeight()*0.14f:std::max(0.2f, glm::length(origin - m_camera.Position()) * 0.14f);
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
    // M70 authored preview uses the same CPU resolver as runtime. It never
    // mutates imported poses, and all lines remain renderer-owned debug draws.
    if(const auto* o=scene.Find(m_document.Selected());o&&o->animation&&o->render){
        auto asset=m_host->Resources().TryGetSkeletal(o->render->meshAsset);
        if(asset){auto root=o->transform;auto model=glm::translate(glm::mat4(1),root.position)*glm::mat4_cast(root.rotation)*glm::scale(glm::mat4(1),root.scale);auto pose=asset->skeleton.rest;
            if(o->animation->fullBodyIK){const auto& k=*o->animation->fullBodyIK;FullBodyIKMapping mapping;FullBodyIKResult result;std::vector<FullBodyIKTarget> targets;std::string error;
                if(PrepareFullBodyIK(asset->skeleton,k,mapping,error)&&FullBodyIKTargetsToModel(k.targets,glm::dvec3(root.position),root.rotation,root.scale,targets,error)&&SolveFullBodyIK(asset->skeleton,pose,mapping,targets,result,error)){
                    pose=result.pose;auto global=PoseGlobalMatrices(asset->skeleton,pose);
                    for(const auto& target:targets){auto chain=std::find_if(mapping.chains.begin(),mapping.chains.end(),[&](const auto& c){return c.id==target.chain;});if(chain==mapping.chains.end())continue;JointTransform actual;std::string diagnostic;if(!DecomposeRigidPose(model*global[chain->end],actual,diagnostic))continue;
                        auto desired=glm::vec3(model*glm::vec4(glm::vec3(target.position),1));auto point=actual.translation+actual.rotation*(actual.scale*target.offset);m_debugLines.Axes(desired,root.rotation*target.orientation,.13f);m_debugLines.Axes(point,actual.rotation*target.frame,.1f);m_debugLines.Line(desired,point,{1,.3f,.1f});}
                    auto reference=PoseGlobalMatrices(asset->skeleton,asset->skeleton.rest);auto center=glm::vec3(model*glm::vec4(glm::vec3(reference[mapping.root][3])+(k.rootMin+k.rootMax)*.5f,1));m_debugLines.Box(center,root.rotation,(k.rootMax-k.rootMin)*.5f*root.scale,{.2f,.8f,1});
                    for(const auto& limit:k.limits){int joint=FindSkeletonJoint(asset->skeleton,limit.joint);JointTransform frame;std::string diagnostic;if(joint>=0&&DecomposeRigidPose(model*global[joint],frame,diagnostic))m_debugLines.Axes(frame.translation,frame.rotation*limit.frame,.15f);}
                }
            }
            if(o->ragdoll&&o->ragdoll->physicalAnimation){auto global=PoseGlobalMatrices(asset->skeleton,pose);for(const auto& region:o->ragdoll->physicalAnimation->regions)if(region.enabled)for(const auto& key:region.joints){int joint=FindSkeletonJoint(asset->skeleton,key);if(joint>=0){auto point=glm::vec3(model*global[joint][3]);int parent=asset->skeleton.parents[joint];if(parent>=0)m_debugLines.Line(point,glm::vec3(model*global[parent][3]),{1,.5f,.1f});}}}
        }
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

void EditorApplication::CancelCameraNavigation() {
    m_cameraGesture.Cancel();
    if (m_host) m_host->GetWindow().SetMouseCaptured(false);
}

void EditorApplication::UpdateCameraNavigation(const EditorCameraGestureInput& input, int mouseDeltaX,
                                              int mouseDeltaY, float deltaSeconds, bool fast) {
    Window& window = m_host->GetWindow();
    const auto previous = m_cameraGesture.Active();
    m_cameraGesture.Update(input);
    const auto active = m_cameraGesture.Active();
    const bool captured = active != EditorCameraGesture::None;
    if (window.IsMouseCaptured() != captured) window.SetMouseCaptured(captured);
    // Starting capture may warp the cursor. Consume only subsequent drag
    // samples so the initial press does not jump the view.
    if (active != previous) mouseDeltaX = mouseDeltaY = 0;
    if (active == EditorCameraGesture::Pan)
        m_camera.Pan(mouseDeltaX, mouseDeltaY, std::max(1, static_cast<int>(m_panels.workspace.viewport.size.y)));
    m_camera.Update(window, deltaSeconds, active == EditorCameraGesture::Look,
                    mouseDeltaX, mouseDeltaY, fast);
    // Keep the existing 8 m object-placement plane; the camera's separate
    // focus depth only determines the pan scale after framing a selection.
    m_panels.cameraFocus = m_camera.Is2D()?m_camera.Focus():m_camera.Position() + m_camera.Forward() * 8.0f;
}

void EditorApplication::FrameEditMode(float deltaSeconds) {
    JUDAS_PROFILE_SCOPE("Editor viewport");
    Window& window = m_host->GetWindow();
    Renderer& renderer = m_host->GetRenderer();
    const ImGuiIO& io = ImGui::GetIO();
    const bool insideViewport = m_panels.workspace.viewport.Contains({io.MousePos.x, io.MousePos.y});

    if(m_camera.Is2D()!=m_panels.view2D){CancelCameraNavigation();m_camera.Set2D(m_panels.view2D);}
    EditorCameraGestureInput navigation;
    navigation.focused = m_viewportFocused;
    navigation.canStart = insideViewport && !io.WantCaptureMouse && !io.WantCaptureKeyboard && !m_drag.Active();
    navigation.cancel = io.WantCaptureKeyboard || io.WantTextInput || ImGui::IsKeyPressed(ImGuiKey_Escape);
    navigation.rightDown = !m_camera.Is2D() && io.MouseDown[ImGuiMouseButton_Right];
    navigation.middleDown = io.MouseDown[ImGuiMouseButton_Middle];
    navigation.leftDown = io.MouseDown[ImGuiMouseButton_Left];
    int dx = 0, dy = 0;
    window.GetMouseDelta(dx, dy);
    // Pan consumes viewport SDL motion directly. Game projects may rename
    // look axes; their logical input map does not redefine editor panning.
    if (m_cameraGesture.Active() == EditorCameraGesture::Pan) {
        dx = m_viewportMouseDeltaX; dy = m_viewportMouseDeltaY;
    }
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool fast = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    UpdateCameraNavigation(navigation, dx, dy, deltaSeconds, fast);
    if (EditorCameraCanDolly(navigation, m_cameraGesture.Active(), true)) {
        m_camera.Dolly(io.MouseWheel);
        m_panels.cameraFocus = m_camera.Is2D()?m_camera.Focus():m_camera.Position() + m_camera.Forward() * 8.0f;
    }

    // Gizmo first: a click on a handle is a drag, not a pick. The viewport
    // owns the mouse only when no panel does and no camera drag owns it.
    const bool viewportOwnsMouse = insideViewport && !io.WantCaptureMouse &&
        m_cameraGesture.Active() == EditorCameraGesture::None && !navigation.middleDown && !navigation.rightDown;
    bool brushOwns=false;
    if(m_terrain&&m_terrain->loaded){m_terrain->Prepare(renderer,m_panels);m_scenePicker.SetDraftMesh(m_terrain->draft.source.meshId,m_terrain->preview);
        Scene flat;std::string error;if(FlattenHierarchy(m_document.GetScene(),flat,error)){glm::vec3 ro,rd;ViewportRay(int(io.MousePos.x),int(io.MousePos.y),ro,rd);brushOwns=m_terrain->Stroke(flat,m_document.Selected(),ro,rd,viewportOwnsMouse&&ImGui::IsMouseClicked(ImGuiMouseButton_Left),viewportOwnsMouse&&io.MouseDown[ImGuiMouseButton_Left],ImGui::IsKeyPressed(ImGuiKey_Escape),m_viewportFocused&&!m_terrain->Busy(m_panels),m_panels.status);}}
    if(m_volumeTerrain&&m_volumeTerrain->loaded){m_volumeTerrain->Prepare(renderer,m_panels);
        if(m_volumeTerrain->editing){if(m_terrain)m_terrain->editing=false;m_scenePicker.SetDraftMesh(m_volumeTerrain->draft.source.meshId,m_volumeTerrain->preview);}
        Scene flat;std::string error;if(FlattenHierarchy(m_document.GetScene(),flat,error)){glm::vec3 ro,rd;ViewportRay(int(io.MousePos.x),int(io.MousePos.y),ro,rd);brushOwns|=m_volumeTerrain->Stroke(flat,m_document.Selected(),ro,rd,viewportOwnsMouse&&ImGui::IsMouseClicked(ImGuiMouseButton_Left),viewportOwnsMouse&&io.MouseDown[ImGuiMouseButton_Left],ImGui::IsKeyPressed(ImGuiKey_Escape),m_viewportFocused&&!m_volumeTerrain->Busy(m_panels),m_panels.status);}}
    if(m_tilemapEditor&&(m_tilemapEditor->painting||m_tilemapEditor->StrokeActive())){glm::vec3 origin,direction;ViewportRay(int(io.MousePos.x),int(io.MousePos.y),origin,direction);
        brushOwns|=m_tilemapEditor->Stroke(m_document,m_host->Assets(),origin,direction,viewportOwnsMouse,
            ImGui::IsMouseClicked(ImGuiMouseButton_Left),ImGui::IsMouseClicked(ImGuiMouseButton_Right),
            io.MouseDown[ImGuiMouseButton_Left],io.MouseDown[ImGuiMouseButton_Right],ImGui::IsKeyPressed(ImGuiKey_Escape),m_panels.status);}
    if(m_tilemapEditor&&m_tilemapEditor->painting){m_gizmoLines.Clear();m_hoverAxis=GizmoAxis::None;}
    else UpdateGizmo(viewportOwnsMouse&&!brushOwns);
    if (viewportOwnsMouse && !brushOwns && !m_drag.Active() && m_hoverAxis == GizmoAxis::None &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        PickAtPixel(static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y));
    }

    // Asset Browser drag released over the viewport: place the mesh where
    // the mouse ray crosses the plane through the camera focus.
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
        if (payload->IsDataType(kAssetDragPayload)) {
            m_pendingDropAsset.assign(static_cast<const char*>(payload->Data), payload->DataSize);
        }
    } else if (!m_pendingDropAsset.empty()) {
        if (insideViewport && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            const AssetRecord* record = m_host->Assets().Find(m_pendingDropAsset);
            if (record && (record->type == AssetType::Mesh || record->type == AssetType::Texture)) {
                const int mx = static_cast<int>(io.MousePos.x), my = static_cast<int>(io.MousePos.y);
                glm::vec3 rayOrigin, rayDirection;
                ViewportRay(mx, my, rayOrigin, rayDirection);
                glm::vec3 hit = m_panels.cameraFocus;
                RayPlaneIntersection(rayOrigin, rayDirection, m_panels.cameraFocus, -m_camera.Forward(), hit);
                std::string error;
                if(record->type==AssetType::Texture){
                    if(!CreateEditorSpriteFromImage(m_document,m_host->Assets(),m_pendingDropAsset,hit,kInvalidSceneObjectId,error)){
                        m_panels.status=error;
                    }else m_panels.status="Placed sprite: "+record->relativePath;
                }else{CreateObjectOfKind(m_document,"mesh",hit,m_pendingDropAsset);m_panels.status="Placed "+record->relativePath;}
            } else {
                m_panels.status = "Drop an image to create a sprite, or a mesh to create a model";
            }
        }
        m_pendingDropAsset.clear();
    }

    Scene scene;std::string hierarchyError;
    if(!FlattenHierarchy(m_document.GetScene(),scene,hierarchyError)){m_panels.status=hierarchyError;return;}
    RefreshAssetDemand();
    const auto& viewport = m_panels.workspace.viewport;
    const int width = std::max(1, static_cast<int>(viewport.size.x));
    const int height = std::max(1, static_cast<int>(viewport.size.y));
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    renderer.SetLighting(glm::normalize(scene.Settings().sunDirection), scene.Settings().sunEnabled?scene.Settings().sunColor*scene.Settings().sunIntensity:glm::vec3(0),
                         scene.Settings().ambientColor);
    const auto& appearance=scene.Settings();m_host->Resources().RequestEnvironment(appearance.environmentAsset);
    renderer.SetSceneAppearance(appearance.linearRendering,appearance.exposure,m_host->Resources().TryGetEnvironment(appearance.environmentAsset),appearance.environmentIntensity,appearance.environmentRotation,appearance.environmentBackground,appearance.backgroundColor);
    RendererProfileScope editorCameraGPU(renderer, "Editor camera");
    // The authoring scene has its own render target and aspect ratio. Panels
    // never obscure or steal pixels from the scene's picking projection.
    std::string targetError;
    if (!renderer.ResizeRenderTarget(m_sceneViewTarget, width, height, targetError) ||
        !renderer.BeginRenderTarget(m_sceneViewTarget)) {
        m_panels.status = "Scene view unavailable: " + targetError;
        renderer.BeginFrame(window.Width(), window.Height());
        renderer.EndFrame();
        return;
    }
    renderer.SetCamera(m_camera.ViewMatrix(), m_camera.ProjectionMatrix(aspect));
    {
        const auto eye=scene.Settings().worldOrigin+glm::dvec3(glm::inverse(renderer.ViewMatrix())[3]);
        auto weather=SampleAuthoredWeather(scene,eye);if(weather.active)renderer.SetWeather(weather,eye,1);else renderer.ClearWeather();
        renderer.SetWeatherShelter({});
    }

    // Clear runtime-view shadow history before terrain and authored previews.
    // Edit mode has no matching shadow pass for this camera.
    for(int slot=0;slot<kShadowMapCount;++slot)renderer.InvalidateShadowSlot(slot);
    renderer.SetDynamicLights(BuildAuthoredLights(scene));
    if(m_terrain)m_terrain->Draw(renderer,scene,m_host->Resources());
    if(m_volumeTerrain)m_volumeTerrain->Draw(renderer,scene,m_host->Resources());
    {
        std::set<SceneObjectId> keep;
        for(const auto& object:scene.Objects())if(object.planet){keep.insert(object.id);auto descriptor=EncodePlanetSettings(*object.planet);auto& p=m_planetPreview[object.id];if(p.first!=descriptor){p.second.reset();p.first=descriptor;p.second=std::make_unique<PlanetTiles>(*object.planet);}
            const auto eye=glm::dvec3(glm::inverse(renderer.ViewMatrix())[3])+scene.Settings().worldOrigin;
            p.second->Prepare(renderer,eye,scene.Settings().worldOrigin,2*std::atan(1./double(renderer.ProjectionMatrix()[1][1])),900,1./60);p.second->Draw(renderer,scene.Settings().worldOrigin);}
        for(auto it=m_planetPreview.begin();it!=m_planetPreview.end();)if(!keep.count(it->first))it=m_planetPreview.erase(it);else ++it;
    }
    DrawAuthoredScene(renderer, scene, m_host->Resources(),false);
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
        for(auto& region:m_regionPreview)DrawAuthoredScene(renderer,region,m_host->Resources(),false);
    }else {m_regionPreview.clear();m_regionPreviewKey.clear();}
    if (m_project.Settings().basicWater) {
        DrawAuthoredWaterSurfaces(renderer, scene, m_host->Resources());
        for (const auto& region : m_regionPreview)
            DrawAuthoredWaterSurfaces(renderer, region, m_host->Resources());
    }
    renderer.DrawWeather();
    DrawEditOverlay(renderer, scene);
    if(m_terrain)renderer.DrawDebugLines(m_terrain->footprint.Lines(),true);
    if(m_volumeTerrain)renderer.DrawDebugLines(m_volumeTerrain->footprint.Lines(),true);
    if(m_tilemapEditor&&m_tilemapEditor->painting)renderer.DrawDebugLines(m_tilemapEditor->overlay.Lines(),false);
    renderer.EndFrame();
    renderer.EndRenderTarget();
    renderer.ClearWeather();
    renderer.BeginFrame(window.Width(), window.Height());
    renderer.EndFrame();
    const auto texture = renderer.EditorImageToken(renderer.RenderTargetTexture(m_sceneViewTarget));
    // Draw-list composition keeps the scene itself outside ImGui's input
    // capture; viewport bounds decide whether a fresh camera/pick can start.
    ImGui::GetBackgroundDrawList()->AddImage(static_cast<ImTextureID>(texture),
        {viewport.position.x, viewport.position.y},
        {viewport.position.x + viewport.size.x, viewport.position.y + viewport.size.y}, {0, 1}, {1, 0});
}

void EditorApplication::DrawModelImportPreview(float deltaSeconds) {
 JUDAS_PROFILE_SCOPE("Model import preview");
 if((!m_panels.showAssetBrowser&&!m_panels.showAnimationTimeline)||m_panels.mode!=EditorMode::Edit||!m_panels.importAccepted)return;
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
    if(!m_document.GetScene().Settings().appearanceResetState.empty()){auto baseline=m_document.GetScene().Settings();std::string error;if(DecodeAppearanceState(baseline.appearanceResetState,baseline,error)&&!baseline.environmentAsset.empty())wanted.push_back(baseline.environmentAsset);}
    for (const SceneObject& o : m_document.GetScene().Objects()) {
        if (!o.render) continue;
        auto overrides=[&](const MaterialOverride& value){for(const auto& texture:value.textures)if(texture&&!texture->empty())wanted.push_back(*texture);};
        overrides(o.render->instanceOverrides);
        for(const auto& slot:o.render->materials){if(!slot.asset.empty())wanted.push_back(slot.asset);overrides(slot.overrides);}
        for(const auto* bindings:{&o.render->partMaterials,&o.render->runtimeMaterials})for(const auto& [_,slot]:*bindings){if(!slot.asset.empty())wanted.push_back(slot.asset);overrides(slot.overrides);}
        if (!o.render->meshAsset.empty()) wanted.push_back(o.render->meshAsset);
        if (!o.render->textureAsset.empty()) wanted.push_back(o.render->textureAsset);
    }
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    if (wanted == m_heldAssets) return;
    ResourceManager& resources = m_host->Resources();
    for (const std::string& id : wanted) {
        if (!std::binary_search(m_heldAssets.begin(), m_heldAssets.end(), id)&&!resources.StreamableVisual(id)) resources.AddRef(id);
    }
    for (const std::string& id : m_heldAssets) {
        if (!std::binary_search(wanted.begin(), wanted.end(), id)&&!resources.StreamableVisual(id)) resources.ReleaseRef(id);
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

void EditorApplication::LoadEditorPreferences() {
    // Store editor preferences in the platform's user-data directory, never
    // beside a scene or in the repository. Malformed preferences cannot block
    // opening a project; layout clamps also protect smaller window sizes.
    char* directory = SDL_GetPrefPath("Judas", "Editor");
    if (!directory) return;
    m_editorPreferencesPath = (fs::path(directory) / "workspace.json").string();
    m_editorIniPath = (fs::path(directory) / "windows.ini").string();
    SDL_free(directory);
    ImGui::GetIO().IniFilename = m_editorIniPath.c_str();
    std::ifstream file(m_editorPreferencesPath);
    if (!file) return;
    // Bound user configuration before parsing. Invalid settings never prevent startup.
    std::string text(4 * 1024 * 1024 + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(file.gcount()));
    std::string error;
    if (!m_preferences.Load(text, m_panels, error))
        m_preferences.status = "Preferences could not be loaded: " + error + ". Defaults are in use; the next save retains a .bak copy.";
}

void EditorApplication::SaveEditorPreferences() {
    if (m_editorPreferencesPath.empty()) return;
    std::string error;
    if (!m_preferences.SaveFile(m_editorPreferencesPath, m_panels, error)) {
        m_preferences.status = "Preferences were not saved: " + error;
        std::fprintf(stderr, "[editor] %s\n", m_preferences.status.c_str());
    } else m_preferences.status = "Editor preferences saved.";
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
    const auto editorTitle = std::string(GetBuildIdentity().releaseName) + " — Editor " + GetBuildIdentity().version +
        " [" + GetBuildIdentity().channel + "] " + GetBuildIdentity().buildId;
    if (!host.Init(editorTitle.c_str(), kWindowWidth, kWindowHeight, true, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    m_host = &host;
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    SDL_SetWindowMinimumSize(window.NativeWindow(), 640, 480);
    window.SetMouseCaptured(false);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const auto editorFont = ResolveEngineDataPath("assets/fonts/DejaVuSans.ttf");
    if (fs::is_regular_file(editorFont)) io.Fonts->AddFontFromFileTTF(editorFont.c_str(), 15.0f);
    LoadEditorPreferences();
    m_preferences.ApplyPending(m_panels);
    if (m_preferences.automaticUpdates && !std::getenv("JUDAS_EDITOR_AUTOTEST")) m_milestoneNotice.Check(false, m_preferences.updateChannel);
    ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(), window.NativeGLContext());
    ImGui_ImplOpenGL3_Init("#version 330 core");
    window.SetEventHook([this, editorWindowId = SDL_GetWindowID(window.NativeWindow())](const SDL_Event& event) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_MOUSEMOTION && event.motion.windowID == editorWindowId) {
            m_viewportMouseDeltaX += event.motion.xrel;
            m_viewportMouseDeltaY += event.motion.yrel;
        } else if (event.type == SDL_WINDOWEVENT && event.window.windowID == editorWindowId) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                m_viewportFocused = false;
                if (m_panels.mode == EditorMode::Edit) CancelCameraNavigation();
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) m_viewportFocused = true;
        }
    });

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
        m_panels.status = m_project.IsLoaded() ? "Project has no startup scene; new scene. Create > objects; right-drag to look, middle-drag to pan."
                                               : "No project. File > New project or Open project. Right-drag to look, middle-drag to pan, WASD/QE to fly.";
    }
    if (m_panels.status.rfind("Opened", 0) == 0) {
        m_panels.status += ". Wheel zooms; right-drag looks; middle-drag pans; WASD/QE fly; click selects; W/E/R transform; X local/world; Ctrl snaps.";
    }

    // Developer/automation hook (see docs/ARCHITECTURE.md, "Milestone 30,
    // Automated evidence"): JUDAS_EDITOR_AUTOTEST=<prefix> selects the first
    // object, enables the debug view, renders the edit view, duplicates and
    // undoes (checking the scene is unchanged), enters Play, runs,
    // screenshots both, stops, verifies the authored scene is untouched,
    // saves it, prints profiler and resource lines, and quits.
    const char* autotest = std::getenv("JUDAS_EDITOR_AUTOTEST");
    int autotestFrame = 0;
    const bool navigationAutotest = autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PAN");
    const bool projectUnsavedAutotest=autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_PROJECT_UNSAVED");
    const bool polishAutotest = autotest && (std::getenv("JUDAS_EDITOR_AUTOTEST_POLISH")||projectUnsavedAutotest);
    const bool pickingAutotest = autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PICKING");
    const bool workspaceAutotest = autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_WORKSPACE");
    glm::vec3 dollyStartPosition{0}, dollyAnchor{0};
    float dollyStartDistance = 0;
    int navigationFailures = 0;
    glm::vec3 navigationStartPosition{0}, navigationStartFocus{0}, navigationStartForward{0};
    glm::vec3 navigationExpectedPosition{0};
    const auto navigationCheck = [&](bool ok, const char* label) {
        if (!ok) ++navigationFailures;
        std::fprintf(stderr, "[editor navigation] %s: %s\n", label, ok ? "PASS" : "FAIL");
    };
    if (polishAutotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PREFS"))
        navigationCheck(m_panels.assetBrowserHeight == 100.0f,
                        "saved asset height below 120 pixels survives preference loading");
    const auto navigationButton = [&](bool down) {
        SDL_Event event{};
        event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event.button.windowID = SDL_GetWindowID(window.NativeWindow());
        event.button.button = SDL_BUTTON_MIDDLE;
        event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
        event.button.x = static_cast<int>(m_panels.workspace.viewport.position.x + m_panels.workspace.viewport.size.x * .5f);
        event.button.y = static_cast<int>(m_panels.workspace.viewport.position.y + m_panels.workspace.viewport.size.y * .5f);
        SDL_PushEvent(&event);
    };
    const auto navigationMotion = [&](int dx, int dy) {
        SDL_Event event{};
        event.type = SDL_MOUSEMOTION;
        event.motion.windowID = SDL_GetWindowID(window.NativeWindow());
        event.motion.x = static_cast<int>(m_panels.workspace.viewport.position.x + m_panels.workspace.viewport.size.x * .5f);
        event.motion.y = static_cast<int>(m_panels.workspace.viewport.position.y + m_panels.workspace.viewport.size.y * .5f);
        event.motion.xrel = dx; event.motion.yrel = dy;
        SDL_PushEvent(&event);
    };
    const auto navigationFocus = [&](bool focused) {
        SDL_Event event{};
        event.type = SDL_WINDOWEVENT;
        event.window.windowID = SDL_GetWindowID(window.NativeWindow());
        event.window.event = focused ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST;
        SDL_PushEvent(&event);
    };
    std::uint64_t profilerFrozenFrame = 0;
    std::size_t profilerFrozenSteps = 0;
    // Exercise the same project-creation path from a relocated SDK, without a
    // development-tree project fixture. Never overwrite an existing directory.
    if (autotest) {
        if (const char* directory = std::getenv("JUDAS_EDITOR_AUTOTEST_NEW_PROJECT")) {
            std::string error;
            const char* twoD=std::getenv("JUDAS_EDITOR_AUTOTEST_NEW_2D");
            const int template2D=twoD?(std::string(twoD)=="side-view"?2:1):0;
            const bool created = !fs::exists(fs::u8path(directory)) &&
                CreateProject(directory,template2D?"SDK 2D Game":"SDK First Game",error,!template2D,template2D);
            std::fprintf(stderr, "[editor onboarding] packaged visible starter: %s %s\n",
                         created ? "PASS" : "FAIL", error.c_str());
            if (!created) return 1;
        }
    }
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
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_BENDING")){
        m_panels.deformableColumns=4;m_panels.deformableRows=1;m_panels.deformableSubdivision=1;m_panels.deformableSize={4,1,1};
        m_panels.deformableDestination="deformables/editor-bendable.judasdeform";
        bool baked=BakeEditorDeformable(m_document,300,m_panels,7);
        const auto* owner=m_document.GetScene().Find(300);
        const auto assigned=owner&&owner->deformable?owner->deformable->asset:std::string();
        bool duplicate=BakeEditorDeformable(m_document,300,m_panels,7);
        const auto* retained=m_document.GetScene().Find(300);
        bool safe=!duplicate&&retained&&retained->deformable&&retained->deformable->asset==assigned;
        std::fprintf(stderr,"[M80 editor] bake bendable structural cook: %s; existing destination leaves assignment unchanged: %s\n",baked?"PASS":"FAIL",safe?"PASS":"FAIL");
        if(!baked||!safe)++navigationFailures;
    }
    if(autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_COLLISION")){
        auto* object=m_document.GetScene().Find(100);auto source=object&&object->render?object->render->meshAsset:AssetId{};
        CollisionCookSettings settings;bool ok=BakeEditorCollision(m_document,100,m_panels,source,settings,"collision/editor-check.judascollision");
        std::fprintf(stderr,"[editor autotest] shared collision cook/assign: %s: %s\n",ok?"PASS":"FAIL",m_panels.status.c_str());
    }
    if (autotest) SaveSceneToString(m_document.GetScene(), autotestBaseline);
    bool animationTimelineRecook=false;
    const bool animationTimelineAutotest=autotest&&std::getenv("JUDAS_EDITOR_AUTOTEST_ANIMATION");
    const char* importRecipe=autotest?std::getenv("JUDAS_EDITOR_AUTOTEST_IMPORT"):nullptr;
    if(importRecipe){m_panels.showAssetBrowser=true;m_panels.modelRecipe=importRecipe;m_panels.importTask=QueueModelImport(host.Jobs(),importRecipe);m_panels.importPublished=false;}
    const char* terrainAutotest=autotest?std::getenv("JUDAS_EDITOR_AUTOTEST_TERRAIN"):nullptr;
    const char* volumeAutotest=autotest?std::getenv("JUDAS_EDITOR_AUTOTEST_VOLUME"):nullptr;
    int volumeUiFrame=0,volumeWaitFrames=0,volumeFailures=0;bool volumeUiReady=false;
    std::vector<float> volumeBefore,volumeEdited;
    const auto volumeCheck=[&](bool ok,const char* label){if(!ok)++volumeFailures;std::fprintf(stderr,"[M84 editor volume] %s: %s\n",ok?"PASS":"FAIL",label);};

    std::vector<float> terrainInitial,terrainEdited;std::vector<std::array<float,4>> terrainPainted;
    int terrainFailures=0,terrainUiFrame=0;bool terrainUiReady=false;
    const auto terrainCheck=[&](bool value,const char* label){if(!value)++terrainFailures;std::fprintf(stderr,"[M75 terrain editor] %s: %s\n",value?"PASS":"FAIL",label);};
    const char* retargetAutotest=autotest?std::getenv("JUDAS_EDITOR_AUTOTEST_RETARGET"):nullptr;
    if(retargetAutotest){std::string error;if(!BeginRetargetAutotest(m_panels,retargetAutotest,error)){std::fprintf(stderr,"[M74 editor retarget] FAIL: %s\n",error.c_str());m_quit=true;}}
    unsigned retargetWaitFrames=0;
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
        window.SetHostTextInput(io.WantTextInput);
        m_viewportMouseDeltaX = m_viewportMouseDeltaY = 0;
        { JUDAS_PROFILE_SCOPE("Input events"); window.PollEvents(); }
        const bool closeRequested = window.ShouldClose();
        // A desktop close is handled like File > Quit, including the document
        // guard. The game runtime still exits on its own normal close request.
        if (closeRequested) window.CancelCloseRequest();
        host.PumpResources();  // M31: GPU upload of finished loads, budget eviction
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        if(m_scriptTools&&m_scriptTools->clockReset){previousCounter=currentCounter;m_scriptTools->clockReset=false;}
        const float deltaSeconds = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        renderer.ResetStats();

        m_preferences.ApplyPending(m_panels);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        UpdateEditorWorkspaceLayout(m_panels, {io.DisplaySize.x, io.DisplaySize.y});

        EditorRequests requests;
        requests.quit = closeRequested;
        // Keyboard shortcuts (edit mode, no text field focused, not flying).
        if (m_panels.mode == EditorMode::Edit && !io.WantCaptureKeyboard) {
            const bool ctrl = io.KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) requests.save = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) requests.undo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) requests.redo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) requests.duplicateId = m_document.Selected();
            if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_F2) && m_cameraGesture.Active() == EditorCameraGesture::None)
                BeginEditorRename(m_document, m_panels);
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) requests.play = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_document.Selected() != kInvalidSceneObjectId) {
                std::string error;
                if(!m_document.DeleteSelection(error))m_panels.status=error;
            }
            if (m_cameraGesture.Active() == EditorCameraGesture::None && !ctrl) {
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
            if(!m_scriptTools)m_scriptTools=std::make_unique<ScriptTools>();
            m_scriptTools->Attach(*m_world,host);
            m_scriptTools->ApplyReload(*m_world);
            m_play->SetPointerCaptureAllowed(!(m_panels.showProfiler && m_panels.profilerInspect) && !m_panels.showScriptTools);
            // The identical frame the runtime runs. Escape opens the M13
            // pause menu, which releases the mouse for the editor panels.
            m_play->Frame(window, renderer, deltaSeconds, /*drawHud=*/true);
            if(m_scriptTools->clockReset){previousCounter=SDL_GetPerformanceCounter();m_scriptTools->clockReset=false;}
            if(m_scriptTools->stopRequested){requests.stop=true;m_scriptTools->stopRequested=false;}
            if(m_scriptTools->quitRequested){requests.quit=true;m_scriptTools->quitRequested=false;}
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
        DrawEditorImportReview(m_panels, requests);
        m_preferences.Draw(m_panels);
        if (m_preferences.ConsumeSaveRequest()) SaveEditorPreferences();
        if(!m_scriptTools)m_scriptTools=std::make_unique<ScriptTools>();
        if(!m_panels.editScriptAsset.empty()){
            const auto* script=host.Assets().Find(m_panels.editScriptAsset);
            if(script&&m_scriptTools->OpenForEditing(script->relativePath,host.Assets()))m_panels.showScriptTools=true;
            else {m_panels.showScriptTools=true;m_panels.status=m_scriptTools->status;}
            m_panels.editScriptAsset.clear();
        }
        m_scriptTools->visible=m_panels.showScriptTools;
        m_scriptTools->Panel(m_world.get(),host.Assets());
        m_panels.showScriptTools=m_scriptTools->visible;
        if(!m_animationTimeline)m_animationTimeline=std::make_unique<AnimationTimeline>();
        m_animationTimeline->visible=m_panels.showAnimationTimeline;
        m_animationTimeline->Panel(m_panels,m_document.Selected());
        m_panels.showAnimationTimeline=m_animationTimeline->visible;
        if(!m_spriteSheetEditor)m_spriteSheetEditor=std::make_unique<SpriteSheetEditor>();
        m_spriteSheetEditor->Panel(m_panels);
        if(!m_tilemapEditor)m_tilemapEditor=std::make_unique<TilemapEditor>();
        m_tilemapEditor->Panel(m_document,m_panels);
        DrawEditorWorkspaceChrome(m_document, m_panels, requests);
        // While playing, the engine's own HUD occupies the top-left corner
        // and the authored panels are read-only anyway; only the menu bar
        // (Stop), the inspector, profiler and the status bar stay up.
        // The hierarchy is also shown while Play is paused (Escape), with
        // each entity's live fidelity, so the M29 state can be inspected.
        if (m_panels.mode == EditorMode::Edit || m_panels.playPaused) {
            DrawHierarchyPanel(m_document, m_panels, requests);
        }
        if (m_panels.mode == EditorMode::Edit) {
            if(!m_terrain)m_terrain=std::make_unique<TerrainEditor>();
            m_terrain->Panel(m_document,m_panels,requests);
            if(!m_volumeTerrain)m_volumeTerrain=std::make_unique<VolumeTerrainEditor>();
            m_volumeTerrain->Panel(m_document,m_panels,requests);
            if(!m_modTools)m_modTools=std::make_unique<ModTools>();
            m_modTools->Panel(m_panels,requests);
            DrawWorldBuildingPanel(m_document, m_panels);
            DrawSceneSettingsPanel(m_document, m_panels);
            DrawAssetBrowserPanel(m_document, m_panels, requests);
            DrawRetargetPanel(m_panels, requests);
            DrawProjectSettingsPanel(m_document, m_panels, requests);
        }
        if (m_panels.mode == EditorMode::Edit || m_panels.playPaused)
            DrawInspectorPanel(m_document, m_panels);
        DrawStreamingPanel(m_document,m_panels,requests);
        DrawRuntimeInspector(m_panels,requests);
        { JUDAS_PROFILE_SCOPE("Profiler UI"); DrawProfilerPanel(m_panels); }
        DrawStatusBar(m_document, m_panels);
        m_milestoneNotice.Draw(m_panels.status, m_preferences.updateChannel);

        // Requests raised by the automation hook on the previous frame,
        // after that frame's UI had already been rendered.
        if (deferredRequests.play) requests.play = true;
        if (deferredRequests.stop) requests.stop = true;
        deferredRequests = EditorRequests{};
        // Export automation submits the same UI request during an active ImGui
        // frame. HandleRequests also owns modal UI and cannot run after Render.
        const char* exportDestination = std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT");
        // Polish intentionally queues Escape at frame 14. Test export only
        // after that cancellation/release has completed, not in the same frame.
        const int exportRequestFrame = polishAutotest ? 17 : 14;
        const bool automatedExport = autotest && autotestFrame == exportRequestFrame && exportDestination;
        if (automatedExport) {
            m_document.BeginEdit();m_document.GetScene().Settings().name+=" [unsaved export]";m_document.CommitEdit();
            m_project.Settings().name+=" [unsaved export]";
            m_panels.exportDestination = exportDestination;
            requests.exportProject = true;
        }
        if(importRecipe&&autotestFrame==1)requests.dropMeshAssetId=m_panels.importAccepted->assetId;
        if (polishAutotest && autotestFrame == 11) requests.newScene = true;
        if(projectUnsavedAutotest&&autotestFrame==11){
            std::string error;m_document.Save(error);
            m_project.Settings().name+=" - unsaved";requests.newScene=false;requests.quit=true;
        }
        if(autotest&&exportDestination&&m_documentActionPending){
            if(auto* dialog=ImGui::FindWindowByName("Save before run or export"))
                ImGui::ActivateItemByID(dialog->GetID(std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT_SAVED")?"Use saved version":"Save all and continue"));
        }
        HandleRequests(requests);
        if(projectUnsavedAutotest&&autotestFrame==11)
            navigationCheck(m_project.IsDirty()&&!m_document.IsDirty()&&m_confirmationTitle=="Unsaved project"&&!m_quit,"project-only edits receive their own close guard");
        if(autotest&&exportDestination&&autotestFrame==exportRequestFrame+5){
            const bool success=m_panels.runProjectInfo.rfind("Exported ",0)==0;
            navigationCheck(success,"explicit export choice produces the requested package");
            if(success){
                Project packaged;Scene scene;std::string error;
                const bool savedOnly=std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT_SAVED")!=nullptr;
                const bool loaded=packaged.Load((fs::u8path(exportDestination)/"game.judasproj").string(),error)&&LoadSceneFromFile(packaged.StartupScenePath(),scene,error);
                navigationCheck(loaded&&((packaged.Settings().name.find("[unsaved export]")!=std::string::npos)==!savedOnly)&&((scene.Settings().name.find("[unsaved export]")!=std::string::npos)==!savedOnly),"exported scene and project match the explicit saved-content choice");
                navigationCheck(m_document.IsDirty()==savedOnly&&m_project.IsDirty()==savedOnly,"use-saved preserves live edits; save-all clears both dirty baselines");
            }
            // Authoring changed deliberately before export; Play/Stop must
            // preserve that current document, whichever disk version shipped.
            SaveSceneToString(m_document.GetScene(),autotestBaseline);
            std::fprintf(stderr,"[editor export choice] %s\n",m_panels.runProjectInfo.c_str());
        }
        if (polishAutotest) {
            if (autotestFrame == 11) {
                std::string actual; SaveSceneToString(m_document.GetScene(), actual);
                navigationCheck(m_documentActionPending && !m_documentActionApproved && actual == autotestBaseline,
                                "new-scene request preserves unsaved document until a choice");
            } else if (autotestFrame == 13) {
                navigationCheck(ImGui::IsPopupOpen(m_confirmationTitle.c_str()) && m_documentActionPending,
                                "unsaved-document confirmation remains open across frames");
            } else if (autotestFrame == 14) {
                navigationCheck(!m_documentActionPending && !m_documentActionApproved && !m_documentActionSaveAs,
                                "Escape cancels unsaved-document action");
            } else if (autotestFrame == 15) {
                const auto& viewport = m_panels.workspace.viewport;
                const auto size = renderer.RenderTargetSize(m_sceneViewTarget);
                navigationCheck(size == glm::ivec2(viewport.size), "resized scene target follows workspace dimensions");
                glm::vec3 origin, direction;
                ViewportRay(static_cast<int>(viewport.position.x + viewport.size.x * .5f),
                            static_cast<int>(viewport.position.y + viewport.size.y * .5f), origin, direction);
                navigationCheck(glm::dot(direction, m_camera.Forward()) > .99999f,
                                "scene picking uses the central viewport projection after resize");
            } else if (autotestFrame == 144) {
                navigationCheck(m_documentActionPending && !m_quit && !window.ShouldClose(),
                                "desktop close request waits for unsaved-document confirmation");
            } else if (autotestFrame == 145) {
                std::string actual; SaveSceneToString(m_document.GetScene(), actual);
                navigationCheck(!m_documentActionPending && !m_quit && actual == autotestBaseline,
                                "cancelled desktop close preserves authored state and editor session");
            }
        }
        if (automatedExport)
            std::fprintf(stderr, "[editor autotest] export project: %s\n", m_panels.runProjectInfo.c_str());
        AdvanceStabilizationAutomation();

        DrawModelImportPreview(deltaSeconds);
        UpdateRetargetPreview(m_panels, renderer, deltaSeconds);
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
            if (autotestFrame == 30 && std::getenv("JUDAS_EDITOR_AUTOTEST_RUNTIME_INSPECTOR") && m_world) {
                m_panels.showRuntimeInspector = true;
                const auto keys = m_world->ScriptKeys();
                if (!keys.empty()) m_panels.runtimeInspectEntity = keys.front().first;
            }
            bool waiting=importRecipe&&(!m_panels.modelPreviewToken||(animationTimelineAutotest&&m_panels.importTask!=m_panels.importAccepted));
            if(waiting&&((m_panels.importTask->done&&!m_panels.importTask->success)||++importWaitFrames>4000)){std::fprintf(stderr,"[editor autotest] import FAIL: %s\n",m_panels.importTask->error.c_str());m_quit=true;}
            if(retargetAutotest){std::string error;int status=RetargetAutotestStatus(m_panels,error);if(status<0||(status==0&&++retargetWaitFrames>4000)){std::fprintf(stderr,"[M74 editor retarget] FAIL: %s\n",error.c_str());m_quit=true;}waiting=waiting||status==0;}
            if(terrainAutotest&&!terrainUiReady){
                // Exercise the human-facing activation button, not direct
                // mutation of loaded/editing as the original smoke did.
                waiting=true;++terrainUiFrame;
                if(terrainUiFrame==1){m_document.Select(1);m_panels.showTerrain=true;navigationFocus(true);}
                if(terrainUiFrame==2){
                    const auto p=m_terrain->editButtonCenter;SDL_WarpMouseInWindow(window.NativeWindow(),int(p.x),int(p.y));
                    SDL_Event event{};event.type=SDL_MOUSEMOTION;event.motion.windowID=SDL_GetWindowID(window.NativeWindow());event.motion.x=int(p.x);event.motion.y=int(p.y);SDL_PushEvent(&event);
                }
                if(terrainUiFrame==4||terrainUiFrame==6){
                    const auto p=m_terrain->editButtonCenter;
                    SDL_WarpMouseInWindow(window.NativeWindow(),int(p.x),int(p.y));
                    SDL_Event event{};event.type=terrainUiFrame==4?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
                    event.button.windowID=SDL_GetWindowID(window.NativeWindow());event.button.button=SDL_BUTTON_LEFT;
                    event.button.state=terrainUiFrame==4?SDL_PRESSED:SDL_RELEASED;event.button.x=int(p.x);event.button.y=int(p.y);SDL_PushEvent(&event);
                }
                if(terrainUiFrame==9){
                    screenshot(std::string(autotest)+".activation.png");
                    terrainCheck(m_terrain->loaded&&m_terrain->editing,"real panel click opens selected source and activates brush");
                    if(!m_terrain->loaded){m_quit=true;}else terrainUiReady=true;
                }
            }
            if(volumeAutotest&&!volumeUiReady){waiting=true;++volumeUiFrame;
                if(volumeUiFrame==1){m_document.Select(1);m_panels.showVolumeTerrain=true;navigationFocus(true);}
                if(volumeUiFrame==2){auto p=m_volumeTerrain->editButtonCenter;SDL_WarpMouseInWindow(window.NativeWindow(),int(p.x),int(p.y));SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window.NativeWindow());e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);}
                if(volumeUiFrame==4||volumeUiFrame==6){auto p=m_volumeTerrain->editButtonCenter;SDL_Event e{};e.type=volumeUiFrame==4?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window.NativeWindow());e.button.button=SDL_BUTTON_LEFT;e.button.state=volumeUiFrame==4?SDL_PRESSED:SDL_RELEASED;e.button.x=int(p.x);e.button.y=int(p.y);SDL_PushEvent(&e);}
                if(volumeUiFrame==9){volumeCheck(m_volumeTerrain->loaded&&m_volumeTerrain->editing,"real panel click opens selected volume and activates brush");volumeUiReady=m_volumeTerrain->loaded;if(!volumeUiReady)m_quit=true;}
            }
            if(volumeAutotest&&volumeUiReady&&(autotestFrame==0||autotestFrame==16)&&(!m_volumeTerrain->preview||m_volumeTerrain->previewRevision!=m_volumeTerrain->draft.Revision())){waiting=true;std::this_thread::sleep_for(std::chrono::milliseconds(1));if(++volumeWaitFrames>4000){volumeCheck(false,"bounded async preview completion");m_quit=true;}}
            if(!waiting)++autotestFrame;
            if(volumeAutotest&&volumeUiReady&&!waiting){
                auto mouse=[&](bool down){SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window.NativeWindow());e.button.button=SDL_BUTTON_LEFT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=int(io.MousePos.x);e.button.y=int(io.MousePos.y);SDL_PushEvent(&e);};
                if(autotestFrame==1){volumeCheck(m_volumeTerrain->draft.Path()==m_project.Resolve(volumeAutotest),"button opened selected model's exact author-owned source");volumeBefore=m_volumeTerrain->draft.source.density;m_volumeTerrain->brush.radius=1.3f;m_volumeTerrain->brush.strength=1;m_panels.showVolumeTerrain=false;m_panels.debug={};m_camera.SetPose({4,5,20},180,0);auto& v=m_panels.workspace.viewport;SDL_WarpMouseInWindow(window.NativeWindow(),int(v.position.x+v.size.x*.5f),int(v.position.y+v.size.y*.5f));navigationFocus(true);}
                else if(autotestFrame==3)mouse(true);
                else if(autotestFrame==4)volumeCheck(m_volumeTerrain->draft.Active()&&m_volumeTerrain->draft.source.density!=volumeBefore,"native viewport press removes real 3D material");
                else if(autotestFrame==5)mouse(false);
                else if(autotestFrame==6){volumeEdited=m_volumeTerrain->draft.source.density;volumeCheck(!m_volumeTerrain->draft.Active(),"release commits one volume gesture");volumeCheck(m_volumeTerrain->draft.Undo()&&m_volumeTerrain->draft.source.density==volumeBefore,"one undo restores complete field");}
                else if(autotestFrame==7)volumeCheck(m_volumeTerrain->draft.Redo()&&m_volumeTerrain->draft.source.density==volumeEdited,"redo restores volumetric edit");
                else if(autotestFrame==8){m_volumeTerrain->brush.kind=VolumeBrushKind::Add;mouse(true);}
                else if(autotestFrame==9){volumeCheck(m_volumeTerrain->draft.Active()&&m_volumeTerrain->draft.source.density!=volumeEdited,"add gesture changes material before cancellation");io.AddKeyEvent(ImGuiKey_Escape,true);}
                else if(autotestFrame==10){io.AddKeyEvent(ImGuiKey_Escape,false);mouse(false);volumeCheck(!m_volumeTerrain->draft.Active()&&m_volumeTerrain->draft.source.density==volumeEdited,"Escape restores in-flight volume gesture");m_volumeTerrain->editing=true;}
                else if(autotestFrame==11)mouse(true);
                else if(autotestFrame==12)navigationFocus(false);
                else if(autotestFrame==13){mouse(false);navigationFocus(true);volumeCheck(!m_volumeTerrain->draft.Active()&&m_volumeTerrain->draft.source.density!=volumeEdited,"focus loss commits an actually changed material gesture");volumeCheck(m_volumeTerrain->draft.Undo()&&m_volumeTerrain->draft.source.density==volumeEdited,"one undo removes the focus-ended add gesture");std::string why;volumeCheck(m_volumeTerrain->draft.Save(why),"save actual viewport volume edit");TerrainCookResult result;volumeCheck(CookVolumeTerrain(m_volumeTerrain->draft.source,m_volumeTerrain->draft.Path(),m_project.RootDir(),host.Assets(),result,why),"normal cook publishes edited model and physical collider");if(!why.empty())std::fprintf(stderr,"[M84 editor error] %s\n",why.c_str());deferredRequests.rescanAssets=true;m_volumeTerrain->editing=false;}
                else if(autotestFrame==17){MeshData gpu;volumeCheck(renderer.ReadMeshForDiagnostics(m_volumeTerrain->mesh,gpu)&&gpu.vertices.size()==m_volumeTerrain->preview->vertices.size()&&glm::length(gpu.vertices[gpu.vertices.size()/2].position-m_volumeTerrain->preview->vertices[gpu.vertices.size()/2].position)<1e-6f,"actual GL volume mesh agrees with edited field extraction");}
                else if(autotestFrame==18){std::vector<SceneObjectId> ids;for(auto& o:m_document.GetScene().Objects())if(o.navigationSurface)ids.push_back(o.id);for(auto id:ids)volumeCheck(BakeEditorNavigation(m_document,id,m_panels),"rebake navigation against edited cave geometry");std::string why;volumeCheck(m_document.Save(why),"save refreshed navigation reference");SaveSceneToString(m_document.GetScene(),autotestBaseline);}
                else if(autotestFrame==21)volumeCheck(m_panels.mode==EditorMode::Play,"saved edited volume enters ordinary Play");
                else if(autotestFrame==150){volumeCheck(m_panels.mode==EditorMode::Edit&&!m_volumeTerrain->draft.Dirty()&&m_volumeTerrain->draft.source.density==volumeEdited,"Stop preserves saved 3D edit without stale gameplay state");std::fprintf(stderr,"[M84 editor volume] failures %d\n",volumeFailures);}
            }

            if(terrainAutotest&&terrainUiReady&&!waiting){
                if(autotestFrame%20==0)std::fprintf(stderr,"[M75 editor progress] frame %d step %.3f ms mode %d\n",autotestFrame,m_panels.profiler.fixedStepMilliseconds,int(m_panels.mode));
                const auto mouse=[&](bool down){SDL_Event event{};event.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;event.button.windowID=SDL_GetWindowID(window.NativeWindow());event.button.button=SDL_BUTTON_LEFT;event.button.state=down?SDL_PRESSED:SDL_RELEASED;event.button.x=int(io.MousePos.x);event.button.y=int(io.MousePos.y);SDL_PushEvent(&event);};
                if(autotestFrame==1){
                    terrainCheck(m_terrain->draft.Path()==m_project.Resolve(terrainAutotest),"panel opened the ordinary selected source");
                    m_terrain->brush.radius=3;m_terrain->brush.strength=.7f;
                    terrainInitial=m_terrain->draft.source.heights;m_document.Select(1);m_panels.showTerrain=false;m_panels.debug={};
                    auto& view=m_panels.workspace.viewport;auto clip=m_camera.ProjectionMatrix(view.size.x/view.size.y)*m_camera.ViewMatrix()*glm::vec4(0,0,10,1);
                    int x=int(view.position.x+(clip.x/clip.w+1)*.5f*view.size.x),y=int(view.position.y+(1-clip.y/clip.w)*.5f*view.size.y);
                    SDL_WarpMouseInWindow(window.NativeWindow(),x,y);navigationFocus(true);
                }else if(autotestFrame==3)mouse(true);
                else if(autotestFrame==4)terrainCheck(m_terrain->draft.Active()&&m_terrain->draft.source.heights!=terrainInitial,"native mouse press sculpts current viewport surface");
                else if(autotestFrame==5)mouse(false);
                else if(autotestFrame==6){terrainEdited=m_terrain->draft.source.heights;terrainCheck(!m_terrain->draft.Active(),"release commits one gesture");terrainCheck(m_terrain->draft.Undo()&&m_terrain->draft.source.heights==terrainInitial,"one undo restores source");}
                else if(autotestFrame==7)terrainCheck(m_terrain->draft.Redo()&&m_terrain->draft.source.heights==terrainEdited,"redo restores authored sculpt");
                else if(autotestFrame==8)mouse(true);
                else if(autotestFrame==9)io.AddKeyEvent(ImGuiKey_Escape,true);
                else if(autotestFrame==10){io.AddKeyEvent(ImGuiKey_Escape,false);mouse(false);terrainCheck(!m_terrain->draft.Active()&&m_terrain->draft.source.heights==terrainEdited,"Escape cancels real viewport stroke");m_terrain->editing=true;}
                else if(autotestFrame==11)mouse(true);
                else if(autotestFrame==12)navigationFocus(false);
                else if(autotestFrame==13){terrainCheck(!m_terrain->draft.Active(),"focus loss safely commits and ends gesture");mouse(false);navigationFocus(true);m_terrain->brush.kind=TerrainBrushKind::Paint;m_terrain->brush.layer=1;}
                else if(autotestFrame==14)mouse(true);
                else if(autotestFrame==15)mouse(false);
                else if(autotestFrame==16){
                    std::string error;terrainCheck(m_terrain->draft.Save(error),"save actual editable sculpt and paint");TerrainCookResult result;
                    terrainCheck(CookTerrain(m_terrain->draft.source,m_terrain->draft.Path(),m_project.RootDir(),host.Assets(),result,error),"same CPU cook publishes runtime geometry and appearance");
                    terrainPainted=m_terrain->draft.source.weights;deferredRequests.rescanAssets=true;m_terrain->editing=false;m_panels.showTerrain=true;
                    MeshData gpu;terrainCheck(renderer.ReadMeshForDiagnostics(m_terrain->mesh,gpu)&&gpu.vertices.size()==m_terrain->preview->vertices.size()&&glm::length(gpu.vertices[gpu.vertices.size()/2].position-m_terrain->preview->vertices[gpu.vertices.size()/2].position)<1e-6f,"actual GL mesh matches current draft geometry");
                }else if(autotestFrame==18&&std::getenv("JUDAS_EDITOR_AUTOTEST_MOD_PACKAGE")){std::vector<SceneObjectId> surfaces;for(const auto& object:m_document.GetScene().Objects())if(object.navigationSurface)surfaces.push_back(object.id);for(auto id:surfaces)terrainCheck(BakeEditorNavigation(m_document,id,m_panels),"editor rebakes navigation against newly cooked owned geometry");std::string error;terrainCheck(m_document.Save(error),"save rebaked ordinary mod scene");SaveSceneToString(m_document.GetScene(),autotestBaseline);
                }else if(autotestFrame==21)terrainCheck(m_panels.mode==EditorMode::Play,"saved/cooked terrain enters normal Play");
                else if(autotestFrame==150){terrainCheck(m_panels.mode==EditorMode::Edit&&m_terrain->draft.source.weights==terrainPainted&&!m_terrain->draft.Dirty(),"Stop preserves saved editable paint state");if(const char* output=std::getenv("JUDAS_EDITOR_AUTOTEST_MOD_PACKAGE")){std::string error;AuthoringDocument manifest;terrainCheck(manifest.Load("modmanifest",m_project.Resolve("mod.json"),error),"editor opens saved mod manifest");auto json=AuthoringJSON::parse(manifest.Source());json["name"]="Editor packaged map";auto previous=manifest.Source();terrainCheck(manifest.Replace(json.dump(2),error)&&manifest.Undo()&&manifest.Source()==previous&&manifest.Redo(),"manifest draft Undo/Redo");terrainCheck(manifest.Save(error),"explicit editor manifest save");JudasMods::Manifest draft;JudasMods::Package package;terrainCheck(JudasMods::ParseManifest(manifest.Source(),draft,error,true)&&JudasMods::AuthorPackage(m_project,draft,package,error)&&JudasMods::WritePackage(output,package,error),"editor shared packaging after actual viewport sculpt/paint/cook/bake/Play-Stop");if(!error.empty())std::fprintf(stderr,"[M76 editor package] %s\n",error.c_str());}std::fprintf(stderr,"[M75 terrain editor] failures %d\n",terrainFailures);}
            }
            if(retargetAutotest && autotestFrame==1) screenshot(std::string(autotest)+".retarget.png");
            if(importRecipe&&autotestFrame==1)std::fprintf(stderr,"[editor autotest] shared import + GPU preview PASS (%zu parts)\n",m_panels.importAccepted->preview->primitives.size());
            if(importRecipe&&autotestFrame==2){auto* placed=m_document.SelectedObject();std::fprintf(stderr,"[editor autotest] ordinary imported placement %s\n",placed&&placed->render&&placed->animation?"PASS":"FAIL");m_document.Undo();std::string restored;SaveSceneToString(m_document.GetScene(),restored);std::fprintf(stderr,"[editor autotest] imported placement one undo %s\n",restored==autotestBaseline?"PASS":"FAIL");}
            if(animationTimelineAutotest&&importRecipe){
                if(autotestFrame==3){m_panels.showAnimationTimeline=true;m_panels.workspace.showAssets=false;}
                if(autotestFrame==5){m_panels.modelPreviewClip=1;m_panels.modelPreviewPlaying=false;m_panels.modelPreviewTime=.3f;}
                if(autotestFrame==8){m_panels.modelPreviewPlaying=false;m_panels.modelPreviewTime=.55f;auto& timeline=*m_animationTimeline;timeline.Remember();timeline.draft.events[0].time=.21f;m_panels.modelPreviewClip=2;}
                if(autotestFrame==9){auto& timeline=*m_animationTimeline;bool retained=timeline.draft.clip=="Walk"&&timeline.draft.Dirty()&&std::abs(timeline.draft.events[0].time-.21f)<1e-6f;std::fprintf(stderr,"[M95 timeline] %s: switching clips retains unsaved marker draft\n",retained?"PASS":"FAIL");m_panels.modelPreviewClip=1;if(!timeline.undo.empty()){timeline.draft.events=std::move(timeline.undo.back());timeline.undo.pop_back();}}
                if(autotestFrame==10&&!animationTimelineRecook){animationTimelineRecook=true;auto& timeline=*m_animationTimeline;bool loaded=timeline.draft.clip=="Walk"&&timeline.draft.events.size()==2;std::fprintf(stderr,"[M95 timeline] %s: clip draft and real GPU preview\n",loaded&&m_panels.modelPreviewToken?"PASS":"FAIL");if(loaded){timeline.Remember();timeline.draft.events[0].time=.2f;std::string error;bool saved=timeline.draft.Save(error);std::fprintf(stderr,"[M95 timeline] %s: explicit event draft save %s\n",saved?"PASS":"FAIL",error.c_str());m_panels.importTask=QueueModelImport(host.Jobs(),importRecipe);m_panels.importPublished=false;}}
                if(autotestFrame==13){m_panels.showProfiler=false;m_panels.showWorldBuilding=false;ImGui::SetWindowFocus("Animation timeline");}
                if(autotestFrame==14){const auto& clips=m_panels.importAccepted->preview->skeletal->clips;bool changed=clips.size()>1&&clips[1].events.size()==2&&std::abs(clips[1].events[0].time-.2f)<1e-6f;std::fprintf(stderr,"[M95 timeline] %s: normal asynchronous recook publishes edited marker\n",changed?"PASS":"FAIL");screenshot(std::string(autotest)+".timeline.png");}
                if(autotestFrame==16)m_panels.showAnimationTimeline=false;
            }
            if(importRecipe&&autotestFrame==6){m_panels.modelPreviewPlaying=true;m_panels.modelPreviewTime=.5f;}
            if(importRecipe&&autotestFrame==12){auto& parts=m_panels.importAccepted->preview->primitives;if(!parts.empty())m_panels.modelPreviewHidden={parts.front().part};}

            if(std::getenv("JUDAS_EDITOR_AUTOTEST_TILEMAP")&&m_tilemapEditor)
                m_tilemapEditor->Autotest(autotestFrame,m_document,m_panels,m_camera,window,[&](){screenshot(std::string(autotest)+".tilemap.png");});
            if(std::getenv("JUDAS_EDITOR_AUTOTEST_SPRITE_ANIMATION")){
                if(autotestFrame==3){for(const auto& [id,record]:host.Assets().Records())if(record.type==AssetType::SpriteSheet){m_panels.spriteSheetEdit=id;m_panels.showSpriteSheetEditor=true;break;}m_panels.showProfiler=false;m_panels.showWorldBuilding=false;}
                if(autotestFrame==8&&m_spriteSheetEditor){m_spriteSheetEditor->cell={24,48};m_spriteSheetEditor->clip=1;m_spriteSheetEditor->previewPlaying=true;}
                if(autotestFrame==10&&m_spriteSheetEditor){
                    auto& tool=*m_spriteSheetEditor;tool.Remember();tool.draft.frames[0].name+=" unsaved guard probe";
                    EditorRequests probe;probe.quit=true;HandleRequests(probe);
                    std::fprintf(stderr,"[Sprite animation editor] %s: unsaved sheet prevents desktop quit\n",!m_quit&&tool.Dirty()?"PASS":"FAIL");
                    tool.draft=tool.undo.back();tool.undo.pop_back();m_panels.status="Sprite sheet preview; gameplay events are disabled";
                }
                if(autotestFrame==13){m_panels.showProfiler=false;m_panels.showWorldBuilding=false;m_panels.showUIDocument=false;ImGui::SetWindowFocus("Sprite sheets and frame animation");}
                if(autotestFrame==14){ImGui::SetWindowFocus("Sprite sheets and frame animation");screenshot(std::string(autotest)+".sprite-sheet.png");std::fprintf(stderr,"[Sprite animation editor] %s: saved sheet loads and preview clock advances without scene mutation\n",m_spriteSheetEditor&&!m_spriteSheetEditor->path.empty()&&m_spriteSheetEditor->previewTime>0&&!m_spriteSheetEditor->Dirty()?"PASS":"FAIL");}
            }
            const std::string prefix = autotest;
            if (pickingAutotest) {
                // A private authored fixture supplies two tiny children under
                // a rotated/scaled parent. Events use the real SDL/ImGui path.
                const auto pointer = [&](glm::vec2 pixel, int buttonState = -1) {
                    SDL_Event event{};
                    event.type = SDL_MOUSEMOTION;
                    event.motion.windowID = SDL_GetWindowID(window.NativeWindow());
                    event.motion.x = static_cast<int>(pixel.x); event.motion.y = static_cast<int>(pixel.y);
                    SDL_PushEvent(&event);
                    if (buttonState >= 0) {
                        event = SDL_Event{};
                        event.type = buttonState ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                        event.button.windowID = SDL_GetWindowID(window.NativeWindow());
                        event.button.button = SDL_BUTTON_LEFT;
                        event.button.state = buttonState ? SDL_PRESSED : SDL_RELEASED;
                        event.button.x = static_cast<int>(pixel.x); event.button.y = static_cast<int>(pixel.y);
                        SDL_PushEvent(&event);
                    }
                };
                const auto wheel = [&](float steps) {
                    SDL_Event event{}; event.type = SDL_MOUSEWHEEL;
                    event.wheel.windowID = SDL_GetWindowID(window.NativeWindow());
                    event.wheel.y = static_cast<int>(steps);
#if SDL_VERSION_ATLEAST(2, 0, 18)
                    event.wheel.preciseY = steps;
#endif
                    SDL_PushEvent(&event);
                };
                const auto childPixel = [&](SceneObjectId id) {
                    Scene flat; std::string error;
                    if (!FlattenHierarchy(m_document.GetScene(), flat, error) || !flat.Find(id)) return glm::vec2(-1);
                    const auto& view = m_panels.workspace.viewport;
                    const auto clip = m_camera.ProjectionMatrix(view.size.x / view.size.y) * m_camera.ViewMatrix() *
                        glm::vec4(flat.Find(id)->transform.position, 1);
                    return glm::vec2(view.position.x + (clip.x / clip.w + 1) * .5f * view.size.x,
                                     view.position.y + (1 - clip.y / clip.w) * .5f * view.size.y);
                };
                if (autotestFrame == 1) {
                    navigationFocus(true); navigationMotion(0, 0);
                    dollyStartPosition = m_camera.Position(); dollyStartDistance = m_camera.FocusDistance();
                    dollyAnchor = dollyStartPosition + m_camera.Forward() * dollyStartDistance;
                } else if (autotestFrame == 2) wheel(1);
                else if (autotestFrame == 3) {
                    navigationCheck(std::abs(m_camera.FocusDistance() - dollyStartDistance * .8f) < 1e-4f &&
                        glm::length(m_camera.Position() + m_camera.Forward() * m_camera.FocusDistance() - dollyAnchor) < 1e-4f,
                        "SDL wheel zooms toward fixed focus in central viewport");
                    wheel(-1);
                } else if (autotestFrame == 4) {
                    navigationCheck(glm::length(m_camera.Position() - dollyStartPosition) < 1e-4f,
                                    "SDL inverse wheel restores camera position");
                    pointer({30, 150});
                } else if (autotestFrame == 5) wheel(2);
                else if (autotestFrame == 6) {
                    navigationCheck(glm::length(m_camera.Position() - dollyStartPosition) < 1e-4f,
                                    "SDL wheel over hierarchy does not move scene camera");
                    m_document.Select(kInvalidSceneObjectId); pointer(childPixel(9001), 1);
                } else if (autotestFrame == 7) pointer(childPixel(9001), 0);
                else if (autotestFrame == 8) {
                    navigationCheck(m_document.Selected() == 9001,
                                    "SDL viewport selects tiny nested box above large floor");
                    screenshot(prefix + ".nested.png");
                    m_document.Select(kInvalidSceneObjectId); pointer(childPixel(9002));
                } else if (autotestFrame == 9) pointer(childPixel(9002), 1);
                else if (autotestFrame == 10) pointer(childPixel(9002), 0);
                else if (autotestFrame == 11) {
                    navigationCheck(m_document.Selected() == 9002,
                                    "SDL viewport selects second child independently of parent proxy");
                    std::string actual; SaveSceneToString(m_document.GetScene(), actual);
                    navigationCheck(actual == autotestBaseline && !m_document.EditInProgress(),
                                    "wheel and selection preserve authored scene and Undo state");
                } else if (autotestFrame == 140)
                    navigationCheck(m_panels.mode == EditorMode::Play && m_world &&
                                    m_world->CountLifecycle().physicsBodies >= 14,
                                    "ordinary Play instantiates the real authored lab bodies");
                else if (autotestFrame == 150)
                    navigationCheck(m_panels.mode == EditorMode::Edit && !window.IsMouseCaptured(),
                                    "Play/Stop restores uncaptured editor after wheel and picking");
            }
            if (polishAutotest) {
                // This counter has already advanced after rendering. Queue
                // Escape for the frame after the dialog's pending check.
                if (autotestFrame == 13) screenshot(prefix + ".unsaved.png");
                if (autotestFrame == 14 || autotestFrame == 145) io.AddKeyEvent(ImGuiKey_Escape, true);
                if (autotestFrame == 15 || autotestFrame == 146) io.AddKeyEvent(ImGuiKey_Escape, false);
                if (autotestFrame == 13) SDL_SetWindowSize(window.NativeWindow(), 960, 640);
                if (autotestFrame == 15) screenshot(prefix + ".resized.png");
                if (autotestFrame == 16) {
                    SDL_SetWindowSize(window.NativeWindow(), kWindowWidth, kWindowHeight);
                    if (std::getenv("JUDAS_EDITOR_AUTOTEST_PREFS")) m_panels.assetBrowserHeight = 200;
                }
                if (autotestFrame == 143) { SDL_Event event{}; event.type = SDL_QUIT; SDL_PushEvent(&event); }
                if (autotestFrame == 146) screenshot(prefix + ".restored.png");
            }
            // M71 input evidence goes through SDL -> ImGui -> FrameEditMode,
            // using the ordinary scene camera and unchanged authored scene.
            if (navigationAutotest) {
                if (autotestFrame == 1) {
                    navigationFocus(true);
                    navigationMotion(0, 0);
                } else if (autotestFrame == 2) {
                    navigationStartPosition = m_camera.Position();
                    navigationStartFocus = m_panels.cameraFocus;
                    navigationStartForward = m_camera.Forward();
                    const float scale = 2.0f * m_camera.FocusDistance() * std::tan(glm::radians(30.0f)) /
                        m_panels.workspace.viewport.size.y;
                    navigationExpectedPosition = navigationStartPosition + scale *
                        (-m_camera.Right() * 80.0f - m_camera.Up() * 40.0f);
                    navigationButton(true);
                } else if (autotestFrame == 3) {
                    navigationCheck(m_cameraGesture.Active() == EditorCameraGesture::Pan && window.IsMouseCaptured(),
                                    "SDL middle press captures only viewport camera");
                    navigationMotion(80, -40);
                } else if (autotestFrame == 4) {
                    navigationCheck(glm::length(m_camera.Position() - navigationExpectedPosition) < 2e-4f &&
                                    glm::length(m_panels.cameraFocus - navigationStartFocus -
                                                (m_camera.Position() - navigationStartPosition)) < 2e-4f &&
                                    glm::length(m_camera.Forward() - navigationStartForward) < 1e-6f,
                                    "SDL drag translates camera and placement focus equally in view plane");
                    std::string afterPan; SaveSceneToString(m_document.GetScene(), afterPan);
                    navigationCheck(afterPan == autotestBaseline && !m_document.EditInProgress() && !m_drag.Active(),
                                    "pan preserves authored objects and gizmo undo state");
                    navigationButton(false);
                } else if (autotestFrame == 5) {
                    navigationCheck(m_cameraGesture.Active() == EditorCameraGesture::None && !window.IsMouseCaptured(),
                                    "SDL middle release frees capture");
                } else if (autotestFrame == 6) {
                    navigationButton(true);
                } else if (autotestFrame == 7) {
                    navigationCheck(m_cameraGesture.Active() == EditorCameraGesture::Pan, "fresh middle press restarts pan");
                    navigationFocus(false);
                } else if (autotestFrame == 8) {
                    navigationCheck(m_cameraGesture.Active() == EditorCameraGesture::None && !window.IsMouseCaptured(),
                                    "SDL window focus loss cancels and releases capture");
                    navigationFocus(true);
                } else if (autotestFrame == 9) {
                    navigationCheck(m_cameraGesture.Active() == EditorCameraGesture::None && !window.IsMouseCaptured(),
                                    "SDL focus regain cannot resume the old drag");
                    navigationButton(false);
                } else if (autotestFrame == 17) {
                    navigationMotion(0, 0);
                } else if (autotestFrame == 18) {
                    navigationButton(true);
                } else if (autotestFrame == 19) {
                    navigationCheck(m_cameraGesture.Active() == EditorCameraGesture::Pan, "pan active before ordinary Play request");
                } else if (autotestFrame == 21) {
                    navigationCheck(m_panels.mode == EditorMode::Play &&
                                    m_cameraGesture.Active() == EditorCameraGesture::None,
                                    "ordinary Play retires editor pan ownership");
                    navigationButton(false);
                } else if (autotestFrame == 150) {
                    navigationCheck(m_panels.mode == EditorMode::Edit && !window.IsMouseCaptured() &&
                                    m_cameraGesture.Active() == EditorCameraGesture::None,
                                    "ordinary Stop returns an uncaptured scene camera");
                }
            }
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
                m_panels.debug = terrainAutotest || volumeAutotest || polishAutotest || pickingAutotest || workspaceAutotest ? DebugViewOptions{} : all;
                m_panels.showProfiler = !std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING") && !terrainAutotest && !volumeAutotest && !retargetAutotest && !polishAutotest && !pickingAutotest && !workspaceAutotest;
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
    else CancelCameraNavigation();
    for (const std::string& id : m_heldAssets)if(!host.Resources().StreamableVisual(id)) host.Resources().ReleaseRef(id);
    m_heldAssets.clear();
    window.SetEventHook(nullptr);
    if (m_sceneViewTarget.IsValid()) renderer.DestroyRenderTarget(m_sceneViewTarget);
    m_planetPreview.clear();
    if(m_terrain){m_terrain->Shutdown(renderer,m_panels.importJobs);m_terrain.reset();}
    if(m_volumeTerrain){m_volumeTerrain->Shutdown(renderer,m_panels.importJobs);m_volumeTerrain.reset();}
    ShutdownRetargetPreview(m_panels, renderer);
    if(m_uiPreviewTarget.IsValid())renderer.DestroyRenderTarget(m_uiPreviewTarget);
    m_uiPreview.reset();
    if(m_modelPreviewTarget.IsValid())renderer.DestroyRenderTarget(m_modelPreviewTarget);
    if(!m_modelPreviewAsset.empty())host.Resources().ReleaseRef(m_modelPreviewAsset);
    m_panels.importTask.reset();
    SaveEditorPreferences();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_previewLocalization.reset();
    m_host = nullptr;
    return terrainFailures || navigationFailures || (m_tilemapEditor&&m_tilemapEditor->TestFailures()) || (m_stabilization && m_stabilization->failures) ? 1 : 0;
}

void EditorApplication::DrawUIAuthoringPreview(){
 if(m_panels.mode!=EditorMode::Edit||!m_panels.showAssetBrowser||!m_panels.uiCanvasPreview||!m_panels.uiPreviewRevision)return;
 JUDAS_PROFILE_SCOPE("UI authoring canvas");auto& renderer=m_host->GetRenderer();auto& resources=m_host->Resources();auto key=m_project.ProjectFile()+m_project.Settings().localization.Encode();if(key!=m_previewLocalizationKey){m_previewLocalization=std::make_unique<LocalizationSession>(m_project.Settings().localization);m_previewLocalization->Bind(&resources);m_previewLocalizationKey=key;}
 std::string error;if(!m_panels.previewLocale.empty())m_previewLocalization->SetLocale(m_panels.previewLocale,error);m_previewLocalization->Refresh();
 if(!m_uiPreview||m_uiPreviewRevision!=m_panels.uiPreviewRevision){auto candidate=std::make_unique<RuntimeUI>(&resources);if(!candidate->Add(m_panels.uiPreviewDocument,"authoring",0,error)){m_panels.status=error;return;}m_uiPreview=std::move(candidate);m_uiPreviewRevision=m_panels.uiPreviewRevision;}
 m_uiPreview->SetLocalization(m_previewLocalization.get());int width=m_panels.uiPreviewResolution.x,height=m_panels.uiPreviewResolution.y;if(!renderer.ResizeRenderTarget(m_uiPreviewTarget,width,height,error)||!renderer.BeginRenderTarget(m_uiPreviewTarget)){m_panels.status=error;return;}
 renderer.ClearWeather();renderer.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.035f,.045f,.065f});renderer.BeginFrame(width,height);renderer.EndFrame();renderer.BeginUIFrame(width,height);m_uiPreview->Draw(renderer,width,height);renderer.EndUIFrame();renderer.EndRenderTarget();m_panels.uiPreviewToken=renderer.EditorImageToken(renderer.RenderTargetTexture(m_uiPreviewTarget));m_panels.uiPreviewLayout.clear();auto handle=m_uiPreview->Find("authoring");for(auto& e:m_panels.uiPreviewDocument.elements)if(auto* l=m_uiPreview->LayoutOf(handle,e.id))m_panels.uiPreviewLayout[e.id]=*l;
 if(m_panels.uiCanvasPick.x>=0){for(auto it=m_panels.uiPreviewDocument.elements.rbegin();it!=m_panels.uiPreviewDocument.elements.rend();++it){auto* l=m_uiPreview->LayoutOf(handle,it->id);if(l&&l->visible&&l->rect.Contains(m_panels.uiCanvasPick)&&l->clip.Contains(m_panels.uiCanvasPick)){m_panels.uiCanvasSelected=it->id;break;}}m_panels.uiCanvasPick={-1,-1};}
}
