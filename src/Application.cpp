#include "Application.h"

#include <SDL2/SDL.h>

#include <cstdio>

#include "Box.h"
#include "Renderer.h"
#include "Window.h"
#include "gl_core33.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr float kMaxDeltaTime = 0.25f;  // clamp stalls (e.g. window drag) so the box can't teleport
}  // namespace

int Application::Run() {
    Window window;
    if (!window.Init("Project Judas - Milestone 1", kWindowWidth, kWindowHeight)) {
        std::fprintf(stderr, "Window initialization failed.\n");
        return 1;
    }

    if (!LoadGLFunctions()) {
        std::fprintf(stderr, "Failed to load required OpenGL functions.\n");
        return 1;
    }

    Renderer renderer;
    if (!renderer.Init()) {
        std::fprintf(stderr, "Renderer initialization failed.\n");
        return 1;
    }

    Box box(kWindowWidth / 2.0f, kWindowHeight / 2.0f);

    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();

    while (!window.ShouldClose()) {
        window.PollEvents();

        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        float deltaTime =
            static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        if (deltaTime > kMaxDeltaTime) {
            deltaTime = kMaxDeltaTime;
        }

        box.Update(window, deltaTime);

        renderer.BeginFrame(window.Width(), window.Height());
        box.Draw(renderer);
        renderer.EndFrame();

        window.SwapBuffers();
    }

    renderer.Shutdown();
    return 0;
}
