#include "Application.h"

#include <SDL2/SDL.h>
#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "CelestialGravity.h"
#include "DynamicBody.h"
#include "FaithfulGravity.h"
#include "FlyingPrimitiveControl.h"
#include "FluidSurface.h"
#include "FluidWorld.h"
#include "GravityContextMap.h"
#include "GravityField.h"
#include "Door.h"
#include "HUD.h"
#include "Interactable.h"
#include "InteractionSystem.h"
#include "Light.h"
#include "LightSwitch.h"
#include "LightTransforms.h"
#include "ModelLoader.h"
#include "ObjectManipulation.h"
#include "PauseMenu.h"
#include "PhysicsWorld.h"
#include "PilotAttachment.h"
#include "PilotControl.h"
#include "PlayerController.h"
#include "RadicalGravity.h"
#include "RadialTerrain.h"
#include "Renderer.h"
#include "ReferenceFrame.h"
#include "ShadowTransforms.h"
#include "SimulationTiming.h"
#include "SphericalVolume.h"
#include "TestHarness.h"
#include "TerrainDemo.h"
#include "TextureLoader.h"
#include "Window.h"
#include "WorldCoordinates.h"
#include "../third_party/stb_image_write.h"

namespace {
GLADapiproc LoadOpenGLProcAddress(const char* name) {
    return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
}

// Composition-root-only constant field for M24's reproducible local
// rotated/zero-gravity demonstration modes. The fluid and every other
// consumer still receive only the same GravityContextMap interface.
class DemoUniformGravity final : public GravityField {
public:
    explicit DemoUniformGravity(const glm::vec3& acceleration) : m_acceleration(acceleration) {}
    glm::vec3 Sample(const glm::vec3&) const override { return m_acceleration; }

private:
    glm::vec3 m_acceleration;
};

ReferenceFrame ReferenceFrameFromBody(const PhysicsWorld& physics, BodyHandle handle) {
    const BodyTransform transform = physics.GetTransform(handle);
    return ReferenceFrame{transform.position, transform.rotation,
                          physics.GetLinearVelocity(handle),
                          physics.GetAngularVelocity(handle)};
}

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;

// M23 demonstration placement. Authored positions throughout this file are
// local to one active scene. JUDAS_WORLD_OFFSET translates that whole scene
// in absolute space without asking float physics or OpenGL to subtract huge
// nearly-equal positions. "far" is a convenient operator acceptance preset.
bool ReadWorldOffset(glm::dvec3& offset) {
    const char* value = std::getenv("JUDAS_WORLD_OFFSET");
    if (value == nullptr || *value == '\0') {
        offset = glm::dvec3(0.0);
        return true;
    }
    if (std::string(value) == "far") {
        offset = glm::dvec3(1.0e9, -2.0e9, 3.0e9);
        return true;
    }
    double x = 0.0, y = 0.0, z = 0.0;
    char trailing = '\0';
    if (std::sscanf(value, "%lf,%lf,%lf%c", &x, &y, &z, &trailing) != 3 ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        std::fprintf(stderr, "JUDAS_WORLD_OFFSET must be far or x,y,z in metres.\n");
        return false;
    }
    offset = glm::dvec3(x, y, z);
    return true;
}

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

// M20's two bodies are ordinary dynamic spheres in unclaimed space. Their
// state is initialized from the analytical two-body circular solution; after
// creation only mutual forces and the ordinary integrator determine motion.
// M21 adds the spacecraft as an ordinary low-mass participant in that field.
const glm::vec3 kOrbitalBarycentre(0.0f, 23.0f, 115.0f);
constexpr float kOrbitalSeparation = 30.0f;
constexpr float kOrbitalMassA = 1.0e14f;
constexpr float kOrbitalMassB = 1.0e14f;
constexpr float kOrbitalRadius = 3.0f;
constexpr float kPlanetThrustForce = 2.0e14f;
const glm::quat kOrbitalFrame = glm::angleAxis(
    0.73f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
const glm::vec3 kOrbitalAxis = glm::normalize(kOrbitalFrame * glm::vec3(0.0f, 0.0f, 1.0f));
const glm::vec3 kOrbitalNormal = glm::normalize(kOrbitalFrame * glm::vec3(0.0f, 1.0f, 0.0f));
const glm::vec3 kOrbitalTangent = glm::normalize(glm::cross(kOrbitalNormal, kOrbitalAxis));
const float kOrbitalRelativeSpeed = std::sqrt(
    CelestialGravity::kGravitationalConstant * (kOrbitalMassA + kOrbitalMassB) /
    kOrbitalSeparation);
const glm::vec3 kOrbitalPositionA = kOrbitalBarycentre -
    kOrbitalAxis * (kOrbitalSeparation * kOrbitalMassB /
                    (kOrbitalMassA + kOrbitalMassB));
const glm::vec3 kOrbitalPositionB = kOrbitalBarycentre +
    kOrbitalAxis * (kOrbitalSeparation * kOrbitalMassA /
                    (kOrbitalMassA + kOrbitalMassB));
const glm::vec3 kOrbitalVelocityA = -kOrbitalTangent *
    (kOrbitalRelativeSpeed * kOrbitalMassB / (kOrbitalMassA + kOrbitalMassB));
const glm::vec3 kOrbitalVelocityB = kOrbitalTangent *
    (kOrbitalRelativeSpeed * kOrbitalMassA / (kOrbitalMassA + kOrbitalMassB));

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

// M24 demonstration geometry. These are five ordinary rigid boxes sharing a
// single body; neither physics nor fluid knows the assembled shape is a cup.
// The authored station follows Planet A's local radial frame only to make a
// convenient level starting arrangement. Gravity still comes from the map.
const glm::vec3 kFluidStationBearing = glm::normalize(glm::vec3(0.11f, 1.0f, 0.11f));
const glm::vec3 kFluidTableHalfExtents(0.85f, 0.12f, 0.42f);
const glm::vec3 kFluidTableColor(0.37f, 0.30f, 0.24f);
const glm::vec3 kCupBottomColor(0.55f, 0.67f, 0.73f);
const glm::vec3 kCupWallColor(0.50f, 0.76f, 0.82f);
const glm::vec3 kFluidColor(0.12f, 0.48f, 0.82f);
constexpr float kCupMass = 120.0f;
constexpr float kFluidParticleSpacing = 0.05f;

// The M25 scene is another authored local physical context, sufficiently
// separated from the earlier demonstration bodies that neither collision
// nor gravity regions overlap. The surface itself is body-local geometry.
const glm::vec3 kTerrainPlanetCenter(300.0f, 0.0f, 0.0f);
const glm::vec3 kTerrainPlanetColor(0.39f, 0.50f, 0.30f);
const glm::vec3 kTerrainPickupColor(0.95f, 0.58f, 0.20f);
const glm::vec3 kTerrainPickupHalfExtents(0.45f);
// At this coarse 125 kg/particle lake resolution, a dense 0.9 m block is
// within the demonstrated stable fluid/rigid mass ratio. The M18 carry law
// scales force with mass, so it remains an ordinary pickable dynamic body.
constexpr float kTerrainPickupMass = 2000.0f;
constexpr float kTerrainGravityRegionRadius = 103.0f;
constexpr std::size_t kTerrainMaxWaterParticles = 200;

std::vector<CompoundBox> MakeOpenCupBoxes() {
    // The shift puts the compound body's origin at the approximate
    // volume-weighted centre of mass. The spawn pose is lowered by the
    // same amount below, leaving every wall at its authored world pose.
    constexpr float centreOfMassShift = 0.153f;
    return {
        {{0.0f, -centreOfMassShift, 0.0f}, {0.18f, 0.015f, 0.18f}},
        {{-0.165f, 0.195f - centreOfMassShift, 0.0f}, {0.015f, 0.18f, 0.18f}},
        {{ 0.165f, 0.195f - centreOfMassShift, 0.0f}, {0.015f, 0.18f, 0.18f}},
        {{0.0f, 0.195f - centreOfMassShift, -0.165f}, {0.15f, 0.18f, 0.015f}},
        {{0.0f, 0.195f - centreOfMassShift,  0.165f}, {0.15f, 0.18f, 0.015f}},
    };
}

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

// --- Milestone 11: the spacecraft's mesh (assets/models/plane.obj) ---
//
// The Milestone 8 flying primitive, repurposed rather than replaced (see
// docs/ARCHITECTURE.md, "Milestone 11") — same DynamicBody, same box
// collider, same kFlyingPrimitive* spawn constants below; only its visual
// representation changes, from a plain DrawBox call to this imported mesh.
// Untextured (an invalid TextureHandle draws through Renderer's existing
// 1x1 white fallback — see Renderer::DrawMesh), tinted by
// kFlyingPrimitiveColor exactly as the box used to be. Authored directly at
// the collider's own footprint (see plane.obj's own header comment), so no
// separate model-to-body correction is needed — scale 1, identity offset.
const char* const kSpacecraftModelPath = "assets/models/plane.obj";

// --- Milestone 13: UI font ---
//
// DejaVu Sans (Bitstream Vera License — see assets/fonts/DejaVuSans-
// LICENSE.txt), baked once at a fixed pixel size via Renderer::LoadFont
// (src/FontLoader.h/.cpp, stb_truetype) — see docs/ARCHITECTURE.md,
// "Milestone 13, Text rendering," for the full vendoring/licensing
// rationale. 48px bakes crisp enough for both the HUD's small telemetry
// text (drawn at a smaller UI scale, see src/HUD.cpp) and the pause menu's
// larger title/button text (drawn at a larger scale) from the SAME atlas —
// one font, one bake, no separate small/large font assets.
const char* const kUIFontPath = "assets/fonts/DejaVuSans.ttf";
constexpr float kUIFontPixelHeight = 48.0f;

// One directional light plus a small constant ambient term — see
// docs/ARCHITECTURE.md, "Milestone 9, Lighting." A plain, fixed world-space
// direction chosen only to rake visibly across both the beacon and the
// planets/plank from a reasonable angle; it has NO relationship to gravity,
// local up, or any other Judas concept (law: no global up anywhere in this
// engine, lighting included).
const glm::vec3 kLightDirection = glm::normalize(glm::vec3(0.4f, 0.7f, 0.35f));
const glm::vec3 kLightColor(1.0f, 0.98f, 0.92f);
const glm::vec3 kAmbientColor(0.16f, 0.17f, 0.19f);

// --- Milestone 15: directional shadow frustum ---
//
// Recentered on the player's own presented position every frame (see
// docs/ARCHITECTURE.md, "Milestone 15, Directional-light shadows," for why
// a single recentered frustum — not cascaded, not whole-world-covering —
// was judged sufficient for this small, bounded demo). `kDirShadowHalf
// Extent` (25m) comfortably covers the player's immediate surroundings —
// enough to see nearby dynamic bodies, staircase steps, and a meaningful
// stretch of curved planet surface all casting/receiving shadows at once;
// `kDirShadowDistance` (40m) both places the shadow camera far enough back
// to never clip nearby geometry and sets the far plane generously past it.
constexpr float kDirShadowHalfExtent = 25.0f;
constexpr float kDirShadowDistance = 40.0f;

// --- Milestone 14: player torch ---
//
// A spotlight carried at the player's own presented eye position, pointed
// along the player's own presented look direction every frame (see
// PlayerController::GetTorchTransform) — never baked, never gravity-
// relative. `kTorchColor` already has intensity folded in (see
// docs/ARCHITECTURE.md, "Milestone 14, Point-light attenuation," for why
// this engine doesn't keep a separate "intensity" scalar); `kTorchRange`
// and the two cone angles were picked by direct interactive tuning against
// this demo's own geometry scale (planets ~20m radius, a person-sized
// player) — comfortably lights a nearby wall/surface without reaching
// across an entire planet. A soft-edged cone (10 degrees narrower "full
// brightness" core inside a 28-degree outer falloff) reads as an ordinary
// handheld flashlight rather than a laser or a floodlight.
const glm::vec3 kTorchColor(3.0f, 2.9f, 2.6f);
constexpr float kTorchRange = 35.0f;
constexpr float kTorchInnerConeDegrees = 18.0f;
constexpr float kTorchOuterConeDegrees = 28.0f;

// --- Milestone 14: spacecraft lights ---
//
// A small, fixed rig of three lights defined entirely in the spacecraft's
// OWN local space (relative to kFlyingPrimitiveHalfExtents(2.0, 0.25,
// 3.0) — see the flying-primitive spawn constants above), transformed into
// world space fresh every frame from the spacecraft's own PRESENTED pose
// (see the drawScene lambda below) — never a world-space position baked
// once at spawn. One forward-facing headlight spotlight at the nose
// (local -Z, matching FlyingPrimitiveControl.cpp's own "forward = local
// -Z" convention), plus two wingtip point "navigation" lights at the
// wingtips (local +-X, at the box collider's own half-extent) using the
// traditional aviation convention — red to port (local -X, left), green to
// starboard (local +X, right) — purely a decorative nod, not a gameplay
// signal of any kind.
const glm::vec3 kShipHeadlightLocalOffset(0.0f, 0.0f, -3.0f);
const glm::vec3 kShipHeadlightLocalDirection(0.0f, 0.0f, -1.0f);
const glm::vec3 kShipHeadlightColor(4.0f, 4.0f, 3.8f);
constexpr float kShipHeadlightRange = 45.0f;
constexpr float kShipHeadlightInnerConeDegrees = 12.0f;
constexpr float kShipHeadlightOuterConeDegrees = 22.0f;

const glm::vec3 kShipPortLightLocalOffset(-2.0f, 0.0f, 0.0f);
const glm::vec3 kShipPortLightColor(2.2f, 0.15f, 0.1f);   // red, port (left)
const glm::vec3 kShipStarboardLightLocalOffset(2.0f, 0.0f, 0.0f);
const glm::vec3 kShipStarboardLightColor(0.1f, 2.2f, 0.2f);  // green, starboard (right)
constexpr float kShipNavLightRange = 12.0f;

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

// --- Milestone 16: the door ---
//
// Placed at its own bearing on Planet A, distinct from the staircase/ramp
// bearings above — a short walk from spawn, easy to find deliberately
// (not stumbled into by accident the way the staircase is), since this
// milestone's own validation checklist specifically wants an approach ->
// prompt -> interact sequence a human can walk through cleanly. Authored
// via the SAME `RotationAligningUpTo`/`PointAboveSphere` convention every
// other piece of curved-surface geometry here already uses, so the door's
// own local "up" (and therefore its hinge axis, see kDoorLocalHingeAxis
// below) is whatever the local radial direction is AT that bearing — not
// world +Y — satisfying this milestone's own "do not assume world Y is
// the hinge/up axis" requirement structurally, the same way the
// staircase/ramp already satisfy "no global up" for step-climbing.
const glm::vec3 kDoorBearing = glm::normalize(glm::vec3(0.55f, 0.9f, 0.05f));
const glm::vec3 kDoorHalfExtents(1.1f, 1.0f, 0.1f);
const glm::vec3 kDoorColor(0.55f, 0.38f, 0.22f);
// The door's own LOCAL up (see Door.h's own "local -X face is always the
// hinge edge" convention for why this is the SWING axis, not a "which
// face is the hinge" choice) — expressed relative to the door's own
// authored orientation, never world space.
const glm::vec3 kDoorLocalHingeAxis(0.0f, 1.0f, 0.0f);
constexpr float kDoorOpenAngleDegrees = 100.0f;
constexpr float kDoorAngularSpeedDegreesPerSecond = 150.0f;

// --- Milestone 16: the light switch (second interactable) ---
//
// Placed just beside the door — a small lever, no physics body (see
// src/LightSwitch.h) — controlling one lamp a short distance away, so
// toggling it produces an obvious, easy-to-see effect (the lamp itself,
// see kLampPosition below) without needing to invent any gameplay purpose
// beyond "a light turns on."
const glm::vec3 kSwitchBearing = glm::normalize(glm::vec3(0.75f, 0.85f, 0.05f));
const glm::vec3 kSwitchHalfExtents(0.06f, 0.18f, 0.04f);
const glm::vec3 kSwitchColor(0.75f, 0.72f, 0.65f);
const glm::vec3 kSwitchLocalHingeAxis(0.0f, 0.0f, 1.0f);  // swings forward/back, not side to side
constexpr float kSwitchToggleAngleDegrees = 40.0f;
constexpr float kSwitchAngularSpeedDegreesPerSecond = 220.0f;
const glm::vec3 kLampColor(3.2f, 2.6f, 1.6f);
constexpr float kLampRange = 10.0f;

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

// Milestone 14: builds this render frame's complete dynamic-light list —
// the ONE place that knows a light is "the player's torch" or "the
// spacecraft's headlight/nav lights"; by the time these reach
// Renderer::SetDynamicLights, they are plain world-space DynamicLight
// values (see src/Light.h). Called once per render frame, AFTER
// presentation alpha is known, so every light this frame is built from the
// SAME presented poses the frame's own DrawMesh calls use — never a
// separately-timed snapshot that could visually lag behind its owner by a
// frame. `torchOn` and the spacecraft's own presented pose are the only
// gameplay state this function reads; it returns plain data and touches no
// GL itself (see docs/ARCHITECTURE.md, "Milestone 14, Light ownership and
// lifetime").
std::vector<DynamicLight> BuildDynamicLights(const PlayerController& player, bool torchOn,
                                              const DynamicBody& spacecraft, const LightSwitch& lightSwitch,
                                              float presentationAlpha) {
    std::vector<DynamicLight> lights;
    lights.reserve(5);

    if (torchOn) {
        DynamicLight torch;
        torch.kind = LightKind::Spot;
        player.GetTorchTransform(presentationAlpha, torch.position, torch.direction);
        torch.color = kTorchColor;
        torch.range = kTorchRange;
        torch.innerConeDegrees = kTorchInnerConeDegrees;
        torch.outerConeDegrees = kTorchOuterConeDegrees;
        // Milestone 15: the torch casts shadows through its own dedicated
        // shadow map (kTorchShadowSlot, src/Light.h) — see
        // Application::Run for where that slot's depth pass is rendered
        // each frame.
        torch.shadowMapIndex = kTorchShadowSlot;
        lights.push_back(torch);
    }

    const glm::vec3 shipPosition = spacecraft.GetPresentedPosition(presentationAlpha);
    const glm::quat shipOrientation = spacecraft.GetPresentedOrientation(presentationAlpha);

    DynamicLight headlight;
    headlight.kind = LightKind::Spot;
    headlight.position = TransformLocalLightPosition(shipPosition, shipOrientation, kShipHeadlightLocalOffset);
    headlight.direction = TransformLocalLightDirection(shipOrientation, kShipHeadlightLocalDirection);
    headlight.color = kShipHeadlightColor;
    headlight.range = kShipHeadlightRange;
    headlight.innerConeDegrees = kShipHeadlightInnerConeDegrees;
    headlight.outerConeDegrees = kShipHeadlightOuterConeDegrees;
    // Milestone 15: the headlight casts shadows through its own dedicated
    // shadow map (kShipHeadlightShadowSlot) — the wingtip nav point
    // lights below deliberately do NOT (see src/Light.h, "point/
    // navigation lights never cast shadows this milestone").
    headlight.shadowMapIndex = kShipHeadlightShadowSlot;
    lights.push_back(headlight);

    DynamicLight portLight;
    portLight.kind = LightKind::Point;
    portLight.position = TransformLocalLightPosition(shipPosition, shipOrientation, kShipPortLightLocalOffset);
    portLight.color = kShipPortLightColor;
    portLight.range = kShipNavLightRange;
    lights.push_back(portLight);

    DynamicLight starboardLight;
    starboardLight.kind = LightKind::Point;
    starboardLight.position =
        TransformLocalLightPosition(shipPosition, shipOrientation, kShipStarboardLightLocalOffset);
    starboardLight.color = kShipStarboardLightColor;
    starboardLight.range = kShipNavLightRange;
    lights.push_back(starboardLight);

    // Milestone 16: the light switch's own lamp — an ordinary point light,
    // present in this frame's list only while `lightSwitch.IsLampOn()`,
    // exactly like the torch above is present only while `torchOn`. No
    // shadow map of its own (point lights never cast shadows this
    // milestone — see src/Light.h). 1 (torch) + 3 (ship) + 1 (lamp) = 5,
    // exactly `kMaxDynamicLights` — see src/Light.h for why that headroom
    // was sized with this in mind.
    if (lightSwitch.IsLampOn()) {
        DynamicLight lamp;
        lamp.kind = LightKind::Point;
        lamp.position = lightSwitch.GetLampPosition();
        lamp.color = lightSwitch.GetLampColor();
        lamp.range = lightSwitch.GetLampRange();
        lights.push_back(lamp);
    }

    return lights;
}
}  // namespace

int Application::Run() {
    glm::dvec3 worldOffset;
    if (!ReadWorldOffset(worldOffset)) return 1;
    const WorldCoordinates worldCoordinates(worldOffset);
    // Opt-in developer/automation tooling (see docs/ARCHITECTURE.md,
    // "Automated testing," and src/TestHarness.h): when set, this run is a
    // scripted, headless verification pass rather than the interactive
    // game. Checked before Window::Init so the window can be created
    // hidden — it's a real GL context either way, just not shown on screen.
    const char* testScriptPath = std::getenv("JUDAS_TEST_SCRIPT");
    const bool isTestRun = testScriptPath != nullptr;
    // Interactive launches now begin at the terrain lake. The accepted
    // M1-M24 scene remains selectable, and existing scripted harness runs
    // retain their established starting state unless explicitly previewing
    // M25. Both scenes use the same engine systems, not separate engines.
    const bool terrainDemoEnabled = isTestRun
        ? std::getenv("JUDAS_TERRAIN_PREVIEW") != nullptr
        : std::getenv("JUDAS_CLASSIC_DEMO") == nullptr;
    // An opt-in visual snapshot of the M24 starting arrangement can be
    // captured by the existing screenshot harness without changing its
    // accepted M1–M23 scripted simulation path.
    const bool fluidDemoEnabled = !terrainDemoEnabled &&
        (!isTestRun || std::getenv("JUDAS_FLUID_PREVIEW") != nullptr);

    Window window;
    if (!window.Init("Project Judas - Milestone 25", kWindowWidth, kWindowHeight,
                      !isTestRun)) {
        std::fprintf(stderr, "Window initialization failed.\n");
        return 1;
    }
    std::fprintf(stderr, "World origin (m): %.3f, %.3f, %.3f\n",
                 worldCoordinates.Origin().x, worldCoordinates.Origin().y,
                 worldCoordinates.Origin().z);
    if (terrainDemoEnabled) {
        std::fprintf(stderr,
                     "M25 terrain: radius %.1f m, 125 initial fluid particles; "
                     "hold B to add real fluid, R to reset, "
                     "JUDAS_CLASSIC_DEMO=1 for the earlier scene.\n",
                     TerrainDemo::kBaseRadius);
    }

    if (gladLoadGL(&LoadOpenGLProcAddress) == 0 || !GLAD_GL_VERSION_3_3) {
        std::fprintf(stderr, "Failed to load the required OpenGL 3.3 Core entry points.\n");
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

    // Milestone 11: the spacecraft's mesh — same load-and-fail-cleanly
    // pattern as the beacon above, no texture (see kSpacecraftModelPath's
    // own comment).
    MeshData spacecraftMeshData;
    if (!LoadObjMesh(kSpacecraftModelPath, spacecraftMeshData, assetError)) {
        std::fprintf(stderr, "%s\n", assetError.c_str());
        return 1;
    }
    const MeshHandle spacecraftMesh = renderer.CreateMesh(spacecraftMeshData);
    // A reusable non-indexed GPU buffer receives the surface extracted from
    // interpolated fluid positions each presentation frame.
    const MeshHandle fluidMesh = renderer.CreateMesh(MeshData{});
    const std::shared_ptr<const RadialTerrain> terrainSurface = terrainDemoEnabled
        ? TerrainDemo::CreateSurface() : nullptr;
    const glm::quat terrainRotation = std::getenv("JUDAS_TERRAIN_ROTATED")
        ? glm::angleAxis(glm::radians(47.0f), glm::normalize(glm::vec3(1.0f, 0.3f, 2.0f)))
        : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const MeshHandle terrainMesh = terrainSurface
        ? renderer.CreateMesh(terrainSurface->BuildMesh(96, 128)) : MeshHandle{};

    renderer.SetLighting(kLightDirection, kLightColor, kAmbientColor);

    // Milestone 13: the UI font — same fail-cleanly-before-any-gameplay-
    // state-exists pattern as the beacon/spacecraft assets above.
    if (!renderer.LoadFont(kUIFontPath, kUIFontPixelHeight, assetError)) {
        std::fprintf(stderr, "%s\n", assetError.c_str());
        return 1;
    }

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
    RadicalGravity terrainGravity(kTerrainPlanetCenter, kRadicalGravityMagnitude);
    FaithfulGravity plankGravity;
    const glm::quat fluidStationRotation = RotationAligningUpTo(kFluidStationBearing);
    const glm::vec3 fluidSurfacePoint = PointAboveSphere(
        kPlanetACenter, kPlanetARadius, kFluidStationBearing, 0.0f);
    const glm::vec3 fluidTablePosition =
        fluidSurfacePoint + kFluidStationBearing * kFluidTableHalfExtents.y;
    const char* fluidGravityMode = std::getenv("JUDAS_FLUID_GRAVITY");
    const std::string selectedFluidGravity = fluidGravityMode ? fluidGravityMode : "normal";
    if (selectedFluidGravity != "normal" && selectedFluidGravity != "rotated" &&
        selectedFluidGravity != "zero") {
        std::fprintf(stderr, "JUDAS_FLUID_GRAVITY must be normal, rotated, or zero.\n");
        return 1;
    }
    const glm::vec3 demoGravity = selectedFluidGravity == "zero" ? glm::vec3(0.0f) :
        glm::angleAxis(glm::radians(50.0f),
                       fluidStationRotation * glm::vec3(0.0f, 0.0f, 1.0f)) *
            (-kFluidStationBearing * kRadicalGravityMagnitude);
    DemoUniformGravity fluidDemoGravity(demoGravity);
    const BoxVolume fluidGravityRegion(fluidTablePosition, glm::vec3(2.2f, 2.5f, 1.8f));
    const BoxVolume plankGravityRegion(kPlankCenter, kPlankGravityRegionHalfExtents);
    const SphericalVolume planetAGravityRegion(kPlanetACenter, kPlanetGravityRegionRadius);
    const SphericalVolume planetBGravityRegion(kPlanetBCenter, kPlanetGravityRegionRadius);
    const SphericalVolume terrainGravityRegion(kTerrainPlanetCenter, kTerrainGravityRegionRadius);
    GravityContextMap gravityContext;
    if (fluidDemoEnabled && selectedFluidGravity != "normal") {
        gravityContext.AddRegion(fluidDemoGravity, fluidGravityRegion);
        std::fprintf(stderr, "Fluid station gravity: %s (%.2f, %.2f, %.2f) m/s^2\n",
                     selectedFluidGravity.c_str(), demoGravity.x, demoGravity.y, demoGravity.z);
    }
    gravityContext.AddRegion(plankGravity, plankGravityRegion);
    gravityContext.AddRegion(planetAGravity, planetAGravityRegion);
    gravityContext.AddRegion(planetBGravity, planetBGravityRegion);
    if (terrainDemoEnabled) gravityContext.AddRegion(terrainGravity, terrainGravityRegion);
    GravityField& gravity = gravityContext;

    const BodyHandle planetABody = physicsWorld.CreateStaticSphere(
        kPlanetACenter, kPlanetARadius, kPlanetFriction, kPlanetRestitution);
    const BodyHandle planetBBody = physicsWorld.CreateStaticSphere(
        kPlanetBCenter, kPlanetBRadius, kPlanetFriction, kPlanetRestitution);
    const BodyHandle terrainBody = terrainSurface
        ? physicsWorld.CreateStaticTerrain(kTerrainPlanetCenter, terrainRotation, terrainSurface,
                                           kPlanetFriction, kPlanetRestitution)
        : BodyHandle{};
    const BodyHandle plankBody = physicsWorld.CreateStaticBox(
        kPlankCenter, kPlankHalfExtents, kPlankFriction, kPlankRestitution);

    BodyHandle fluidTableBody;
    if (fluidDemoEnabled) {
        fluidTableBody = physicsWorld.CreateStaticBox(
            fluidTablePosition, fluidStationRotation, kFluidTableHalfExtents, 0.8f, 0.0f);
    }

    // Milestone 10: the staircase + ramp step/slope test geometry — see
    // SpawnStepTestGeometry's own comment and docs/ARCHITECTURE.md,
    // "Milestone 10."
    const std::vector<StaticTestBody> stepTestBodies = SpawnStepTestGeometry(physicsWorld);

    // Milestone 16: the door and light switch — see their own kDoor*/
    // kSwitch* constants above for the full placement/authoring
    // reasoning. Both hinge edges are authored via the same
    // RotationAligningUpTo/PointAboveSphere convention the staircase/ramp
    // already use, so each one's own local "up" (and hinge axis) matches
    // the local radial direction at its own bearing, never world +Y.
    const glm::quat doorOrientation = RotationAligningUpTo(kDoorBearing);
    const glm::vec3 doorHingePosition = PointAboveSphere(kPlanetACenter, kPlanetARadius, kDoorBearing, 0.0f);
    Door door(physicsWorld, doorHingePosition, doorOrientation, kDoorHalfExtents, kDoorLocalHingeAxis,
              glm::radians(kDoorOpenAngleDegrees), glm::radians(kDoorAngularSpeedDegreesPerSecond),
              kDoorColor);

    const glm::quat switchOrientation = RotationAligningUpTo(kSwitchBearing);
    const glm::vec3 switchHingePosition =
        PointAboveSphere(kPlanetACenter, kPlanetARadius, kSwitchBearing, 1.1f);
    const glm::vec3 lampPosition = PointAboveSphere(kPlanetACenter, kPlanetARadius, kSwitchBearing, 2.5f);
    LightSwitch lightSwitch(switchHingePosition, switchOrientation, kSwitchHalfExtents,
                             kSwitchLocalHingeAxis, glm::radians(kSwitchToggleAngleDegrees),
                             glm::radians(kSwitchAngularSpeedDegreesPerSecond), kSwitchColor, lampPosition,
                             kLampColor, kLampRange);

    const glm::vec3 terrainPlayerSpawn = terrainSurface
        ? kTerrainPlanetCenter + terrainRotation * TerrainDemo::LocalPointAbove(
              *terrainSurface, TerrainDemo::kBasinAX, -7.0f, 3.0f)
        : kPlayerSpawnPosition;
    PlayerController player(terrainPlayerSpawn, kPlayerSpawnYawDegrees);
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
    const std::size_t terrainPickupBodyIndex = terrainSurface
        ? dynamicBodies.size() : std::numeric_limits<std::size_t>::max();
    if (terrainSurface) {
        const glm::vec3 propPosition = kTerrainPlanetCenter + terrainRotation *
            TerrainDemo::LocalPointAbove(*terrainSurface, TerrainDemo::kBasinAX - 2.0f,
                                         -5.5f, 1.4f);
        DynamicBody::Visual visual;
        visual.shape = DynamicBody::Shape::Box;
        visual.halfExtents = kTerrainPickupHalfExtents;
        visual.color = kTerrainPickupColor;
        const BodyHandle handle = physicsWorld.CreateDynamicBox(
            propPosition, visual.halfExtents, kTerrainPickupMass, 0.9f,
            kDynamicObjectRestitution);
        dynamicBodies.emplace_back(handle, visual, propPosition,
                                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }

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

    // M20: two genuine massive bodies. The rotated initial frame makes the
    // demonstration plane arbitrary; it is not a physics preference.
    const auto addOrbitalBody = [&](const glm::vec3& position, float mass,
                                    const glm::vec3& color) {
        DynamicBody::Visual visual;
        visual.shape = DynamicBody::Shape::Sphere;
        visual.radius = kOrbitalRadius;
        visual.color = color;
        const BodyHandle handle = physicsWorld.CreateDynamicSphere(
            position, kOrbitalRadius, mass, kDynamicObjectFriction, kDynamicObjectRestitution);
        dynamicBodies.emplace_back(handle, visual, position,
                                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        return handle;
    };
    BodyHandle orbitalBodyA;
    BodyHandle orbitalBodyB;
    if (!isTestRun) {
        orbitalBodyA = addOrbitalBody(kOrbitalPositionA, kOrbitalMassA,
                                      glm::vec3(0.25f, 0.7f, 0.95f));
        orbitalBodyB = addOrbitalBody(kOrbitalPositionB, kOrbitalMassB,
                                      glm::vec3(0.95f, 0.55f, 0.2f));
        physicsWorld.SetLinearVelocity(orbitalBodyA, kOrbitalVelocityA);
        physicsWorld.SetLinearVelocity(orbitalBodyB, kOrbitalVelocityB);
    }
    std::vector<BodyHandle> celestialBodies;
    if (orbitalBodyA.IsValid()) {
        celestialBodies = {orbitalBodyA, orbitalBodyB,
                           dynamicBodies[flyingPrimitiveBodyIndex].Handle()};
    }
    CelestialGravity celestialGravity(std::move(celestialBodies));

    // Two physically identical, pickable open vessels. The bodies' child
    // boxes are ordinary collision geometry; the fluid never stores a cup
    // identity or a per-container quantity. Keep the authored initial poses
    // alongside the same DynamicBody presentation/reset snapshots used by
    // every M7–M23 dynamic object.
    const std::vector<CompoundBox> cupBoxes = MakeOpenCupBoxes();
    const std::size_t firstCupBodyIndex = dynamicBodies.size();
    std::array<BodyHandle, 2> cupHandles{};
    std::array<glm::vec3, 2> cupSpawnPositions{};
    if (fluidDemoEnabled) {
        for (std::size_t i = 0; i < cupHandles.size(); ++i) {
            const float side = i == 0 ? -0.34f : 0.34f;
            const glm::vec3 localCenter(side,
                kFluidTableHalfExtents.y + 0.168f, 0.0f);
            const glm::vec3 position = fluidTablePosition + fluidStationRotation * localCenter;
            const BodyHandle handle = physicsWorld.CreateDynamicCompoundBoxes(
                position, cupBoxes, kCupMass, 0.8f, 0.0f);
            physicsWorld.ResetBody(handle, position, fluidStationRotation);
            DynamicBody::Visual visual;
            visual.shape = DynamicBody::Shape::Box;
            visual.halfExtents = glm::vec3(0.18f, 0.195f, 0.18f);
            visual.color = kCupWallColor;
            dynamicBodies.emplace_back(handle, visual, position, fluidStationRotation);
            cupHandles[i] = handle;
            cupSpawnPositions[i] = position;
        }
    }

    FluidSettings fluidSettings;
    if (terrainSurface) {
        // Same M24 solver at a coarser, metre-scale resolution. Lengths
        // scale with the 10x particle spacing and mass with its cube;
        // density and gravity remain physical, not tuned to a visual lake.
        fluidSettings.particleRadius *= 10.0f;
        fluidSettings.smoothingRadius *= 10.0f;
        fluidSettings.maxDensityCorrection *= 10.0f;
    }
    FluidWorld fluidWorld(fluidSettings);
    const auto resetFluid = [&]() {
        fluidWorld.Clear();
        if (terrainSurface) {
            const float spacing = TerrainDemo::kWaterSpacing;
            const float mass = fluidWorld.Settings().restDensity * spacing * spacing * spacing;
            const glm::vec3 localCenter = TerrainDemo::LocalPointAbove(
                *terrainSurface, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 0.65f);
            for (int y = 0; y < 5; ++y) {
                for (int z = -2; z <= 2; ++z) {
                    for (int x = -2; x <= 2; ++x) {
                        const glm::vec3 local = localCenter +
                            glm::vec3(static_cast<float>(x) * spacing,
                                      static_cast<float>(y) * spacing,
                                      static_cast<float>(z) * spacing);
                        fluidWorld.AddParticle(kTerrainPlanetCenter + terrainRotation * local,
                                               glm::vec3(0.0f), mass);
                    }
                }
            }
            return;
        }
        if (!cupHandles[0].IsValid()) return;
        const float mass = fluidWorld.Settings().restDensity *
            kFluidParticleSpacing * kFluidParticleSpacing * kFluidParticleSpacing;
        for (int y = 0; y < 5; ++y) {
            for (int z = -2; z <= 2; ++z) {
                for (int x = -2; x <= 2; ++x) {
                    const glm::vec3 local(static_cast<float>(x) * kFluidParticleSpacing,
                                          0.05f - 0.153f +
                                              static_cast<float>(y) * kFluidParticleSpacing,
                                          static_cast<float>(z) * kFluidParticleSpacing);
                    fluidWorld.AddParticle(cupSpawnPositions[0] + fluidStationRotation * local,
                                           glm::vec3(0.0f), mass);
                }
            }
        }
    };
    resetFluid();
    if (!fluidWorld.Particles().empty()) {
        std::vector<glm::vec3> initialPositions;
        initialPositions.reserve(fluidWorld.Particles().size());
        for (const FluidParticle& particle : fluidWorld.Particles())
            initialPositions.push_back(particle.position);
        renderer.UpdateMeshVertices(fluidMesh,
            BuildFluidSurface(initialPositions,
                              terrainSurface ? fluidSettings.smoothingRadius : 0.105f,
                              terrainSurface ? 0.40f : 0.05f, 0.45f));
    }

    // M18's explicit whitelist now includes the two ordinary dynamic cups;
    // spacecraft, celestial bodies, planets, plank, door and table stay out.
    std::vector<std::size_t> pickupBodyIndices;
    for (std::size_t i = 0; i < flyingPrimitiveBodyIndex; ++i) pickupBodyIndices.push_back(i);
    for (std::size_t i = firstCupBodyIndex; i < dynamicBodies.size(); ++i)
        pickupBodyIndices.push_back(i);
    std::vector<BodyHandle> pickupHandles;
    pickupHandles.reserve(pickupBodyIndices.size());
    for (std::size_t i : pickupBodyIndices) {
        pickupHandles.push_back(dynamicBodies[i].Handle());
    }
    ObjectManipulation objectManipulation(std::move(pickupHandles));
    std::vector<PickupInteractable> pickupTargets;
    pickupTargets.reserve(pickupBodyIndices.size());
    for (std::size_t i : pickupBodyIndices) {
        pickupTargets.emplace_back(dynamicBodies[i], objectManipulation, physicsWorld);
    }
    // Player/Application code still sees only the established M16
    // Interactable interface, while manipulation remains a separate
    // Judas-owned gameplay boundary.
    std::vector<Interactable*> interactables{&door, &lightSwitch};
    for (PickupInteractable& target : pickupTargets) interactables.push_back(&target);
    FlyingPrimitiveControl flyingPrimitiveControl;
    flyingPrimitiveControl.handle = dynamicBodies[flyingPrimitiveBodyIndex].Handle();
    // Milestone 11: the secured-pilot relationship (see src/PilotAttachment.h)
    // — always established/cleared in lockstep with flyingPrimitiveControl's
    // own `controlled` flag (see src/PilotControl.h), but kept as a
    // separate struct since it is genuinely a distinct concept: `controlled`
    // is an input-routing flag, `pilotAttachment` is fixed-step simulation
    // state (the player's own authoritative pose derives from it).
    PilotAttachment pilotAttachment;

    // Milestone 17: this is presentation selection only. The spacecraft
    // camera below deliberately continues using its existing anchored,
    // third-person path while piloting.
    PlayerViewMode playerViewMode = terrainSurface
        ? PlayerViewMode::FirstPerson : PlayerViewMode::ThirdPerson;

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
    // `includePlayerModel` (Milestone 15): the torch is carried at the
    // player's own eye position, which sits INSIDE the player's own
    // rendered body box (kEyeHeightAboveCenter=0.7m is within the box's
    // own 0.9m half-height) — rendering that box into the torch's OWN
    // shadow pass makes the player's own body self-shadow almost the
    // entire cone, since the "occluder" sits essentially at the light
    // itself. Every other pass (the directional sun's shadow pass, the
    // spacecraft headlight's shadow pass, and the ordinary color pass)
    // legitimately wants the player's body included — a person can
    // correctly cast a shadow from the sun, or block their own
    // spacecraft's headlight by standing in front of it. Only the
    // torch's own shadow pass excludes it — see docs/ARCHITECTURE.md,
    // "Milestone 15, Post-validation bugfix," for the full diagnosis.
    // `includePlayerModel` defaults to true so this lambda still satisfies
    // RunTestHarness's own `std::function<void(Renderer&, float)>`
    // parameter unchanged (TestHarness never runs a shadow pass at all —
    // see docs/ARCHITECTURE.md, "Milestone 15" — so it always wants the
    // player included, exactly as every milestone before this one).
    const auto drawScene = [&](Renderer& r, float presentationAlpha,
                               bool includePlayerModel = true, bool includeTerrain = true) {
        // The older objects continue simulating in their distant context;
        // this authored terrain view submits its own nearby scene. The
        // accepted classic scene remains selectable at launch.
        if (!terrainSurface) {
            r.DrawSphere(kPlanetACenter, kPlanetARadius, kPlanetAColor);
            r.DrawSphere(kPlanetBCenter, kPlanetBRadius, kPlanetBColor);
            r.DrawBox(kPlankCenter, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                      kPlankHalfExtents, kPlankColor);
            if (fluidTableBody.IsValid()) {
                r.DrawBox(fluidTablePosition, fluidStationRotation, kFluidTableHalfExtents,
                          kFluidTableColor);
            }
            for (const StaticTestBody& body : stepTestBodies) {
                r.DrawBox(body.position, body.rotation, body.halfExtents, body.color);
            }
            r.DrawMesh(beaconMesh, kBeaconPosition, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                       glm::vec3(1.0f), beaconTexture, glm::vec3(1.0f));
            door.Draw(r, presentationAlpha);
            lightSwitch.Draw(r, presentationAlpha);
        }
        if (terrainMesh.IsValid() && includeTerrain) {
            r.DrawMesh(terrainMesh, kTerrainPlanetCenter, terrainRotation, glm::vec3(1.0f),
                       TextureHandle{}, kTerrainPlanetColor);
        }

        // Milestone 11: while attached, render the pilot coherently with
        // the SAME presented spacecraft pose used for the camera and the
        // spacecraft's own mesh below — applying the stored attachment
        // transform to that presented pose (rather than interpolating the
        // player's own authoritative before/after snapshots independently)
        // avoids any visible relative separation during rotation. Reuses
        // ApplyPilotAttachment unchanged: a BodyTransform is just a
        // position+rotation pair, and the presented spacecraft pose is
        // exactly that. See docs/ARCHITECTURE.md, "Milestone 11, Camera
        // and presentation."
        glm::vec3 playerRenderPosition;
        glm::quat playerRenderOrientation;
        if (flyingPrimitiveControl.controlled && pilotAttachment.attached) {
            const BodyTransform shipPresented{
                dynamicBodies[flyingPrimitiveBodyIndex].GetPresentedPosition(presentationAlpha),
                dynamicBodies[flyingPrimitiveBodyIndex].GetPresentedOrientation(presentationAlpha)};
            ApplyPilotAttachment(pilotAttachment, shipPresented, playerRenderPosition,
                                  playerRenderOrientation);
        } else {
            playerRenderPosition = player.GetPresentedPosition(presentationAlpha);
            playerRenderOrientation = player.GetPresentedOrientation(presentationAlpha);
        }
        const bool playerModelVisible = playerViewMode == PlayerViewMode::ThirdPerson ||
                                        flyingPrimitiveControl.controlled;
        if (includePlayerModel && playerModelVisible) {
            r.DrawBox(playerRenderPosition, playerRenderOrientation, player.GetRenderHalfExtents(),
                      kPlayerColor);
        }

        for (std::size_t i = 0; i < dynamicBodies.size(); ++i) {
            if (terrainSurface && i != terrainPickupBodyIndex) continue;
            const DynamicBody& body = dynamicBodies[i];
            if (i == flyingPrimitiveBodyIndex) {
                // Milestone 11: the spacecraft draws through the imported
                // mesh path instead of DrawBox — see kSpacecraftModelPath's
                // own comment for why no separate model-to-body correction
                // or texture is needed here.
                r.DrawMesh(spacecraftMesh, body.GetPresentedPosition(presentationAlpha),
                           body.GetPresentedOrientation(presentationAlpha), glm::vec3(1.0f),
                           TextureHandle{}, kFlyingPrimitiveColor);
                continue;
            }
            if (cupHandles[0].IsValid() &&
                i >= firstCupBodyIndex && i < firstCupBodyIndex + cupHandles.size()) {
                const glm::vec3 parentPosition = body.GetPresentedPosition(presentationAlpha);
                const glm::quat parentRotation = body.GetPresentedOrientation(presentationAlpha);
                for (std::size_t part = 0; part < cupBoxes.size(); ++part) {
                    if (part != 0 && !r.IsShadowPass()) continue;
                    const CompoundBox& box = cupBoxes[part];
                    r.DrawBox(parentPosition + parentRotation * box.localCenter,
                              parentRotation, box.halfExtents,
                              part == 0 ? kCupBottomColor : kCupWallColor);
                }
                continue;
            }
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
        // The fluid receives ordinary lighting/shadows in the color pass;
        // its tiny, constantly rebuilt surface does not need to occupy
        // three separate depth maps each frame.
        if (!fluidWorld.Particles().empty() && !r.IsShadowPass()) {
            r.DrawMesh(fluidMesh, glm::vec3(0.0f), glm::quat(1, 0, 0, 0),
                       glm::vec3(1.0f), TextureHandle{}, kFluidColor);
        }
    };

    const auto drawTransparentCupWalls = [&](float presentationAlpha) {
        if (!cupHandles[0].IsValid()) return;
        renderer.BeginTransparentPass();
        for (std::size_t i = 0; i < cupHandles.size(); ++i) {
            const DynamicBody& body = dynamicBodies[firstCupBodyIndex + i];
            const glm::vec3 position = body.GetPresentedPosition(presentationAlpha);
            const glm::quat rotation = body.GetPresentedOrientation(presentationAlpha);
            for (std::size_t part = 1; part < cupBoxes.size(); ++part) {
                const CompoundBox& box = cupBoxes[part];
                renderer.DrawBox(position + rotation * box.localCenter, rotation,
                                 box.halfExtents, kCupWallColor, 0.35f);
            }
        }
        renderer.EndTransparentPass();
    };

    // Milestone 13: the pause menu and HUD — interactive-loop-only (see
    // docs/ARCHITECTURE.md, "Milestone 13, Automated testing"): the
    // JUDAS_TEST_SCRIPT harness verifies gameplay/physics behavior
    // headlessly and was deliberately not taught to script UI interaction;
    // src/PauseMenu.h/src/UIWidgets.h's own navigation/input-ownership
    // logic is covered instead by the standalone judas_ui_tests
    // executable (no window/GL needed — see tests/UITests.cpp), and
    // on-screen appearance/interaction by human validation, exactly as
    // this milestone's brief requires.
    PauseMenu pauseMenu;
    HUD hud;

    // Milestone 14: the player torch's on/off state — plain gameplay
    // input state, same category as flyingPrimitiveControl.controlled,
    // owned here (not inside PlayerController) since it's presentation
    // state a light-building step reads, not something PlayerController's
    // own simulation needs to know about. Toggled by `T`, gated behind
    // the same `!pauseMenu.IsOpen()` boundary every other piece of
    // gameplay input already is (see the interactive loop below) — see
    // docs/ARCHITECTURE.md, "Milestone 14, Input ownership."
    bool torchOn = false;

    int exitCode = 0;
    if (isTestRun) {
        const auto drawHarnessScene = [&](Renderer& r, float alpha) {
            drawScene(r, alpha);
            drawTransparentCupWalls(alpha);
        };
        exitCode = RunTestHarness(window, renderer, physicsWorld, player, gravity, dynamicBodies,
                                   flyingPrimitiveControl, pilotAttachment, drawHarnessScene, testScriptPath);
    } else {
        float physicsAccumulator = 0.0f;
        std::size_t emittedTerrainParticles = 0;
        std::size_t terrainFixedSteps = 0;
        const char* terrainScreenshotPath = std::getenv("JUDAS_TERRAIN_SCREENSHOT");
        bool terrainScreenshotWritten = false;

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
        const bool fluidDiagnosticsEnabled = std::getenv("JUDAS_FLUID_DIAGNOSTICS") != nullptr;
        int fluidDiagnosticFrames = 0;
        int fluidDiagnosticSteps = 0;
        double accumulatedFluidMilliseconds = 0.0;
        double accumulatedSurfaceMilliseconds = 0.0;
        double accumulatedSceneMilliseconds = 0.0;
        double accumulatedFrameMilliseconds = 0.0;
        constexpr int kLiveTelemetryFrameInterval = 10;
        int liveTelemetryFrameCounter = 0;
        if (liveTelemetryEnabled) {
            std::printf(
                "frame,posX,posY,posZ,upX,upY,upZ,yaw,pitch,lookX,lookY,lookZ,grounded,velX,velY,"
                "velZ,gravX,gravY,gravZ\n");
            std::fflush(stdout);
        }

        // Milestone 13: mouse capture tracks the menu's own open/closed
        // state (see Window::SetMouseCaptured's doc comment) — only
        // changed ON THE TRANSITION, not every frame, since re-asserting
        // relative mouse mode every frame would fight SDL's own internal
        // accumulator for no benefit (see docs/ARCHITECTURE.md, "Milestone
        // 13, Input ownership").
        bool wasPauseMenuOpen = false;

        while (!window.ShouldClose() && !pauseMenu.QuitRequested()) {
            window.PollEvents();

            // --- Milestone 13: the single input-routing boundary ---
            //
            // Everything UI-related is handled here, in one place, BEFORE
            // any gameplay system sees this frame's input — see
            // docs/ARCHITECTURE.md, "Milestone 13, Input ownership," for
            // why this satisfies "don't scatter `if (menuOpen)` checks
            // throughout gameplay systems": PlayerController,
            // FlyingPrimitiveControl, and PilotControl are never told a
            // menu exists. Instead, the block below either lets gameplay
            // run this frame or doesn't — gameplay code itself stays
            // exactly as it was through Milestone 12.
            //
            // Escape is context-sensitive (see PauseMenu::HandleBackRequest):
            // closed -> open, nested screen -> back one level, root screen
            // -> resume. Up/Down/Enter/click only affect an OPEN menu —
            // consumed unconditionally either way so a stray press doesn't
            // leak into next frame, but only acted on while there's a menu
            // to act on.
            if (window.ConsumeUIBackRequest()) {
                pauseMenu.HandleBackRequest();
            }
            const bool uiUp = window.ConsumeUINavigateUpRequest();
            const bool uiDown = window.ConsumeUINavigateDownRequest();
            const bool uiActivate = window.ConsumeUIActivateRequest();
            int uiClickX = 0, uiClickY = 0;
            const bool uiClicked = window.ConsumeUIClickRequest(uiClickX, uiClickY);
            if (pauseMenu.IsOpen()) {
                pauseMenu.Layout(window.Width(), window.Height());
                if (uiUp) pauseMenu.NavigateUp();
                if (uiDown) pauseMenu.NavigateDown();
                if (uiActivate) pauseMenu.Activate();
                int mouseX = 0, mouseY = 0;
                window.GetMousePosition(mouseX, mouseY);
                pauseMenu.HandleMouseMove(glm::vec2(static_cast<float>(mouseX), static_cast<float>(mouseY)));
                if (uiClicked) {
                    pauseMenu.HandleMouseClick(glm::vec2(static_cast<float>(uiClickX), static_cast<float>(uiClickY)));
                }
            }
            if (pauseMenu.IsOpen() != wasPauseMenuOpen) {
                window.SetMouseCaptured(!pauseMenu.IsOpen());
                wasPauseMenuOpen = pauseMenu.IsOpen();
            }

            // Milestone 14: drained EVERY frame, regardless of pause
            // state — same reasoning as the UI requests just above:
            // pressing `T` while the menu owns input must not leave a
            // stale toggle sitting in Window ready to fire the instant
            // the menu closes (see docs/ARCHITECTURE.md, "Milestone 14,
            // Input ownership"). Whether it's actually ACTED on is
            // decided below, inside the `!pauseMenu.IsOpen()` gate, same
            // as every other piece of gameplay input.
            const bool torchToggleRequested = window.ConsumeTorchToggleRequest();
            // Milestone 16: same drain-always shape as torchToggleRequested
            // above, for the exact same reason — see
            // docs/ARCHITECTURE.md, "Milestone 16, Input ownership."
            const bool interactRequested = window.ConsumeInteractRequest();
            // Drain even while the menu owns input, so V cannot toggle late
            // when gameplay resumes.
            const bool viewToggleRequested = window.ConsumeViewToggleRequest();
            const bool throwRequested = window.ConsumeThrowRequest();
            const bool sasToggleRequested = window.ConsumeSasToggleRequest();

            // Milestone 16: recomputed every render frame from the
            // player's own CURRENT authoritative position/look direction
            // (frozen, like everything else, while paused) — this is the
            // entire "stop offering interaction when it is no longer
            // valid" mechanism: SelectInteractable simply returns nullptr
            // the moment nothing qualifies, with no separate state to
            // clear. Computed unconditionally (harmless/read-only) so the
            // HUD prompt below stays accurate even while paused; only
            // ACTING on `interactRequested` is gated on pause state, in
            // the block below.
            Interactable* interactTarget =
                SelectInteractable(player.GetPosition(), player.GetLookDirection(), interactables);

            const Uint64 currentCounter = SDL_GetPerformanceCounter();
            float frameDeltaTime = static_cast<float>(currentCounter - previousCounter) /
                                    static_cast<float>(frequency);
            previousCounter = currentCounter;
            if (frameDeltaTime > SimulationTiming::kMaxFrameDeltaTime) {
                frameDeltaTime = SimulationTiming::kMaxFrameDeltaTime;
            }

            // Milestone 13 pause policy: while the menu is open, gameplay
            // input is never read (no mouse look, no jump/reset/control-
            // toggle latching) AND the fixed-step simulation does not
            // advance at all — no physics integration, no gravity, no
            // player FixedUpdate, not even accumulating the leftover
            // `physicsAccumulator` time toward a future step. The world is
            // completely frozen, not merely "ignoring input" — see
            // docs/ARCHITECTURE.md, "Milestone 13, Pause policy," for why
            // this was chosen over the alternative (keep simulating, e.g.
            // so a player already falling keeps falling behind the menu):
            // a frozen world matches this brief's own "gameplay input
            // suppressed while menus own input" requirement literally, and
            // avoids the stranger case of a spacecraft coasting into
            // something while its own pilot is stuck in a menu unable to
            // react. Resuming picks up exactly where it left off — no
            // catch-up, no dropped/compounded backlog, since the
            // accumulator itself never advanced while paused.
            if (!pauseMenu.IsOpen()) {
                ApplyPlayerViewToggle(playerViewMode, viewToggleRequested,
                                      /*gameplayOwnsInput=*/true);
                // Mouse look and jump-key latching happen every render
                // frame, independent of how many fixed physics steps run
                // this frame.
                player.UpdateFrameInput(window);

                if (window.ConsumeResetRequest()) {
                    objectManipulation.Drop();
                    player.Reset();
                    for (DynamicBody& body : dynamicBodies) {
                        body.ResetToSpawn(physicsWorld);
                    }
                    resetFluid();
                    emittedTerrainParticles = 0;
                    terrainFixedSteps = 0;
                    terrainScreenshotWritten = false;
                    physicsWorld.SetLinearVelocity(orbitalBodyA, kOrbitalVelocityA);
                    physicsWorld.SetLinearVelocity(orbitalBodyB, kOrbitalVelocityB);
                    flyingPrimitiveControl.controlled = false;
                    pilotAttachment.attached = false;
                    SetSpacecraftSasEnabled(flyingPrimitiveControl, false, physicsWorld);
                    physicsAccumulator = 0.0f;
                }

                // Milestone 8/11: F toggles input authority (and, as of
                // Milestone 11, the secured-pilot attachment — see
                // src/PilotControl.h) between the player and the spacecraft.
                // Taking control is gated on the player's OWN current support
                // state (never a global teleport-to-it) — releasing control is
                // always allowed, in any orientation. See
                // docs/ARCHITECTURE.md, "Milestone 11."
                if (window.ConsumeControlToggleRequest()) {
                    const bool wasControlled = flyingPrimitiveControl.controlled;
                    HandlePilotToggleRequest(flyingPrimitiveControl, pilotAttachment, player,
                                             physicsWorld, gravity);
                    if (!wasControlled && flyingPrimitiveControl.controlled) objectManipulation.Drop();
                }

                // M21: X toggles the attitude controller only while the
                // player owns spacecraft controls. The request is drained
                // above even while the menu owns input.
                if (sasToggleRequested && flyingPrimitiveControl.controlled) {
                    SetSpacecraftSasEnabled(flyingPrimitiveControl,
                                             !flyingPrimitiveControl.sasEnabled, physicsWorld);
                }

                // Milestone 14: T toggles the player's torch. The
                // request itself was already drained unconditionally
                // above (see torchToggleRequested) so a press during
                // pause can never fire late on resume; ACTING on it is
                // still gated behind `!pauseMenu.IsOpen()` like every
                // other piece of gameplay input, so a press that arrives
                // in the render frame the menu happens to be closing on
                // doesn't sneak through either.
                if (torchToggleRequested) {
                    torchOn = !torchOn;
                }

                // Milestone 16: G triggers the selected interaction;
                // M18 reuses that path for eligible dynamic-body pickup.
                // interactable, if any — gated behind `!pauseMenu.IsOpen()`
                // exactly like every other piece of gameplay input here,
                // so a menu can never accidentally trigger a world
                // interaction (see docs/ARCHITECTURE.md, "Milestone 16,
                // Input ownership"). `PlayerController`/this call site
                // only ever know `interactTarget` as an `Interactable*`
                // — never a `Door`/`LightSwitch` by name.
                if (interactRequested && interactTarget && interactTarget->CanInteract()) {
                    interactTarget->Interact();
                } else if (interactRequested && objectManipulation.IsHolding() &&
                           !flyingPrimitiveControl.controlled) {
                    objectManipulation.Drop();
                }
                if (throwRequested && !flyingPrimitiveControl.controlled) {
                    objectManipulation.Throw(physicsWorld, player.GetLookDirection(), 8.0f);
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
                    // immediately after and adds pilot forces/torques to the
                    // same body. Enabled SAS remains active even when the
                    // pilot is not controlling the spacecraft.
                    PrepareDynamicBodiesForStep(dynamicBodies, gravity, physicsWorld,
                                                 SimulationTiming::kFixedTimestep);
                    celestialGravity.ApplyForces(physicsWorld);
                    // M20 operator thrusters are real constant forces. Their
                    // directions are reconstructed from current barycentric
                    // position/velocity, never from a world axis or orbit path.
                    if (orbitalBodyA.IsValid()) {
                        const glm::vec3 positionA = physicsWorld.GetTransform(orbitalBodyA).position;
                        const glm::vec3 positionB = physicsWorld.GetTransform(orbitalBodyB).position;
                        const glm::vec3 velocityA = physicsWorld.GetLinearVelocity(orbitalBodyA);
                        const glm::vec3 velocityB = physicsWorld.GetLinearVelocity(orbitalBodyB);
                        const float massA = physicsWorld.GetMass(orbitalBodyA);
                        const float massB = physicsWorld.GetMass(orbitalBodyB);
                        const float totalMass = massA + massB;
                        const glm::vec3 barycentre = (positionA * massA + positionB * massB) /
                                                     totalMass;
                        const glm::vec3 baryVelocity = (velocityA * massA + velocityB * massB) /
                                                       totalMass;
                        const glm::vec3 radial = glm::normalize(positionA - barycentre);
                        const glm::vec3 relativeVelocity = velocityA - baryVelocity;
                        const glm::vec3 tangentVelocity = relativeVelocity -
                            radial * glm::dot(relativeVelocity, radial);
                        glm::vec3 thrustDirection(0.0f);
                        if (window.IsActionActive(Action::PlanetRadialThrust)) {
                            thrustDirection = radial;
                        } else if (glm::length(tangentVelocity) > 1.0e-5f) {
                            const glm::vec3 tangent = glm::normalize(tangentVelocity);
                            if (window.IsActionActive(Action::PlanetProgradeThrust)) {
                                thrustDirection = tangent;
                            } else if (window.IsActionActive(Action::PlanetRetrogradeThrust)) {
                                thrustDirection = -tangent;
                            }
                        }
                        if (glm::length(thrustDirection) > 0.0f) {
                            physicsWorld.ApplyForce(orbitalBodyA,
                                                    thrustDirection * kPlanetThrustForce);
                        }
                    }
                    if (objectManipulation.IsHolding() && !flyingPrimitiveControl.controlled) {
                        const glm::vec3 carryTarget = ComputeCarryTarget(
                            player.GetPosition(), player.GetOrientation(), player.GetLookDirection(),
                            0.7f, 1.7f);
                        objectManipulation.ApplyCarryForce(physicsWorld, carryTarget, player.GetVelocity());
                        // A compound held body may need an attitude as well as
                        // a position. Mouse look tips it by applying torque;
                        // single-shape M18 objects retain their old carry law.
                        if (physicsWorld.GetBodyBoxes(objectManipulation.HeldBody()).size() > 1) {
                            objectManipulation.ApplyCarryOrientationTorque(
                                physicsWorld, ComputeCarryOrientation(
                                    player.GetOrientation(), player.GetLookDirection()));
                        }
                    }
                    ApplyFlyingPrimitiveControl(flyingPrimitiveControl, window, physicsWorld);
                    // Milestone 16: advances the door's own open/close
                    // animation and writes its new pose directly to
                    // PhysicsWorld (see Door::FixedUpdate) BEFORE the
                    // player's own FixedUpdate below, so this step's
                    // move-and-slide sees the door's up-to-date collision
                    // pose rather than last step's. The light switch has
                    // no physics body (see src/LightSwitch.h) — its own
                    // FixedUpdate only advances its visible lever angle.
                    door.FixedUpdate(physicsWorld, SimulationTiming::kFixedTimestep);
                    lightSwitch.FixedUpdate(SimulationTiming::kFixedTimestep);
                    physicsWorld.Step(SimulationTiming::kFixedTimestep);
                    if (terrainSurface && window.IsActionActive(Action::AddTerrainWater) &&
                        fluidWorld.Particles().size() < kTerrainMaxWaterParticles) {
                        const float spacing = TerrainDemo::kWaterSpacing;
                        const glm::vec3 source = TerrainDemo::LocalPointAbove(
                            *terrainSurface, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 2.8f);
                        const int column = static_cast<int>(emittedTerrainParticles % 9);
                        const glm::vec3 spread(
                            static_cast<float>(column % 3 - 1) * 0.3f, 0.0f,
                            static_cast<float>(column / 3 - 1) * 0.3f);
                        fluidWorld.AddParticle(
                            kTerrainPlanetCenter + terrainRotation * (source + spread),
                            glm::vec3(0.0f), fluidSettings.restDensity * spacing * spacing * spacing);
                        ++emittedTerrainParticles;
                    }
                    if (!fluidWorld.Particles().empty()) {
                        const auto fluidStart = std::chrono::steady_clock::now();
                        std::vector<FluidBoxCollider> fluidBoxes;
                        const auto appendBodyBoxes = [&](BodyHandle handle) {
                            const std::vector<BodyBox> previous =
                                physicsWorld.GetPreviousBodyBoxes(handle);
                            const std::vector<BodyBox> current = physicsWorld.GetBodyBoxes(handle);
                            for (std::size_t part = 0; part < current.size(); ++part) {
                                fluidBoxes.push_back(FluidBoxCollider{
                                    handle,
                                    BodyTransform{previous[part].center, previous[part].rotation},
                                    BodyTransform{current[part].center, current[part].rotation},
                                    current[part].halfExtents});
                            }
                        };
                        appendBodyBoxes(fluidTableBody);
                        appendBodyBoxes(plankBody);
                        for (const StaticTestBody& body : stepTestBodies)
                            appendBodyBoxes(body.handle);
                        std::vector<FluidSphereCollider> fluidSpheres;
                        for (const BodyHandle planet : {planetABody, planetBBody}) {
                            const BodyTransform previous = physicsWorld.GetPreviousTransform(planet);
                            const BodyTransform current = physicsWorld.GetTransform(planet);
                            const float radius = planet.id == planetABody.id
                                ? kPlanetARadius : kPlanetBRadius;
                            fluidSpheres.push_back({planet, previous, current, radius});
                        }
                        for (const DynamicBody& body : dynamicBodies) {
                            if (body.GetVisual().shape == DynamicBody::Shape::Sphere) {
                                fluidSpheres.push_back({body.Handle(),
                                    physicsWorld.GetPreviousTransform(body.Handle()),
                                    physicsWorld.GetTransform(body.Handle()),
                                    body.GetVisual().radius});
                            } else {
                                appendBodyBoxes(body.Handle());
                            }
                        }
                        std::vector<FluidTerrainCollider> fluidTerrains;
                        if (terrainSurface) {
                            fluidTerrains.push_back({terrainBody,
                                physicsWorld.GetPreviousTransform(terrainBody),
                                physicsWorld.GetTransform(terrainBody), terrainSurface.get()});
                        }
                        std::vector<FluidContactImpulse> fluidImpulses;
                        fluidWorld.Step(SimulationTiming::kFixedTimestep, gravity,
                                        fluidBoxes, fluidSpheres, fluidTerrains, &fluidImpulses);
                        for (const FluidContactImpulse& contact : fluidImpulses) {
                            physicsWorld.ApplyImpulseAtPoint(contact.owner,
                                                              contact.impulse, contact.point);
                        }
                        if (fluidDiagnosticsEnabled) {
                            accumulatedFluidMilliseconds +=
                                std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - fluidStart).count();
                            ++fluidDiagnosticSteps;
                        }
                    }
                    AdvancePlayerForPiloting(flyingPrimitiveControl, pilotAttachment, player, physicsWorld,
                                              window, gravity, SimulationTiming::kFixedTimestep);
                    SyncDynamicBodiesFromPhysics(dynamicBodies, physicsWorld);

                    physicsAccumulator -= SimulationTiming::kFixedTimestep;
                    ++stepsThisFrame;
                    if (terrainSurface) ++terrainFixedSteps;
                }
                if (stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame) {
                    // Hit the catch-up cap: drop the backlog instead of
                    // letting it compound into future frames.
                    physicsAccumulator = 0.0f;
                }
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

            if (!fluidWorld.Particles().empty()) {
                const auto surfaceStart = std::chrono::steady_clock::now();
                std::vector<glm::vec3> waterPositions;
                waterPositions.reserve(fluidWorld.Particles().size());
                for (std::size_t i = 0; i < fluidWorld.Particles().size(); ++i)
                    waterPositions.push_back(fluidWorld.PresentedPosition(i, presentationAlpha));
                renderer.UpdateMeshVertices(fluidMesh,
                    BuildFluidSurface(waterPositions,
                                      terrainSurface ? fluidSettings.smoothingRadius : 0.105f,
                                      terrainSurface ? 0.40f : 0.05f, 0.45f));
                if (fluidDiagnosticsEnabled) {
                    accumulatedSurfaceMilliseconds +=
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - surfaceStart).count();
                }
            }

            const int windowHeight = std::max(window.Height(), 1);
            const float aspectRatio =
                static_cast<float>(window.Width()) / static_cast<float>(windowHeight);

            // Milestone 14: rebuilt fresh every render frame from this
            // frame's own presented poses — never cached across frames,
            // so a moving/rotating light source (the torch, the
            // spacecraft's own lights) never lags behind what's actually
            // drawn this frame. Computed once here (not inline at the
            // SetDynamicLights call site below) so Milestone 15's shadow
            // passes can read each shadow-casting light's own position/
            // direction/cone straight out of it, rather than recomputing
            // the same transforms a second time.
            const std::vector<DynamicLight> lights = BuildDynamicLights(
                player, torchOn, dynamicBodies[flyingPrimitiveBodyIndex], lightSwitch, presentationAlpha);

            // Milestone 15: shadow passes — one per shadow-casting light,
            // each rendering the SAME scene geometry (via the SAME
            // drawScene lambda the color pass below uses) depth-only into
            // its own dedicated shadow map, BEFORE the normal color pass
            // that will go on to sample those depth textures. See
            // docs/ARCHITECTURE.md, "Milestone 15," for the full design.
            // The directional "sun" and the spacecraft headlight are
            // always active, so their passes always run; the torch's own
            // slot simply isn't touched this frame when the torch is off
            // (see Renderer.h's BeginShadowPass comment for why a stale
            // previous-frame depth texture there is harmless — nothing
            // this frame's light list references it).
            const glm::mat4 dirShadowMatrix =
                ComputeDirectionalShadowMatrix(player.GetPresentedPosition(presentationAlpha),
                                                kLightDirection, kDirShadowHalfExtent, kDirShadowDistance);
            const auto sceneStart = std::chrono::steady_clock::now();
            renderer.BeginShadowPass(kDirectionalShadowSlot, dirShadowMatrix);
            drawScene(renderer, presentationAlpha, /*includePlayerModel=*/true);
            renderer.EndShadowPass();

            for (const DynamicLight& light : lights) {
                if (light.shadowMapIndex != kTorchShadowSlot &&
                    light.shadowMapIndex != kShipHeadlightShadowSlot) {
                    continue;
                }
                const glm::mat4 spotShadowMatrix = ComputeSpotShadowMatrix(
                    light.position, light.direction, light.outerConeDegrees, light.range);
                renderer.BeginShadowPass(light.shadowMapIndex, spotShadowMatrix);
                // The torch is attached essentially INSIDE the player's own
                // rendered body (see the drawScene lambda's own comment
                // above) — exclude it from only the torch's own shadow
                // pass; the spacecraft headlight has no such conflict.
                const bool terrainWithinLight = !terrainSurface ||
                    glm::distance(light.position, kTerrainPlanetCenter) <=
                    light.range + terrainSurface->BoundRadius();
                drawScene(renderer, presentationAlpha,
                          /*includePlayerModel=*/light.shadowMapIndex != kTorchShadowSlot,
                          /*includeTerrain=*/terrainWithinLight);
                renderer.EndShadowPass();
            }

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
                    : player.GetViewMatrix(presentationAlpha, playerViewMode);
            renderer.SetCamera(view, player.GetProjectionMatrix(aspectRatio));
            // Continues rendering normally while paused (see
            // docs/ARCHITECTURE.md, "Milestone 14, Input ownership") — the
            // world is frozen, but the frozen scene remains correctly
            // (and readably, and with correct shadows) lit.
            renderer.SetDynamicLights(lights);
            drawScene(renderer, presentationAlpha, /*includePlayerModel=*/true);
            drawTransparentCupWalls(presentationAlpha);
            renderer.EndFrame();
            if (fluidDiagnosticsEnabled) {
                accumulatedSceneMilliseconds +=
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - sceneStart).count();
            }

            // Milestone 13: the HUD + pause menu overlay, drawn last so
            // they composite on top of the 3D scene above — see
            // Renderer::BeginUIFrame's own doc comment for why this is a
            // separate screen-space/unlit/blended pass rather than more
            // DrawMesh calls.
            renderer.BeginUIFrame(window.Width(), window.Height());
            if (pauseMenu.IsHudVisible()) {
                HUDViewData hudData;
                hudData.grounded = player.IsGrounded();
                hudData.gravityMagnitude = glm::length(gravity.Sample(player.GetPosition()));
                hudData.controllingSpacecraft = flyingPrimitiveControl.controlled;
                hudData.pilotAttached = pilotAttachment.attached;
                hudData.spacecraftSasEnabled = flyingPrimitiveControl.sasEnabled;
                hudData.worldOrigin = worldCoordinates.Origin();
                hudData.absolutePlayerPosition = worldCoordinates.ToGlobal(player.GetPosition());
                const BodyHandle shipHandle = flyingPrimitiveControl.handle;
                const glm::vec3 shipWorldPosition = physicsWorld.GetTransform(shipHandle).position;
                const glm::vec3 shipWorldVelocity = physicsWorld.GetLinearVelocity(shipHandle);
                const ReferenceFrame shipFrame = ReferenceFrameFromBody(physicsWorld, shipHandle);
                hudData.spacecraftLinearSpeed = glm::length(shipWorldVelocity);

                const glm::vec3 pilotWorldVelocity = pilotAttachment.attached
                    ? VelocityToWorld(shipFrame, pilotAttachment.localOffset, glm::vec3(0.0f))
                    : player.GetVelocity();
                hudData.pilotWorldSpeed = glm::length(pilotWorldVelocity);
                hudData.pilotRelativeSpacecraftSpeed = glm::length(RelativeVelocityToFrame(
                    shipFrame, player.GetPosition(), pilotWorldVelocity));

                if (orbitalBodyA.IsValid()) {
                    const ReferenceFrame celestialFrame =
                        ReferenceFrameFromBody(physicsWorld, orbitalBodyA);
                    hudData.celestialReferenceAvailable = true;
                    hudData.celestialBodyWorldSpeed = glm::length(celestialFrame.linearVelocity);
                    hudData.spacecraftRelativeCelestialSpeed = glm::length(RelativeVelocityToFrame(
                        celestialFrame, shipWorldPosition, shipWorldVelocity));
                }
                if (objectManipulation.IsHolding()) {
                    hudData.interactPrompt = interactTarget
                        ? interactTarget->GetPromptText() + " | H Throw"
                        : "G Drop | H Throw";
                    if (physicsWorld.GetBodyBoxes(objectManipulation.HeldBody()).size() > 1)
                        hudData.interactPrompt += " | Look to tip";
                } else {
                    hudData.interactPrompt = interactTarget ? interactTarget->GetPromptText() : std::string();
                }
                if (terrainSurface) {
                    if (!hudData.interactPrompt.empty()) hudData.interactPrompt += " | ";
                    hudData.interactPrompt += "Hold B: add water";
                }
                hud.Draw(renderer, window.Width(), window.Height(), hudData);
            }
            pauseMenu.Draw(renderer, window.Width(), window.Height());
            renderer.EndUIFrame();

            // Opt-in visual validation of the fully simulated basin after
            // ten seconds of fixed-step settling, using the existing
            // renderer readback path. Never feeds presentation back into
            // fluid or terrain simulation.
            if (terrainSurface && terrainScreenshotPath && !terrainScreenshotWritten &&
                terrainFixedSteps >= 600) {
                std::vector<unsigned char> pixels;
                renderer.CaptureFrame(window.Width(), window.Height(), pixels);
                const int written = stbi_write_png(terrainScreenshotPath,
                    window.Width(), window.Height(), 3, pixels.data(), window.Width() * 3);
                std::fprintf(stderr, "M25 screenshot %s: %s\n",
                             written ? "written" : "failed", terrainScreenshotPath);
                terrainScreenshotWritten = true;
            }

            window.SwapBuffers();
            if (fluidDiagnosticsEnabled) {
                ++fluidDiagnosticFrames;
                accumulatedFrameMilliseconds += frameDeltaTime * 1000.0;
                if (fluidDiagnosticFrames % 120 == 0) {
                    const FluidDiagnostics state = fluidWorld.GetDiagnostics();
                    if (terrainSurface) {
                        std::size_t inSecondBasin = 0;
                        float minimumClearance = std::numeric_limits<float>::max();
                        for (const FluidParticle& particle : fluidWorld.Particles()) {
                            const glm::vec3 local = glm::conjugate(terrainRotation) *
                                (particle.position - kTerrainPlanetCenter);
                            const float clearance = terrainSurface->Sample(local).signedDistance;
                            if (local.x > 3.0f && local.x < 7.0f &&
                                std::abs(local.z - TerrainDemo::kBasinZ) < 3.0f &&
                                clearance < 1.1f) ++inSecondBasin;
                            minimumClearance = std::min(minimumClearance, clearance);
                        }
                        std::fprintf(stderr,
                            "M25 timing: %zu particles, %zu across into second basin; "
                            "%.3f ms/fluid step (%d steps); %.3f ms/surface+upload; "
                            "%.3f ms/scene CPU submit; %.3f ms/frame wall; "
                            "mass %.1f kg; min terrain clearance %.3f m; "
                            "mean/max over-density %.2f/%.2f%%; kinetic %.1f J\n",
                            state.particleCount, inSecondBasin,
                            accumulatedFluidMilliseconds / std::max(fluidDiagnosticSteps, 1),
                            fluidDiagnosticSteps,
                            accumulatedSurfaceMilliseconds / fluidDiagnosticFrames,
                            accumulatedSceneMilliseconds / fluidDiagnosticFrames,
                            accumulatedFrameMilliseconds / fluidDiagnosticFrames,
                            state.totalMass, minimumClearance,
                            state.meanPositiveDensityError * 100.0f,
                            state.maxPositiveDensityError * 100.0f,
                            state.kineticEnergy);
                    } else {
                        const BodyTransform cupPose = physicsWorld.GetTransform(cupHandles[0]);
                        std::fprintf(stderr,
                        "M24 timing: %zu particles; %.3f ms/fluid step (%d steps); "
                        "%.3f ms/surface+upload; %.3f ms/scene CPU submit; %.3f ms/frame wall; "
                        "mass %.3f kg; mean/max over-density %.2f/%.2f%%; "
                        "COM (%.3f, %.3f, %.3f) m; kinetic %.3f J; "
                        "cup (%.3f, %.3f, %.3f) m, speed %.3f m/s, spin %.3f rad/s\n",
                        fluidWorld.Particles().size(),
                        accumulatedFluidMilliseconds / std::max(fluidDiagnosticSteps, 1),
                        fluidDiagnosticSteps,
                        accumulatedSurfaceMilliseconds / fluidDiagnosticFrames,
                        accumulatedSceneMilliseconds / fluidDiagnosticFrames,
                        accumulatedFrameMilliseconds / fluidDiagnosticFrames,
                        state.totalMass, state.meanPositiveDensityError * 100.0f,
                        state.maxPositiveDensityError * 100.0f,
                        state.centerOfMass.x, state.centerOfMass.y, state.centerOfMass.z,
                        state.kineticEnergy,
                        cupPose.position.x, cupPose.position.y, cupPose.position.z,
                        glm::length(physicsWorld.GetLinearVelocity(cupHandles[0])),
                        glm::length(physicsWorld.GetAngularVelocity(cupHandles[0])));
                    }
                    std::fflush(stderr);
                }
            }
        }
    }

    player.Destroy(physicsWorld);
    door.Destroy(physicsWorld);
    for (const DynamicBody& body : dynamicBodies) {
        physicsWorld.DestroyBody(body.Handle());
    }
    for (const StaticTestBody& body : stepTestBodies) {
        physicsWorld.DestroyBody(body.handle);
    }
    physicsWorld.DestroyBody(planetABody);
    physicsWorld.DestroyBody(planetBBody);
    physicsWorld.DestroyBody(plankBody);
    if (fluidTableBody.IsValid()) physicsWorld.DestroyBody(fluidTableBody);
    if (terrainBody.IsValid()) physicsWorld.DestroyBody(terrainBody);
    physicsWorld.Shutdown();
    renderer.DestroyMesh(beaconMesh);
    renderer.DestroyTexture(beaconTexture);
    renderer.DestroyMesh(spacecraftMesh);
    renderer.DestroyMesh(fluidMesh);
    if (terrainMesh.IsValid()) renderer.DestroyMesh(terrainMesh);
    renderer.Shutdown();
    return exitCode;
}
