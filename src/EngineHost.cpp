#include "EngineHost.h"

#include <SDL2/SDL.h>
#include <glad/gl.h>

#include "EnginePaths.h"

namespace {
GLADapiproc LoadOpenGLProcAddress(const char* name) {
    return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
}

// Milestone 13: DejaVu Sans baked once at 48px serves both HUD and menu
// text from a single atlas (see docs/ARCHITECTURE.md, "Milestone 13").
const char* const kUIFontPath = "assets/fonts/DejaVuSans.ttf";
constexpr float kUIFontPixelHeight = 48.0f;
}  // namespace

EngineHost::~EngineHost() {
    Shutdown();
}

bool EngineHost::Init(const char* title, int width, int height, bool visible, std::string& outError) {
    if (!m_window.Init(title, width, height, visible)) {
        outError = "Window initialization failed.";
        return false;
    }
    if (gladLoadGL(&LoadOpenGLProcAddress) == 0 || !GLAD_GL_VERSION_3_3) {
        outError = "Failed to load the required OpenGL 3.3 Core entry points.";
        return false;
    }
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
    m_resources = std::make_unique<ResourceManager>(&m_renderer, &m_assetDatabase, m_jobs.get());
    return true;
}

void EngineHost::PumpResources() {
    if (m_resources) m_resources->Pump();
}

void EngineHost::OpenProjectAssets(const std::string& projectRoot, const std::string& assetsDir) {
    if (m_resources) m_resources->ReleaseAll();
    m_assetDatabase.Scan(projectRoot, assetsDir);
}

void EngineHost::Shutdown() {
    // Reverse of Init: resources (GPU objects, on this thread, while the
    // context exists) -> workers -> renderer -> window.
    if (m_resources) m_resources->Shutdown();
    m_resources.reset();
    if (m_jobs) m_jobs->Shutdown();
    m_jobs.reset();
    if (m_rendererInitialized) {
        m_renderer.Shutdown();
        m_rendererInitialized = false;
    }
    m_window.Shutdown();
}
