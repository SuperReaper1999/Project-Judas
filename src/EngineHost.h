#pragma once

#include <memory>
#include <string>

#include "AssetDatabase.h"
#include "JobSystem.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Window.h"

// Milestone 28: the engine's process-level services — the OS window and
// GL context, the OpenGL 3.3 Core loader, the Renderer and its UI font,
// and (Milestone 30) the project's AssetDatabase with the ResourceManager
// that turns its assets into GPU resources, and (Milestone 31) the
// JobSystem those loads run on — brought up in one order and torn down in
// the reverse:
//
//   up:   window/GL context -> GLAD -> Renderer (+font) -> JobSystem -> ResourceManager
//   down: ResourceManager::Shutdown (cancel, wait for its jobs, destroy GPU
//         objects on this thread) -> JobSystem::Shutdown (cancel queued,
//         join workers) -> Renderer::Shutdown -> window/context
//
// so no worker outlives the manager whose tasks it runs and no GL call
// happens after the context is gone. The runtime (`judas`) and the editor
// (`judas_editor`) both start here; neither reimplements startup. Nothing
// about scenes, players or demos lives at this level.
//
// The UI font is ENGINE data (assets/fonts/DejaVuSans.ttf beside the
// engine, resolved through src/EnginePaths.h), not project content: a
// project does not have to ship a font to draw the HUD.
class EngineHost {
public:
    EngineHost() = default;
    ~EngineHost();
    EngineHost(const EngineHost&) = delete;
    EngineHost& operator=(const EngineHost&) = delete;

    bool Init(const char* title, int width, int height, bool visible, std::string& outError);
    void Shutdown();

    // Points the asset database at a project's assets directory (rescans)
    // and releases every resource loaded from the previous one (in-flight
    // loads are cancelled and their completions discarded).
    void OpenProjectAssets(const std::string& projectRoot, const std::string& assetsDir);
    // Once per frame on this thread: installs finished loads (GPU upload)
    // and enforces the resource budget.
    void PumpResources();

    Window& GetWindow() { return m_window; }
    Renderer& GetRenderer() { return m_renderer; }
    AssetDatabase& Assets() { return m_assetDatabase; }
    const AssetDatabase& Assets() const { return m_assetDatabase; }
    ResourceManager& Resources() { return *m_resources; }
    JobSystem& Jobs() { return *m_jobs; }

private:
    Window m_window;
    Renderer m_renderer;
    AssetDatabase m_assetDatabase;
    std::unique_ptr<JobSystem> m_jobs;
    std::unique_ptr<ResourceManager> m_resources;
    bool m_rendererInitialized = false;
};
