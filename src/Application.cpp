#include "Application.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FaithfulGravity.h"
#include "GravityField.h"
#include "PhysicsWorld.h"
#include "PlayerController.h"
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

// Caps how many fixed steps a single render frame will run to catch up;
// see docs/ARCHITECTURE.md, "Simulation timing," for the full rationale
// (unchanged since Milestone 3).
constexpr int kMaxPhysicsStepsPerFrame = 8;

// The Milestone 3 floor + falling cube are kept: harmless, and useful as a
// second proof that PhysicsWorld/GravityField work generally, not only for
// the player.
const glm::vec3 kFloorHalfExtents(10.0f, 0.5f, 10.0f);
const glm::vec3 kFloorPosition(0.0f, -0.5f, 0.0f);
const glm::vec3 kFloorColor(0.35f, 0.35f, 0.4f);
constexpr float kFloorFriction = 0.8f;
constexpr float kFloorRestitution = 0.1f;

const glm::vec3 kCubeHalfExtents(0.5f, 0.5f, 0.5f);
const glm::vec3 kCubeInitialPosition(3.0f, 5.0f, -2.0f);  // off to the side of the player's spawn
const glm::quat kCubeInitialRotation(1.0f, 0.0f, 0.0f, 0.0f);  // identity
const glm::vec3 kCubeColor(0.9f, 0.3f, 0.2f);
constexpr float kCubeMass = 2.0f;
constexpr float kCubeFriction = 0.5f;
constexpr float kCubeRestitution = 0.3f;

// The floor's top surface is at y = 0 (kFloorPosition.y + kFloorHalfExtents.y).
const glm::vec3 kPlayerSpawnFeetPosition(0.0f, 0.0f, 3.0f);
constexpr float kPlayerSpawnYawDegrees = -90.0f;
const glm::vec3 kPlayerColor(0.2f, 0.6f, 0.9f);
}  // namespace

int Application::Run() {
    Window window;
    if (!window.Init("Project Judas - Milestone 4", kWindowWidth, kWindowHeight)) {
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

    // Application (the composition root) is the one place that knows the
    // concrete gravity implementation. Everything downstream — including
    // the player controller — talks to it only through the GravityField
    // interface. See GravityField.h and docs/ARCHITECTURE.md.
    FaithfulGravity faithfulGravity;
    GravityField& gravity = faithfulGravity;

    const BodyHandle floorBody = physicsWorld.CreateStaticBox(
        kFloorPosition, kFloorHalfExtents, kFloorFriction, kFloorRestitution);
    const BodyHandle cubeBody =
        physicsWorld.CreateDynamicBox(kCubeInitialPosition, kCubeHalfExtents, kCubeMass,
                                       kCubeFriction, kCubeRestitution);

    PlayerController player(kPlayerSpawnFeetPosition, kPlayerSpawnYawDegrees);
    if (!player.Spawn(physicsWorld, gravity)) {
        std::fprintf(stderr, "Player spawn failed.\n");
        return 1;
    }

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

        // Mouse look and jump-key latching happen every render frame,
        // independent of how many fixed physics steps run this frame.
        player.UpdateFrameInput(window);

        if (window.ConsumeResetRequest()) {
            physicsWorld.ResetBody(cubeBody, kCubeInitialPosition, kCubeInitialRotation);
            player.Reset(physicsWorld);
            physicsAccumulator = 0.0f;
        }

        // Fixed-timestep physics: render-frame delta time only decides how
        // many fixed steps run this frame, never the size of a step itself.
        physicsAccumulator += frameDeltaTime;
        int stepsThisFrame = 0;
        while (physicsAccumulator >= kFixedTimestep && stepsThisFrame < kMaxPhysicsStepsPerFrame) {
            // Judas samples its own gravity field and hands the result to
            // each physics body itself; the physics middleware's global
            // gravity stays disabled (see PhysicsWorld::Init).
            const glm::vec3 cubePosition = physicsWorld.GetTransform(cubeBody).position;
            physicsWorld.ApplyLinearAcceleration(cubeBody, gravity.Sample(cubePosition),
                                                  kFixedTimestep);
            physicsWorld.Step(kFixedTimestep);

            player.FixedUpdate(window, physicsWorld, gravity, kFixedTimestep);

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
        renderer.SetCamera(player.GetViewMatrix(physicsWorld), player.GetProjectionMatrix(aspectRatio));
        renderer.DrawBox(floorTransform.position, floorTransform.rotation, kFloorHalfExtents,
                          kFloorColor);
        renderer.DrawBox(cubeTransform.position, cubeTransform.rotation, kCubeHalfExtents,
                          kCubeColor);
        renderer.DrawBox(player.GetRenderCenter(physicsWorld), glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                          player.GetRenderHalfExtents(), kPlayerColor);
        renderer.EndFrame();

        window.SwapBuffers();
    }

    player.Destroy(physicsWorld);
    physicsWorld.DestroyBody(cubeBody);
    physicsWorld.DestroyBody(floorBody);
    physicsWorld.Shutdown();
    renderer.Shutdown();
    return 0;
}
