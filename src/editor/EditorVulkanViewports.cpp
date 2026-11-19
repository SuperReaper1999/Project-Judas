#include "EditorVulkanViewports.h"

#ifdef JUDAS_VULKAN
#include "Renderer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "vulkan/VulkanBackend.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <cstdio>
#include <memory>

namespace {
struct DetachedViewport {
    SDL_Window *window = nullptr;
    JudasVulkan::PlatformWindow *surface = nullptr;
    Renderer *renderer = nullptr;
    RenderTargetHandle target;
};

void ReportFailure(ImGuiViewport *viewport, const std::string &error) {
    std::fprintf(stderr, "Vulkan editor viewport could not be presented: %s\n", error.c_str());
    viewport->PlatformRequestClose = true;
}

void CreateViewport(ImGuiViewport *viewport) {
    auto state = std::make_unique<DetachedViewport>();
    state->window = SDL_GetWindowFromID(static_cast<Uint32>(reinterpret_cast<std::uintptr_t>(viewport->PlatformHandle)));
    std::string error;
    state->surface = JudasVulkan::CreatePlatformWindow(state->window, error);
    if (!state->surface) {
        ReportFailure(viewport, error);
        return;
    }
    viewport->RendererUserData = state.release();
}

void DestroyViewport(ImGuiViewport *viewport) {
    std::unique_ptr<DetachedViewport> state(static_cast<DetachedViewport *>(viewport->RendererUserData));
    viewport->RendererUserData = nullptr;
    if (!state) return;
    JudasVulkan::DestroyPlatformWindow(state->surface);
    if (state->renderer) state->renderer->DestroyRenderTarget(state->target);
}

void RenderViewport(ImGuiViewport *viewport, void *rendererArg) {
    auto *state = static_cast<DetachedViewport *>(viewport->RendererUserData);
    auto *draw = viewport->DrawData;
    if (!state || !draw || draw->DisplaySize.x <= 0 || draw->DisplaySize.y <= 0) return;

    int width = 0, height = 0;
    SDL_Vulkan_GetDrawableSize(state->window, &width, &height);
    if (width <= 0 || height <= 0 ||
        (SDL_GetWindowFlags(state->window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN))) return;

    auto &renderer = *static_cast<Renderer *>(rendererArg);
    state->renderer = &renderer;
    std::string error;
    if (!renderer.ResizeRenderTarget(state->target, width, height, error, true) ||
        !renderer.BeginRenderTarget(state->target, true)) {
        ReportFailure(viewport, error.empty() ? "Cannot bind the viewport colour target" : error);
        return;
    }
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear)) {
        const bool scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (scissor) glEnable(GL_SCISSOR_TEST);
    }
    const auto previousScale = draw->FramebufferScale;
    draw->FramebufferScale = {width / draw->DisplaySize.x, height / draw->DisplaySize.y};
    ImGui_ImplOpenGL3_RenderDrawData(draw);
    draw->FramebufferScale = previousScale;
    renderer.EndRenderTarget();

    const auto token = renderer.EditorImageToken(renderer.RenderTargetTexture(state->target));
    if (!JudasVulkan::PresentPlatformWindow(*state->surface, static_cast<std::uint32_t>(token), error))
        ReportFailure(viewport, error);
}
} // namespace

bool InstallVulkanEditorViewports() {
    if (!JudasVulkan::PlatformWindowsAvailable() ||
        !(ImGui::GetIO().BackendFlags & ImGuiBackendFlags_PlatformHasViewports))
        return false;
    auto &platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = CreateViewport;
    platform.Renderer_DestroyWindow = DestroyViewport;
    platform.Renderer_RenderWindow = RenderViewport;
    return true;
}

bool ReadVulkanEditorViewport(const ImGuiViewport &viewport, TextureData &pixels) {
    auto *state = static_cast<DetachedViewport *>(viewport.RendererUserData);
    return state && state->renderer &&
           state->renderer->ReadTextureForDiagnostics(state->renderer->RenderTargetTexture(state->target), pixels);
}
#else
bool InstallVulkanEditorViewports() { return false; }
bool ReadVulkanEditorViewport(const ImGuiViewport &, TextureData &) { return false; }
#endif
