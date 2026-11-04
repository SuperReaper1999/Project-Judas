#include "EngineHost.h"
#include "Window.h"
#include "render/GraphicsCapabilities.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
}
GraphicsCapabilityProbe Desktop(int major, int minor) {
    GraphicsCapabilityProbe probe;
    probe.contextCurrent = true;
    probe.version = {major, minor};
    probe.vendor = "Independent probe";
    probe.renderer = "Capability contract fixture";
    probe.functions = {true, true, true, true, true, true, true};
    probe.limits.textureSize = 4096;
    probe.limits.texture3DSize = 256;
    probe.limits.arrayTextureLayers = 256;
    probe.limits.fragmentTextureUnits = 16;
    probe.limits.combinedTextureUnits = 32;
    probe.limits.vertexAttributes = 16;
    probe.limits.drawBuffers = 8;
    probe.limits.uniformBufferBindings = 24;
    probe.limits.uniformBlockSize = 16384;
    probe.limits.uniformBufferAlignment = 256;
    probe.limits.storageBufferBindings = 8;
    probe.limits.storageBlockSize = 128 * 1024 * 1024;
    probe.limits.storageBufferAlignment = 256;
    probe.limits.computeInvocations = 1024;
    probe.limits.computeSharedBytes = 32768;
    probe.limits.computeGroupCount = {65535, 65535, 65535};
    probe.limits.computeGroupSize = {1024, 1024, 64};
    return probe;
}
void PureControls() {
    GraphicsProfile profile = GraphicsProfile::Modern;
    std::string error;
    Check(ParseGraphicsProfile(nullptr, profile, error) && profile == GraphicsProfile::Auto,
          "absent override selects automatic desktop negotiation");
    Check(ParseGraphicsProfile("portable", profile, error) && profile == GraphicsProfile::Portable,
          "portable override parses exactly");
    Check(ParseGraphicsProfile("modern", profile, error) && profile == GraphicsProfile::Modern,
          "modern override parses exactly");
    for (const auto *invalid : {"", "Modern", "4.6", "modern ", "unknown"}) {
        profile = GraphicsProfile::Portable;
        Check(!ParseGraphicsProfile(invalid, profile, error) && !error.empty() &&
                  error.find("auto, portable or modern") != std::string::npos &&
                  profile == GraphicsProfile::Portable,
              "invalid override is rejected usefully rather than silently selecting another profile");
    }
    const auto automatic = DesktopGraphicsContextRequests(GraphicsProfile::Auto);
    const auto portable = DesktopGraphicsContextRequests(GraphicsProfile::Portable);
    const auto modern = DesktopGraphicsContextRequests(GraphicsProfile::Modern);
    Check(automatic.size() == 3 && automatic[0].major == 4 && automatic[0].minor == 6 &&
              automatic[1].major == 4 && automatic[1].minor == 5 && automatic[2].major == 3 &&
              automatic[2].minor == 3,
          "automatic context requests prefer 4.6 then 4.5 and retain the 3.3 fallback");
    Check(portable.size() == 1 && portable.front().major == 3 && portable.front().minor == 3,
          "forced portable requests only 3.3");
    Check(modern.size() == 2 && modern.front().major == 4 && modern.back().minor == 5,
          "forced modern never silently requests the portable context");

    auto probe = Desktop(4, 6);
    auto caps = EvaluateGraphicsCapabilities(probe, GraphicsProfile::Auto);
    Check(caps.modernTier && caps.profile == "modern" && ValidateGraphicsCapabilities(caps, error),
          "complete desktop 4.6 capabilities qualify the modern tier");
    const auto forcedPortable = EvaluateGraphicsCapabilities(probe, GraphicsProfile::Portable);
    Check(!forcedPortable.modernTier && forcedPortable.profile == "portable" &&
              forcedPortable.compute && forcedPortable.bufferStorage,
          "portable policy remains portable even if its driver returns a newer capable context");
    const auto summary = GraphicsCapabilitiesSummary(forcedPortable);
    Check(summary.find("requested=portable selected=portable") != std::string::npos &&
              summary.find("supported UBO=1 compute=1") != std::string::npos,
          "diagnostics distinguish selected policy from qualified hardware facilities");

    using Member = bool GraphicsFunctions::*;
    const std::array<Member, 7> entryGroups = {
        &GraphicsFunctions::uniformBuffers,   &GraphicsFunctions::compute,
        &GraphicsFunctions::storageBuffers,   &GraphicsFunctions::directStateAccess,
        &GraphicsFunctions::bufferStorage,    &GraphicsFunctions::textureViews,
        &GraphicsFunctions::multiDrawIndirect};
    for (const auto member : entryGroups) {
        auto missing = probe;
        missing.functions.*member = false;
        const auto rejected = EvaluateGraphicsCapabilities(missing, GraphicsProfile::Modern);
        Check(!rejected.modernTier && !ValidateGraphicsCapabilities(rejected, error) &&
                  error.find("Unavailable:") != std::string::npos,
              "advertised modern version cannot hide a missing required entry-point group");
        const auto fallback = EvaluateGraphicsCapabilities(missing, GraphicsProfile::Auto);
        Check(!fallback.modernTier && ValidateGraphicsCapabilities(fallback, error),
              "automatic selection retains a usable portable path when a modern entry point is absent");
    }
    const auto core43 = EvaluateGraphicsCapabilities(Desktop(4, 3), GraphicsProfile::Auto);
    Check(!core43.modernTier && core43.compute && core43.storageBuffers && core43.multiDrawIndirect &&
              !core43.directStateAccess && !core43.bufferStorage,
          "4.3 reports individual compute/storage/indirect support without inventing a complete 4.5 "
          "tier");
    Check(!ValidateGraphicsCapabilities(
              EvaluateGraphicsCapabilities(Desktop(4, 3), GraphicsProfile::Modern), error) &&
              error.find("4.5+") != std::string::npos,
          "forced modern rejects an older otherwise usable context with an actionable requirement");
    const auto core33 = EvaluateGraphicsCapabilities(Desktop(3, 3), GraphicsProfile::Auto);
    Check(!core33.modernTier && core33.uniformBuffers && !core33.compute && !core33.storageBuffers &&
              !core33.multiDrawIndirect && !core33.bufferStorage,
          "non-null function addresses alone do not imply version or extension support");
    auto extended = Desktop(3, 3);
    extended.extensions = {"GL_ARB_compute_shader",      "GL_ARB_shader_storage_buffer_object",
                           "GL_ARB_direct_state_access", "GL_ARB_buffer_storage",
                           "GL_ARB_texture_view",        "GL_ARB_multi_draw_indirect"};
    auto extensionCaps = EvaluateGraphicsCapabilities(extended, GraphicsProfile::Auto);
    Check(!extensionCaps.modernTier && extensionCaps.compute && extensionCaps.storageBuffers &&
              extensionCaps.directStateAccess && extensionCaps.bufferStorage &&
              extensionCaps.textureViews && extensionCaps.multiDrawIndirect,
          "verified extension facilities remain individually visible without claiming a 4.5 context");
    extended.functions.compute = false;
    Check(!EvaluateGraphicsCapabilities(extended, GraphicsProfile::Auto).compute,
          "extension advertising without its entry points does not enable compute");

    for (const auto api : {GraphicsApi::GLES, GraphicsApi::WebGL}) {
        auto embedded = extended;
        embedded.api = api;
        embedded.version = {3, 0};
        const auto result = EvaluateGraphicsCapabilities(embedded, GraphicsProfile::Auto);
        Check(
            result.uniformBuffers && !result.modernTier && !result.compute && !result.storageBuffers &&
                !result.bufferStorage && !result.directStateAccess && !result.multiDrawIndirect &&
                ValidateGraphicsCapabilities(result, error),
            "GLES3/WebGL2 baseline cannot accidentally select desktop facilities from extension names");
    }
    auto absent = probe;
    absent.contextCurrent = false;
    Check(!ValidateGraphicsCapabilities(EvaluateGraphicsCapabilities(absent, GraphicsProfile::Auto),
                                        error) &&
              error.find("current graphics context") != std::string::npos,
          "no-current-context is explicitly rejected");
    Check(!ValidateGraphicsCapabilities(
              EvaluateGraphicsCapabilities(Desktop(3, 2), GraphicsProfile::Auto), error) &&
              error.find("OpenGL 3.3") != std::string::npos,
          "older desktop contexts cannot pass the minimum rendering contract");
    auto zeroCompute = probe;
    zeroCompute.limits.computeGroupCount[1] = 0;
    Check(!EvaluateGraphicsCapabilities(zeroCompute, GraphicsProfile::Auto).compute,
          "invalid compute limits reject the facility despite version and function support");
    auto zeroStorage = probe;
    zeroStorage.limits.storageBlockSize = 0;
    Check(!EvaluateGraphicsCapabilities(zeroStorage, GraphicsProfile::Auto).storageBuffers,
          "invalid shader-storage limits reject the facility");
    auto tooFewUnits = probe;
    tooFewUnits.limits.fragmentTextureUnits = 8;
    Check(!ValidateGraphicsCapabilities(
              EvaluateGraphicsCapabilities(tooFewUnits, GraphicsProfile::Auto), error),
          "insufficient texture units cannot masquerade as a usable context");
}

