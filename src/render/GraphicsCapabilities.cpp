#include "GraphicsCapabilities.h"
#include "GraphicsApi.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {
bool AtLeast(GraphicsVersion version, int major, int minor) {
    return version.major > major || (version.major == major && version.minor >= minor);
}
bool HasExtension(const GraphicsCapabilityProbe &probe, const char *name) {
    return std::find(probe.extensions.begin(), probe.extensions.end(), name) != probe.extensions.end();
}
bool DesktopFeature(const GraphicsCapabilityProbe &probe, int major, int minor, const char *extension) {
    return probe.api == GraphicsApi::Desktop &&
           (AtLeast(probe.version, major, minor) || HasExtension(probe, extension));
}
std::string VersionText(GraphicsVersion version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor);
}
std::string MissingModernFeatures(const GraphicsCapabilities &caps) {
    std::string missing;
    const auto add = [&](bool available, const char *name) {
        if (!available) {
            if (!missing.empty())
                missing += ", ";
            missing += name;
        }
    };
    add(caps.uniformBuffers, "uniform buffers");
    add(caps.compute, "compute shaders");
    add(caps.storageBuffers, "shader-storage buffers");
    add(caps.directStateAccess, "direct state access");
    add(caps.bufferStorage, "immutable mapped-buffer storage and synchronization");
    add(caps.textureViews, "immutable texture views");
    add(caps.multiDrawIndirect, "multi-draw indirect");
    return missing;
}
} // namespace

const char *GraphicsProfileName(GraphicsProfile profile) {
    switch (profile) {
    case GraphicsProfile::Auto:
        return "auto";
    case GraphicsProfile::Portable:
        return "portable";
    case GraphicsProfile::Modern:
        return "modern";
    }
    return "auto";
}

bool ParseGraphicsProfile(const char *value, GraphicsProfile &profile, std::string &error) {
    error.clear();
    const std::string text = value ? value : "auto";
    if (text == "auto")
        profile = GraphicsProfile::Auto;
    else if (text == "portable")
        profile = GraphicsProfile::Portable;
    else if (text == "modern")
        profile = GraphicsProfile::Modern;
    else {
        error = "Invalid JUDAS_GL_PROFILE='" + text + "'. Choose exactly auto, portable or modern.";
        return false;
    }
    return true;
}

std::vector<GraphicsVersion> DesktopGraphicsContextRequests(GraphicsProfile profile) {
    if (profile == GraphicsProfile::Portable)
        return {{3, 3}};
    if (profile == GraphicsProfile::Modern)
        return {{4, 6}, {4, 5}};
    return {{4, 6}, {4, 5}, {3, 3}};
}

GraphicsCapabilities EvaluateGraphicsCapabilities(const GraphicsCapabilityProbe &probe,
                                                  GraphicsProfile requested) {
    GraphicsCapabilities caps;
    caps.api = probe.api;
    caps.requestedProfile = requested;
    caps.version = probe.version;
    caps.limits = probe.limits;
    caps.contextCurrent = probe.contextCurrent;
    caps.vendor = probe.vendor;
    caps.renderer = probe.renderer;
    caps.versionString = probe.versionString;
    if (!probe.contextCurrent)
        return caps;
    const bool embeddedUniforms = probe.api != GraphicsApi::Desktop && AtLeast(probe.version, 3, 0);
    caps.uniformBuffers =
        probe.functions.uniformBuffers && probe.limits.uniformBufferBindings > 0 &&
        probe.limits.uniformBlockSize > 0 &&
        (embeddedUniforms || DesktopFeature(probe, 3, 1, "GL_ARB_uniform_buffer_object"));
    caps.compute =
        probe.functions.compute && probe.limits.computeInvocations > 0 &&
        std::all_of(probe.limits.computeGroupCount.begin(), probe.limits.computeGroupCount.end(),
                    [](int value) { return value > 0; }) &&
        std::all_of(probe.limits.computeGroupSize.begin(), probe.limits.computeGroupSize.end(),
                    [](int value) { return value > 0; }) &&
        DesktopFeature(probe, 4, 3, "GL_ARB_compute_shader");
    caps.storageBuffers = probe.functions.storageBuffers && probe.limits.storageBufferBindings > 0 &&
                          probe.limits.storageBlockSize > 0 &&
                          DesktopFeature(probe, 4, 3, "GL_ARB_shader_storage_buffer_object");
    caps.directStateAccess =
        probe.functions.directStateAccess && DesktopFeature(probe, 4, 5, "GL_ARB_direct_state_access");
    caps.bufferStorage =
        probe.functions.bufferStorage && DesktopFeature(probe, 4, 4, "GL_ARB_buffer_storage");
    caps.textureViews =
        probe.functions.textureViews && DesktopFeature(probe, 4, 3, "GL_ARB_texture_view");
    caps.multiDrawIndirect =
        probe.functions.multiDrawIndirect && DesktopFeature(probe, 4, 3, "GL_ARB_multi_draw_indirect");
    caps.modernTier = requested != GraphicsProfile::Portable && probe.api == GraphicsApi::Desktop &&
                      AtLeast(probe.version, 4, 5) && MissingModernFeatures(caps).empty();
    caps.profile = probe.api == GraphicsApi::WebGL  ? "webgl2"
                   : probe.api == GraphicsApi::GLES ? "gles3"
                   : caps.modernTier                ? "modern"
                                                    : "portable";
    return caps;
}

