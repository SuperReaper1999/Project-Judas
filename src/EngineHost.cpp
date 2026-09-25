#include "EngineHost.h"

#include <SDL2/SDL.h>
#include <glad/gl.h>

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
    if (!m_renderer.LoadFont(kUIFontPath, kUIFontPixelHeight, outError)) return false;
    m_assets = std::make_unique<RenderAssetCache>(&m_renderer);
    return true;
}

void EngineHost::Shutdown() {
    m_assets.reset();
    if (m_rendererInitialized) {
        m_renderer.Shutdown();
        m_rendererInitialized = false;
    }
    m_window.Shutdown();
}
