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

void EditorShell::RegisterPanels(EditorSceneViewport& scene,EditorPlaySession& session,EditorToolServices tools){
    panels.Add({"scene-view","Scene","Scene",kEditorAllPanelPhases,&EditorPanelState::showSceneView},[this,&scene](auto& c){views.DrawScene(c,scene);});
    panels.Add({"game-view","Game","Scene",kEditorAllPanelPhases,&EditorPanelState::showGameView},[this,&session](auto& c){views.DrawGame(c,session);});
    RegisterEditorCorePanels(panels);
    RegisterEditorToolPanels(panels,tools);
}
void EditorShell::Draw(EditorContext& c,EditorPreferences& preferences){
    views.BeginFrame();c.panels.sceneViewHovered=false;
    const bool stopped=c.phase==EditorPanelPhase::DebugStopped;
    ImGui::BeginDisabled(stopped);
    DrawEditorMainMenu(c.document,c.panels,c.requests,[&]{panels.DrawWindowMenu(c);});
    ImGui::EndDisabled();
    workspace.BeginFrame(c.panels,preferences);
    ImGui::BeginDisabled(stopped);
    DrawEditorWorkspaceChrome(c.document,c.panels,c.requests,[&]{DrawEditorWorkspaceSelector(preferences,c.panels);});
    ImGui::EndDisabled();
    panels.DrawAll(c);
    DrawStatusBar(c.document,c.panels);
}
void EditorShell::DrawStopped(EditorContext& context,EditorPreferences& preferences){
    auto stopped=context;stopped.phase=EditorPanelPhase::DebugStopped;
    Draw(stopped,preferences);
}
void EditorShell::Present(EngineHost& host){
    auto& window=host.GetWindow();
    SDL_GL_MakeCurrent(window.NativeWindow(),window.NativeGLContext());
    int width=1,height=1;SDL_GL_GetDrawableSize(window.NativeWindow(),&width,&height);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);
    const auto background=ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    glClearColor(background.x,background.y,background.z,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable){
        SDL_Window* backupWindow=SDL_GL_GetCurrentWindow();SDL_GLContext backupContext=SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupWindow,backupContext);
    }
}
