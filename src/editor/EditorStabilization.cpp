#include "EditorStabilization.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "WorldCoordinates.h"
#include "WorldState.h"
#include "WorldStreaming.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
void EditorApplication::AdvanceStabilizationAutomation() {
    if (!m_stabilization)
        return;
    StabilizationAutomation &a = *m_stabilization;
    const auto check = [&](bool ok, const char *label) {
        if (!a.Check(ok, label))
            m_quit = true;
        return ok;
    };
    const auto request = [&](EditorRequests r) { HandleRequests(r); };
    const auto sceneText = [&]() {
        std::string text;
        SaveSceneToString(m_document.GetScene(), text);
        return text;
    };
    const auto read = [](const fs::path &path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto handle = [&](EntityId id) {
        return m_playSession.world->DynamicBodies()[m_playSession.world->FindEntity(id)->slot].Handle();
    };
    const auto sameState = [](const EntityPhysicalState &x, const EntityPhysicalState &y) {
        return x.position == y.position && x.rotation == y.rotation && x.linearVelocity == y.linearVelocity &&
               x.angularVelocity == y.angularVelocity;
    };
    std::string error;
    std::error_code ec;
    if (++a.frames > 30000) {
        check(false, "progress_watchdog");
        return;
    }
    if (a.phase == 0) {
        if (!check(!m_host->Resources().BlockingMode(), "normal_async_mode"))
            return;
        const fs::path root = fs::path(a.output) / "project";
        if (!check(!fs::exists(root), "fresh_generated_project"))
            return;
        fs::create_directories(a.output, ec);
        if (!check(!ec && CreateProject(root.string(), "FTFT6 Tiny", error), "create_project"))
            return;
        if (!check(!m_document.GetScene().Objects().empty() &&
                       m_project.Settings().startupScene == "Scenes/main.judas",
                   "starter_scene_and_project_startup"))
            return;
        bool ordinary = true;
        for (const SceneObject &o : m_document.GetScene().Objects())
            ordinary = ordinary && !o.celestial && !o.atmosphere && !o.fluidVolume;
        if (!check(ordinary, "non_planetary_project"))
            return;
        for (int k = 0; k < 3; ++k) {
            EditorRequests r;
            r.createKind = "dynamic-box";
            r.createPosition = glm::vec3(-6.0f + 4.0f * k, 8.0f, 0.0f);
            request(r);
            if (k == 0)
                a.moving = m_document.Selected();
            if (k == 1)
                a.removed = m_document.Selected();
            if (k == 2)
                a.coarse = m_document.Selected();
        }
        if (!check(a.moving && a.removed && a.coarse, "create_authored_entities"))
            return;
        const std::string before = sceneText();
        m_document.BeginEdit();
        m_document.GetScene().Find(a.moving)->body->initialLinearVelocity = glm::vec3(0.75f, 0.0f, 0.0f);
        m_document.GetScene().Find(a.coarse)->body->initialLinearVelocity = glm::vec3(-0.5f, 0.0f, 0.0f);
        m_document.CommitEdit();
        const std::string edited = sceneText();
        EditorRequests r;
        r.undo = true;
        request(r);
        if (!check(sceneText() == before, "undo_component_edit"))
            return;
        r = {};
        r.redo = true;
        request(r);
        if (!check(sceneText() == edited && edited != before, "redo_component_edit"))
            return;
        r = {};
        r.duplicateId = a.moving;
        request(r);
        const SceneObjectId duplicate = m_document.Selected();
        if (!check(duplicate != a.moving && m_document.GetScene().Find(duplicate), "duplicate_entity"))
            return;
        // Same document transaction as the editor's Delete key.
        m_document.BeginEdit();
        m_document.GetScene().DestroyObject(duplicate);
        m_document.CommitEdit();
        if (!check(!m_document.GetScene().Find(duplicate), "delete_authored_entity"))
            return;
        r = {};
        r.undo = true;
        request(r);
        if (!check(m_document.GetScene().Find(duplicate) != nullptr, "undo_delete"))
            return;
        r = {};
        r.redo = true;
        request(r);
        if (!check(!m_document.GetScene().Find(duplicate), "redo_delete"))
            return;
        const fs::path source = fs::path(a.output) / "import.obj";
        {
            std::ofstream file(source);
            file << "v -1 0 0\nv 1 0 0\nv 0 2 0\nf 1 2 3\n";
        }
        r = {};
        r.importSource = source.string();
        r.importDestination = "models/original.obj";
        request(r);
        a.assetId = m_panels.browserSelection;
        const AssetRecord *imported = m_host->Assets().Find(a.assetId);
        if (!check(imported && !imported->missing, "import_real_obj"))
            return;
        a.assetPath = imported->path;
        r = {};
        r.dropMeshAssetId = a.assetId;
        r.createPosition = glm::vec3(4.0f, 0.0f, -3.0f);
        request(r);
        a.mesh = m_document.Selected();
        if (!check(m_document.GetScene().Find(a.mesh)->render->meshAsset == a.assetId,
                   "authored_stable_asset_reference"))
            return;
        a.phase = 1;
    } else if (a.phase == 1) {
        if (m_host->Resources().StateOf(a.assetId) != ResourceState::Ready)
            return;
        if (!check(m_host->Resources().TryGetMesh(a.assetId).IsValid() &&
                       m_host->Resources().DecodeThreadOf(a.assetId) != m_host->Resources().OwnerThread(),
                   "async_decode_and_real_gpu_mesh"))
            return;
        EditorRequests r;
        r.moveAssetId = a.assetId;
        r.moveAssetTo = "renamed/moved.obj";
        request(r);
        const AssetRecord *moved = m_host->Assets().Find(a.assetId);
        if (!check(moved && moved->path != a.assetPath && fs::exists(moved->path) &&
                       !fs::exists(a.assetPath) &&
                       m_document.GetScene().Find(a.mesh)->render->meshAsset == a.assetId,
                   "move_preserves_identity_and_reference"))
            return;
        a.assetPath = moved->path;
        fs::rename(a.assetPath, a.assetPath + ".missing", ec);
        if (!check(!ec, "prepare_missing_asset"))
            return;
        r = {};
        r.rescanAssets = true;
        request(r);
        const AssetRecord *missing = m_host->Assets().Find(a.assetId);
        if (!check(missing && missing->missing && !m_host->Assets().Problems().empty(),
                   "missing_asset_database_diagnostic"))
            return;
        a.phase = 2;
    } else if (a.phase == 2) {
        if (m_host->Resources().StateOf(a.assetId) != ResourceState::Failed)
            return;
        if (!check(!m_host->Resources().ErrorOf(a.assetId).empty() &&
                       !m_host->Resources().TryGetMesh(a.assetId).IsValid(),
                   "missing_asset_resource_failure"))
            return;
        fs::rename(a.assetPath + ".missing", a.assetPath, ec);
        if (!check(!ec, "restore_missing_asset"))
            return;
        const fs::path duplicate = fs::path(m_project.AssetsDir()) / "duplicate.obj";
        fs::copy_file(a.assetPath, duplicate, fs::copy_options::none, ec);
        if (!check(!ec, "prepare_duplicate_asset"))
            return;
        fs::copy_file(a.assetPath + kAssetMetaExtension, duplicate.string() + kAssetMetaExtension,
                      fs::copy_options::none, ec);
        if (!check(!ec, "prepare_duplicate_metadata"))
            return;
        EditorRequests r;
        r.rescanAssets = true;
        request(r);
        bool reported = false;
        for (const AssetProblem &problem : m_host->Assets().Problems())
            reported = reported || problem.message.find("duplicate asset id") != std::string::npos;
        if (!check(reported && m_host->Assets().Records().size() == 1,
                   "duplicate_metadata_reported_without_second_identity"))
            return;
        fs::remove(duplicate, ec);
        fs::remove(duplicate.string() + kAssetMetaExtension, ec);
        r = {};
        r.rescanAssets = true;
        request(r);
        if (!check(m_host->Assets().Problems().empty() &&
                       m_host->Assets().Find(a.assetId)->path == a.assetPath,
                   "asset_repair_recovers_original_identity"))
            return;
        a.phase = 3;
    } else if (a.phase == 3) {
        if (m_host->Resources().StateOf(a.assetId) != ResourceState::Ready)
            return;
        if (!check(m_host->Resources().TryGetMesh(a.assetId).IsValid(), "resource_recovers_after_rescan"))
            return;
        if (!check(m_document.SaveAs(m_project.Resolve("Scenes/edited.judas"), error),
                   "save_authored_scene_as"))
            return;
        m_project.Settings().startupScene = "Scenes/edited.judas";
        EditorRequests r;
        r.saveProject = true;
        request(r);
        a.baseline = sceneText();
        if (!check(ComputeSceneFingerprint(m_document.GetScene(), a.fingerprint, error),
                   "authored_fingerprint"))
            return;
        const std::string projectFile = m_project.ProjectFile();
        if (!check(OpenProject(projectFile, error) && OpenScene(m_project.Settings().startupScene, error),
                   "reopen_project_startup_scene"))
            return;
        if (!check(sceneText() == a.baseline && !m_document.IsDirty() &&
                       m_project.Settings().startupScene == "Scenes/edited.judas",
                   "save_reopen_preserves_authored_data"))
            return;
        r = {};
        r.play = true;
        request(r);
        if (!check(m_playSession.play && m_playSession.world && m_panels.mode == EditorMode::Play,
                   "play_builds_real_runtime"))
            return;
        if (!check(!m_playSession.world->HasFluid() && m_playSession.world->CelestialParticipants().empty() &&
                       !m_playSession.world->GetAtmosphere(),
                   "play_requires_no_planetary_systems"))
            return;
        a.phase = 4;
    } else if (a.phase == 4) {
        if (m_playSession.play->FixedStepsSinceReset() < 24)
            return;
        a.playSteps = m_playSession.play->FixedStepsSinceReset();
        EntityPhysicalState moved;
        if (!check(m_playSession.world->GetEntityState(a.moving, moved) && moved.position.x > -6.0f &&
                       moved.position.y < 8.0f && m_panels.profiler.drawCalls > 0,
                   "ordinary_loop_advances_and_renders_scene"))
            return;
        const BodyHandle old = handle(a.moving);
        if (!check(m_playSession.world->SetEntityFidelity(a.moving, SimulationFidelity::Dormant, &error) &&
                       m_playSession.world->FindEntity(a.moving)->lifecycle == EntityLifecycle::Unloaded &&
                       !m_playSession.world->Physics().IsDynamicBody(old),
                   "unload_releases_physics_preserves_identity"))
            return;
        if (!check(m_playSession.world->SetEntityFidelity(a.moving, SimulationFidelity::Full, &error) &&
                       handle(a.moving).id != old.id && !m_playSession.world->Physics().IsDynamicBody(old),
                   "reconstruction_invalidates_stale_handle"))
            return;
        EntityPhysicalState reconstructed;
        m_playSession.world->GetEntityState(a.moving, reconstructed);
        if (!check(sameState(moved, reconstructed), "reconstruction_preserves_moving_state"))
            return;
        if (!check(m_playSession.world->SetEntityFidelity(a.moving, SimulationFidelity::Dormant, &error) &&
                       m_playSession.world->GetEntityState(a.moving, a.dormantState),
                   "return_to_dormant"))
            return;
        if (!check(m_playSession.world->SetEntityFidelity(a.coarse, SimulationFidelity::Coarse, &error) &&
                       m_playSession.world->FindEntity(a.coarse)->lifecycle == EntityLifecycle::Active &&
                       m_playSession.world->GetEntityState(a.coarse, a.coarseBefore),
                   "coarse_remains_active"))
            return;
        if (!check(m_playSession.world->DestroyEntity(a.removed, &error) &&
                       m_playSession.world->FindEntity(a.removed)->lifecycle == EntityLifecycle::Destroyed &&
                       !m_playSession.world->SetEntityFidelity(a.removed, SimulationFidelity::Full, &error),
                   "destroy_is_permanent"))
            return;
        SceneObject definition = *m_document.GetScene().Find(a.removed);
        definition.id = 0;
        definition.name = "Runtime created";
        definition.transform.position = glm::vec3(10.0f, 6.0f, 0.0f);
        a.created = m_playSession.world->CreateEntity(definition, nullptr, &error);
        if (!check(a.created >= kRuntimeEntityIdBase && m_playSession.world->FindEntity(a.created),
                   "create_runtime_entity"))
            return;
        a.phaseStep = m_playSession.play->FixedStepsSinceReset();
        a.phase = 5;
    } else if (a.phase == 5) {
        if (m_playSession.play->FixedStepsSinceReset() < a.phaseStep + 12)
            return;
        a.playSteps = m_playSession.play->FixedStepsSinceReset();
        EntityPhysicalState dormant, coarse;
        if (!check(m_playSession.world->GetEntityState(a.moving, dormant) &&
                       sameState(dormant, a.dormantState),
                   "dormant_does_not_simulate"))
            return;
        if (!check(m_playSession.world->GetEntityState(a.coarse, coarse) &&
                       coarse.position != a.coarseBefore.position &&
                       m_playSession.world->FindEntity(a.coarse)->coarseStepsSimulated >= 12,
                   "coarse_simulates_in_real_loop"))
            return;
        EditorRequests r;
        r.saveWorldState = true;
        request(r);
        a.saveBytes = read(m_playSession.play->WorldStatePath());
        if (!check(!a.saveBytes.empty(), "save_mixed_runtime_deltas"))
            return;
        r = {};
        r.stop = true;
        request(r);
        if (!check(!m_playSession.world && !m_playSession.play && sceneText() == a.baseline,
                   "stop_restores_exact_authored_scene"))
            return;
        r = {};
        r.play = true;
        request(r);
        if (!check(m_playSession.world && m_playSession.play, "play_reloads_saved_world"))
            return;
        EntityPhysicalState restored;
        if (!check(m_playSession.world->GetEntityState(a.moving, restored) &&
                       sameState(restored, a.dormantState) &&
                       m_playSession.world->FindEntity(a.moving)->id == a.moving &&
                       m_playSession.world->FindEntity(a.moving)->fidelity == SimulationFidelity::Full,
                   "delta_reconstructs_identity_and_pose_not_fidelity"))
            return;
        if (!check(m_playSession.world->FindEntity(a.removed)->lifecycle == EntityLifecycle::Destroyed &&
                       !m_playSession.world->SetEntityFidelity(a.removed, SimulationFidelity::Full, &error) &&
                       m_playSession.world->FindEntity(a.created),
                   "destroyed_and_created_deltas_survive_play_restart"))
            return;
        r = {};
        r.stop = true;
        request(r);
        m_document.BeginEdit();
        m_document.GetScene().Find(a.moving)->transform.position.x += 0.25f;
        m_document.CommitEdit();
        const std::string incompatible = sceneText();
        const std::string statePath = WorldStatePathFor(m_document.Path());
        r = {};
        r.play = true;
        request(r);
        if (!check(!m_playSession.world && !m_playSession.play && m_panels.mode == EditorMode::Edit &&
                       m_panels.status.find("fingerprint") != std::string::npos &&
                       sceneText() == incompatible && read(statePath) == a.saveBytes,
                   "incompatible_play_rejected_without_authoring_or_save_mutation"))
            return;
        r = {};
        r.undo = true;
        request(r);
        if (!check(sceneText() == a.baseline && m_document.Save(error),
                   "restore_compatible_authored_baseline"))
            return;
        std::ofstream result(fs::path(a.output) / "result.json");
        result << "{\n  \"overall_pass\": true,\n  \"checks\": " << a.checks + 1
               << ",\n  \"frames\": " << a.frames << ",\n  \"fixed_steps\": " << a.playSteps
               << ",\n  \"project\": \"project/FTFT6_Tiny.judasproj\",\n"
               << "  \"startup_scene\": \"project/Scenes/edited.judas\",\n  \"authored_fingerprint\": \""
               << a.fingerprint << "\",\n  \"asset_id\": \"" << a.assetId
               << "\",\n  \"moved_entity\": " << a.moving << ",\n  \"destroyed_entity\": " << a.removed
               << ",\n  \"created_entity\": " << a.created << "\n}\n";
        result.close();
        if (!check(static_cast<bool>(result), "write_result_metadata"))
            return;
        std::fprintf(stderr, "FTFT6 SUMMARY PASS checks=%u fixed_steps=%zu frames=%u\n", a.checks,
                     a.playSteps, a.frames);
        m_quit = true;
    }
}
