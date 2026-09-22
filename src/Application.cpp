#include "Application.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "DynamicBody.h"
#include "FaithfulGravity.h"
#include "FlyingPrimitiveControl.h"
#include "GravityContextMap.h"
#include "GravityField.h"
#include "ModelLoader.h"
#include "PhysicsWorld.h"
#include "PlayerController.h"
#include "RadicalGravity.h"
#include "Renderer.h"
#include "SimulationTiming.h"
#include "SphericalVolume.h"
#include "TestHarness.h"
#include "TextureLoader.h"
#include "Window.h"
#include "gl_core33.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;

// Milestone 7-Final's demo: two independent spherical worlds, each with its
// own RadicalGravity source, connected by one flat static plank — replacing
// Milestone 7-B's single sphere + flat FaithfulGravity platform (recoverable
// via the milestone-7b tag; see docs/ARCHITECTURE.md law #9, demo content is
// replaced, not accumulated). This is the final proof that GravityResolver
// (see src/GravityResolver.h, law #12) handles more than one *simultaneously
// active radial source*, not just one radial + one uniform field — neither
// planet is privileged, both are ordinary RadicalGravity instances wired up
// identically. No new gravity implementation, no new zone shape: exactly the
// same two systems Milestone 7-B built, used twice.
const glm::vec3 kPlanetACenter(0.0f, 0.0f, 0.0f);
constexpr float kPlanetARadius = 20.0f;
const glm::vec3 kPlanetAColor(0.3f, 0.45f, 0.35f);

// Separated from Planet A by 55m center-to-center, i.e. a genuine 15m
// surface-to-surface gap (2*20m radius + 15m) — the two worlds are visibly,
// physically distinct bodies, not a single world wearing two names. Placed
// along +Z, mirroring Milestone 7-B's departure-direction convention.
const glm::vec3 kPlanetBCenter(0.0f, 0.0f, 55.0f);
constexpr float kPlanetBRadius = 20.0f;
const glm::vec3 kPlanetBColor(0.35f, 0.3f, 0.45f);

constexpr float kPlanetFriction = 0.8f;
constexpr float kPlanetRestitution = 0.1f;

// Both planets share the same magnitude — this demonstrates two
// simultaneously active radial *sources*, not different physics per world.
constexpr float kRadicalGravityMagnitude = 9.81f;

// Spawned above Planet A, facing +Z (yaw 180 degrees puts "forward" toward
// +Z — see PlayerController::GetViewMatrix's front-vector convention) so a
// plain W-hold walks the player toward the plank with no turn needed.
const glm::vec3 kPlayerSpawnPosition =
    kPlanetACenter + glm::vec3(0.0f, kPlanetARadius + 3.0f, 0.0f);
constexpr float kPlayerSpawnYawDegrees = 180.0f;
const glm::vec3 kPlayerColor(0.2f, 0.6f, 0.9f);

// --- Milestone 7-A/7-Final: dynamic test objects ---
//
// Demo/composition-root data only, exactly as in Milestone 7-A — none of
// this is visible to DynamicBody, PlayerController, or GravityField, which
// only ever see a position and an acceleration. This milestone spreads
// objects across all three useful regions (Planet A, the plank, Planet B)
// specifically to prove the gravity-resolution mechanism is not
// player-specific or region-specific — see docs/ARCHITECTURE.md.
constexpr float kDynamicObjectFriction = 0.6f;
constexpr float kDynamicObjectRestitution = 0.15f;
constexpr float kCubeHalfExtent = 0.5f;
constexpr float kCubeMass = 5.0f;
constexpr float kSphereObjectRadius = 0.5f;
constexpr float kSphereObjectMass = 4.0f;
const glm::vec3 kCubeColor(0.85f, 0.35f, 0.2f);
const glm::vec3 kSphereObjectColor(0.9f, 0.8f, 0.2f);

// A point offset from a sphere's center along `direction` (not necessarily
// unit length — normalized here) at `heightAboveSurface` beyond that
// sphere's own radius. Purely a demo-authoring convenience for placing test
// objects at varied, legible locations around either planet — engine code
// never does this kind of sphere-relative placement itself. Unchanged from
// Milestone 7-A, now reused for two different sphere centers.
glm::vec3 PointAboveSphere(const glm::vec3& center, float radius, const glm::vec3& direction,
                            float heightAboveSurface) {
    return center + glm::normalize(direction) * (radius + heightAboveSurface);
}

