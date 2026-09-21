#include "Application.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "DynamicBody.h"
#include "FaithfulGravity.h"
#include "GravityField.h"
#include "GravityResolver.h"
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
// As of Milestone 7-B, a second, genuinely simultaneous FaithfulGravity
// environment also exists (see kPlatform* below and
// docs/ARCHITECTURE.md, "Gravity resolution") — GravityResolver, not this
// file, is what makes both coherent at once; Application still only ever
// constructs the concrete implementations and wires them up.
//
// The radius grew from Milestone 5/6's 8m to 20m in Milestone 7-A — still a
// small hand-authored test world, not a planet, but large enough to give a
// walking player (4 m/s) and several separated dynamic test objects room to
// exist without crowding the same few square meters. See
// docs/ARCHITECTURE.md, "Physics test world."
const glm::vec3 kSphereCenter(0.0f, 0.0f, 0.0f);
constexpr float kSphereRadius = 20.0f;
const glm::vec3 kSphereColor(0.3f, 0.45f, 0.35f);
constexpr float kSphereFriction = 0.8f;
constexpr float kSphereRestitution = 0.1f;

// Comparable in magnitude to FaithfulGravity's 9.81 m/s^2, per the brief —
// this demonstrates changing gravity DIRECTION, not different physics.
constexpr float kRadicalGravityMagnitude = 9.81f;

// Spawned above the sphere so the player visibly falls onto it. The
// demo/composition-root layer is allowed to know the sphere exists;
// PlayerController itself never does — see docs/ARCHITECTURE.md, "Spherical
// physical test world." Unchanged from Milestone 5-7-A — see
// kSphereGravityZoneCenter below for why Milestone 7-B's escape mechanism
// doesn't require touching this.
const glm::vec3 kPlayerSpawnPosition = kSphereCenter + glm::vec3(0.0f, kSphereRadius + 3.0f, 0.0f);
constexpr float kPlayerSpawnYawDegrees = -90.0f;
const glm::vec3 kPlayerColor(0.2f, 0.6f, 0.9f);

// --- Milestone 7-A: dynamic test objects ---
//
// A small, hand-authored collection of ordinary Jolt dynamic bodies —
// demo/composition-root data only, per docs/ARCHITECTURE.md, "Physics test
// world." None of this is visible to DynamicBody, PlayerController, or
// GravityField: those only ever see a position and an acceleration.
constexpr float kDynamicObjectFriction = 0.6f;
constexpr float kDynamicObjectRestitution = 0.15f;
constexpr float kCubeHalfExtent = 0.5f;
constexpr float kCubeMass = 5.0f;
constexpr float kSphereObjectRadius = 0.5f;
constexpr float kSphereObjectMass = 4.0f;
const glm::vec3 kCubeColor(0.85f, 0.35f, 0.2f);
const glm::vec3 kSphereObjectColor(0.9f, 0.8f, 0.2f);

// A point offset from the sphere's center along `direction` (not
// necessarily unit length — normalized here) at `heightAboveSurface`
// beyond the sphere's own radius. Purely a demo-authoring convenience for
// placing test objects at varied, legible locations around the sphere —
// engine code never does this kind of sphere-relative placement itself.
glm::vec3 PointAboveSphere(const glm::vec3& center, float radius, const glm::vec3& direction,
                            float heightAboveSurface) {
    return center + glm::normalize(direction) * (radius + heightAboveSurface);
}

// Four objects: two that begin slightly above the surface and fall onto
// it, two that begin already resting on it (per the brief's minimum
// arrangement). CubeA/SphereA sit close together near the player's own
// spawn point — reachable on foot immediately, close enough to collide
// with each other and to be pushed by the player. CubeB/SphereB sit at
// deliberately different locations around the sphere (near the "equator"
// and near the far pole) so their local gravity direction is visibly
// different from the player's and from each other's.
struct DynamicObjectSpawn {
    DynamicBody::Shape shape;
    glm::vec3 position;
    glm::vec3 halfExtentsOrRadius;  // x = radius for spheres
    glm::vec3 color;
    float mass;
};

