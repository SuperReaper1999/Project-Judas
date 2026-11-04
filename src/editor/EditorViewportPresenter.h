#pragma once
#include "EditorWorkspaceLayout.h"
#include "ResourceManager.h"
#include <SDL2/SDL.h>
struct EditorContext;
struct EditorSceneViewport;
struct EditorPlaySession;
class Window;
class Renderer;

struct EditorViewSurface {
    EditorWorkspaceRect bounds;
    glm::vec2 nativeOrigin{0};
    Uint32 windowId=0;
    bool visible=false,hovered=false,focused=false;
};
// Presentation owns images/input bounds, never a simulation or script runtime.
class EditorViewportPresenter {
public:
    void BeginFrame();
    void DrawScene(EditorContext&,EditorSceneViewport&);
    void DrawGame(EditorContext&,EditorPlaySession&);
    void ConfigureInput(Window&,bool playing);
    void RequestSceneFocus(){focusScene=true;}
    void RequestGameFocus(){focusGame=true;}
    bool BeginGameTarget(Renderer&,bool renderHidden=false);
    void EndGameTarget(Renderer&);
    void Destroy(Renderer&);
    const RenderTargetHandle& GameTarget() const { return gameTarget; }
    const EditorViewSurface& Scene()const{return scene;}
    const EditorViewSurface& Game()const{return game;}
private:
    EditorViewSurface scene,game;
    RenderTargetHandle gameTarget;
    bool focusScene=false,focusGame=false;
};
