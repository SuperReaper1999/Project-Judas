#include "EditorVisuals.h"
#include "EditorShell.h"
#include "EditorCorePanels.h"
#include "EditorToolPanels.h"
#include "EditorPanels.h"
#include "EditorPreferences.h"
#include "EditorSceneViewport.h"
#include "EditorPlaySession.h"
#include "EngineHost.h"
#include "Window.h"
#include "glad/gl.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <SDL2/SDL.h>

void EditorShell::RegisterPanels(EditorSceneViewport &scene, EditorPlaySession &session,
                                 EditorToolServices tools) {
    panels.Add({"scene-view", "Scene", "Scene", kEditorAllPanelPhases, &EditorPanelState::showSceneView},
               [this, &scene](auto &c) { views.DrawScene(c, scene); });
    panels.Add({"game-view", "Game", "Scene", kEditorAllPanelPhases, &EditorPanelState::showGameView},
               [this, &session](auto &c) { views.DrawGame(c, session); });
    RegisterEditorCorePanels(panels);
    RegisterEditorToolPanels(panels, tools);
}
void EditorShell::Draw(EditorContext &c, EditorPreferences &preferences) {
    views.BeginFrame();
    c.panels.sceneViewHovered = false;
    const bool stopped = c.phase == EditorPanelPhase::DebugStopped;
    ImGui::BeginDisabled(stopped);
    DrawEditorMainMenu(c.document, c.panels, c.requests, [&] { panels.DrawWindowMenu(c); });
    ImGui::EndDisabled();
    workspace.BeginFrame(c.panels, preferences);
    ImGui::BeginDisabled(stopped);
    DrawEditorWorkspaceChrome(
        c.document, c.panels, c.requests, [&] { DrawEditorWorkspaceSelector(preferences, c.panels); },
        [&] {
            using Icon = EditorVisuals::Icon;
            const float width = ImGui::GetIO().DisplaySize.x / ImGui::GetIO().FontGlobalScale;
            auto workflow = [&](const char *label, const char *preset, Icon icon) {
                const bool selected = preferences.WorkspaceName() == preset && !views.Game().focused;
                const bool authorOnly = std::string(preset) == "Audio" || std::string(preset) == "UI";
                const bool unavailable = authorOnly && c.phase != EditorPanelPhase::Edit;
                ImGui::BeginDisabled(unavailable);
                if (EditorVisuals::ActionButton(label, icon, selected, {0, 32.f * ImGui::GetIO().FontGlobalScale})) {
                    preferences.QueueWorkspace(preset);
                    if (std::string(preset) == "Scene")
                        views.RequestSceneFocus();
                }
                ImGui::EndDisabled();
                if (unavailable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Stop Play to use this authoring workspace.");
            };
            workflow("Scene", "Scene", Icon::Scene);
            ImGui::SameLine();
            if (EditorVisuals::ActionButton("Playtest", Icon::Play, views.Game().focused, {0, 32.f * ImGui::GetIO().FontGlobalScale})) {
                c.panels.showGameView = true;
                views.RequestGameFocus();
            }
            ImGui::SetItemTooltip(
                "View the actual game. Press Play to start or Stop to return to authoring.");
            if (width >= 720) {
                ImGui::SameLine();
                workflow(width < 1000 ? "Scripts" : "Scripting", "Scripting", Icon::Script);
                ImGui::SameLine();
                workflow(width < 1000 ? "Animate" : "Animation", "Animation", Icon::Animation);
                ImGui::SameLine();
                workflow("Audio", "Audio", Icon::Audio);
                ImGui::SameLine();
                workflow("UI", "UI", Icon::UI);
            } else {
                ImGui::SameLine();
                if (EditorVisuals::ActionButton("Workflows...", Icon::Settings))
                    ImGui::OpenPopup("editorWorkflows");
                if (ImGui::BeginPopup("editorWorkflows")) {
                    for (const auto &name : EditorPreferences::WorkspaceNames()) {
                        const bool unavailable =
                            c.phase != EditorPanelPhase::Edit && (name == "Audio" || name == "UI");
                        if (ImGui::MenuItem(name.c_str(), nullptr, false, !unavailable))
                            preferences.QueueWorkspace(name);
                        if (unavailable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Stop Play to use this authoring workspace.");
                    }
                    ImGui::EndPopup();
                }
            }
        });
    ImGui::EndDisabled();
    panels.DrawAll(c);
    DrawStatusBar(c.document, c.panels);
}
void EditorShell::DrawStopped(EditorContext &context, EditorPreferences &preferences) {
    auto stopped = context;
    stopped.phase = EditorPanelPhase::DebugStopped;
    Draw(stopped, preferences);
}
void EditorShell::Present(EngineHost &host) {
    auto &window = host.GetWindow();
    SDL_GL_MakeCurrent(window.NativeWindow(), window.NativeGLContext());
    int width = 1, height = 1;
    SDL_GL_GetDrawableSize(window.NativeWindow(), &width, &height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    const auto background = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    glClearColor(background.x, background.y, background.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window *backupWindow = SDL_GL_GetCurrentWindow();
        SDL_GLContext backupContext = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupWindow, backupContext);
    }
}
