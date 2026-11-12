#include "PerformanceProfiler.h"
#include "EngineHost.h"

#include <SDL2/SDL.h>
#include "GraphicsApi.h"

#include "EnginePaths.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
GLADapiproc LoadOpenGLProcAddress(const char* name) {
    return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
}
// The Vulkan backend supplies Judas-owned implementations of the same entry
// points; nothing is resolved from an OpenGL driver.
GLADapiproc LoadVulkanProcAddress(const char* name) {
    return reinterpret_cast<GLADapiproc>(JudasVulkan::ProcAddress(name));
}

// GLAD's required API is 3.3. Optional diagnostics resolve their own extension
// entry points instead of increasing the engine's graphics requirement.
constexpr GLenum kDebugOutput = 0x92E0;
constexpr GLenum kDebugOutputSynchronous = 0x8242;
constexpr GLenum kDebugTypeError = 0x824C;
constexpr GLenum kDebugTypePerformance = 0x8250;
constexpr GLenum kDebugSeverityHigh = 0x9146;
constexpr GLenum kDebugSeverityMedium = 0x9147;
constexpr GLenum kDebugSeverityNotification = 0x826B;
constexpr GLenum kResetNotificationStrategy = 0x8256;
constexpr unsigned kMaximumGpuMessages = 256;
std::atomic<unsigned> gpuMessageCount{0};
std::atomic<std::uintptr_t> nextGpuContextTag{1};

bool HasGlExtension(const char* name) {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint index = 0; index < count; ++index) {
        const auto* text = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, GLuint(index)));
        if (text && std::strcmp(text, name) == 0) return true;
    }
    return false;
}

void GLAD_API_PTR GpuDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity,
                                  GLsizei length, const GLchar* message, const void* contextTag) {
    if (severity == kDebugSeverityNotification || type == kDebugTypePerformance ||
        (severity != kDebugSeverityHigh && severity != kDebugSeverityMedium && type != kDebugTypeError))
        return;
    const unsigned count = gpuMessageCount.fetch_add(1, std::memory_order_relaxed);
    if (count > kMaximumGpuMessages) return;
    if (count == kMaximumGpuMessages) {
        std::fprintf(stderr, "[GPU] debug-message limit reached; further messages suppressed\n");
        return;
    }
    // Async driver callbacks must never touch the host, call GL, or retain a
    // pointer into a context's lifetime. The opaque tag is an integer only.
    std::fprintf(stderr, "[GPU] context=%zu source=0x%x type=0x%x severity=0x%x id=%u message=%.*s\n",
                 static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(contextTag)),
                 source, type, severity, id, message ? std::clamp(int(length), 0, 1024) : 0,
                 message ? message : "");
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
        outError = m_window.LastError().empty() ? "Window initialization failed." : m_window.LastError();
        return false;
    }
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    if (!glGetString(GL_VERSION)) { outError = "OpenGL ES 3 context unavailable"; return false; }
#else
    if (gladLoadGL(m_window.UsesVulkan() ? &LoadVulkanProcAddress : &LoadOpenGLProcAddress) == 0 || !GLAD_GL_VERSION_3_3) {
        outError = "Failed to load the required OpenGL 3.3 Core entry points.";
        return false;
    }
#endif
    m_graphicsCapabilities = QueryGraphicsCapabilities(m_window.RequestedGraphicsProfile());
    if (!ValidateGraphicsCapabilities(m_graphicsCapabilities, outError)) return false;
    std::fprintf(stderr, "%s\n", GraphicsCapabilitiesSummary(m_graphicsCapabilities).c_str());
    StartGpuDiagnostics();
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
    if (m_resources) {m_resources->AdvanceVisualFrame();m_resources->Pump();}
    m_audio.Update();
    PollGpuReset();
}

