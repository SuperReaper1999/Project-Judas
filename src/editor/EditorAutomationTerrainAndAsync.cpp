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
void EditorAutomation::RunTerrainAndAsyncChecks(EditorRequests &deferredRequests) {
    auto &io = ImGui::GetIO();
    if (autotestFrame == 30 && std::getenv("JUDAS_EDITOR_AUTOTEST_RUNTIME_INSPECTOR") &&
        app.m_playSession.world) {
        app.m_panels.showRuntimeInspector = true;
        const auto keys = app.m_playSession.world->ScriptKeys();
        if (!keys.empty())
            app.m_panels.runtimeInspectEntity = keys.front().first;
    }
    bool waiting = importRecipe &&
                   (!app.m_panels.modelPreviewToken ||
                    (animationTimelineAutotest && app.m_panels.importTask != app.m_panels.importAccepted));
    if (waiting &&
        ((app.m_panels.importTask->done && !app.m_panels.importTask->success) || ++importWaitFrames > 4000)) {
        std::fprintf(stderr, "[editor autotest] import FAIL: %s\n", app.m_panels.importTask->error.c_str());
        app.m_quit = true;
    }
    if (retargetAutotest) {
        std::string error;
        int status = RetargetAutotestStatus(app.m_panels, error);
        if (status < 0 || (status == 0 && ++retargetWaitFrames > 4000)) {
            std::fprintf(stderr, "[M74 editor retarget] FAIL: %s\n", error.c_str());
            app.m_quit = true;
        }
        waiting = waiting || status == 0;
    }
    if (terrainAutotest && !terrainUiReady) {
        // Exercise the human-facing activation button, not direct
        // mutation of loaded/editing as the original smoke did.
        waiting = true;
        ++terrainUiFrame;
        if (terrainUiFrame == 1) {
            app.m_document.Select(1);
            app.m_panels.showTerrain = true;
            navigationFocus(true);
        }
        if (terrainUiFrame == 2) {
            const auto p = app.m_terrain->editButtonCenter;
            const auto native = NativePoint(p, "Terrain - sculpt / paint");
            SDL_WarpMouseInWindow(native.window, native.position.x, native.position.y);
            SDL_Event event{};
            event.type = SDL_MOUSEMOTION;
            event.motion.windowID = SDL_GetWindowID(native.window);
            event.motion.x = native.position.x;
            event.motion.y = native.position.y;
            SDL_PushEvent(&event);
        }
        if (terrainUiFrame == 4 || terrainUiFrame == 6) {
            const auto p = app.m_terrain->editButtonCenter;
            const auto native = NativePoint(p, "Terrain - sculpt / paint");
            SDL_WarpMouseInWindow(native.window, native.position.x, native.position.y);
            SDL_Event event{};
            event.type = terrainUiFrame == 4 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            event.button.windowID = SDL_GetWindowID(native.window);
            event.button.button = SDL_BUTTON_LEFT;
            event.button.state = terrainUiFrame == 4 ? SDL_PRESSED : SDL_RELEASED;
            event.button.x = native.position.x;
            event.button.y = native.position.y;
            SDL_PushEvent(&event);
        }
        if (terrainUiFrame == 9) {
            screenshot(std::string(autotest) + ".activation.png");
            terrainCheck(app.m_terrain->loaded && app.m_terrain->editing,
                         "real panel click opens selected source and activates brush");
            if (!app.m_terrain->loaded) {
                app.m_quit = true;
            } else
                terrainUiReady = true;
        }
    }
    if (volumeAutotest && !volumeUiReady) {
        waiting = true;
        ++volumeUiFrame;
        if (volumeUiFrame == 1) {
            app.m_document.Select(1);
            app.m_panels.showVolumeTerrain = true;
            navigationFocus(true);
        }
        if (volumeUiFrame == 2) {
            auto p = app.m_volumeTerrain->editButtonCenter;
            const auto native = NativePoint(p, "Volumetric terrain - dig / build");
            SDL_WarpMouseInWindow(native.window, native.position.x, native.position.y);
            SDL_Event e{};
            e.type = SDL_MOUSEMOTION;
            e.motion.windowID = SDL_GetWindowID(native.window);
            e.motion.x = native.position.x;
            e.motion.y = native.position.y;
            SDL_PushEvent(&e);
        }
        if (volumeUiFrame == 4 || volumeUiFrame == 6) {
            auto p = app.m_volumeTerrain->editButtonCenter;
            const auto native = NativePoint(p, "Volumetric terrain - dig / build");
            SDL_Event e{};
            e.type = volumeUiFrame == 4 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            e.button.windowID = SDL_GetWindowID(native.window);
            e.button.button = SDL_BUTTON_LEFT;
            e.button.state = volumeUiFrame == 4 ? SDL_PRESSED : SDL_RELEASED;
            e.button.x = native.position.x;
            e.button.y = native.position.y;
            SDL_PushEvent(&e);
        }
        if (volumeUiFrame == 9) {
            volumeCheck(app.m_volumeTerrain->loaded && app.m_volumeTerrain->editing,
                        "real panel click opens selected volume and activates brush");
            volumeUiReady = app.m_volumeTerrain->loaded;
            if (!volumeUiReady)
                app.m_quit = true;
        }
    }
    if (volumeAutotest && volumeUiReady && (autotestFrame == 0 || autotestFrame == 16) &&
        (!app.m_volumeTerrain->preview ||
         app.m_volumeTerrain->previewRevision != app.m_volumeTerrain->draft.Revision())) {
        waiting = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (++volumeWaitFrames > 4000) {
            volumeCheck(false, "bounded async preview completion");
            app.m_quit = true;
        }
    }
    if (!waiting)
        ++autotestFrame;
    if (volumeAutotest && volumeUiReady && !waiting) {
        auto mouse = [&](bool down) {
            const auto native = NativePoint({io.MousePos.x, io.MousePos.y});
            SDL_Event e{};
            e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            e.button.windowID = SDL_GetWindowID(native.window);
            e.button.button = SDL_BUTTON_LEFT;
            e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
            e.button.x = native.position.x;
            e.button.y = native.position.y;
            SDL_PushEvent(&e);
        };
        if (autotestFrame == 1) {
            volumeCheck(app.m_volumeTerrain->draft.Path() == app.m_project.Resolve(volumeAutotest),
                        "button opened selected model's exact author-owned source");
            volumeBefore = app.m_volumeTerrain->draft.source.density;
            app.m_volumeTerrain->brush.radius = 1.3f;
            app.m_volumeTerrain->brush.strength = 1;
            app.m_panels.showVolumeTerrain = false;
            app.m_panels.debug = {};
            app.m_sceneViewport.camera.SetPose({4, 5, 20}, 180, 0);
            auto &v = app.m_panels.workspace.viewport;
            WarpPoint(v.position + v.size * .5f);
            navigationFocus(true);
        } else if (autotestFrame == 3)
            mouse(true);
        else if (autotestFrame == 4)
            volumeCheck(app.m_volumeTerrain->draft.Active() &&
                            app.m_volumeTerrain->draft.source.density != volumeBefore,
                        "native viewport press removes real 3D material");
        else if (autotestFrame == 5)
            mouse(false);
        else if (autotestFrame == 6) {
            volumeEdited = app.m_volumeTerrain->draft.source.density;
            volumeCheck(!app.m_volumeTerrain->draft.Active(), "release commits one volume gesture");
            volumeCheck(app.m_volumeTerrain->draft.Undo() &&
                            app.m_volumeTerrain->draft.source.density == volumeBefore,
                        "one undo restores complete field");
        } else if (autotestFrame == 7)
            volumeCheck(app.m_volumeTerrain->draft.Redo() &&
                            app.m_volumeTerrain->draft.source.density == volumeEdited,
                        "redo restores volumetric edit");
        else if (autotestFrame == 8) {
            app.m_volumeTerrain->brush.kind = VolumeBrushKind::Add;
            mouse(true);
        } else if (autotestFrame == 9) {
            volumeCheck(app.m_volumeTerrain->draft.Active() &&
                            app.m_volumeTerrain->draft.source.density != volumeEdited,
                        "add gesture changes material before cancellation");
            io.AddKeyEvent(ImGuiKey_Escape, true);
        } else if (autotestFrame == 10) {
            io.AddKeyEvent(ImGuiKey_Escape, false);
            mouse(false);
            volumeCheck(!app.m_volumeTerrain->draft.Active() &&
                            app.m_volumeTerrain->draft.source.density == volumeEdited,
                        "Escape restores in-flight volume gesture");
            app.m_volumeTerrain->editing = true;
        } else if (autotestFrame == 11)
            mouse(true);
        else if (autotestFrame == 12)
            navigationFocus(false);
        else if (autotestFrame == 13) {
            mouse(false);
            navigationFocus(true);
            volumeCheck(!app.m_volumeTerrain->draft.Active() &&
                            app.m_volumeTerrain->draft.source.density != volumeEdited,
                        "focus loss commits an actually changed material gesture");
            volumeCheck(app.m_volumeTerrain->draft.Undo() &&
                            app.m_volumeTerrain->draft.source.density == volumeEdited,
                        "one undo removes the focus-ended add gesture");
            std::string why;
            volumeCheck(app.m_volumeTerrain->draft.Save(why), "save actual viewport volume edit");
            TerrainCookResult result;
            volumeCheck(CookVolumeTerrain(app.m_volumeTerrain->draft.source,
                                          app.m_volumeTerrain->draft.Path(), app.m_project.RootDir(),
                                          host.Assets(), result, why),
                        "normal cook publishes edited model and physical collider");
            if (!why.empty())
                std::fprintf(stderr, "[M84 editor error] %s\n", why.c_str());
            deferredRequests.rescanAssets = true;
            app.m_volumeTerrain->editing = false;
        } else if (autotestFrame == 17) {
            MeshData gpu;
            volumeCheck(
                renderer.ReadMeshForDiagnostics(app.m_volumeTerrain->mesh, gpu) &&
                    gpu.vertices.size() == app.m_volumeTerrain->preview->vertices.size() &&
                    glm::length(gpu.vertices[gpu.vertices.size() / 2].position -
                                app.m_volumeTerrain->preview->vertices[gpu.vertices.size() / 2].position) <
                        1e-6f,
                "actual GL volume mesh agrees with edited field extraction");
        } else if (autotestFrame == 18) {
            std::vector<SceneObjectId> ids;
            for (auto &o : app.m_document.GetScene().Objects())
                if (o.navigationSurface)
                    ids.push_back(o.id);
            for (auto id : ids)
                volumeCheck(BakeEditorNavigation(app.m_document, id, app.m_panels),
                            "rebake navigation against edited cave geometry");
            std::string why;
            volumeCheck(app.m_document.Save(why), "save refreshed navigation reference");
            SaveSceneToString(app.m_document.GetScene(), autotestBaseline);
        } else if (autotestFrame == 21)
            volumeCheck(app.m_panels.mode == EditorMode::Play, "saved edited volume enters ordinary Play");
        else if (autotestFrame == 150) {
            volumeCheck(app.m_panels.mode == EditorMode::Edit && !app.m_volumeTerrain->draft.Dirty() &&
                            app.m_volumeTerrain->draft.source.density == volumeEdited,
                        "Stop preserves saved 3D edit without stale gameplay state");
            std::fprintf(stderr, "[M84 editor volume] failures %d\n", volumeFailures);
        }
    }

    if (terrainAutotest && terrainUiReady && !waiting) {
        if (autotestFrame % 20 == 0)
            std::fprintf(stderr, "[M75 editor progress] frame %d step %.3f ms mode %d\n", autotestFrame,
                         app.m_panels.profiler.fixedStepMilliseconds, int(app.m_panels.mode));
        const auto mouse = [&](bool down) {
            const auto native = NativePoint({io.MousePos.x, io.MousePos.y});
            SDL_Event event{};
            event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            event.button.windowID = SDL_GetWindowID(native.window);
            event.button.button = SDL_BUTTON_LEFT;
            event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
            event.button.x = native.position.x;
            event.button.y = native.position.y;
            SDL_PushEvent(&event);
        };
        if (autotestFrame == 1) {
            terrainCheck(app.m_terrain->draft.Path() == app.m_project.Resolve(terrainAutotest),
                         "panel opened the ordinary selected source");
            app.m_terrain->brush.radius = 3;
            app.m_terrain->brush.strength = .7f;
            terrainInitial = app.m_terrain->draft.source.heights;
            app.m_document.Select(1);
            app.m_panels.showTerrain = false;
            app.m_panels.debug = {};
            auto &view = app.m_panels.workspace.viewport;
            auto clip = app.m_sceneViewport.camera.ProjectionMatrix(view.size.x / view.size.y) *
                        app.m_sceneViewport.camera.ViewMatrix() * glm::vec4(0, 0, 10, 1);
            int x = int(view.position.x + (clip.x / clip.w + 1) * .5f * view.size.x),
                y = int(view.position.y + (1 - clip.y / clip.w) * .5f * view.size.y);
            WarpPoint({float(x), float(y)});
            navigationFocus(true);
        } else if (autotestFrame == 3)
            mouse(true);
        else if (autotestFrame == 4)
            terrainCheck(app.m_terrain->draft.Active() &&
                             app.m_terrain->draft.source.heights != terrainInitial,
                         "native mouse press sculpts current viewport surface");
        else if (autotestFrame == 5)
            mouse(false);
        else if (autotestFrame == 6) {
            terrainEdited = app.m_terrain->draft.source.heights;
            terrainCheck(!app.m_terrain->draft.Active(), "release commits one gesture");
            terrainCheck(app.m_terrain->draft.Undo() && app.m_terrain->draft.source.heights == terrainInitial,
                         "one undo restores source");
        } else if (autotestFrame == 7)
            terrainCheck(app.m_terrain->draft.Redo() && app.m_terrain->draft.source.heights == terrainEdited,
                         "redo restores authored sculpt");
        else if (autotestFrame == 8)
            mouse(true);
        else if (autotestFrame == 9)
            io.AddKeyEvent(ImGuiKey_Escape, true);
        else if (autotestFrame == 10) {
            io.AddKeyEvent(ImGuiKey_Escape, false);
            mouse(false);
            terrainCheck(!app.m_terrain->draft.Active() &&
                             app.m_terrain->draft.source.heights == terrainEdited,
                         "Escape cancels real viewport stroke");
            app.m_terrain->editing = true;
        } else if (autotestFrame == 11)
            mouse(true);
        else if (autotestFrame == 12)
            navigationFocus(false);
        else if (autotestFrame == 13) {
            terrainCheck(!app.m_terrain->draft.Active(), "focus loss safely commits and ends gesture");
            mouse(false);
            navigationFocus(true);
            app.m_terrain->brush.kind = TerrainBrushKind::Paint;
            app.m_terrain->brush.layer = 1;
        } else if (autotestFrame == 14)
            mouse(true);
        else if (autotestFrame == 15)
            mouse(false);
        else if (autotestFrame == 16) {
            std::string error;
            terrainCheck(app.m_terrain->draft.Save(error), "save actual editable sculpt and paint");
            TerrainCookResult result;
            terrainCheck(CookTerrain(app.m_terrain->draft.source, app.m_terrain->draft.Path(),
                                     app.m_project.RootDir(), host.Assets(), result, error),
                         "same CPU cook publishes runtime geometry and appearance");
            terrainPainted = app.m_terrain->draft.source.weights;
            deferredRequests.rescanAssets = true;
            app.m_terrain->editing = false;
            app.m_panels.showTerrain = true;
            MeshData gpu;
            terrainCheck(renderer.ReadMeshForDiagnostics(app.m_terrain->mesh, gpu) &&
                             gpu.vertices.size() == app.m_terrain->preview->vertices.size() &&
                             glm::length(gpu.vertices[gpu.vertices.size() / 2].position -
                                         app.m_terrain->preview->vertices[gpu.vertices.size() / 2].position) <
                                 1e-6f,
                         "actual GL mesh matches current draft geometry");
        } else if (autotestFrame == 18 && std::getenv("JUDAS_EDITOR_AUTOTEST_MOD_PACKAGE")) {
            std::vector<SceneObjectId> surfaces;
            for (const auto &object : app.m_document.GetScene().Objects())
                if (object.navigationSurface)
                    surfaces.push_back(object.id);
            for (auto id : surfaces)
                terrainCheck(BakeEditorNavigation(app.m_document, id, app.m_panels),
                             "editor rebakes navigation against newly cooked owned geometry");
            std::string error;
            terrainCheck(app.m_document.Save(error), "save rebaked ordinary mod scene");
            SaveSceneToString(app.m_document.GetScene(), autotestBaseline);
        } else if (autotestFrame == 21)
            terrainCheck(app.m_panels.mode == EditorMode::Play, "saved/cooked terrain enters normal Play");
        else if (autotestFrame == 150) {
            terrainCheck(app.m_panels.mode == EditorMode::Edit &&
                             app.m_terrain->draft.source.weights == terrainPainted &&
                             !app.m_terrain->draft.Dirty(),
                         "Stop preserves saved editable paint state");
            if (const char *output = std::getenv("JUDAS_EDITOR_AUTOTEST_MOD_PACKAGE")) {
                std::string error;
                AuthoringDocument manifest;
                terrainCheck(manifest.Load("modmanifest", app.m_project.Resolve("mod.json"), error),
                             "editor opens saved mod manifest");
                auto json = AuthoringJSON::parse(manifest.Source());
                json["name"] = "Editor packaged map";
                auto previous = manifest.Source();
                terrainCheck(manifest.Replace(json.dump(2), error) && manifest.Undo() &&
                                 manifest.Source() == previous && manifest.Redo(),
                             "manifest draft Undo/Redo");
                terrainCheck(manifest.Save(error), "explicit editor manifest save");
                JudasMods::Manifest draft;
                JudasMods::Package package;
                terrainCheck(
                    JudasMods::ParseManifest(manifest.Source(), draft, error, true) &&
                        JudasMods::AuthorPackage(app.m_project, draft, package, error) &&
                        JudasMods::WritePackage(output, package, error),
                    "editor shared packaging after actual viewport sculpt/paint/cook/bake/Play-Stop");
                if (!error.empty())
                    std::fprintf(stderr, "[M76 editor package] %s\n", error.c_str());
            }
            std::fprintf(stderr, "[M75 terrain editor] failures %d\n", terrainFailures);
        }
    }
}
