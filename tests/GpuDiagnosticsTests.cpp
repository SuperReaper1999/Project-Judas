// Native diagnostic opt-in/lifecycle controls. The injected messages below are
// deliberate test controls, not observed engine or driver failures.
#include "EngineHost.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
namespace {
constexpr GLenum kCallbackFunction = 0x8244;
constexpr GLenum kSynchronousOutput = 0x8242;
constexpr GLenum kApplicationSource = 0x824A;
constexpr GLenum kErrorType = 0x824C;
constexpr GLenum kPerformanceType = 0x8250;
constexpr GLenum kOtherType = 0x8251;
constexpr GLenum kHighSeverity = 0x9146;
constexpr GLenum kNotificationSeverity = 0x826B;
using GetPointer = void (GLAD_API_PTR *)(GLenum, void**);
using Insert = void (GLAD_API_PTR *)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*);
unsigned checks = 0, failures = 0;
void Check(bool yes, const char* description) {
    ++checks;
    failures += !yes;
    std::printf("%s %s\n", yes ? "PASS" : "FAIL", description);
}

bool HasDebugExtension() {
    GLint major = 0, minor = 0, count = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major > 4 || (major == 4 && minor >= 3)) return true;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint index = 0; index < count; ++index) {
        const auto* value = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, GLuint(index)));
        if (value && std::strcmp(value, "GL_KHR_debug") == 0) return true;
    }
    return false;
}

GetPointer PointerQuery() {
    auto query = reinterpret_cast<GetPointer>(SDL_GL_GetProcAddress("glGetPointerv"));
    if (!query) query = reinterpret_cast<GetPointer>(SDL_GL_GetProcAddress("glGetPointervKHR"));
    return query;
}
} // namespace
#endif

int main(int argc, char** argv) {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    (void)argc;
    (void)argv;
    std::puts("UNAVAILABLE native diagnostic callback test on this platform");
    return 77;
#else
    const bool enabled = argc > 1 && std::strcmp(argv[1], "off") != 0;
    const bool flood = argc > 1 && std::strcmp(argv[1], "flood") == 0;
    SDL_setenv("JUDAS_GPU_DIAGNOSTICS", enabled ? "1" : "0", 1);
    EngineHost host;
    std::string error;
    if (!host.Init("GPU diagnostic controls", 64, 64, false, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!HasDebugExtension() || !PointerQuery()) {
        std::puts("UNAVAILABLE KHR_debug controls; production fallback must remain usable");
        Check(glGetError() == GL_NO_ERROR, "unsupported optional diagnostics leave GL clean");
        return failures ? 1 : 77;
    }
    auto getPointer = PointerQuery();
    void* callback = nullptr;
    getPointer(kCallbackFunction, &callback);
    Check((callback != nullptr) == enabled, "callback registration exactly follows opt-in");
    if (enabled)
        Check(glIsEnabled(kSynchronousOutput) == GL_FALSE, "diagnostics do not force synchronous GL output");
    auto insert = reinterpret_cast<Insert>(SDL_GL_GetProcAddress("glDebugMessageInsert"));
    if (!insert) insert = reinterpret_cast<Insert>(SDL_GL_GetProcAddress("glDebugMessageInsertKHR"));
    Check(insert != nullptr, "KHR_debug insertion control is available");
    if (insert) {
        const int count = flood ? 270 : 1;
        for (int i = 0; i < count; ++i)
            insert(kApplicationSource, kErrorType, 778901, kHighSeverity, -1,
                   "JUDAS_GPU_TEST_ERROR (synthetic positive control)");
        insert(kApplicationSource, kPerformanceType, 778902, kHighSeverity, -1,
               "JUDAS_GPU_TEST_PERFORMANCE_MUST_BE_FILTERED");
        insert(kApplicationSource, kOtherType, 778903, kNotificationSeverity, -1,
               "JUDAS_GPU_TEST_NOTIFICATION_MUST_BE_FILTERED");
    }
    // One intentional API error checks real API error reporting. This test's
    // sole drain is not a production per-frame/per-draw glGetError loop.
    glEnable(GLenum(0xFFFFFFFF));
    Check(glGetError() == GL_INVALID_ENUM, "real API-error negative control remains observable");
    for (int frame = 0; frame < 3; ++frame) {
        host.PumpResources();
        host.GetRenderer().BeginFrame(64, 64);
        host.GetRenderer().EndFrame();
        host.GetWindow().SwapBuffers();
    }
    Check(glGetError() == GL_NO_ERROR, "diagnostic control run leaves no additional GL errors");
    host.Shutdown();
    host.Shutdown();
    Check(!GraphicsContextCurrent(), "callback owner teardown releases its context idempotently");

    SDL_setenv("JUDAS_GPU_DIAGNOSTICS", "0", 1);
    EngineHost next;
    Check(next.Init("GPU diagnostic next context", 64, 64, false, error), "fresh default host initializes after diagnostic teardown");
    if (GraphicsContextCurrent()) {
        callback = nullptr;
        PointerQuery()(kCallbackFunction, &callback);
        Check(callback == nullptr, "fresh default context inherits no diagnostic callback");
    }
    next.Shutdown();
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
#endif
}
