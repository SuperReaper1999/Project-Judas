#include "../authoring/AuthoringDocument.h"
#include "AnimationTimeline.h"
#include "AuthoringJSON.h"
#include "ComponentEditors.h"
#include "EditorApplication.h"
#include "EditorAutomation.h"
#include "EditorStabilization.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "ModAuthoring.h"
#include "ModelCook.h"
#include "PerformanceProfiler.h"
#include "Prefab.h"
#include "RetargetPanel.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "ScriptTools.h"
#include "SpriteSheetEditor.h"
#include "TerrainTools.h"
#include "TilemapEditor.h"
#include "UtilityAIEditor.h"
#include "VolumeTerrainTools.h"
#include "WorldStreaming.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
void EditorAutomation::RunSimulationChecks(EditorRequests &deferredRequests) {
    auto &io = ImGui::GetIO();
    const std::string prefix = autotest;
    // Opt-in M56 diagnostic interaction; ordinary Play timing/state is unchanged.
    if (std::getenv("JUDAS_EDITOR_AUTOTEST_PROFILER")) {
        if (autotestFrame == 23)
            io.AddKeyEvent(ImGuiKey_F8, true);
        if (autotestFrame == 24)
            io.AddKeyEvent(ImGuiKey_F8, false);
        if (autotestFrame == 28) {
            auto frames = PerformanceProfiler::Get().Timeline();
            profilerFrozenFrame = frames.empty() ? 0 : frames.back().id;
            profilerFrozenSteps = app.m_playSession.play->FixedStepsSinceReset();
            PerformanceProfiler::Get().Freeze(true);
        }
        if (autotestFrame == 40) {
            auto frames = PerformanceProfiler::Get().Timeline();
            bool ok = profilerFrozenFrame && !frames.empty() && frames.back().id == profilerFrozenFrame &&
                      app.m_playSession.play->FixedStepsSinceReset() > profilerFrozenSteps &&
                      !app.m_playSession.play->IsPaused() && !window.IsMouseCaptured() &&
                      app.m_playSession.world->pointerCapture;
            std::fprintf(stderr,
                         "[editor autotest] live cursor + frozen capture while simulation continues: %s\n",
                         ok ? "PASS" : "FAIL");
            PerformanceProfiler::Get().Freeze(false);
            app.m_panels.showProfiler = false;
        }
        if (autotestFrame == 43) {
            std::fprintf(stderr, "[editor autotest] closing profiler restores capture: %s\n",
                         window.IsMouseCaptured() ? "PASS" : "FAIL");
            app.m_panels.profilerInspect = false;
            app.m_panels.showProfiler = true;
        }
    }
    if (autotestFrame == 5) {
        if (std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING")) {
            app.m_panels.showWorldBuilding = true;
            app.m_panels.showUIDocument = true;
            app.m_panels.uiCanvasPreview = true;
            for (auto &[id, record] : app.m_host->Assets().Records())
                if (record.type == AssetType::UI) {
                    app.m_panels.browserSelection = id;
                    break;
                }
            if (const char *locale = std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING_LOCALE"))
                app.m_panels.previewLocale = locale;
            auto &objects = app.m_document.GetScene().Objects();
            if (objects.size() > 2) {
                app.m_document.Select(objects[0].id);
                app.m_document.Select(objects[2].id, true);
                std::string error;
                bool edited =
                    app.m_document.BatchTransform({.25f, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, error, true, true);
                app.m_document.Undo();
                std::string restored;
                SaveSceneToString(app.m_document.GetScene(), restored);
                std::fprintf(stderr, "[editor autotest] shared authoring batch + undo %s: %s\n",
                             edited && restored == autotestBaseline ? "PASS" : "FAIL", error.c_str());
            }
        }
        if (std::getenv("JUDAS_EDITOR_AUTOTEST_STREAMING"))
            app.m_panels.worldPreview = true;
        if (!app.m_document.GetScene().Objects().empty())
            app.m_document.Select(app.m_document.GetScene().Objects().front().id);
        if (const char *selection = std::getenv("JUDAS_EDITOR_AUTOTEST_SELECT")) {
            char *end = nullptr;
            const auto id = std::strtoull(selection, &end, 10);
            if (end != selection && *end == '\0' && app.m_document.GetScene().Find(id))
                app.m_document.Select(id);
            else
                navigationCheck(false, "requested visual-review selection exists");
        }
        DebugViewOptions all;
        all.collisionShapes = all.playerCapsule = all.contacts = all.gravity = all.frameAxes = all.lights =
            all.interactionRanges = all.lifecycle = all.terrainNormals = all.fluidParticles = all.atmosphere =
                true;
        app.m_panels.debug =
            terrainAutotest || volumeAutotest || polishAutotest || pickingAutotest || workspaceAutotest
                ? DebugViewOptions{}
                : all;
        app.m_panels.showProfiler = !std::getenv("JUDAS_EDITOR_AUTOTEST_NEW_2D") &&
                                    !std::getenv("JUDAS_EDITOR_AUTOTEST_AUTHORING") && !terrainAutotest &&
                                    !volumeAutotest && !retargetAutotest && !polishAutotest &&
                                    !pickingAutotest && !workspaceAutotest;
    } else if (autotestFrame == 10) {
        // Duplicate + undo must leave the scene exactly as authored.
        const SceneObjectId selected = app.m_document.Selected();
        if (selected != kInvalidSceneObjectId) {
            const SceneObjectId copy = DuplicateObject(app.m_document, selected);
            std::string afterDuplicate;
            SaveSceneToString(app.m_document.GetScene(), afterDuplicate);
            app.m_document.Undo();
            std::string afterUndo;
            SaveSceneToString(app.m_document.GetScene(), afterUndo);
            std::fprintf(stderr, "[editor autotest] duplicate created id %llu: scene %s; undo restores: %s\n",
                         static_cast<unsigned long long>(copy),
                         afterDuplicate != autotestBaseline ? "CHANGED" : "unchanged(!)",
                         afterUndo == autotestBaseline ? "IDENTICAL" : "DIFFERENT");
            app.m_document.Select(selected);
        }
    } else if (autotestFrame == 15 && std::getenv("JUDAS_EDITOR_AUTOTEST_RUN")) {
        // Launch the runtime on the project exactly as Run project
        // does; with JUDAS_TEST_SCRIPT inherited, the child runs the
        // harness on the startup scene and exits by itself.
        std::string message;
        const bool launched = app.RunProject(message);
        std::fprintf(stderr, "[editor autotest] run project: %s: %s\n", launched ? "LAUNCHED" : "FAILED",
                     message.c_str());
    } else if (autotestFrame == 20) {
        screenshot(prefix + ".edit.png");
        if (std::getenv("JUDAS_EDITOR_AUTOTEST_STREAMING"))
            std::fprintf(stderr, "[editor autotest] additive preview: %s (%zu sources)\n",
                         !app.m_sceneViewport.regionPreview.empty() ? "PASS" : "FAIL",
                         app.m_sceneViewport.regionPreview.size());
        std::fprintf(stderr, "[editor autotest] edit frame: %u draw calls, %u triangles, %u debug lines\n",
                     app.m_panels.profiler.drawCalls, app.m_panels.profiler.triangles,
                     app.m_panels.profiler.debugLines);
        deferredRequests.play = true;
    } else if (autotestFrame == 140) {
        screenshot(prefix + ".play.png");
        if (std::getenv("JUDAS_EDITOR_AUTOTEST_STREAMING")) {
            std::string error;
            auto *stream =
                app.m_playSession.world->SceneControl()->Streaming(*app.m_playSession.world, error);
            std::fprintf(stderr, "[editor autotest] shared world additive activation: %s\n",
                         stream && stream->Resolve("gallery-0", 100) ? "PASS" : "FAIL");
        }
        const ProfilerData &p = app.m_panels.profiler;
        std::fprintf(
            stderr,
            "[editor autotest] play frame: %d steps, step %.3f ms, %zu bodies, %zu contacts, %u draw "
            "calls, %u triangles, %u shadow passes, %u lights, %u debug lines, %zu particles\n",
            p.fixedStepsThisFrame, p.fixedStepMilliseconds, p.physicsBodies, p.contacts, p.drawCalls,
            p.triangles, p.shadowPasses, p.dynamicLights, p.debugLines, p.fluidParticles);
        std::fprintf(stderr,
                     "[editor autotest] resources: %zu meshes, %zu textures, %zu terrain meshes, hits "
                     "%llu, misses %llu, failed %zu, uploads %llu, resident %llu bytes\n",
                     p.resources.loadedMeshes, p.resources.loadedTextures, p.resources.loadedTerrainMeshes,
                     p.resources.hits, p.resources.misses, p.resources.failed, p.resources.uploads,
                     static_cast<unsigned long long>(p.resources.bytesResident));
        std::fprintf(stderr,
                     "[editor autotest] jobs: %u workers, %llu completed, %llu failed, %llu cancelled\n",
                     p.jobs.workers, p.jobs.completed, p.jobs.failed, p.jobs.cancelled);
        deferredRequests.stop = true;
    } else if (autotestFrame == 150) {
        std::string after;
        SaveSceneToString(app.m_document.GetScene(), after);
        std::fprintf(stderr, "[editor autotest] authored scene after play/stop is %s\n",
                     after == autotestBaseline ? "IDENTICAL" : "DIFFERENT");
        std::string e;
        const bool saved = SaveSceneToFile(app.m_document.GetScene(), prefix + ".saved.judas", e);
        std::fprintf(stderr, "[editor autotest] save %s\n", saved ? "ok" : e.c_str());
        app.m_quit = true;
    }
}