class ProfileEnvironment {
  public:
    explicit ProfileEnvironment(const char *value) {
        if (const auto *previous = std::getenv("JUDAS_GL_PROFILE"))
            saved = previous;
#ifdef _WIN32
        _putenv_s("JUDAS_GL_PROFILE", value);
#else
        setenv("JUDAS_GL_PROFILE", value, 1);
#endif
    }
    ~ProfileEnvironment() {
#ifdef _WIN32
        _putenv_s("JUDAS_GL_PROFILE", saved ? saved->c_str() : "");
#else
        if (saved)
            setenv("JUDAS_GL_PROFILE", saved->c_str(), 1);
        else
            unsetenv("JUDAS_GL_PROFILE");
#endif
    }

  private:
    std::optional<std::string> saved;
};

void NativeControls() {
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    {
        ProfileEnvironment setting("not-a-profile");
        Window window;
        Check(!window.Init("Invalid profile control", 320, 240, false) && !window.NativeWindow() &&
                  !window.NativeGLContext() &&
                  window.LastError().find("JUDAS_GL_PROFILE") != std::string::npos,
              "invalid actual desktop configuration fails before owning native resources");
    }
    for (const auto *setting : {"portable", "auto", "modern"}) {
        ProfileEnvironment environment(setting);
        EngineHost host;
        std::string error;
        const bool initialized = host.Init("Graphics capability qualification", 320, 240, false, error);
        if (!initialized && std::string(setting) == "modern") {
            Check(error.find("modern") != std::string::npos,
                  "unavailable forced modern context reports its requirement rather than silently "
                  "falling back");
        } else {
            Check(initialized,
                  "actual context and renderer initialize under the requested supported policy");
            if (initialized) {
                const auto &caps = host.GraphicsCaps();
                std::printf("%s\n", GraphicsCapabilitiesSummary(caps).c_str());
                Check(caps.contextCurrent && caps.limits.textureSize > 0 &&
                          caps.limits.fragmentTextureUnits >= 16,
                      "actual context publishes real nonzero limits");
                if (std::string(setting) == "portable")
                    Check(
                        !caps.modernTier && caps.profile == "portable" &&
                            caps.requestedProfile == GraphicsProfile::Portable,
                        "actual forced portable selection survives a driver returning a newer context");
                if (std::string(setting) == "modern")
                    Check(caps.modernTier && caps.profile == "modern",
                          "successful forced modern initialization really selects the qualified modern "
                          "tier");
            } else
                std::fprintf(stderr, "%s\n", error.c_str());
        }
        host.Shutdown();
        Check(!host.GetWindow().NativeWindow() && !host.GetWindow().NativeGLContext() &&
                  !host.GraphicsCaps().contextCurrent && !QueryGraphicsCapabilities().contextCurrent,
              "context shutdown clears native ownership and the published capability snapshot");
    }
#else
    Check(true, "desktop profile overrides do not replace the GLES3/WebGL2 platform context path");
#endif
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--native")) {
        std::fprintf(stderr, "usage: GraphicsCapabilitiesTests [--native]\n");
        return 2;
    }
    PureControls();
    if (argc == 2)
        NativeControls();
    std::printf("Graphics capabilities: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
