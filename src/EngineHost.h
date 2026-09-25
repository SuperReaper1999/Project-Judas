#pragma once

#include <string>

#include "RenderAssetCache.h"
#include "Renderer.h"
#include "Window.h"

// Milestone 28: the engine's process-level services — the OS window and
// GL context, the OpenGL 3.3 Core loader, the Renderer and its UI font,
// and the render asset cache — brought up in one order and torn down in
// the reverse. The runtime (`judas`) and the editor (`judas_editor`) both
// start here; neither reimplements startup. Nothing about scenes, players
// or demos lives at this level.
class EngineHost {
public:
    EngineHost() = default;
    ~EngineHost();
    EngineHost(const EngineHost&) = delete;
    EngineHost& operator=(const EngineHost&) = delete;

    bool Init(const char* title, int width, int height, bool visible, std::string& outError);
    void Shutdown();

    Window& GetWindow() { return m_window; }
    Renderer& GetRenderer() { return m_renderer; }
    RenderAssetCache& Assets() { return *m_assets; }

private:
    Window m_window;
    Renderer m_renderer;
    std::unique_ptr<RenderAssetCache> m_assets;
    bool m_rendererInitialized = false;
};
