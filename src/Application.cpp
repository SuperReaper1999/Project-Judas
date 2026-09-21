#include "Application.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>

#include <glm/glm.hpp>

#include "Camera.h"
#include "Renderer.h"
#include "Window.h"
#include "gl_core33.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr float kMaxDeltaTime = 0.25f;  // clamp stalls (e.g. window drag) so movement can't jump

struct DemoCube {
    glm::vec3 position;
    glm::vec3 color;
};

// Three cubes at visibly different positions/depths, purely to demonstrate
// perspective and depth testing. Not a scene format of any kind.
const std::array<DemoCube, 3> kDemoCubes = {{
    {glm::vec3(0.0f, 0.0f, -3.0f), glm::vec3(0.9f, 0.3f, 0.2f)},   // near, centered
    {glm::vec3(2.5f, 0.5f, -8.0f), glm::vec3(0.2f, 0.7f, 0.9f)},   // farther away, offset right
    {glm::vec3(-2.0f, -1.0f, -5.5f), glm::vec3(0.4f, 0.85f, 0.3f)},  // between the two, offset left/down
}};
}  // namespace

int Application::Run() {
    Window window;
    if (!window.Init("Project Judas - Milestone 2", kWindowWidth, kWindowHeight)) {
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

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f), /*yawDegrees=*/-90.0f, /*pitchDegrees=*/0.0f);

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

        camera.Update(window, deltaTime);

        const int windowHeight = std::max(window.Height(), 1);
        const float aspectRatio =
            static_cast<float>(window.Width()) / static_cast<float>(windowHeight);

        renderer.BeginFrame(window.Width(), window.Height());
        renderer.SetCamera(camera.GetViewMatrix(), camera.GetProjectionMatrix(aspectRatio));
        for (const DemoCube& cube : kDemoCubes) {
            renderer.DrawCube(cube.position, cube.color);
        }
        renderer.EndFrame();

        window.SwapBuffers();
    }

    renderer.Shutdown();
    return 0;
}
