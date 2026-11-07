#include "../Renderer.h"

// Context-owned target allocation and one-level offscreen pass scope. Scene
// policy and draw submission remain with the caller.

RenderTargetHandle Renderer::CreateRenderTarget(int width, int height, std::string& error,bool display) {
    error.clear();
    GLint maximum = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width < 1 || height < 1 || width > maximum || height > maximum || m_activeTarget.IsValid()) {
        error = "invalid render-target dimensions or creation during an active target pass"; return {};
    }
    GLint draw = 0, read = 0, texture = 0, depth = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture); glGetIntegerv(GL_RENDERBUFFER_BINDING, &depth);
    GpuTarget target; target.width = width; target.height = height;
    GpuTexture color;color.width=width;color.height=height; color.sceneLinear=m_linearRendering&&!display;color.renderTarget=true;color.uploadedBytes = static_cast<std::size_t>(width) * height * (color.sceneLinear?8:4);
    glGenTextures(1, &color.textureId); glBindTexture(GL_TEXTURE_2D, color.textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, color.sceneLinear?GL_RGBA16F:GL_RGBA8, width, height, 0, GL_RGBA, color.sceneLinear?GL_HALF_FLOAT:GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &target.depth); glBindRenderbuffer(GL_RENDERBUFFER, target.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glGenFramebuffers(1, &target.framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.textureId, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(depth));
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &target.framebuffer); glDeleteRenderbuffers(1, &target.depth);
        glDeleteTextures(1, &color.textureId);
        error = "render-target framebuffer incomplete: " + std::to_string(status); return {};
    }
    color.alive = true; target.color=InstallTexture(color);
    m_targets.push_back(target);
    TraceResourceOperation(ResourceTracePoint::TextureCreated, target.color.id, color.uploadedBytes);
    return RenderTargetHandle{static_cast<unsigned int>(m_targets.size() - 1)};
}

bool Renderer::ResizeRenderTarget(RenderTargetHandle& target, int width, int height, std::string& error,bool display) {
    if (m_activeTarget.IsValid()) { error = "cannot resize during a target pass"; return false; }
    if (target.IsValid() && target.id < m_targets.size()) {
        const auto& old = m_targets[target.id];
        if (old.framebuffer && old.width == width && old.height == height && m_textures[old.color.id].sceneLinear==(m_linearRendering&&!display)) { error.clear(); return true; }
    }
    auto replacement = CreateRenderTarget(width, height, error,display);
    if (!replacement.IsValid()) return false;
    DestroyRenderTarget(target); target = replacement; return true;
}

void Renderer::DestroyRenderTarget(RenderTargetHandle target) {
    if (!target.IsValid() || target.id >= m_targets.size()) return;
    if (m_activeTarget.id == target.id) EndRenderTarget();
    auto& gpu = m_targets[target.id];
    const GLuint oldFramebuffer = gpu.framebuffer, oldDepth = gpu.depth;
    const GLuint oldColor = gpu.color.IsValid() && gpu.color.id < m_textures.size() ? m_textures[gpu.color.id].textureId : 0;
    if (gpu.framebuffer) glDeleteFramebuffers(1, &gpu.framebuffer);
    if (gpu.depth) glDeleteRenderbuffers(1, &gpu.depth);
    DestroyTexture(gpu.color); gpu = GpuTarget{};
    if ((oldFramebuffer && glIsFramebuffer(oldFramebuffer)) || (oldDepth && glIsRenderbuffer(oldDepth)) ||
        (oldColor && glIsTexture(oldColor))) ++m_stats.targetDeletionFailures;
}

TextureHandle Renderer::RenderTargetTexture(RenderTargetHandle target) const {
    if (!target.IsValid() || target.id >= m_targets.size() || !m_targets[target.id].framebuffer) return {};
    return m_targets[target.id].color;
}

bool Renderer::BeginRenderTarget(RenderTargetHandle target, bool preserveContents) {
    if (m_activeTarget.IsValid() || m_shadowPassActive || !RenderTargetTexture(target).IsValid()) return false;
    // Pending transparent geometry belongs to the current camera and target.
    // Publish it before changing either; it must not spill into the child view.
    FlushMaterialBlends();FinishGridWater();
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_savedDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_savedReadFramebuffer);
    glGetIntegerv(GL_VIEWPORT, m_savedViewport);
    m_savedRenderMask=m_renderMask;m_savedRenderLayer=m_renderLayer;
    m_savedView = m_view; m_savedProjection = m_projection;m_savedPixelGrid=m_spritePixelGrid; m_activeTarget = target;
    m_suspendedLinearPass = {m_linearPass,
                            {m_hdrFbo, m_hdrColor, m_hdrDepth, m_hdrWidth, m_hdrHeight},
                            m_outputFramebuffer};
    m_linearPass = false;
    const auto& gpu = m_targets[target.id];
    glBindFramebuffer(GL_FRAMEBUFFER, gpu.framebuffer);
    if (!preserveContents) BeginFrame(gpu.width, gpu.height);
    else glViewport(0, 0, gpu.width, gpu.height);
    ++m_stats.offscreenPasses; return true;
}

void Renderer::EndRenderTarget(bool restoreCamera) {
    if (!m_activeTarget.IsValid()) return;
    FlushMaterialBlends();FinishGridWater();const bool background=m_environmentBackground;
    if(m_weather.enabled)m_environmentBackground=false;
    ResolveLinearPass(m_textures[m_targets[m_activeTarget.id].color.id].sceneLinear);m_environmentBackground=background;
    const auto parent = m_suspendedLinearPass;
    m_hdrFbo = parent.target.fbo;
    m_hdrColor = parent.target.color;
    m_hdrDepth = parent.target.depth;
    m_hdrWidth = parent.target.width;
    m_hdrHeight = parent.target.height;
    m_outputFramebuffer = parent.outputFramebuffer;
    m_linearPass = parent.active;
    m_suspendedLinearPass = {};
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_savedDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_savedReadFramebuffer));
    glViewport(m_savedViewport[0], m_savedViewport[1], m_savedViewport[2], m_savedViewport[3]);
    if (restoreCamera) {
        SetCamera(m_savedView,m_savedProjection);m_spritePixelGrid=m_savedPixelGrid;
        m_renderMask=m_savedRenderMask;m_renderLayer=m_savedRenderLayer;
    }
    m_activeTarget = {};
}

Renderer::TargetDiagnostics Renderer::RenderTargetDiagnostics() const {
    TargetDiagnostics result; GLint framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, &result.viewport[0]); result.defaultFramebuffer = framebuffer == 0;
    result.passActive = m_activeTarget.IsValid();
    for (const auto& gpu : m_targets) if (gpu.framebuffer) ++result.liveTargets;
    return result;
}

glm::ivec2 Renderer::RenderTargetSize(RenderTargetHandle target) const {
    if (!RenderTargetTexture(target).IsValid()) return glm::ivec2(0);
    return glm::ivec2(m_targets[target.id].width, m_targets[target.id].height);
}
void Renderer::FinishForDiagnostics() const { glFinish(); }