const DynamicObjectSpawn kDynamicObjectSpawns[] = {
    {DynamicBody::Shape::Box,
     PointAboveSphere(kSphereCenter, kSphereRadius, glm::vec3(2.5f, kSphereRadius, 0.9f), 2.0f),
     glm::vec3(kCubeHalfExtent), kCubeColor, kCubeMass},
    {DynamicBody::Shape::Sphere,
     PointAboveSphere(kSphereCenter, kSphereRadius, glm::vec3(2.5f, kSphereRadius, 1.3f), 0.05f),
     glm::vec3(kSphereObjectRadius), kSphereObjectColor, kSphereObjectMass},
    {DynamicBody::Shape::Box,
     PointAboveSphere(kSphereCenter, kSphereRadius, glm::vec3(1.0f, 0.3f, 0.0f), 1.5f),
     glm::vec3(kCubeHalfExtent), kCubeColor, kCubeMass},
    {DynamicBody::Shape::Sphere,
     PointAboveSphere(kSphereCenter, kSphereRadius, glm::vec3(0.0f, -1.0f, 0.2f), 0.05f),
     glm::vec3(kSphereObjectRadius), kSphereObjectColor, kSphereObjectMass},
};

std::vector<DynamicBody> SpawnDynamicObjects(PhysicsWorld& physics) {
    std::vector<DynamicBody> bodies;
    bodies.reserve(std::size(kDynamicObjectSpawns));
    for (const DynamicObjectSpawn& spawn : kDynamicObjectSpawns) {
        DynamicBody::Visual visual;
        visual.shape = spawn.shape;
        visual.color = spawn.color;

        BodyHandle handle;
        if (spawn.shape == DynamicBody::Shape::Box) {
            visual.halfExtents = spawn.halfExtentsOrRadius;
            handle = physics.CreateDynamicBox(spawn.position, visual.halfExtents, spawn.mass,
                                               kDynamicObjectFriction, kDynamicObjectRestitution);
        } else {
            visual.radius = spawn.halfExtentsOrRadius.x;
            handle = physics.CreateDynamicSphere(spawn.position, visual.radius, spawn.mass,
                                                  kDynamicObjectFriction, kDynamicObjectRestitution);
        }
        bodies.emplace_back(handle, visual, spawn.position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }
    return bodies;
}

// --- Milestone 7-B: flat FaithfulGravity environment ---
//
// A second, simultaneously-active physical environment — demo/composition-
// root data only, exactly like the sphere above. Positioned near the
// sphere's "equator," along +Z specifically — not +X or +Y, which would
// put it near existing Milestone 7-A dynamic-object spawns (see
// kDynamicObjectSpawns above); +Z is otherwise unused — and deliberately
// not near a pole, so a departing player experiences a dramatically
// different local-up once FaithfulGravity takes over (see
// docs/ARCHITECTURE.md, "Demonstration environment"). At the player's
// default spawn orientation, reaching it is a plain strafe-right (`D`),
// not a turn: `right = cross(forward, up)` at spawn's yaw already points
// along +Z.
//
// The platform's own top surface sits below the world-space height its
// influence zone is centered on, so a player entering that zone from
// roughly the same height still has some room to visibly fall the last
// stretch under FaithfulGravity before landing — not snapped or teleported
// onto it (see docs/ARCHITECTURE.md, "Support remains physical").
const glm::vec3 kPlatformCenter(0.0f, -3.0f, 38.0f);
const glm::vec3 kPlatformHalfExtents(12.0f, 1.0f, 12.0f);
const glm::vec3 kPlatformColor(0.5f, 0.5f, 0.55f);
constexpr float kPlatformFriction = 0.8f;
constexpr float kPlatformRestitution = 0.1f;

// GravityResolver zone parameters (see docs/ARCHITECTURE.md, "Gravity
// resolution" and "Transition semantics" for the full derivation,
// including the two dead ends this replaced). Sized by simulating
// GravityResolver's exact algorithm against the player's real jump speed
// (5 m/s) and RadicalGravity's real 9.81 m/s^2 magnitude — not guessed:
//
// - A jump launched straight outward only ever reaches ~1.27m against
//   undiminished 9.81 m/s^2 deceleration (v^2 = u^2 - 2*a*d), so *some*
//   weakening of the sphere's own gravity within that reach is
//   mathematically unavoidable for escape to be possible via an ordinary
//   jump at all — no placement of the platform changes this, since
//   FaithfulGravity's direction is always exactly -Y and can never have a
//   component that assists outward (away-from-sphere-center) motion.
// - That weakening must NOT be uniform across the whole sphere (measuring
//   distance from the sphere's own center, kSphereCenter, does exactly
//   that): reproduced directly — an ordinary standing jump taken at
//   *spawn*, nowhere near the platform, escaped into the near-zero-gravity
//   region and never came back down. So kSphereGravityZoneCenter is NOT
//   kSphereCenter — it's a point 50m in -Z, far on the opposite side from
//   the platform. RadicalGravity's own direction/magnitude still always
//   comes from the sphere's true center (kSphereCenter) unchanged; only
//   this zone's WEIGHT is measured from the offset point, which is what
//   makes the falloff spatially localized to the departure/platform-facing
//   region instead of affecting the entire sphere: verified directly that
//   spawn, all four Milestone 7-A dynamic-object spawns, and the departure
//   region's own antipode all remain at exactly full (1.0) weight, while
//   only the vicinity of the departure point fades at all.
// - The platform zone's own outer radius must NOT reach the sphere's true
//   surface at the departure point, or the player's local-up starts
//   tilting away from the sphere's true surface normal before they even
//   leave the ground — which silently breaks the *grounded* check the
//   jump itself depends on (a jump only ever begins while supported; see
//   PlayerController::FixedUpdate), so the jump never fires at all.
//
// Verified (not just derived): jump speeds from 4.0 to 6.0 m/s all
// successfully land on the platform; a spawn-area jump, far from the
// departure point, now returns to the sphere exactly as it always has;
// gravity at the platform's own surface remains exactly pure FaithfulGravity
// — see docs/ARCHITECTURE.md, "Transition semantics," for the full numbers.
// Falloff centers/radii are demo composition data, exactly like the
// sphere/platform geometry above — GravityResolver itself has no idea any
// of this corresponds to "a sphere" and "a platform," only points and
// distances.
const glm::vec3 kSphereGravityZoneCenter = kSphereCenter + glm::vec3(0.0f, 0.0f, -50.0f);
constexpr float kSphereGravityZoneInnerRadius = 70.6f;
constexpr float kSphereGravityZoneOuterRadius = 71.9f;

const glm::vec3 kPlatformGravityZoneCenter = kPlatformCenter + glm::vec3(0.0f, kPlatformHalfExtents.y, 0.0f);
constexpr float kPlatformGravityZoneInnerRadius = 8.0f;
constexpr float kPlatformGravityZoneOuterRadius = 16.0f;
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
    if (!window.Init("Project Judas - Milestone 7-B", kWindowWidth, kWindowHeight, !isTestRun)) {
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

    // Application (the composition root) is the one place that knows which
    // concrete gravity implementations exist and where each is active.
    // Milestone 5 proved a consumer needs zero changes when the ONE active
    // implementation changes; Milestone 7-B proves the same is true when
    // MORE THAN ONE is simultaneously active and a consumer can move
    // between them — GravityResolver (itself a GravityField, see
    // src/GravityResolver.h and docs/ARCHITECTURE.md, "Gravity
    // resolution") is the only new piece, and it's the only thing bound to
    // `gravity` below. RadicalGravity/FaithfulGravity are constructed
    // exactly as before and never touch each other or know a resolver
    // exists.
    RadicalGravity radicalGravity(kSphereCenter, kRadicalGravityMagnitude);
    FaithfulGravity flatGravity;
    GravityResolver gravityResolver;
    gravityResolver.AddZone(radicalGravity, kSphereGravityZoneCenter, kSphereGravityZoneInnerRadius,
                             kSphereGravityZoneOuterRadius);
    gravityResolver.AddZone(flatGravity, kPlatformGravityZoneCenter, kPlatformGravityZoneInnerRadius,
                             kPlatformGravityZoneOuterRadius);
    GravityField& gravity = gravityResolver;

    const BodyHandle sphereBody = physicsWorld.CreateStaticSphere(
        kSphereCenter, kSphereRadius, kSphereFriction, kSphereRestitution);
    const BodyHandle platformBody = physicsWorld.CreateStaticBox(
        kPlatformCenter, kPlatformHalfExtents, kPlatformFriction, kPlatformRestitution);

    PlayerController player(kPlayerSpawnPosition, kPlayerSpawnYawDegrees);
    if (!player.Spawn(physicsWorld)) {
        std::fprintf(stderr, "Player spawn failed.\n");
        return 1;
    }

    // Milestone 7-A: several ordinary Jolt dynamic bodies sharing the same
    // GravityField the player uses — see docs/ARCHITECTURE.md, "Multiple
    // gravity consumers." Created here (composition root), not inside
    // PlayerController or DynamicBody, exactly like the static sphere
    // above.
    std::vector<DynamicBody> dynamicBodies = SpawnDynamicObjects(physicsWorld);

    // Shared between the normal interactive loop and the test harness, so
    // a screenshot taken by the harness shows exactly what the real game
    // would have rendered that frame. `presentationAlpha` blends the
    // player's rendered box between its previous and current fixed-step
    // pose — see docs/ARCHITECTURE.md, "Simulation/presentation boundary"
    // — the same value passed to PlayerController::GetViewMatrix so the
    // camera and the player box always move in visual lockstep. The sphere
    // is static and never interpolated; it has no "previous" pose to blend
    // from. Dynamic bodies use the exact same alpha via DynamicBody's own
    // GetPresentedPosition/Orientation.
    const auto drawScene = [&](Renderer& r, float presentationAlpha) {
        r.DrawSphere(kSphereCenter, kSphereRadius, kSphereColor);
        r.DrawBox(kPlatformCenter, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), kPlatformHalfExtents,
                  kPlatformColor);
        r.DrawBox(player.GetPresentedPosition(presentationAlpha),
                  player.GetPresentedOrientation(presentationAlpha), player.GetRenderHalfExtents(),
                  kPlayerColor);
        for (const DynamicBody& body : dynamicBodies) {
            const DynamicBody::Visual& visual = body.GetVisual();
            if (visual.shape == DynamicBody::Shape::Box) {
                r.DrawBox(body.GetPresentedPosition(presentationAlpha),
                          body.GetPresentedOrientation(presentationAlpha), visual.halfExtents,
                          visual.color);
            } else {
                r.DrawSphere(body.GetPresentedPosition(presentationAlpha), visual.radius,
                             visual.color);
            }
        }
    };

    int exitCode = 0;
    if (isTestRun) {
        exitCode = RunTestHarness(window, renderer, physicsWorld, player, gravity, dynamicBodies,
                                   drawScene, testScriptPath);
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
                for (DynamicBody& body : dynamicBodies) {
                    body.ResetToSpawn(physicsWorld);
                }
                physicsAccumulator = 0.0f;
            }

            // Fixed-timestep physics: render-frame delta time only decides
            // how many fixed steps run this frame, never the size of a
            // step itself.
            physicsAccumulator += frameDeltaTime;
            int stepsThisFrame = 0;
            while (physicsAccumulator >= SimulationTiming::kFixedTimestep &&
                   stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
                // Dynamic bodies sample gravity and hand it to Jolt BEFORE
                // Step() integrates it into their position — the same
                // "Judas samples, physics obeys" ordering the player uses,
                // just applied to a list. See docs/ARCHITECTURE.md,
                // "Multiple gravity consumers."
                PrepareDynamicBodiesForStep(dynamicBodies, gravity, physicsWorld,
                                             SimulationTiming::kFixedTimestep);
                physicsWorld.Step(SimulationTiming::kFixedTimestep);
                player.FixedUpdate(window, physicsWorld, gravity, SimulationTiming::kFixedTimestep);
                SyncDynamicBodiesFromPhysics(dynamicBodies, physicsWorld);

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
    for (const DynamicBody& body : dynamicBodies) {
        physicsWorld.DestroyBody(body.Handle());
    }
    physicsWorld.DestroyBody(sphereBody);
    physicsWorld.DestroyBody(platformBody);
    physicsWorld.Shutdown();
    renderer.Shutdown();
    return exitCode;
}