bool ValidateGraphicsCapabilities(const GraphicsCapabilities &caps, std::string &error) {
    error.clear();
    if (!caps.diagnostic.empty()) {
        error = caps.diagnostic;
        return false;
    }
    if (!caps.contextCurrent) {
        error = "No current graphics context is available for capability discovery.";
        return false;
    }
    if (!AtLeast(caps.version, 3, caps.api == GraphicsApi::Desktop ? 3 : 0)) {
        error = "Judas requires " +
                std::string(caps.api == GraphicsApi::Desktop ? "OpenGL 3.3 Core"
                                                             : "OpenGL ES 3 / WebGL 2") +
                "; the current context reports " + VersionText(caps.version) + ".";
        return false;
    }
    if (caps.limits.fragmentTextureUnits < 16 || caps.limits.textureSize < 1) {
        error = "The graphics context does not expose the required sixteen fragment texture units "
                "and a valid maximum texture size. Check the graphics driver and selected backend.";
        return false;
    }
    if (caps.requestedProfile == GraphicsProfile::Modern && !caps.modernTier) {
        error = "JUDAS_GL_PROFILE=modern requires a desktop OpenGL 4.5+ context and its complete "
                "modern feature entry points. Current context: " +
                VersionText(caps.version) + ".";
        const auto missing = MissingModernFeatures(caps);
        if (!missing.empty())
            error += " Unavailable: " + missing + ".";
        error += " Use JUDAS_GL_PROFILE=auto or portable for the compatible rendering path.";
        return false;
    }
    return true;
}

std::string GraphicsCapabilitiesSummary(const GraphicsCapabilities &caps) {
    std::ostringstream text;
    text << "[Graphics] API=";
    if (caps.api == GraphicsApi::WebGL)
        text << "WebGL2 (GLES " << VersionText(caps.version) << ")";
    else
        text << (caps.api == GraphicsApi::Desktop ? "OpenGL " : "OpenGL ES ")
             << VersionText(caps.version);
    text << " requested=" << GraphicsProfileName(caps.requestedProfile) << " selected=" << caps.profile
         << " vendor=" << caps.vendor << " renderer=" << caps.renderer
         << "\n[Graphics] supported UBO=" << caps.uniformBuffers << " compute=" << caps.compute
         << " SSBO=" << caps.storageBuffers << " DSA=" << caps.directStateAccess
         << " bufferStorage=" << caps.bufferStorage << " textureViews=" << caps.textureViews
         << " MDI=" << caps.multiDrawIndirect
         << "\n[Graphics] limits texture=" << caps.limits.textureSize
         << " texture3D=" << caps.limits.texture3DSize
         << " arrayLayers=" << caps.limits.arrayTextureLayers
         << " fragmentUnits=" << caps.limits.fragmentTextureUnits
         << " uniformBindings=" << caps.limits.uniformBufferBindings
         << " uniformBlockBytes=" << caps.limits.uniformBlockSize
         << " storageBindings=" << caps.limits.storageBufferBindings
         << " storageBlockBytes=" << caps.limits.storageBlockSize
         << " computeInvocations=" << caps.limits.computeInvocations
         << " computeSharedBytes=" << caps.limits.computeSharedBytes;
    return text.str();
}

