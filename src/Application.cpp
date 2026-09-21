#include "Application.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GravityField.h"
#include "PhysicsWorld.h"
#include "PlayerController.h"
#include "RadicalGravity.h"
#include "Renderer.h"
#include "SimulationTiming.h"
#include "TestHarness.h"
#include "Window.h"
#include "gl_core33.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;

// Milestone 5's demo: a spherical world with radial gravity, replacing
// Milestone 3/4's flat floor + cube (recoverable via the milestone-4 tag).
// A flat-gravity floor and a radial-gravity sphere active in the same scene
// would be physically incoherent — the demo picks ONE active GravityField
// (see docs/ARCHITECTURE.md, "Gravity implementations"), and this milestone's
// point is specifically to exercise the non-flat one.
const glm::vec3 kSphereCenter(0.0f, 0.0f, 0.0f);
constexpr float kSphereRadius = 8.0f;
const glm::vec3 kSphereColor(0.3f, 0.45f, 0.35f);
constexpr float kSphereFriction = 0.8f;
constexpr float kSphereRestitution = 0.1f;

// Comparable in magnitude to FaithfulGravity's 9.81 m/s^2, per the brief —
// this demonstrates changing gravity DIRECTION, not different physics.
constexpr float kRadicalGravityMagnitude = 9.81f;

// Spawned above the sphere so the player visibly falls onto it. The
// demo/composition-root layer is allowed to know the sphere exists;
// PlayerController itself never does — see docs/ARCHITECTURE.md, "Spherical
// physical test world."
const glm::vec3 kPlayerSpawnPosition = kSphereCenter + glm::vec3(0.0f, kSphereRadius + 3.0f, 0.0f);
constexpr float kPlayerSpawnYawDegrees = -90.0f;
const glm::vec3 kPlayerColor(0.2f, 0.6f, 0.9f);
}  // namespace

int Application::Run() {
    // Opt-in developer/automation tooling (see docs/ARCHITECTURE.md,
    // "Automated testing," and src/TestHarness.h): when set, this run is a
    // scripted, headless verification pass rather than the interactive
    // game. Checked before Window::Init so the window can be created
    // hidden — it's a real GL context either way, just not shown on screen.
    const char* testScriptPath = std::getenv("JUDAS_TEST_SCRIPT");
    const bool isTestRun = testScriptPath != nullptr;

    Window window;
    if (!window.Init("Project Judas - Milestone 6", kWindowWidth, kWindowHeight, !isTestRun)) {
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
    // concrete gravity implementation. Everything downstream — the player
    // controller included — talks to it only through the GravityField
    // interface. Swapping FaithfulGravity for RadicalGravity here is the
    // entire change needed to move from Milestone 3/4's flat world to this
    // milestone's spherical one; nothing else in the engine knows which
    // implementation is active.
    RadicalGravity radicalGravity(kSphereCenter, kRadicalGravityMagnitude);
    GravityField& gravity = radicalGravity;

    const BodyHandle sphereBody = physicsWorld.CreateStaticSphere(
        kSphereCenter, kSphereRadius, kSphereFriction, kSphereRestitution);

    PlayerController player(kPlayerSpawnPosition, kPlayerSpawnYawDegrees);
    if (!player.Spawn(physicsWorld)) {
        std::fprintf(stderr, "Player spawn failed.\n");
        return 1;
    }

    // Shared between the normal interactive loop and the test harness, so
    // a screenshot taken by the harness shows exactly what the real game
    // would have rendered that frame. `presentationAlpha` blends the
    // player's rendered box between its previous and current fixed-step
    // pose — see docs/ARCHITECTURE.md, "Simulation/presentation boundary"
    // — the same value passed to PlayerController::GetViewMatrix so the
    // camera and the player box always move in visual lockstep. The sphere
    // is static and never interpolated; it has no "previous" pose to blend
    // from.
    const auto drawScene = [&](Renderer& r, float presentationAlpha) {
        r.DrawSphere(kSphereCenter, kSphereRadius, kSphereColor);
        r.DrawBox(player.GetPresentedPosition(presentationAlpha),
                  player.GetPresentedOrientation(presentationAlpha), player.GetRenderHalfExtents(),
                  kPlayerColor);
    };

    int exitCode = 0;
    if (isTestRun) {
        exitCode =
            RunTestHarness(window, renderer, physicsWorld, player, gravity, drawScene, testScriptPath);
    } else {
        float physicsAccumulator = 0.0f;

        const Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64 previousCounter = SDL_GetPerformanceCounter();

        while (!window.ShouldClose()) {
            window.PollEvents();

            const Uint64 currentCounter = SDL_GetPerformanceCounter();
            float frameDeltaTime = static_cast<float>(currentCounter - previousCounter) /
                                    static_cast<float>(frequency);
            previousCounter = currentCounter;
            if (frameDeltaTime > SimulationTiming::kMaxFrameDeltaTime) {
                frameDeltaTime = SimulationTiming::kMaxFrameDeltaTime;
            }

            // Mouse look and jump-key latching happen every render frame,
            // independent of how many fixed physics steps run this frame.
            player.UpdateFrameInput(window);

            if (window.ConsumeResetRequest()) {
                player.Reset();
                physicsAccumulator = 0.0f;
            }

            // Fixed-timestep physics: render-frame delta time only decides
            // how many fixed steps run this frame, never the size of a
            // step itself.
            physicsAccumulator += frameDeltaTime;
            int stepsThisFrame = 0;
            while (physicsAccumulator >= SimulationTiming::kFixedTimestep &&
                   stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
                physicsWorld.Step(SimulationTiming::kFixedTimestep);
                player.FixedUpdate(window, physicsWorld, gravity, SimulationTiming::kFixedTimestep);

                physicsAccumulator -= SimulationTiming::kFixedTimestep;
                ++stepsThisFrame;
            }
            if (stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame) {
                // Hit the catch-up cap: drop the backlog instead of
                // letting it compound into future frames.
                physicsAccumulator = 0.0f;
            }

            // How far real time has progressed into an as-yet-unsimulated
            // fixed step, as a fraction of one step — the presentation
            // interpolation factor. See docs/ARCHITECTURE.md, "Diagnosis"
            // and "Simulation/presentation boundary," for why this exists:
            // render-frame timing doesn't divide evenly into the fixed
            // 1/60s simulation rate, so presenting the latest fixed-step
            // state directly (this value implicitly always 1.0) produced
            // visible judder — some frames repeating a state, others
            // jumping by two steps' worth of motion — even though the
            // underlying simulation itself is smooth.
            const float presentationAlpha = physicsAccumulator / SimulationTiming::kFixedTimestep;

            const int windowHeight = std::max(window.Height(), 1);
            const float aspectRatio =
                static_cast<float>(window.Width()) / static_cast<float>(windowHeight);

            renderer.BeginFrame(window.Width(), window.Height());
            renderer.SetCamera(player.GetViewMatrix(presentationAlpha),
                                player.GetProjectionMatrix(aspectRatio));
            drawScene(renderer, presentationAlpha);
            renderer.EndFrame();

            window.SwapBuffers();
        }
    }

    player.Destroy(physicsWorld);
    physicsWorld.DestroyBody(sphereBody);
    physicsWorld.Shutdown();
    renderer.Shutdown();
    return exitCode;
}
