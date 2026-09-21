#include "Application.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Camera.h"
#include "GravityField.h"
#include "PhysicsWorld.h"
#include "Renderer.h"
#include "Window.h"
#include "gl_core33.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr float kMaxFrameDeltaTime = 0.25f;  // clamp stalls before they ever reach the accumulator

// A conventional fixed physics timestep. Chosen (1/60s) because it's a
// common, well-tested rate for rigid-body simulation, not for any reason
// specific to this scene.
constexpr float kFixedTimestep = 1.0f / 60.0f;

// Caps how many fixed steps a single render frame will run to catch up.
// Combined with kMaxFrameDeltaTime above (which already bounds how much
// time a single frame can hand the accumulator), this is a second,
// explicit guard against a stall turning into an ever-growing backlog of
// physics steps ("spiral of death"): if the cap is hit, the remaining
// accumulated time is dropped rather than carried into future frames.
constexpr int kMaxPhysicsStepsPerFrame = 8;

const glm::vec3 kFloorHalfExtents(10.0f, 0.5f, 10.0f);
const glm::vec3 kFloorPosition(0.0f, -0.5f, 0.0f);
const glm::vec3 kFloorColor(0.35f, 0.35f, 0.4f);
constexpr float kFloorFriction = 0.8f;
constexpr float kFloorRestitution = 0.1f;

const glm::vec3 kCubeHalfExtents(0.5f, 0.5f, 0.5f);
const glm::vec3 kCubeInitialPosition(0.0f, 5.0f, 0.0f);
const glm::quat kCubeInitialRotation(1.0f, 0.0f, 0.0f, 0.0f);  // identity
const glm::vec3 kCubeColor(0.9f, 0.3f, 0.2f);
constexpr float kCubeMass = 2.0f;
constexpr float kCubeFriction = 0.5f;
constexpr float kCubeRestitution = 0.3f;
}  // namespace

int Application::Run() {
    Window window;
    if (!window.Init("Project Judas - Milestone 3", kWindowWidth, kWindowHeight)) {
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

    PhysicsWorld physicsWorld;
    if (!physicsWorld.Init()) {
        std::fprintf(stderr, "Physics initialization failed.\n");
        return 1;
    }

    GravityField gravityField;

    const BodyHandle floorBody = physicsWorld.CreateStaticBox(
        kFloorPosition, kFloorHalfExtents, kFloorFriction, kFloorRestitution);
    const BodyHandle cubeBody =
        physicsWorld.CreateDynamicBox(kCubeInitialPosition, kCubeHalfExtents, kCubeMass,
                                       kCubeFriction, kCubeRestitution);

    // Camera positioned to see the whole demo (floor + falling cube) at
    // launch without requiring the user to fly around first; free-flight
    // navigation from Milestone 2 is otherwise unchanged.
    Camera camera(glm::vec3(0.0f, 4.0f, 12.0f), /*yawDegrees=*/-90.0f, /*pitchDegrees=*/-20.0f);

    float physicsAccumulator = 0.0f;

    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();

    while (!window.ShouldClose()) {
        window.PollEvents();

        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        float frameDeltaTime =
            static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        if (frameDeltaTime > kMaxFrameDeltaTime) {
            frameDeltaTime = kMaxFrameDeltaTime;
        }

        camera.Update(window, frameDeltaTime);

        if (window.ConsumeResetRequest()) {
            physicsWorld.ResetBody(cubeBody, kCubeInitialPosition, kCubeInitialRotation);
            physicsAccumulator = 0.0f;
        }

        // Fixed-timestep physics: render-frame delta time only decides how
        // many fixed steps run this frame, never the size of a step itself.
        physicsAccumulator += frameDeltaTime;
        int stepsThisFrame = 0;
        while (physicsAccumulator >= kFixedTimestep && stepsThisFrame < kMaxPhysicsStepsPerFrame) {
            // Judas samples its own gravity field and hands the result to
            // the physics body itself; the physics middleware's global
            // gravity stays disabled (see PhysicsWorld::Init).
            const glm::vec3 cubePosition = physicsWorld.GetTransform(cubeBody).position;
            const glm::vec3 acceleration = gravityField.Sample(cubePosition);
            physicsWorld.ApplyLinearAcceleration(cubeBody, acceleration, kFixedTimestep);

            physicsWorld.Step(kFixedTimestep);

            physicsAccumulator -= kFixedTimestep;
            ++stepsThisFrame;
        }
        if (stepsThisFrame == kMaxPhysicsStepsPerFrame) {
            // Hit the catch-up cap: drop the backlog instead of letting it
            // compound into future frames.
            physicsAccumulator = 0.0f;
        }

        const int windowHeight = std::max(window.Height(), 1);
        const float aspectRatio =
            static_cast<float>(window.Width()) / static_cast<float>(windowHeight);

        const BodyTransform floorTransform = physicsWorld.GetTransform(floorBody);
        const BodyTransform cubeTransform = physicsWorld.GetTransform(cubeBody);

        renderer.BeginFrame(window.Width(), window.Height());
        renderer.SetCamera(camera.GetViewMatrix(), camera.GetProjectionMatrix(aspectRatio));
        renderer.DrawBox(floorTransform.position, floorTransform.rotation, kFloorHalfExtents,
                          kFloorColor);
        renderer.DrawBox(cubeTransform.position, cubeTransform.rotation, kCubeHalfExtents,
                          kCubeColor);
        renderer.EndFrame();

        window.SwapBuffers();
    }

    physicsWorld.DestroyBody(cubeBody);
    physicsWorld.DestroyBody(floorBody);
    physicsWorld.Shutdown();
    renderer.Shutdown();
    return 0;
}