GraphicsCapabilities QueryGraphicsCapabilities(GraphicsProfile profile) {
    GraphicsCapabilityProbe probe;
#if defined(__EMSCRIPTEN__)
    probe.api = GraphicsApi::WebGL;
#elif defined(__ANDROID__)
    probe.api = GraphicsApi::GLES;
#endif
    probe.contextCurrent = GraphicsContextCurrent();
    if (!probe.contextCurrent)
        return EvaluateGraphicsCapabilities(probe, profile);
    const auto text = [](GLenum field) {
        const auto *value = glGetString(field);
        return value ? std::string(reinterpret_cast<const char *>(value)) : std::string{};
    };
    probe.versionString = text(GL_VERSION);
    probe.vendor = text(GL_VENDOR);
    probe.renderer = text(GL_RENDERER);
    glGetIntegerv(GL_MAJOR_VERSION, &probe.version.major);
    glGetIntegerv(GL_MINOR_VERSION, &probe.version.minor);
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint index = 0; index < count; ++index)
        if (const auto *extension = glGetStringi(GL_EXTENSIONS, GLuint(index)))
            probe.extensions.emplace_back(reinterpret_cast<const char *>(extension));
    auto &limits = probe.limits;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limits.textureSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &limits.texture3DSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &limits.arrayTextureLayers);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &limits.fragmentTextureUnits);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &limits.combinedTextureUnits);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &limits.vertexAttributes);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &limits.drawBuffers);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &limits.uniformBufferBindings);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &limits.uniformBlockSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &limits.uniformBufferAlignment);
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    // These builds intentionally retain the qualified GLES3/WebGL2 backend.
    // Desktop extension names must not accidentally enable desktop-only paths.
    probe.functions.uniformBuffers = true;
#else
    auto &functions = probe.functions;
    functions.uniformBuffers = glBindBufferBase && glBindBufferRange && glGetUniformBlockIndex &&
                               glUniformBlockBinding && glGetActiveUniformBlockiv;
    functions.compute = glDispatchCompute && glMemoryBarrier && glGetIntegeri_v;
    functions.storageBuffers = glBindBufferBase && glBindBufferRange && glGetProgramResourceIndex &&
                               glShaderStorageBlockBinding && glGetProgramResourceiv && glGetInteger64v;
    functions.directStateAccess =
        glCreateBuffers && glNamedBufferData && glNamedBufferSubData && glNamedBufferStorage &&
        glCreateTextures && glTextureStorage2D && glTextureSubImage2D && glBindTextureUnit &&
        glCreateVertexArrays && glVertexArrayVertexBuffer && glVertexArrayAttribFormat &&
        glVertexArrayAttribIFormat && glEnableVertexArrayAttrib && glVertexArrayAttribBinding &&
        glVertexArrayElementBuffer && glCreateFramebuffers && glNamedFramebufferTexture &&
        glCheckNamedFramebufferStatus;
    functions.bufferStorage = glBufferStorage && glMapBufferRange && glFlushMappedBufferRange &&
                              glFenceSync && glClientWaitSync && glDeleteSync;
    functions.textureViews = glTextureView && glTexStorage2D;
    functions.multiDrawIndirect = glMultiDrawArraysIndirect && glMultiDrawElementsIndirect;
    if (functions.storageBuffers &&
        DesktopFeature(probe, 4, 3, "GL_ARB_shader_storage_buffer_object")) {
        glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &limits.storageBufferBindings);
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &limits.storageBufferAlignment);
        GLint64 bytes = 0;
        glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &bytes);
        limits.storageBlockSize = std::uint64_t(std::max<GLint64>(0, bytes));
    }
    if (functions.compute && DesktopFeature(probe, 4, 3, "GL_ARB_compute_shader")) {
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &limits.computeInvocations);
        glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &limits.computeSharedBytes);
        for (GLuint axis = 0; axis < 3; ++axis) {
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, axis, &limits.computeGroupCount[axis]);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, axis, &limits.computeGroupSize[axis]);
        }
    }
#endif
    return EvaluateGraphicsCapabilities(probe, profile);
}

GraphicsCapabilities QueryGraphicsCapabilities() {
    GraphicsProfile profile = GraphicsProfile::Auto;
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    std::string error;
    if (!ParseGraphicsProfile(std::getenv("JUDAS_GL_PROFILE"), profile, error)) {
        GraphicsCapabilities caps;
        caps.diagnostic = error;
        return caps;
    }
#endif
    return QueryGraphicsCapabilities(profile);
}
