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
void EditorAutomation::RunWorkspaceChecks() {
    auto &io = ImGui::GetIO();
    const std::string prefix = autotest;
    if (navigationAutotest && autotestFrame < 10) {
        const auto &v = app.m_shell.views.Scene();
        std::fprintf(
            stderr,
            "[editor navigation input] frame=%d mouse=%.1f,%.1f rect=%.1f,%.1f %.1f,%.1f hover=%d focus=%d middle=%d gesture=%d captured=%d text=%d\n",
            autotestFrame, io.MousePos.x, io.MousePos.y, v.bounds.position.x, v.bounds.position.y,
            v.bounds.size.x, v.bounds.size.y, v.hovered, v.focused, io.MouseDown[ImGuiMouseButton_Middle],
            int(app.m_sceneViewport.gesture.Active()), window.IsMouseCaptured(), io.WantTextInput);
    }
    if (pickingAutotest) {
        // A private authored fixture supplies two tiny children under
        // a rotated/scaled parent. Events use the real SDL/ImGui path.
        const auto pointer = [&](glm::vec2 pixel, int buttonState = -1) {
            WarpPoint(pixel);
            const auto native = NativePoint(pixel);
            SDL_Event event{};
            event.type = SDL_MOUSEMOTION;
            event.motion.windowID = SDL_GetWindowID(SceneWindow());
            event.motion.x = native.position.x;
            event.motion.y = native.position.y;
            SDL_PushEvent(&event);
            if (buttonState >= 0) {
                event = SDL_Event{};
                event.type = buttonState ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                event.button.windowID = SDL_GetWindowID(SceneWindow());
                event.button.button = SDL_BUTTON_LEFT;
                event.button.state = buttonState ? SDL_PRESSED : SDL_RELEASED;
                event.button.x = native.position.x;
                event.button.y = native.position.y;
                SDL_PushEvent(&event);
            }
        };
        const auto wheel = [&](float steps) {
            SDL_Event event{};
            event.type = SDL_MOUSEWHEEL;
            event.wheel.windowID = SDL_GetWindowID(SceneWindow());
            event.wheel.y = static_cast<int>(steps);
#if SDL_VERSION_ATLEAST(2, 0, 18)
            event.wheel.preciseY = steps;
#endif
            SDL_PushEvent(&event);
        };
        const auto childPixel = [&](SceneObjectId id) {
            Scene flat;
            std::string error;
            if (!FlattenHierarchy(app.m_document.GetScene(), flat, error) || !flat.Find(id))
                return glm::vec2(-1);
            const auto &view = app.m_panels.workspace.viewport;
            const auto clip = app.m_sceneViewport.camera.ProjectionMatrix(view.size.x / view.size.y) *
                              app.m_sceneViewport.camera.ViewMatrix() *
                              glm::vec4(flat.Find(id)->transform.position, 1);
            return glm::vec2(view.position.x + (clip.x / clip.w + 1) * .5f * view.size.x,
                             view.position.y + (1 - clip.y / clip.w) * .5f * view.size.y);
        };
        if (autotestFrame == 1) {
            navigationFocus(true);
            navigationMotion(0, 0);
            dollyStartPosition = app.m_sceneViewport.camera.Position();
            dollyStartDistance = app.m_sceneViewport.camera.FocusDistance();
            dollyAnchor = dollyStartPosition + app.m_sceneViewport.camera.Forward() * dollyStartDistance;
        } else if (autotestFrame == 2)
            wheel(1);
        else if (autotestFrame == 3) {
            navigationCheck(std::abs(app.m_sceneViewport.camera.FocusDistance() - dollyStartDistance * .8f) <
                                    1e-4f &&
                                glm::length(app.m_sceneViewport.camera.Position() +
                                            app.m_sceneViewport.camera.Forward() *
                                                app.m_sceneViewport.camera.FocusDistance() -
                                            dollyAnchor) < 1e-4f,
                            "SDL wheel zooms toward fixed focus in central viewport");
            wheel(-1);
        } else if (autotestFrame == 4) {
            navigationCheck(glm::length(app.m_sceneViewport.camera.Position() - dollyStartPosition) < 1e-4f,
                            "SDL inverse wheel restores camera position");
            pointer({30, 150});
        } else if (autotestFrame == 5)
            wheel(2);
        else if (autotestFrame == 6) {
            navigationCheck(glm::length(app.m_sceneViewport.camera.Position() - dollyStartPosition) < 1e-4f,
                            "SDL wheel over hierarchy does not move scene camera");
            app.m_document.Select(kInvalidSceneObjectId);
            pointer(childPixel(9001), 1);
        } else if (autotestFrame == 7)
            pointer(childPixel(9001), 0);
        else if (autotestFrame == 8) {
            navigationCheck(app.m_document.Selected() == 9001,
                            "SDL viewport selects tiny nested box above large floor");
            screenshot(prefix + ".nested.png");
            app.m_document.Select(kInvalidSceneObjectId);
            pointer(childPixel(9002));
        } else if (autotestFrame == 9)
            pointer(childPixel(9002), 1);
        else if (autotestFrame == 10)
            pointer(childPixel(9002), 0);
        else if (autotestFrame == 11) {
            navigationCheck(app.m_document.Selected() == 9002,
                            "SDL viewport selects second child independently of parent proxy");
            std::string actual;
            SaveSceneToString(app.m_document.GetScene(), actual);
            navigationCheck(actual == autotestBaseline && !app.m_document.EditInProgress(),
                            "wheel and selection preserve authored scene and Undo state");
        } else if (autotestFrame == 140)
            navigationCheck(app.m_panels.mode == EditorMode::Play && app.m_playSession.world &&
                                app.m_playSession.world->CountLifecycle().physicsBodies >= 14,
                            "ordinary Play instantiates the real authored lab bodies");
        else if (autotestFrame == 150)
            navigationCheck(app.m_panels.mode == EditorMode::Edit && !window.IsMouseCaptured(),
                            "Play/Stop restores uncaptured editor after wheel and picking");
    }
    if (polishAutotest) {
        // This counter has already advanced after rendering. Queue
        // Escape for the frame after the dialog's pending check.
        if (autotestFrame == 13)
            screenshot(prefix + ".unsaved.png");
        if (autotestFrame == 14 || autotestFrame == 145)
            io.AddKeyEvent(ImGuiKey_Escape, true);
        if (autotestFrame == 15 || autotestFrame == 146)
            io.AddKeyEvent(ImGuiKey_Escape, false);
        if (autotestFrame == 13)
            SDL_SetWindowSize(window.NativeWindow(), 960, 640);
        if (autotestFrame == 15)
            screenshot(prefix + ".resized.png");
        if (autotestFrame == 16) {
            SDL_SetWindowSize(window.NativeWindow(), initialWindowWidth, initialWindowHeight);
        }
        if (autotestFrame == 143) {
            SDL_Event event{};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        }
        if (autotestFrame == 146)
            screenshot(prefix + ".restored.png");
    }
    // M71 input evidence goes through SDL -> ImGui -> FrameEditMode,
    // using the ordinary scene camera and unchanged authored scene.
    if (navigationAutotest) {
        if (autotestFrame == 1) {
            navigationFocus(true);
            navigationMotion(0, 0);
        } else if (autotestFrame == 2) {
            navigationStartPosition = app.m_sceneViewport.camera.Position();
            navigationStartFocus = app.m_panels.cameraFocus;
            navigationStartForward = app.m_sceneViewport.camera.Forward();
            const float scale = 2.0f * app.m_sceneViewport.camera.FocusDistance() *
                                std::tan(glm::radians(30.0f)) / app.m_panels.workspace.viewport.size.y;
            navigationExpectedPosition =
                navigationStartPosition + scale * (-app.m_sceneViewport.camera.Right() * 80.0f -
                                                   app.m_sceneViewport.camera.Up() * 40.0f);
            navigationButton(true);
        } else if (autotestFrame == 3) {
            navigationCheck(app.m_sceneViewport.gesture.Active() == EditorCameraGesture::Pan &&
                                window.IsMouseCaptured(),
                            "SDL middle press captures only viewport camera");
            navigationMotion(80, -40);
        } else if (autotestFrame == 4) {
            navigationCheck(
                glm::length(app.m_sceneViewport.camera.Position() - navigationExpectedPosition) < 2e-4f &&
                    glm::length(app.m_panels.cameraFocus - navigationStartFocus -
                                (app.m_sceneViewport.camera.Position() - navigationStartPosition)) < 2e-4f &&
                    glm::length(app.m_sceneViewport.camera.Forward() - navigationStartForward) < 1e-6f,
                "SDL drag translates camera and placement focus equally in view plane");
            std::string afterPan;
            SaveSceneToString(app.m_document.GetScene(), afterPan);
            navigationCheck(afterPan == autotestBaseline && !app.m_document.EditInProgress() &&
                                !app.m_sceneViewport.drag.Active(),
                            "pan preserves authored objects and gizmo undo state");
            navigationButton(false);
        } else if (autotestFrame == 5) {
            navigationCheck(app.m_sceneViewport.gesture.Active() == EditorCameraGesture::None &&
                                !window.IsMouseCaptured(),
                            "SDL middle release frees capture");
        } else if (autotestFrame == 6) {
            navigationButton(true);
        } else if (autotestFrame == 7) {
            navigationCheck(app.m_sceneViewport.gesture.Active() == EditorCameraGesture::Pan,
                            "fresh middle press restarts pan");
            navigationFocus(false);
        } else if (autotestFrame == 8) {
            navigationCheck(app.m_sceneViewport.gesture.Active() == EditorCameraGesture::None &&
                                !window.IsMouseCaptured(),
                            "SDL window focus loss cancels and releases capture");
            navigationFocus(true);
        } else if (autotestFrame == 9) {
            navigationCheck(app.m_sceneViewport.gesture.Active() == EditorCameraGesture::None &&
                                !window.IsMouseCaptured(),
                            "SDL focus regain cannot resume the old drag");
            navigationButton(false);
        } else if (autotestFrame == 17) {
            navigationMotion(0, 0);
        } else if (autotestFrame == 18) {
            navigationButton(true);
        } else if (autotestFrame == 19) {
            navigationCheck(app.m_sceneViewport.gesture.Active() == EditorCameraGesture::Pan,
                            "pan active before ordinary Play request");
        } else if (autotestFrame == 21) {
            navigationCheck(app.m_panels.mode == EditorMode::Play &&
                                app.m_sceneViewport.gesture.Active() == EditorCameraGesture::None,
                            "ordinary Play retires editor pan ownership");
            navigationButton(false);
        } else if (autotestFrame == 150) {
            navigationCheck(app.m_panels.mode == EditorMode::Edit && !window.IsMouseCaptured() &&
                                app.m_sceneViewport.gesture.Active() == EditorCameraGesture::None,
                            "ordinary Stop returns an uncaptured scene camera");
        }
    }
}