// Milestone 10: the shortest-arc rotation that takes world +Y to an
// arbitrary target direction — used only to ORIENT demo geometry (steps,
// a ramp) so their own local "up" matches wherever local gravity actually
// points at the spot they're placed, e.g. a bearing on the curved sphere
// far from its pole, where that's nothing like world +Y. Demo-authoring
// convenience only, deliberately re-derived here rather than shared with
// PlayerController.cpp's own identical-shaped RotationBetweenUnitVectors —
// that one is Judas's own runtime gravity-orientation logic; this one is
// composition-root placement math for hand-authored static geometry, the
// same category PointAboveSphere above already is.
glm::quat RotationAligningUpTo(const glm::vec3& targetUp) {
    const glm::vec3 from(0.0f, 1.0f, 0.0f);
    const glm::vec3 to = glm::normalize(targetUp);
    const float d = glm::clamp(glm::dot(from, to), -1.0f, 1.0f);
    if (d > 0.9999f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (d < -0.9999f) {
        const glm::vec3 axis = std::abs(from.x) < 0.9f ? glm::cross(from, glm::vec3(1.0f, 0.0f, 0.0f))
                                                        : glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::normalize(glm::cross(from, to));
    return glm::angleAxis(std::acos(d), axis);
}

struct DynamicObjectSpawn {
    DynamicBody::Shape shape;
    glm::vec3 position;
    glm::vec3 halfExtentsOrRadius;  // x = radius for spheres
    glm::vec3 color;
    float mass;
};

// Six objects, two per region (Planet A, the plank, Planet B) — the minimum
// arrangement that actually exercises all three gravity contexts
// simultaneously, per the brief's "prove this architecture is not secretly
// player-specific." Planet A/B objects are placed away from the plank
// direction (their own local gravity stays undiminished full strength — see
// docs/ARCHITECTURE.md's SAMPLE_GRAVITY verification), so they fall and
// settle independent of anything happening on the bridge. Plank objects are
// placed directly above its surface at different points along its length —
// their resting positions are never scripted or aligned to gravity by hand;
// they fall and land exactly like every other body here.
const DynamicObjectSpawn kDynamicObjectSpawns[] = {
    // Planet A
    {DynamicBody::Shape::Box,
     PointAboveSphere(kPlanetACenter, kPlanetARadius, glm::vec3(1.0f, 1.0f, -0.5f), 2.0f),
     glm::vec3(kCubeHalfExtent), kCubeColor, kCubeMass},
    {DynamicBody::Shape::Sphere,
     PointAboveSphere(kPlanetACenter, kPlanetARadius, glm::vec3(0.8f, 0.3f, -0.9f), 0.05f),
     glm::vec3(kSphereObjectRadius), kSphereObjectColor, kSphereObjectMass},
    // The plank
    {DynamicBody::Shape::Box, glm::vec3(-3.0f, 21.0f, 18.0f), glm::vec3(kCubeHalfExtent),
     kCubeColor, kCubeMass},
    {DynamicBody::Shape::Sphere, glm::vec3(3.0f, 19.0f, 40.0f), glm::vec3(kSphereObjectRadius),
     kSphereObjectColor, kSphereObjectMass},
    // Planet B
    {DynamicBody::Shape::Box,
     PointAboveSphere(kPlanetBCenter, kPlanetBRadius, glm::vec3(1.0f, 1.0f, 0.5f), 2.0f),
     glm::vec3(kCubeHalfExtent), kCubeColor, kCubeMass},
    {DynamicBody::Shape::Sphere,
     PointAboveSphere(kPlanetBCenter, kPlanetBRadius, glm::vec3(0.8f, 0.3f, 0.9f), 0.05f),
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

// --- Milestone 7-Final: the connecting plank ---
//
// A single flat static box bridging the gap between Planet A's and Planet
// B's near-facing surfaces. Its own physical surface normal is fixed
// (+Y, an ordinary flat box) for its entire length — deliberately NOT
// reoriented or segmented to track gravity — because "the plank's fixed
// surface normal and changing gravity direction remain separate concepts"
// is an explicit requirement of this milestone (see docs/ARCHITECTURE.md,
// "Support"): support comes from PhysicsWorld::SweepPlayerShape's real
// collision query against this one static shape, never from which gravity
// zone currently has the most weight at the player's position.
//
// Placed close enough to each planet's own surface (a real but modest
// ~1m gap/step at each end, well inside normal walk/jump range) that the
// player is never required to leap through open space to reach it — see
// "Physics ownership" below for why that no longer matters as much as it
// once did anyway.
//
// This plank has now been through TWO failed gravity-model attempts before
// this one — see docs/ARCHITECTURE.md, "Gravity context ownership," for
// the full retrospective on both. First, GravityResolver (Milestone 7-B's
// falloff-weighted blend of every source within reach): a consumer beside
// the plank, off its own travel path, felt an artificial sideways pull
// toward the line connecting the two planets, because both planets'
// RadicalGravity were sampled and blended from anywhere within their
// (necessarily generous) falloff radii. Second, this file's own prior
// GravityContextMap design: it fixed the OFF-path contamination (bounded
// regions, never sampling a field outside its own domain) but still
// blended the two planets' raw radial math together INSIDE an explicit
// transition — so standing ON the plank's own surface, near its edges,
// still tilted ~18-20 degrees toward the planets' shared axis. Both
// attempts shared the same deeper mistake: treating the plank as empty
// space where two planetary fields happen to overlap, rather than giving
// it its own coherent local gravity.
//
// This build does the latter: the plank is its own plain gravity-context
// region, using the EXISTING, unmodified FaithfulGravity — its hardcoded
// (0,-9.81,0) already matches this axis-aligned, flat-topped box's own
// surface normal exactly, with no new gravity implementation needed.
// Gravity is now IDENTICAL everywhere on the plank's surface: not
// approximately vertical, not tilted less than before -- exactly
// (0,-9.81,0), full stop, the same way a real flat room's gravity doesn't
// care how far you are from the wall. Crossing from a planet's own region
// onto the plank's is a literal, instantaneous change in the sampled
// value between one fixed step and the next; PlayerController's existing
// rate-capped reorientation and continuous airborne velocity integration
// (architectural law #14) already turn that into a smooth reorientation
// over about a second, exactly as they always have -- no blending was
// ever needed to keep the takeover smooth.
const glm::vec3 kPlankCenter(0.0f, 17.0f, 27.5f);
const glm::vec3 kPlankHalfExtents(6.0f, 1.0f, 20.0f);
const glm::vec3 kPlankColor(0.55f, 0.5f, 0.4f);
constexpr float kPlankFriction = 0.8f;
constexpr float kPlankRestitution = 0.1f;

// The plank's own gravity-context region: a BoxVolume padded a little
// beyond its own collision box in every direction (vs kPlankHalfExtents),
// so positions just above, beside, or past either end of the plank's
// physical surface -- e.g. mid-jump -- are still claimed by IT rather than
// falling through to a planet's region or into unclaimed (zero-gravity)
// space. This region is registered BEFORE either planet's (see Run()) so
// it wins deliberately within its own footprint even where it geometrically
// overlaps a planet's own spherical region near the plank's ends --
// GravityContextMap resolves an overlap by registration order, and this is
// that mechanism used on purpose: the more specific region (the plank)
// takes priority over the more general one (an entire planet) wherever
// both could apply.
const glm::vec3 kPlankGravityRegionHalfExtents(8.0f, 7.0f, 22.0f);

// Each planet's own plain gravity-context region: a SphericalVolume
// comfortably covering its own surface plus a margin for jumping a little
// above it (26m vs a 20m planet radius), but well short of the 27.5m
// halfway point to the other planet's center -- so the two planets' own
// regions never overlap EACH OTHER (registration order only needs to
// settle the plank-vs-planet overlap above; two same-kind regions
// overlapping would have no principled winner, so this composition root
// keeps that from happening at all).
constexpr float kPlanetGravityRegionRadius = 26.0f;

// --- Milestone 8: the flying primitive ---
//
// One more ordinary DynamicBody (see docs/ARCHITECTURE.md, "Milestone 8")
// — created here exactly like kDynamicObjectSpawns' six objects above, then
// appended to the same dynamicBodies list so it gets identical gravity,
// collision, presentation interpolation, and R-triggered reset for free.
// What makes it "the flying primitive" is entirely external to DynamicBody
// itself: a separate FlyingPrimitiveControl (src/FlyingPrimitiveControl.h)
// holding just its handle and a `controlled` bool, toggled by F.
//
// Spawned resting on the PLANK, roughly at its midpoint, rather than
// directly on either planet's own curved surface. Deliberate, evidence-
// based placement, not aesthetic, for two reasons found while testing this
// milestone: (1) box-vs-sphere contact (src/Contacts.cpp's SphereVsBox)
// only ever produces a single contact point — the same "a box needs more
// than one contact point to rest flat without rocking" limitation already
// documented for box-vs-box before BoxVsBoxManifold existed
// (docs/ARCHITECTURE.md, law #15's bug note) — so a flat box this size
// resting on a curved sphere via one contact point would be genuinely
// unstable; (2) placing it too close to either planet's own end of the
// plank let the primitive's far corner geometrically overlap that planet's
// collision SPHERE as well as the plank underneath it, producing an extra,
// unwanted contact that pushed it noticeably higher than the plank's own
// surface — reproduced directly and confirmed by isolating the plank
// contact alone (settles at exactly plank-top + half-height, ~18.25) versus
// the full scene (settled almost 0.35m higher). The plank's own midpoint is
// far enough from both planets' spheres that neither ever engages. The
// plank is itself a static box, so resting on it gets the real multi-point
// BoxVsBoxManifold and settles flat, the same mechanism every dynamic cube
// in kDynamicObjectSpawns already relies on when it lands on the plank.
// Still an easy walk from the player's own spawn point, continuing straight
// down the same traversal path M7-Final's own validation already walks
// (spawn -> down the sphere -> onto the plank -> along it).
constexpr float kFlyingPrimitiveMass = 80.0f;
constexpr float kFlyingPrimitiveFriction = 0.8f;
constexpr float kFlyingPrimitiveRestitution = 0.1f;
const glm::vec3 kFlyingPrimitiveHalfExtents(2.0f, 0.25f, 3.0f);
const glm::vec3 kFlyingPrimitiveColor(0.75f, 0.75f, 0.8f);
const glm::vec3 kFlyingPrimitiveSpawnPosition(0.0f, 19.0f, 24.0f);

// --- Milestone 9: the beacon (model/texture/lighting demonstration) ---
//
// A single imported static model (assets/models/beacon.obj, a hand-authored
// low-poly pyramid — see that file's own header comment for full
// provenance) with a real texture (assets/textures/beacon.png). Purely
// decorative: NOT a physics body, not attached to anything gameplay-wise —
// see docs/ARCHITECTURE.md, "Milestone 9," for why entangling it with an
// existing physics object was deliberately avoided ("models are visual, not
// automatically physical"). Placed a short walk from the player's own
// spawn point on Planet A, at the same pole direction so it rests visibly
// upright without needing its own orientation logic, purely for immediate
// visibility — this is demonstration placement, not physics.
const glm::vec3 kBeaconPosition = kPlanetACenter + glm::vec3(-4.0f, kPlanetARadius, 3.0f);
const char* const kBeaconModelPath = "assets/models/beacon.obj";
const char* const kBeaconTexturePath = "assets/textures/beacon.png";

// One directional light plus a small constant ambient term — see
// docs/ARCHITECTURE.md, "Milestone 9, Lighting." A plain, fixed world-space
// direction chosen only to rake visibly across both the beacon and the
// planets/plank from a reasonable angle; it has NO relationship to gravity,
// local up, or any other Judas concept (law: no global up anywhere in this
// engine, lighting included).
const glm::vec3 kLightDirection = glm::normalize(glm::vec3(0.4f, 0.7f, 0.35f));
const glm::vec3 kLightColor(1.0f, 0.98f, 0.92f);
const glm::vec3 kAmbientColor(0.16f, 0.17f, 0.19f);

// --- Milestone 10: step/slope test geometry ---
//
// A small staircase and one ramp, both placed on Planet A a short walk
// from spawn but at a bearing measurably off its exact pole — see
// docs/ARCHITECTURE.md, "Milestone 10" — deliberately NOT at the pole
// itself (where local radial "up" exactly coincides with world +Y and
// would silently hide any accidental world-Y assumption in the step-climb
// code), and deliberately not placed FAR off the pole either: an easy,
// short, reliably-aimable walk from spawn matters for a human tester
// actually finding and trying these — see "Milestone 10" for the
// standalone unit tests (tests/StepClimbTests.cpp) that separately verify
// the step-climb primitives themselves under a much more extreme rotation
// than this demo placement bothers with. Both pieces use
// RotationAligningUpTo so each one's own local "up" matches the actual
// radial direction at its bearing, the same way a real object resting on
// a sphere would need to.
//
// The staircase: kStepCount steps, each riser comfortably under
// PlayerController's own kMaxStepHeight (0.55m) so it's climbable purely
// by walking into it, no jump — the main point of this milestone. Each
// step is a full "pillar" from the planet's surface up to its own top
// (rather than separate floating risers+treads), so there's no gap
// between consecutive steps for a sweep to fall through.
constexpr float kStepRise = 0.3f;
constexpr float kStepRun = 0.7f;
constexpr float kStepHalfWidth = 1.0f;
constexpr int kStepCount = 4;
const glm::vec3 kStaircaseColor(0.5f, 0.45f, 0.55f);
// Zero X component deliberately: this sits exactly along the player's own
// default spawn-facing direction (yaw 180 -> +Z, see kPlayerSpawnYawDegrees)
// tilted toward Planet A's own local "forward," so a plain W-hold from
// spawn walks straight into it with no turn needed — reachable by
// accident on the way to the plank, not just by deliberate aiming.
const glm::vec3 kStaircaseBearing = glm::normalize(glm::vec3(0.0f, 0.95f, 0.3f));
constexpr float kStaircaseFriction = 0.8f;
constexpr float kStaircaseRestitution = 0.0f;

// The ramp: one long, shallow-angled box (kRampTiltDegrees from the local
// surface normal — comfortably under the ~50-degree walkable-slope limit,
// PlayerController.cpp's own kMinGroundDot), tilted about its own local
// tangent "right" axis so it reads as an ordinary incline rather than a
// step at all — proving slopes remain collision-derived and gravity-
// relative under an arbitrary local frame, not merely "still work on the
// one sphere direction already exercised by the main traversal path."
const glm::vec3 kRampHalfExtents(1.5f, 0.15f, 2.5f);
const glm::vec3 kRampColor(0.45f, 0.5f, 0.55f);
const glm::vec3 kRampBearing = glm::normalize(glm::vec3(-0.35f, 0.9f, 0.25f));
constexpr float kRampTiltDegrees = 25.0f;
constexpr float kRampFriction = 0.8f;
constexpr float kRampRestitution = 0.0f;

// A static body whose position/rotation/half-extents/color are already
// fully known at spawn time (nothing about it ever moves) — the same
// reasoning the planets/plank already rely on to draw themselves from
// their own authored constants rather than querying
// PhysicsWorld::GetTransform every frame for a transform that can never
// change. Just enough to draw and, at shutdown, destroy each one.
struct StaticTestBody {
    BodyHandle handle;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 halfExtents;
    glm::vec3 color;
};

// Builds the staircase's `kStepCount` static boxes and the one ramp box.
// Demo-authoring geometry only — none of this is visible to
// PlayerController, which only ever sees the resulting collision shapes
// through ordinary SweepPlayerShape queries, exactly like every other
// piece of static world geometry.
std::vector<StaticTestBody> SpawnStepTestGeometry(PhysicsWorld& physics) {
    std::vector<StaticTestBody> bodies;

    const glm::vec3 baseSurfacePoint =
        PointAboveSphere(kPlanetACenter, kPlanetARadius, kStaircaseBearing, 0.0f);
    const glm::quat staircaseRotation = RotationAligningUpTo(kStaircaseBearing);
    const glm::vec3 climbDirection = glm::normalize(staircaseRotation * glm::vec3(0.0f, 0.0f, 1.0f));

    for (int i = 0; i < kStepCount; ++i) {
        const float topHeight = static_cast<float>(i + 1) * kStepRise;
        const glm::vec3 center = baseSurfacePoint + kStaircaseBearing * (topHeight * 0.5f) +
                                  climbDirection * (static_cast<float>(i) * kStepRun + kStepRun * 0.5f);
        const glm::vec3 halfExtents(kStepHalfWidth, topHeight * 0.5f, kStepRun * 0.5f);
        const BodyHandle handle = physics.CreateStaticBox(center, staircaseRotation, halfExtents,
                                                            kStaircaseFriction, kStaircaseRestitution);
        bodies.push_back({handle, center, staircaseRotation, halfExtents, kStaircaseColor});
    }

    const glm::quat rampBaseRotation = RotationAligningUpTo(kRampBearing);
    const glm::vec3 rampRight = rampBaseRotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::quat rampRotation =
        glm::angleAxis(glm::radians(kRampTiltDegrees), rampRight) * rampBaseRotation;
    const glm::vec3 rampCenter =
        PointAboveSphere(kPlanetACenter, kPlanetARadius, kRampBearing, kRampHalfExtents.y);
    const BodyHandle rampHandle = physics.CreateStaticBox(rampCenter, rampRotation, kRampHalfExtents,
                                                            kRampFriction, kRampRestitution);
    bodies.push_back({rampHandle, rampCenter, rampRotation, kRampHalfExtents, kRampColor});

    return bodies;
}
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
    if (!window.Init("Project Judas - Milestone 9", kWindowWidth, kWindowHeight,
                      !isTestRun)) {
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

    // Milestone 9: load the demo's one imported model/texture pair before
    // anything else that might fail, so a bad asset path is reported and
    // exits cleanly rather than leaving partially-constructed physics/
    // gameplay state behind. Paths are relative to the process's current
    // working directory — see docs/ARCHITECTURE.md, "Milestone 9, Assets,"
    // for why (this engine has no asset-root/working-directory abstraction
    // to resolve them through instead): run `judas` from the repository
    // root, exactly as the existing build/run instructions already show.
    MeshData beaconMeshData;
    std::string assetError;
    if (!LoadObjMesh(kBeaconModelPath, beaconMeshData, assetError)) {
        std::fprintf(stderr, "%s\n", assetError.c_str());
        return 1;
    }
    TextureData beaconTextureData;
    if (!LoadTextureFromFile(kBeaconTexturePath, beaconTextureData, assetError)) {
        std::fprintf(stderr, "%s\n", assetError.c_str());
        return 1;
    }
    const MeshHandle beaconMesh = renderer.CreateMesh(beaconMeshData);
    const TextureHandle beaconTexture = renderer.CreateTexture(beaconTextureData);
    renderer.SetLighting(kLightDirection, kLightColor, kAmbientColor);

    PhysicsWorld physicsWorld;
    if (!physicsWorld.Init()) {
        std::fprintf(stderr, "Physics initialization failed.\n");
        return 1;
    }

    // Application (the composition root) is the one place that knows which
    // concrete gravity implementations exist and where each has authority.
    // Both worlds use the exact same RadicalGravity class, constructed
    // twice with different centers — proof that neither planet is
    // privileged by the architecture itself, only by which coordinates
    // Application happens to hand it. The plank gets the existing,
    // unmodified FaithfulGravity — its own coherent local context, not a
    // blend of the two planets. GravityContextMap (see src/
    // GravityContextMap.h, docs/ARCHITECTURE.md "Gravity context
    // ownership") is, again, the only thing bound to `gravity` below;
    // every consumer stays exactly as implementation-agnostic as Milestone
    // 7-B already proved — none of this region wiring is visible past this
    // function. The plank's region is registered FIRST so it deliberately
    // wins over a planet's more general region within its own footprint
    // (see the comment on kPlankGravityRegionHalfExtents above).
    RadicalGravity planetAGravity(kPlanetACenter, kRadicalGravityMagnitude);
    RadicalGravity planetBGravity(kPlanetBCenter, kRadicalGravityMagnitude);
    FaithfulGravity plankGravity;
    const BoxVolume plankGravityRegion(kPlankCenter, kPlankGravityRegionHalfExtents);
    const SphericalVolume planetAGravityRegion(kPlanetACenter, kPlanetGravityRegionRadius);
    const SphericalVolume planetBGravityRegion(kPlanetBCenter, kPlanetGravityRegionRadius);
    GravityContextMap gravityContext;
    gravityContext.AddRegion(plankGravity, plankGravityRegion);
    gravityContext.AddRegion(planetAGravity, planetAGravityRegion);
    gravityContext.AddRegion(planetBGravity, planetBGravityRegion);
    GravityField& gravity = gravityContext;

    const BodyHandle planetABody = physicsWorld.CreateStaticSphere(
        kPlanetACenter, kPlanetARadius, kPlanetFriction, kPlanetRestitution);
    const BodyHandle planetBBody = physicsWorld.CreateStaticSphere(
        kPlanetBCenter, kPlanetBRadius, kPlanetFriction, kPlanetRestitution);
    const BodyHandle plankBody = physicsWorld.CreateStaticBox(
        kPlankCenter, kPlankHalfExtents, kPlankFriction, kPlankRestitution);

    // Milestone 10: the staircase + ramp step/slope test geometry — see
    // SpawnStepTestGeometry's own comment and docs/ARCHITECTURE.md,
    // "Milestone 10."
    const std::vector<StaticTestBody> stepTestBodies = SpawnStepTestGeometry(physicsWorld);

    PlayerController player(kPlayerSpawnPosition, kPlayerSpawnYawDegrees);
    if (!player.Spawn(physicsWorld)) {
        std::fprintf(stderr, "Player spawn failed.\n");
        return 1;
    }

    // Milestone 7-A/7-Final: several ordinary dynamic bodies sharing
    // the same GravityField the player uses — see docs/ARCHITECTURE.md,
    // "Multiple gravity consumers." Created here (composition root), not
    // inside PlayerController or DynamicBody, exactly like the static
    // planets above.
    std::vector<DynamicBody> dynamicBodies = SpawnDynamicObjects(physicsWorld);

    // Milestone 8: the flying primitive, appended as one more DynamicBody
    // (see the kFlyingPrimitive* constants above) so it shares every
    // existing per-body mechanism (gravity, collision, presentation,
    // reset). `flyingPrimitiveBodyIndex` recovers its entry for the
    // control-anchored camera below; `flyingPrimitiveControl` is the entire
    // separate control-ownership mechanism (see src/FlyingPrimitiveControl.h).
    {
        DynamicBody::Visual visual;
        visual.shape = DynamicBody::Shape::Box;
        visual.halfExtents = kFlyingPrimitiveHalfExtents;
        visual.color = kFlyingPrimitiveColor;
        const BodyHandle handle = physicsWorld.CreateDynamicBox(
            kFlyingPrimitiveSpawnPosition, kFlyingPrimitiveHalfExtents, kFlyingPrimitiveMass,
            kFlyingPrimitiveFriction, kFlyingPrimitiveRestitution);
        dynamicBodies.emplace_back(handle, visual, kFlyingPrimitiveSpawnPosition,
                                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }
    const std::size_t flyingPrimitiveBodyIndex = dynamicBodies.size() - 1;
    FlyingPrimitiveControl flyingPrimitiveControl;
    flyingPrimitiveControl.handle = dynamicBodies[flyingPrimitiveBodyIndex].Handle();

    // Shared between the normal interactive loop and the test harness, so
    // a screenshot taken by the harness shows exactly what the real game
    // would have rendered that frame. `presentationAlpha` blends the
    // player's rendered box between its previous and current fixed-step
    // pose — see docs/ARCHITECTURE.md, "Simulation/presentation boundary"
    // — the same value passed to PlayerController::GetViewMatrix so the
    // camera and the player box always move in visual lockstep. Both
    // planets are static and never interpolated; neither has a "previous"
    // pose to blend from. The plank is likewise static. Dynamic bodies use
    // the exact same alpha via DynamicBody's own GetPresentedPosition/
    // Orientation.
    const auto drawScene = [&](Renderer& r, float presentationAlpha) {
        r.DrawSphere(kPlanetACenter, kPlanetARadius, kPlanetAColor);
        r.DrawSphere(kPlanetBCenter, kPlanetBRadius, kPlanetBColor);
        r.DrawBox(kPlankCenter, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), kPlankHalfExtents, kPlankColor);
        // Milestone 10: the staircase + ramp — static, drawn from their own
        // authored transforms exactly like the plank above.
        for (const StaticTestBody& body : stepTestBodies) {
            r.DrawBox(body.position, body.rotation, body.halfExtents, body.color);
        }
        // Milestone 9: the one imported, textured, lit model in this demo —
        // static, undressed by any physics transform (see kBeaconPosition's
        // own comment). A white tint so the texture's own colors show
        // unmodified; scale 1 (the model's own authored units are already a
        // sensible size — see assets/models/beacon.obj).
        r.DrawMesh(beaconMesh, kBeaconPosition, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f),
                   beaconTexture, glm::vec3(1.0f));
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
                                   flyingPrimitiveControl, drawScene, testScriptPath);
    } else {
        float physicsAccumulator = 0.0f;

        const Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64 previousCounter = SDL_GetPerformanceCounter();

        // Opt-in live telemetry for the INTERACTIVE loop — distinct from
        // JUDAS_TEST_SCRIPT, which replaces real input entirely. This lets
        // a human play with the real window/keyboard/mouse while position,
        // orientation, and the actual look/viewpoint direction stream to
        // stdout for direct inspection, without affecting ordinary play
        // when unset. Throttled (not every frame) since 60Hz would flood
        // whatever's reading it.
        const bool liveTelemetryEnabled = std::getenv("JUDAS_LIVE_TELEMETRY") != nullptr;
        constexpr int kLiveTelemetryFrameInterval = 10;
        int liveTelemetryFrameCounter = 0;
        if (liveTelemetryEnabled) {
            std::printf(
                "frame,posX,posY,posZ,upX,upY,upZ,yaw,pitch,lookX,lookY,lookZ,grounded,velX,velY,"
                "velZ,gravX,gravY,gravZ\n");
            std::fflush(stdout);
        }

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
                flyingPrimitiveControl.controlled = false;
                physicsAccumulator = 0.0f;
            }

            // Milestone 8: F toggles input authority between the player and
            // the flying primitive. Taking control is gated on the player's
            // OWN current support state (never a global teleport-to-it) —
            // releasing control is always allowed. See
            // docs/ARCHITECTURE.md, "Milestone 8."
            if (window.ConsumeControlToggleRequest()) {
                if (flyingPrimitiveControl.controlled) {
                    flyingPrimitiveControl.controlled = false;
                } else if (player.IsGrounded() &&
                           player.GetSupportBodyHandle().id == flyingPrimitiveControl.handle.id) {
                    flyingPrimitiveControl.controlled = true;
                }
            }

            // Fixed-timestep physics: render-frame delta time only decides
            // how many fixed steps run this frame, never the size of a
            // step itself.
            physicsAccumulator += frameDeltaTime;
            int stepsThisFrame = 0;
            while (physicsAccumulator >= SimulationTiming::kFixedTimestep &&
                   stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
                // Dynamic bodies (the flying primitive included) sample
                // gravity and hand it to the physics engine BEFORE Step()
                // integrates it into their position — the same "Judas
                // samples, physics obeys" ordering the player uses, just
                // applied to a list. See docs/ARCHITECTURE.md, "Multiple
                // gravity consumers." ApplyFlyingPrimitiveControl runs
                // immediately after: if controlled, it overrides the
                // primitive's velocity from input, exactly overwriting what
                // gravity just contributed that step (see
                // src/FlyingPrimitiveControl.h) — otherwise it's a no-op
                // and the primitive falls/rests like any other body.
                PrepareDynamicBodiesForStep(dynamicBodies, gravity, physicsWorld,
                                             SimulationTiming::kFixedTimestep);
                ApplyFlyingPrimitiveControl(flyingPrimitiveControl, window, physicsWorld, gravity);
                physicsWorld.Step(SimulationTiming::kFixedTimestep);
                player.FixedUpdate(window, physicsWorld, gravity, SimulationTiming::kFixedTimestep,
                                    !flyingPrimitiveControl.controlled);
                SyncDynamicBodiesFromPhysics(dynamicBodies, physicsWorld);

                physicsAccumulator -= SimulationTiming::kFixedTimestep;
                ++stepsThisFrame;
            }
            if (stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame) {
                // Hit the catch-up cap: drop the backlog instead of
                // letting it compound into future frames.
                physicsAccumulator = 0.0f;
            }

            if (liveTelemetryEnabled && (liveTelemetryFrameCounter++ % kLiveTelemetryFrameInterval) == 0) {
                const glm::vec3 pos = player.GetPosition();
                const glm::vec3 up = player.GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
                const glm::vec3 look = player.GetLookDirection();
                const glm::vec3 vel = player.GetVelocity();
                const glm::vec3 grav = gravity.Sample(pos);
                std::printf("%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,"
                            "%.3f,%.3f,%.3f,%.3f\n",
                            liveTelemetryFrameCounter, pos.x, pos.y, pos.z, up.x, up.y, up.z,
                            player.GetYaw(), player.GetPitch(), look.x, look.y, look.z,
                            player.IsGrounded() ? 1 : 0, vel.x, vel.y, vel.z, grav.x, grav.y, grav.z);
                std::fflush(stdout);
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
            // Milestone 8: while controlling the flying primitive, anchor
            // the SAME camera (identical offset/look math — see
            // PlayerController::GetViewMatrix's two overloads) to its own
            // presented pose instead of the player's, so flying it has a
            // working viewpoint. Mouse look still comes from the player's
            // own m_yaw/m_pitch either way.
            const glm::mat4 view =
                flyingPrimitiveControl.controlled
                    ? player.GetViewMatrix(
                          dynamicBodies[flyingPrimitiveBodyIndex].GetPresentedPosition(presentationAlpha),
                          dynamicBodies[flyingPrimitiveBodyIndex].GetPresentedOrientation(presentationAlpha))
                    : player.GetViewMatrix(presentationAlpha);
            renderer.SetCamera(view, player.GetProjectionMatrix(aspectRatio));
            drawScene(renderer, presentationAlpha);
            renderer.EndFrame();

            window.SwapBuffers();
        }
    }

    player.Destroy(physicsWorld);
    for (const DynamicBody& body : dynamicBodies) {
        physicsWorld.DestroyBody(body.Handle());
    }
    for (const StaticTestBody& body : stepTestBodies) {
        physicsWorld.DestroyBody(body.handle);
    }
    physicsWorld.DestroyBody(planetABody);
    physicsWorld.DestroyBody(planetBBody);
    physicsWorld.DestroyBody(plankBody);
    physicsWorld.Shutdown();
    renderer.DestroyMesh(beaconMesh);
    renderer.DestroyTexture(beaconTexture);
    renderer.Shutdown();
    return exitCode;
}
