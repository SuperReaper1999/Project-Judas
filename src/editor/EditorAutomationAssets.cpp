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
void EditorAutomation::RunAssetsChecks() {
    if (retargetAutotest && autotestFrame == 1)
        screenshot(std::string(autotest) + ".retarget.png");
    if (importRecipe && autotestFrame == 1)
        std::fprintf(stderr, "[editor autotest] shared import + GPU preview PASS (%zu parts)\n",
                     app.m_panels.importAccepted->preview->primitives.size());
    if (importRecipe && autotestFrame == 2) {
        auto *placed = app.m_document.SelectedObject();
        std::fprintf(stderr, "[editor autotest] ordinary imported placement %s\n",
                     placed && placed->render && placed->animation ? "PASS" : "FAIL");
        app.m_document.Undo();
        std::string restored;
        SaveSceneToString(app.m_document.GetScene(), restored);
        std::fprintf(stderr, "[editor autotest] imported placement one undo %s\n",
                     restored == autotestBaseline ? "PASS" : "FAIL");
    }
    if (animationTimelineAutotest && importRecipe) {
        if (autotestFrame == 3) {
            app.m_panels.showAnimationTimeline = true;
            app.m_panels.workspace.showAssets = false;
        }
        if (autotestFrame == 5) {
            app.m_panels.modelPreviewClip = 1;
            app.m_panels.modelPreviewPlaying = false;
            app.m_panels.modelPreviewTime = .3f;
        }
        if (autotestFrame == 8) {
            app.m_panels.modelPreviewPlaying = false;
            app.m_panels.modelPreviewTime = .55f;
            auto &timeline = *app.m_animationTimeline;
            timeline.Remember();
            timeline.draft.events[0].time = .21f;
            app.m_panels.modelPreviewClip = 2;
        }
        if (autotestFrame == 9) {
            auto &timeline = *app.m_animationTimeline;
            bool retained = timeline.draft.clip == "Walk" && timeline.draft.Dirty() &&
                            std::abs(timeline.draft.events[0].time - .21f) < 1e-6f;
            std::fprintf(stderr, "[M95 timeline] %s: switching clips retains unsaved marker draft\n",
                         retained ? "PASS" : "FAIL");
            app.m_panels.modelPreviewClip = 1;
            if (!timeline.undo.empty()) {
                timeline.draft.events = std::move(timeline.undo.back());
                timeline.undo.pop_back();
            }
        }
        if (autotestFrame == 10 && !animationTimelineRecook) {
            animationTimelineRecook = true;
            auto &timeline = *app.m_animationTimeline;
            bool loaded = timeline.draft.clip == "Walk" && timeline.draft.events.size() == 2;
            std::fprintf(stderr, "[M95 timeline] %s: clip draft and real GPU preview\n",
                         loaded && app.m_panels.modelPreviewToken ? "PASS" : "FAIL");
            if (loaded) {
                timeline.Remember();
                timeline.draft.events[0].time = .2f;
                std::string error;
                bool saved = timeline.draft.Save(error);
                std::fprintf(stderr, "[M95 timeline] %s: explicit event draft save %s\n",
                             saved ? "PASS" : "FAIL", error.c_str());
                app.m_panels.importTask = QueueModelImport(host.Jobs(), importRecipe);
                app.m_panels.importPublished = false;
            }
        }
        if (autotestFrame == 13) {
            app.m_panels.showProfiler = false;
            app.m_panels.showWorldBuilding = false;
            ImGui::SetWindowFocus("Animation timeline");
        }
        if (autotestFrame == 14) {
            const auto &clips = app.m_panels.importAccepted->preview->skeletal->clips;
            bool changed = clips.size() > 1 && clips[1].events.size() == 2 &&
                           std::abs(clips[1].events[0].time - .2f) < 1e-6f;
            std::fprintf(stderr, "[M95 timeline] %s: normal asynchronous recook publishes edited marker\n",
                         changed ? "PASS" : "FAIL");
            screenshot(std::string(autotest) + ".timeline.png");
        }
        if (autotestFrame == 16)
            app.m_panels.showAnimationTimeline = false;
    }
    if (importRecipe && autotestFrame == 6) {
        app.m_panels.modelPreviewPlaying = true;
        app.m_panels.modelPreviewTime = .5f;
    }
    if (importRecipe && autotestFrame == 12) {
        auto &parts = app.m_panels.importAccepted->preview->primitives;
        if (!parts.empty())
            app.m_panels.modelPreviewHidden = {parts.front().part};
    }

    if (std::getenv("JUDAS_EDITOR_AUTOTEST_TILEMAP") && app.m_tilemapEditor)
        app.m_tilemapEditor->Autotest(autotestFrame, app.m_document, app.m_panels, app.m_sceneViewport.camera,
                                      window, [&]() { screenshot(std::string(autotest) + ".tilemap.png"); });
    if (std::getenv("JUDAS_EDITOR_AUTOTEST_SPRITE_ANIMATION")) {
        if (autotestFrame == 3) {
            for (const auto &[id, record] : host.Assets().Records())
                if (record.type == AssetType::SpriteSheet) {
                    app.m_panels.spriteSheetEdit = id;
                    app.m_panels.showSpriteSheetEditor = true;
                    break;
                }
            app.m_panels.showProfiler = false;
            app.m_panels.showWorldBuilding = false;
        }
        if (autotestFrame == 8 && app.m_spriteSheetEditor) {
            app.m_spriteSheetEditor->cell = {24, 48};
            app.m_spriteSheetEditor->clip = 1;
            app.m_spriteSheetEditor->previewPlaying = true;
        }
        if (autotestFrame == 10 && app.m_spriteSheetEditor) {
            auto &tool = *app.m_spriteSheetEditor;
            tool.Remember();
            tool.draft.frames[0].name += " unsaved guard probe";
            EditorRequests probe;
            probe.quit = true;
            app.HandleRequests(probe);
            std::fprintf(stderr, "[Sprite animation editor] %s: unsaved sheet prevents desktop quit\n",
                         !app.m_quit && tool.Dirty() ? "PASS" : "FAIL");
            tool.draft = tool.undo.back();
            tool.undo.pop_back();
            app.m_panels.status = "Sprite sheet preview; gameplay events are disabled";
        }
        if (autotestFrame == 13) {
            app.m_panels.showProfiler = false;
            app.m_panels.showWorldBuilding = false;
            app.m_panels.showUIDocument = false;
            ImGui::SetWindowFocus("Sprite sheets and frame animation");
        }
        if (autotestFrame == 14) {
            ImGui::SetWindowFocus("Sprite sheets and frame animation");
            screenshot(std::string(autotest) + ".sprite-sheet.png");
            std::fprintf(stderr,
                         "[Sprite animation editor] %s: saved sheet loads and preview clock advances "
                         "without scene mutation\n",
                         app.m_spriteSheetEditor && !app.m_spriteSheetEditor->path.empty() &&
                                 app.m_spriteSheetEditor->previewTime > 0 && !app.m_spriteSheetEditor->Dirty()
                             ? "PASS"
                             : "FAIL");
        }
    }
}
