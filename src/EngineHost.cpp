#include "PerformanceProfiler.h"
#include "EngineHost.h"

#include <SDL2/SDL.h>
#include "GraphicsApi.h"

#include "EnginePaths.h"

namespace {
#ifndef __ANDROID__
GLADapiproc LoadOpenGLProcAddress(const char* name) {
    return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
}

#endif
// Default packaged DejaVu font serves compatibility HUD/menu calls. M58
// shapes/rasterizes on demand through the same path as authored runtime UI.
const char* const kUIFontPath = "assets/fonts/DejaVuSans.ttf";
constexpr float kUIFontPixelHeight = 48.0f;
}  // namespace

EngineHost::~EngineHost() {
    Shutdown();
}

bool EngineHost::Init(const char* title, int width, int height, bool visible, std::string& outError,
                      ResourceTrace trace) {
    JUDAS_PROFILE_SCOPE("Host startup");
    m_resourceTrace = std::move(trace);
    m_renderer.SetResourceTrace(m_resourceTrace);
    if (!m_window.Init(title, width, height, visible)) {
        outError = "Window initialization failed.";
        return false;
    }
#ifdef __ANDROID__
    if (!glGetString(GL_VERSION)) { outError = "OpenGL ES 3 context unavailable"; return false; }
#else
    if (gladLoadGL(&LoadOpenGLProcAddress) == 0 || !GLAD_GL_VERSION_3_3) {
        outError = "Failed to load the required OpenGL 3.3 Core entry points.";
        return false;
    }
#endif
    if (!m_renderer.Init()) {
        outError = "Renderer initialization failed.";
        return false;
    }
    m_rendererInitialized = true;
    const std::string fontPath = ResolveEngineDataPath(kUIFontPath);
    if (!m_renderer.LoadFont(fontPath.c_str(), kUIFontPixelHeight, outError)) {
        outError += " (engine font " + fontPath + "; set JUDAS_ENGINE_ROOT to the repository root)";
        return false;
    }
    m_jobs = std::make_unique<JobSystem>();  // worker count derived from the hardware
    m_audio.SetJobSystem(m_jobs.get());
    m_resources = std::make_unique<ResourceManager>(&m_renderer, &m_assetDatabase, m_jobs.get(), &m_audio);
    m_resources->SetTrace(m_resourceTrace);
    return true;
}

void EngineHost::PumpResources() {
    JUDAS_PROFILE_SCOPE("Resource pump");
    if (m_resources) m_resources->Pump();
    m_audio.Update();
}

void EngineHost::OpenProjectAssets(const std::string& projectRoot, const std::string& assetsDir) {
    JUDAS_PROFILE_SCOPE("Project resources");
    m_audio.StopAll();
    m_renderer.ResetProjectText();
    if (m_resources) m_resources->ReleaseAll();
    m_assetDatabase.Scan(projectRoot, assetsDir);
}

void EngineHost::Shutdown() {
    ProfileFrame profileShutdown("host shutdown");
    JUDAS_PROFILE_SCOPE("Host shutdown");
    // Reverse of Init: resources (GPU objects, on this thread, while the
    // context exists) -> workers -> renderer -> window.
    m_audio.StopAll();
    if (m_resources) m_resources->Shutdown();
    m_resources.reset();
    m_audio.Shutdown();
    m_audio.SetJobSystem(nullptr);
    if (m_jobs) {
        m_jobs->Shutdown();
        if (m_resourceTrace) {
            ResourceTraceEvent event{ResourceTracePoint::WorkersJoined, {}, {}};
            event.contextCurrent = SDL_GL_GetCurrentContext() != nullptr;
            m_resourceTrace(event);
        }
    }
    m_jobs.reset();
    if (m_rendererInitialized) {
        m_renderer.Shutdown();
        m_rendererInitialized = false;
    }
    const bool hadContext = m_window.NativeGLContext() != nullptr;
    m_window.Shutdown();
    if (hadContext && m_resourceTrace) {
        ResourceTraceEvent event{ResourceTracePoint::ContextDestroyed, {}, {}};
        event.contextCurrent = SDL_GL_GetCurrentContext() != nullptr;
        m_resourceTrace(event);
    }
}

void EngineHost::OpenContentView(const AssetDatabase& assets){
    m_audio.StopAll();m_renderer.ResetProjectText();if(m_resources)m_resources->ReleaseAll();m_assetDatabase=assets;
}
