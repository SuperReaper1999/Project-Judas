#pragma once
#include "EditorPanelRegistry.h"
#include "render/RenderTypes.h"
#include "EditorViewportPresenter.h"
#include "EditorWorkspace.h"
struct EditorContext;
struct EditorSceneViewport;
struct EditorPlaySession;
struct EditorToolServices;
class EditorPreferences;
class EngineHost;
class Renderer;

// Dock/UI orchestration only. It never updates a world or evaluates scripts.
class EditorShell {
  public:
    void RegisterPanels(EditorSceneViewport &, EditorPlaySession &, EditorToolServices);
    void Draw(EditorContext &, EditorPreferences &);
    void DrawStopped(EditorContext &, EditorPreferences &);
    void Present(EngineHost &);
    void Destroy(Renderer &);
    EditorViewportPresenter views;

  private:
    TextureHandle brandTexture;
    bool brandAttempted = false;
    EditorPanelRegistry panels;
    EditorWorkspace workspace;
};
