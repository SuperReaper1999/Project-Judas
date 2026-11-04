#pragma once
#include "EditorPanelRegistry.h"
#include "EditorViewportPresenter.h"
#include "EditorWorkspace.h"
struct EditorContext;
struct EditorSceneViewport;
struct EditorPlaySession;
struct EditorToolServices;
class EditorPreferences;
class EngineHost;

// Dock/UI orchestration only. It never updates a world or evaluates scripts.
class EditorShell {
  public:
    void RegisterPanels(EditorSceneViewport &, EditorPlaySession &, EditorToolServices);
    void Draw(EditorContext &, EditorPreferences &);
    void DrawStopped(EditorContext &, EditorPreferences &);
    void Present(EngineHost &);
    EditorViewportPresenter views;

  private:
    EditorPanelRegistry panels;
    EditorWorkspace workspace;
};
