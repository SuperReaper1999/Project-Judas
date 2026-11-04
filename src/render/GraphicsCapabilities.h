#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// API-independent description of the current context. Renderer consumers do
// not need GL headers to inspect a tier, limit or unsupported feature.
enum class GraphicsApi { Desktop, GLES, WebGL };
enum class GraphicsProfile { Auto, Portable, Modern };

struct GraphicsVersion {
    int major = 0, minor = 0;
};

struct GraphicsLimits {
    int textureSize = 0, texture3DSize = 0, arrayTextureLayers = 0;
    int fragmentTextureUnits = 0, combinedTextureUnits = 0;
    int vertexAttributes = 0, drawBuffers = 0;
    int uniformBufferBindings = 0, uniformBlockSize = 0, uniformBufferAlignment = 0;
    int storageBufferBindings = 0, storageBufferAlignment = 0;
    std::uint64_t storageBlockSize = 0;
    int computeInvocations = 0, computeSharedBytes = 0;
    std::array<int, 3> computeGroupCount{}, computeGroupSize{};
};

// Each bit represents the complete entry-point group needed by that facility,
// not merely an advertised version/extension. The probe is also the pure-test
// boundary; actual context discovery is owned by QueryGraphicsCapabilities.
struct GraphicsFunctions {
    bool uniformBuffers = false, compute = false, storageBuffers = false;
    bool directStateAccess = false, bufferStorage = false, textureViews = false;
    bool multiDrawIndirect = false;
};

struct GraphicsCapabilityProbe {
    GraphicsApi api = GraphicsApi::Desktop;
    GraphicsVersion version;
    GraphicsFunctions functions;
    GraphicsLimits limits;
    bool contextCurrent = false;
    std::vector<std::string> extensions;
    std::string vendor, renderer, versionString;
};

struct GraphicsCapabilities {
    GraphicsApi api = GraphicsApi::Desktop;
    GraphicsProfile requestedProfile = GraphicsProfile::Auto;
    GraphicsVersion version;
    GraphicsLimits limits;
    bool contextCurrent = false;
    // Qualified hardware facilities. Forced portable can still report support
    // on a driver which supplies a newer context for a 3.3 request. Modern
    // renderer paths must additionally respect modernTier/profile selection.
    bool uniformBuffers = false, compute = false, storageBuffers = false;
    bool directStateAccess = false, bufferStorage = false, textureViews = false;
    bool multiDrawIndirect = false, modernTier = false;
    std::string profile = "unavailable", vendor, renderer, versionString, diagnostic;
};

const char *GraphicsProfileName(GraphicsProfile);
bool ParseGraphicsProfile(const char *value, GraphicsProfile &, std::string &error);
std::vector<GraphicsVersion> DesktopGraphicsContextRequests(GraphicsProfile);
GraphicsCapabilities EvaluateGraphicsCapabilities(const GraphicsCapabilityProbe &, GraphicsProfile);
bool ValidateGraphicsCapabilities(const GraphicsCapabilities &, std::string &error);
std::string GraphicsCapabilitiesSummary(const GraphicsCapabilities &);

// The explicit form does not read process configuration. The no-argument form
// honors JUDAS_GL_PROFILE on desktop; Android/WebGL keep their existing API.
GraphicsCapabilities QueryGraphicsCapabilities(GraphicsProfile);
GraphicsCapabilities QueryGraphicsCapabilities();