void EngineHost::StartGpuDiagnostics() {
    const char* enabled = std::getenv("JUDAS_GPU_DIAGNOSTICS");
    if (!enabled || std::strcmp(enabled, "1") != 0) return;
    if (m_window.UsesVulkan()) {
        std::fprintf(stderr, "[GPU] OpenGL debug callbacks do not apply to the Vulkan backend; "
                             "use JUDAS_VULKAN_VALIDATION=1\n");
        return;
    }
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    std::fprintf(stderr, "[GPU] native diagnostic callbacks unavailable on this platform\n");
#else
    GLint major = 0, minor = 0, flags = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    std::fprintf(stderr, "[GPU] vendor=%s renderer=%s version=%s context_flags=0x%x\n",
                 reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
                 reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                 reinterpret_cast<const char*>(glGetString(GL_VERSION)), flags);
    const bool coreDebug = major > 4 || (major == 4 && minor >= 3);
    if (coreDebug || HasGlExtension("GL_KHR_debug")) {
        using Control = void (GLAD_API_PTR *)(GLenum, GLenum, GLenum, GLsizei, const GLuint*, GLboolean);
        auto install = reinterpret_cast<GpuCallbackInstaller>(SDL_GL_GetProcAddress("glDebugMessageCallback"));
        auto control = reinterpret_cast<Control>(SDL_GL_GetProcAddress("glDebugMessageControl"));
        if (!install) install = reinterpret_cast<GpuCallbackInstaller>(SDL_GL_GetProcAddress("glDebugMessageCallbackKHR"));
        if (!control) control = reinterpret_cast<Control>(SDL_GL_GetProcAddress("glDebugMessageControlKHR"));
        if (install && control) {
            control(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
            control(GL_DONT_CARE, GL_DONT_CARE, kDebugSeverityHigh, 0, nullptr, GL_TRUE);
            control(GL_DONT_CARE, GL_DONT_CARE, kDebugSeverityMedium, 0, nullptr, GL_TRUE);
            control(GL_DONT_CARE, kDebugTypeError, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            control(GL_DONT_CARE, kDebugTypePerformance, GL_DONT_CARE, 0, nullptr, GL_FALSE);
            control(GL_DONT_CARE, GL_DONT_CARE, kDebugSeverityNotification, 0, nullptr, GL_FALSE);
            const auto tag = nextGpuContextTag.fetch_add(1, std::memory_order_relaxed);
            install(GpuDebugMessage, reinterpret_cast<const void*>(tag));
            m_gpuCallbackInstaller = install;
            glDisable(kDebugOutputSynchronous);
            glEnable(kDebugOutput);
            std::fprintf(stderr, "[GPU] asynchronous KHR_debug enabled; max_messages=%u max_message_bytes=1024\n",
                         kMaximumGpuMessages);
        }
    }
    if (!m_gpuCallbackInstaller)
        std::fprintf(stderr, "[GPU] KHR_debug unavailable; driver journal remains necessary\n");
    const bool robustness = HasGlExtension("GL_KHR_robustness") || HasGlExtension("GL_ARB_robustness");
    if (robustness) {
        for (const char* name : {"glGetGraphicsResetStatus", "glGetGraphicsResetStatusKHR", "glGetGraphicsResetStatusARB"}) {
            m_gpuResetQuery = reinterpret_cast<GpuResetQuery>(SDL_GL_GetProcAddress(name));
            if (m_gpuResetQuery) break;
        }
        GLint strategy = 0;
        glGetIntegerv(kResetNotificationStrategy, &strategy);
        std::fprintf(stderr, "[GPU] reset_query=%s notification_strategy=0x%x (not a promise of crash detection)\n",
                     m_gpuResetQuery ? "available" : "unavailable", strategy);
        PollGpuReset();
    }
#endif
}

void EngineHost::PollGpuReset() {
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    if (m_gpuResetQuery && SDL_GL_GetCurrentContext() == m_window.NativeGLContext()) {
        const GLenum state = m_gpuResetQuery();
        if (state != GL_NO_ERROR && state != m_gpuLastReset)
            std::fprintf(stderr, "[GPU] graphics reset status=0x%x\n", state);
        m_gpuLastReset = state;
    }
#endif
}

void EngineHost::StopGpuDiagnostics() {
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    if (m_gpuCallbackInstaller && SDL_GL_GetCurrentContext() == m_window.NativeGLContext()) {
        PollGpuReset();
        m_gpuCallbackInstaller(nullptr, nullptr);
        glDisable(kDebugOutput);
    }
    m_gpuCallbackInstaller = nullptr;
    m_gpuResetQuery = nullptr;
    m_gpuLastReset = GL_NO_ERROR;
#endif
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
            event.contextCurrent = GraphicsContextCurrent();
            m_resourceTrace(event);
        }
    }
    m_jobs.reset();
    if (m_rendererInitialized) {
        m_renderer.Shutdown();
        m_rendererInitialized = false;
    }
    StopGpuDiagnostics();
    const bool hadContext = GraphicsContextCurrent();
    m_window.Shutdown();
    m_graphicsCapabilities = {};
    if (hadContext && m_resourceTrace) {
        ResourceTraceEvent event{ResourceTracePoint::ContextDestroyed, {}, {}};
        event.contextCurrent = GraphicsContextCurrent();
        m_resourceTrace(event);
    }
}

void EngineHost::OpenContentView(const AssetDatabase& assets){
    m_audio.StopAll();m_renderer.ResetProjectText();if(m_resources)m_resources->ReleaseAll();m_assetDatabase=assets;
}
