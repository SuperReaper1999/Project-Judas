// Scene authoring tool (developer tooling, not part of the engine): bakes
// the technology demonstration's M27-era constants into Judas scene files,
// and writes the tiny game project's scene. Every placement formula below is
// the exact expression the M27 composition root evaluated at startup.
//
// Build target: judas_scene_author (CMake). Run from the repository root:
//
//   judas_scene_author assets/scenes projects/tiny_game/Scenes
//
// Regenerates the nine demonstration scenes (deterministic: an unchanged
// generator writes byte-identical files) and projects/tiny_game/Scenes/
// main.judas. Mesh references are the fixed AssetIds recorded in the
// assets' .judasmeta sidecars (assets/models/*.obj.judasmeta,
// assets/textures/beacon.png.judasmeta).
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AtmosphereField.h"
#include "CelestialGravity.h"
#include "RadialTerrain.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "TerrainDemo.h"

namespace {
const glm::vec3 kPlanetACenter(0.0f, 0.0f, 0.0f);
constexpr float kPlanetARadius = 20.0f;
const glm::vec3 kPlanetAColor(0.3f, 0.45f, 0.35f);
const glm::vec3 kPlanetBCenter(0.0f, 0.0f, 55.0f);
constexpr float kPlanetBRadius = 20.0f;
const glm::vec3 kPlanetBColor(0.35f, 0.3f, 0.45f);
const glm::vec3 kOrbitalBarycentre(0.0f, 23.0f, 115.0f);
constexpr float kOrbitalSeparation = 30.0f;
constexpr float kOrbitalMassA = 1.0e14f;
constexpr float kOrbitalMassB = 1.0e14f;
constexpr float kOrbitalRadius = 3.0f;
constexpr float kPlanetThrustForce = 2.0e14f;
const glm::quat kOrbitalFrame = glm::angleAxis(0.73f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
const glm::vec3 kOrbitalAxis = glm::normalize(kOrbitalFrame * glm::vec3(0.0f, 0.0f, 1.0f));
const glm::vec3 kOrbitalNormal = glm::normalize(kOrbitalFrame * glm::vec3(0.0f, 1.0f, 0.0f));
const glm::vec3 kOrbitalTangent = glm::normalize(glm::cross(kOrbitalNormal, kOrbitalAxis));
const float kOrbitalRelativeSpeed = std::sqrt(
    CelestialGravity::kGravitationalConstant * (kOrbitalMassA + kOrbitalMassB) / kOrbitalSeparation);
const glm::vec3 kOrbitalPositionA = kOrbitalBarycentre - kOrbitalAxis * (kOrbitalSeparation * kOrbitalMassB / (kOrbitalMassA + kOrbitalMassB));
const glm::vec3 kOrbitalPositionB = kOrbitalBarycentre + kOrbitalAxis * (kOrbitalSeparation * kOrbitalMassA / (kOrbitalMassA + kOrbitalMassB));
const glm::vec3 kOrbitalVelocityA = -kOrbitalTangent * (kOrbitalRelativeSpeed * kOrbitalMassB / (kOrbitalMassA + kOrbitalMassB));
const glm::vec3 kOrbitalVelocityB = kOrbitalTangent * (kOrbitalRelativeSpeed * kOrbitalMassA / (kOrbitalMassA + kOrbitalMassB));
constexpr float kPlanetFriction = 0.8f;
constexpr float kPlanetRestitution = 0.1f;
constexpr float kRadicalGravityMagnitude = 9.81f;
const glm::vec3 kPlayerSpawnPosition = kPlanetACenter + glm::vec3(0.0f, kPlanetARadius + 3.0f, 0.0f);
constexpr float kPlayerSpawnYawDegrees = 180.0f;
const glm::vec3 kFluidStationBearing = glm::normalize(glm::vec3(0.11f, 1.0f, 0.11f));
const glm::vec3 kFluidTableHalfExtents(0.85f, 0.12f, 0.42f);
const glm::vec3 kFluidTableColor(0.37f, 0.30f, 0.24f);
const glm::vec3 kCupBottomColor(0.55f, 0.67f, 0.73f);
const glm::vec3 kCupWallColor(0.50f, 0.76f, 0.82f);
constexpr float kCupMass = 120.0f;
constexpr float kFluidParticleSpacing = 0.05f;
const glm::vec3 kTerrainPlanetCenter(300.0f, 0.0f, 0.0f);
const glm::vec3 kTerrainPlanetColor(0.39f, 0.50f, 0.30f);
const glm::vec3 kTerrainPickupColor(0.95f, 0.58f, 0.20f);
const glm::vec3 kTerrainPickupHalfExtents(0.45f);
constexpr float kTerrainPickupMass = 2000.0f;
constexpr float kTerrainGravityRegionRadius = 103.0f;
constexpr std::size_t kTerrainMaxWaterParticles = 200;
constexpr float kTerrainGravitationalParameter = kRadicalGravityMagnitude * TerrainDemo::kBaseRadius * TerrainDemo::kBaseRadius;
constexpr float kAtmosphereTopRadius = 110.0f;
constexpr float kAtmosphereReferenceDensity = 0.05f;
constexpr float kAtmospherePolytropicExponent = 1.4f;
constexpr float kSpacecraftDragCoefficient = 1.0f;
const glm::vec3 kFireBlockHalfExtents(0.25f);
constexpr float kFireBlockMass = 5.0f;
const std::array<glm::vec3, 3> kFireBlockShipLocalPositions{{{-1.1f, 0.53f, -1.2f}, {-1.1f, 0.53f, -0.65f}, {1.4f, 0.53f, 2.2f}}};
const std::array<glm::vec3, 3> kFireBlockColors{{{0.65f, 0.33f, 0.19f}, {0.75f, 0.43f, 0.22f}, {0.58f, 0.42f, 0.24f}}};
constexpr float kDynamicObjectFriction = 0.6f;
constexpr float kDynamicObjectRestitution = 0.15f;
constexpr float kCubeHalfExtent = 0.5f;
constexpr float kCubeMass = 5.0f;
constexpr float kSphereObjectRadius = 0.5f;
constexpr float kSphereObjectMass = 4.0f;
const glm::vec3 kCubeColor(0.85f, 0.35f, 0.2f);
const glm::vec3 kSphereObjectColor(0.9f, 0.8f, 0.2f);
const glm::vec3 kPlankCenter(0.0f, 17.0f, 27.5f);
const glm::vec3 kPlankHalfExtents(6.0f, 1.0f, 20.0f);
const glm::vec3 kPlankColor(0.55f, 0.5f, 0.4f);
constexpr float kPlankFriction = 0.8f;
constexpr float kPlankRestitution = 0.1f;
const glm::vec3 kPlankGravityRegionHalfExtents(8.0f, 7.0f, 22.0f);
constexpr float kPlanetGravityRegionRadius = 26.0f;
constexpr float kFlyingPrimitiveMass = 80.0f;
constexpr float kFlyingPrimitiveFriction = 0.8f;
constexpr float kFlyingPrimitiveRestitution = 0.1f;
const glm::vec3 kFlyingPrimitiveHalfExtents(2.0f, 0.25f, 3.0f);
const glm::vec3 kFlyingPrimitiveColor(0.75f, 0.75f, 0.8f);
const glm::vec3 kFlyingPrimitiveSpawnPosition(0.0f, 19.0f, 24.0f);
const glm::vec3 kBeaconPosition = kPlanetACenter + glm::vec3(-4.0f, kPlanetARadius, 3.0f);
constexpr float kStepRise = 0.3f;
constexpr float kStepRun = 0.7f;
constexpr float kStepHalfWidth = 1.0f;
constexpr int kStepCount = 4;
const glm::vec3 kStaircaseColor(0.5f, 0.45f, 0.55f);
const glm::vec3 kStaircaseBearing = glm::normalize(glm::vec3(0.0f, 0.95f, 0.3f));
const glm::vec3 kRampHalfExtents(1.5f, 0.15f, 2.5f);
const glm::vec3 kRampColor(0.45f, 0.5f, 0.55f);
const glm::vec3 kRampBearing = glm::normalize(glm::vec3(-0.35f, 0.9f, 0.25f));
constexpr float kRampTiltDegrees = 25.0f;
const glm::vec3 kDoorBearing = glm::normalize(glm::vec3(0.55f, 0.9f, 0.05f));
const glm::vec3 kDoorHalfExtents(1.1f, 1.0f, 0.1f);
const glm::vec3 kDoorColor(0.55f, 0.38f, 0.22f);
const glm::vec3 kSwitchBearing = glm::normalize(glm::vec3(0.75f, 0.85f, 0.05f));
const glm::vec3 kSwitchHalfExtents(0.06f, 0.18f, 0.04f);
const glm::vec3 kSwitchColor(0.75f, 0.72f, 0.65f);
const glm::vec3 kLampColor(3.2f, 2.6f, 1.6f);
constexpr float kLampRange = 10.0f;

glm::vec3 PointAboveSphere(const glm::vec3& center, float radius, const glm::vec3& direction, float height) {
    return center + glm::normalize(direction) * (radius + height);
}
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
    return glm::angleAxis(std::acos(d), glm::normalize(glm::cross(from, to)));
}
std::vector<CompoundBox> MakeOpenCupBoxes() {
    constexpr float c = 0.153f;
    return {{{0.0f, -c, 0.0f}, {0.18f, 0.015f, 0.18f}},
            {{-0.165f, 0.195f - c, 0.0f}, {0.015f, 0.18f, 0.18f}},
            {{0.165f, 0.195f - c, 0.0f}, {0.015f, 0.18f, 0.18f}},
            {{0.0f, 0.195f - c, -0.165f}, {0.15f, 0.18f, 0.015f}},
            {{0.0f, 0.195f - c, 0.165f}, {0.15f, 0.18f, 0.015f}}};
}

SceneObject& Add(Scene& s, const std::string& name, const glm::vec3& pos, const glm::quat& rot = glm::quat(1, 0, 0, 0)) {
    SceneObject& o = s.CreateObject(name);
    o.transform.position = pos;
    o.transform.rotation = rot;
    return o;
}
SceneRenderComponent RBox(const glm::vec3& he, const glm::vec3& color) {
    SceneRenderComponent r; r.shape = SceneShape::Box; r.halfExtents = he; r.color = color; return r;
}
SceneRenderComponent RSphere(float radius, const glm::vec3& color) {
    SceneRenderComponent r; r.shape = SceneShape::Sphere; r.radius = radius; r.color = color; return r;
}
SceneBodyComponent BStaticBox(const glm::vec3& he, float friction, float restitution) {
    SceneBodyComponent b; b.motion = SceneBodyMotion::Static; b.shape = SceneShape::Box; b.halfExtents = he;
    b.friction = friction; b.restitution = restitution; return b;
}
SceneBodyComponent BStaticSphere(float r, float friction, float restitution) {
    SceneBodyComponent b; b.motion = SceneBodyMotion::Static; b.shape = SceneShape::Sphere; b.radius = r;
    b.friction = friction; b.restitution = restitution; return b;
}
SceneBodyComponent BDynBox(const glm::vec3& he, float mass, float friction, float restitution) {
    SceneBodyComponent b; b.motion = SceneBodyMotion::Dynamic; b.shape = SceneShape::Box; b.halfExtents = he;
    b.mass = mass; b.friction = friction; b.restitution = restitution; return b;
}
SceneBodyComponent BDynSphere(float r, float mass, float friction, float restitution) {
    SceneBodyComponent b; b.motion = SceneBodyMotion::Dynamic; b.shape = SceneShape::Sphere; b.radius = r;
    b.mass = mass; b.friction = friction; b.restitution = restitution; return b;
}

// Classic: fluidMode 0 normal, 1 rotated station gravity, 2 zero.
Scene MakeClassic(int fluidMode) {
    Scene s;
    s.Settings().name = fluidMode == 0 ? "Classic abuse chamber"
                        : fluidMode == 1 ? "Classic abuse chamber (rotated fluid gravity)"
                                         : "Classic abuse chamber (zero fluid gravity)";
    s.Settings().fluidScale = 1.0f;

    const glm::quat stationRot = RotationAligningUpTo(kFluidStationBearing);
    const glm::vec3 stationSurface = PointAboveSphere(kPlanetACenter, kPlanetARadius, kFluidStationBearing, 0.0f);
    const glm::vec3 tablePos = stationSurface + kFluidStationBearing * kFluidTableHalfExtents.y;

    // Gravity regions in registration order: fluid station override
    // (variants only), plank, planet A, planet B.
    if (fluidMode != 0) {
        const glm::vec3 g = fluidMode == 2 ? glm::vec3(0.0f)
            : glm::angleAxis(glm::radians(50.0f), stationRot * glm::vec3(0.0f, 0.0f, 1.0f)) *
                  (-kFluidStationBearing * kRadicalGravityMagnitude);
        // Uniform gravity points along the object's local -Y: orient the
        // region object so -Y is g's direction; zero keeps identity.
        glm::quat rot(1, 0, 0, 0);
        float magnitude = 0.0f;
        if (glm::length(g) > 0.0f) {
            magnitude = glm::length(g);
            rot = RotationAligningUpTo(-g);
        }
        SceneObject& o = Add(s, "Fluid station gravity", tablePos, rot);
        SceneGravityComponent grav;
        grav.kind = SceneGravityKind::Uniform;
        grav.magnitude = magnitude;
        grav.regionShape = SceneRegionShape::Box;
        grav.regionHalfExtents = glm::vec3(2.2f, 2.5f, 1.8f);
        o.gravity = grav;
    }
    {
        SceneObject& o = Add(s, "The plank", kPlankCenter);
        o.render = RBox(kPlankHalfExtents, kPlankColor);
        o.body = BStaticBox(kPlankHalfExtents, kPlankFriction, kPlankRestitution);
        SceneGravityComponent g; g.kind = SceneGravityKind::Uniform; g.magnitude = 9.81f;
        g.regionShape = SceneRegionShape::Box; g.regionHalfExtents = kPlankGravityRegionHalfExtents;
        o.gravity = g;
    }
    {
        SceneObject& o = Add(s, "Planet A", kPlanetACenter);
        o.render = RSphere(kPlanetARadius, kPlanetAColor);
        o.body = BStaticSphere(kPlanetARadius, kPlanetFriction, kPlanetRestitution);
        SceneGravityComponent g; g.kind = SceneGravityKind::Radial; g.magnitude = kRadicalGravityMagnitude;
        g.regionShape = SceneRegionShape::Sphere; g.regionRadius = kPlanetGravityRegionRadius;
        o.gravity = g;
    }
    {
        SceneObject& o = Add(s, "Planet B", kPlanetBCenter);
        o.render = RSphere(kPlanetBRadius, kPlanetBColor);
        o.body = BStaticSphere(kPlanetBRadius, kPlanetFriction, kPlanetRestitution);
        SceneGravityComponent g; g.kind = SceneGravityKind::Radial; g.magnitude = kRadicalGravityMagnitude;
        g.regionShape = SceneRegionShape::Sphere; g.regionRadius = kPlanetGravityRegionRadius;
        o.gravity = g;
    }
    {
        SceneObject& o = Add(s, "Fluid table", tablePos, stationRot);
        o.render = RBox(kFluidTableHalfExtents, kFluidTableColor);
        o.body = BStaticBox(kFluidTableHalfExtents, 0.8f, 0.0f);
    }
    // Staircase + ramp
    {
        const glm::vec3 base = PointAboveSphere(kPlanetACenter, kPlanetARadius, kStaircaseBearing, 0.0f);
        const glm::quat rot = RotationAligningUpTo(kStaircaseBearing);
        const glm::vec3 climb = glm::normalize(rot * glm::vec3(0.0f, 0.0f, 1.0f));
        for (int i = 0; i < kStepCount; ++i) {
            const float top = static_cast<float>(i + 1) * kStepRise;
            const glm::vec3 center = base + kStaircaseBearing * (top * 0.5f) +
                                     climb * (static_cast<float>(i) * kStepRun + kStepRun * 0.5f);
            const glm::vec3 he(kStepHalfWidth, top * 0.5f, kStepRun * 0.5f);
            SceneObject& o = Add(s, "Stair step " + std::to_string(i + 1), center, rot);
            o.render = RBox(he, kStaircaseColor);
            o.body = BStaticBox(he, 0.8f, 0.0f);
        }
        const glm::quat rampBase = RotationAligningUpTo(kRampBearing);
        const glm::vec3 rampRight = rampBase * glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::quat rampRot = glm::angleAxis(glm::radians(kRampTiltDegrees), rampRight) * rampBase;
        const glm::vec3 rampCenter = PointAboveSphere(kPlanetACenter, kPlanetARadius, kRampBearing, kRampHalfExtents.y);
        SceneObject& o = Add(s, "Ramp", rampCenter, rampRot);
        o.render = RBox(kRampHalfExtents, kRampColor);
        o.body = BStaticBox(kRampHalfExtents, 0.8f, 0.0f);
    }
    {
        SceneObject& o = Add(s, "Beacon", kBeaconPosition);
        SceneRenderComponent r; r.shape = SceneShape::Mesh; r.meshAsset = "0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21";
        r.textureAsset = "5e4d3c2b1a0f4e6d9c8b7a6f5e4d3c2b"; r.color = glm::vec3(1.0f);
        o.render = r;
    }
    {
        const glm::quat rot = RotationAligningUpTo(kDoorBearing);
        const glm::vec3 hinge = PointAboveSphere(kPlanetACenter, kPlanetARadius, kDoorBearing, 0.0f);
        SceneObject& o = Add(s, "Door", hinge, rot);
        o.render = RBox(kDoorHalfExtents, kDoorColor);
        SceneDoorComponent d; d.localHingeAxis = glm::vec3(0.0f, 1.0f, 0.0f); d.openAngleDegrees = 100.0f;
        d.angularSpeedDegreesPerSecond = 150.0f;
        o.door = d;
    }
    {
        const glm::quat rot = RotationAligningUpTo(kSwitchBearing);
        const glm::vec3 hinge = PointAboveSphere(kPlanetACenter, kPlanetARadius, kSwitchBearing, 1.1f);
        const glm::vec3 lamp = PointAboveSphere(kPlanetACenter, kPlanetARadius, kSwitchBearing, 2.5f);
        SceneObject& o = Add(s, "Light switch", hinge, rot);
        o.render = RBox(kSwitchHalfExtents, kSwitchColor);
        SceneLightSwitchComponent sw; sw.localHingeAxis = glm::vec3(0.0f, 0.0f, 1.0f); sw.toggleAngleDegrees = 40.0f;
        sw.angularSpeedDegreesPerSecond = 220.0f;
        sw.lampLocalOffset = glm::conjugate(rot) * (lamp - hinge);
        sw.lampColor = kLampColor; sw.lampRange = kLampRange;
        o.lightSwitch = sw;
    }
    {
        SceneObject& o = Add(s, "Player start", kPlayerSpawnPosition);
        ScenePlayerStartComponent p; p.yawDegrees = kPlayerSpawnYawDegrees; p.view = ScenePlayerView::ThirdPerson;
        o.playerStart = p;
    }
    // Six pickable test objects, in the M7-A spawn order.
    struct Spawn { bool box; glm::vec3 pos; std::string name; };
    const Spawn spawns[] = {
        {true, PointAboveSphere(kPlanetACenter, kPlanetARadius, glm::vec3(1.0f, 1.0f, -0.5f), 2.0f), "Cube (Planet A)"},
        {false, PointAboveSphere(kPlanetACenter, kPlanetARadius, glm::vec3(0.8f, 0.3f, -0.9f), 0.05f), "Ball (Planet A)"},
        {true, glm::vec3(-3.0f, 21.0f, 18.0f), "Cube (plank)"},
        {false, glm::vec3(3.0f, 19.0f, 40.0f), "Ball (plank)"},
        {true, PointAboveSphere(kPlanetBCenter, kPlanetBRadius, glm::vec3(1.0f, 1.0f, 0.5f), 2.0f), "Cube (Planet B)"},
        {false, PointAboveSphere(kPlanetBCenter, kPlanetBRadius, glm::vec3(0.8f, 0.3f, 0.9f), 0.05f), "Ball (Planet B)"},
    };
    for (const Spawn& sp : spawns) {
        SceneObject& o = Add(s, sp.name, sp.pos);
        if (sp.box) {
            o.render = RBox(glm::vec3(kCubeHalfExtent), kCubeColor);
            o.body = BDynBox(glm::vec3(kCubeHalfExtent), kCubeMass, kDynamicObjectFriction, kDynamicObjectRestitution);
        } else {
            o.render = RSphere(kSphereObjectRadius, kSphereObjectColor);
            o.body = BDynSphere(kSphereObjectRadius, kSphereObjectMass, kDynamicObjectFriction, kDynamicObjectRestitution);
        }
        o.body->pickable = true;
    }
    {
        SceneObject& o = Add(s, "Spacecraft", kFlyingPrimitiveSpawnPosition);
        SceneRenderComponent r; r.shape = SceneShape::Mesh; r.meshAsset = "9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5"; r.color = kFlyingPrimitiveColor;
        o.render = r;
        o.body = BDynBox(kFlyingPrimitiveHalfExtents, kFlyingPrimitiveMass, kFlyingPrimitiveFriction, kFlyingPrimitiveRestitution);
        SceneVehicleComponent v; v.gravity = SceneVehicleGravity::Local; v.headlight = true; v.navigationLights = true;
        v.dragCoefficient = kSpacecraftDragCoefficient; v.initialPilotAttached = false;
        o.vehicle = v;
    }
    {
        SceneObject& a = Add(s, "Orbital body A", kOrbitalPositionA);
        a.render = RSphere(kOrbitalRadius, glm::vec3(0.25f, 0.7f, 0.95f));
        a.body = BDynSphere(kOrbitalRadius, kOrbitalMassA, kDynamicObjectFriction, kDynamicObjectRestitution);
        a.body->initialLinearVelocity = kOrbitalVelocityA;
        SceneCelestialComponent c; c.operatorThrustForce = kPlanetThrustForce; a.celestial = c;
        SceneObject& b = Add(s, "Orbital body B", kOrbitalPositionB);
        b.render = RSphere(kOrbitalRadius, glm::vec3(0.95f, 0.55f, 0.2f));
        b.body = BDynSphere(kOrbitalRadius, kOrbitalMassB, kDynamicObjectFriction, kDynamicObjectRestitution);
        b.body->initialLinearVelocity = kOrbitalVelocityB;
        SceneCelestialComponent cb; b.celestial = cb;
    }
    glm::vec3 cupAPosition(0.0f);
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? -0.34f : 0.34f;
        const glm::vec3 local(side, kFluidTableHalfExtents.y + 0.168f, 0.0f);
        const glm::vec3 pos = tablePos + stationRot * local;
        if (i == 0) cupAPosition = pos;
        SceneObject& o = Add(s, i == 0 ? "Cup A" : "Cup B", pos, stationRot);
        SceneRenderComponent r; r.shape = SceneShape::Compound; r.color = kCupBottomColor;
        r.secondaryColor = kCupWallColor; r.secondaryAlpha = 0.35f;
        o.render = r;
        SceneBodyComponent b; b.motion = SceneBodyMotion::Dynamic; b.shape = SceneShape::Compound;
        b.compoundBoxes = MakeOpenCupBoxes(); b.mass = kCupMass; b.friction = 0.8f; b.restitution = 0.0f;
        b.pickable = true;
        o.body = b;
    }
    {
        // Cup A's water: lattice x,z in -2..2, y 0..4, at local
        // (0, 0.05 - 0.153, 0) from the cup, in the station frame.
        SceneObject& o = Add(s, "Cup A water", cupAPosition + stationRot * glm::vec3(0.0f, 0.05f - 0.153f, 0.0f), stationRot);
        SceneFluidVolumeComponent f; f.spacing = kFluidParticleSpacing; f.countX = f.countY = f.countZ = 5;
        o.fluidVolume = f;
    }
    return s;
}

// Terrain: pass=true for the atmospheric orbital pass preset.
Scene MakeTerrain(bool rotated, bool pass) {
    Scene s;
    s.Settings().name = std::string("Terrain planet") + (pass ? " — atmospheric pass" : "") + (rotated ? " (rotated)" : "");
    s.Settings().fluidScale = 10.0f;
    const glm::quat R = rotated ? glm::angleAxis(glm::radians(47.0f), glm::normalize(glm::vec3(1.0f, 0.3f, 2.0f)))
                                : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const std::shared_ptr<const RadialTerrain> surface = TerrainDemo::CreateSurface();
    const glm::vec3 C = kTerrainPlanetCenter;
    {
        SceneObject& o = Add(s, "Terrain planet", C, R);
        SceneRenderComponent r; r.shape = SceneShape::Terrain; r.color = kTerrainPlanetColor;
        o.render = r;
        SceneBodyComponent b; b.motion = SceneBodyMotion::Static; b.shape = SceneShape::Terrain;
        b.terrainSurface = "m25-radial"; b.friction = kPlanetFriction; b.restitution = kPlanetRestitution;
        o.body = b;
        SceneGravityComponent g; g.kind = SceneGravityKind::Radial; g.magnitude = kRadicalGravityMagnitude;
        g.regionShape = SceneRegionShape::Sphere; g.regionRadius = kTerrainGravityRegionRadius;
        o.gravity = g;
        SceneCelestialComponent c; c.gravitationalParameter = kTerrainGravitationalParameter; o.celestial = c;
        SceneAtmosphereComponent a; a.referenceRadius = TerrainDemo::kBaseRadius; a.topRadius = kAtmosphereTopRadius;
        a.referenceDensity = kAtmosphereReferenceDensity; a.polytropicExponent = kAtmospherePolytropicExponent;
        a.oxidizerMassFraction = 0.21f; a.referenceTemperatureKelvin = 300.0f;
        o.atmosphere = a;
    }
    // Ship placement
    const glm::vec3 shipSurfaceLocal = TerrainDemo::LocalPointAbove(*surface, -3.0f, -3.0f, 0.5f);
    const glm::vec3 orbitRadial = R * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 orbitTangent = R * glm::vec3(1.0f, 0.0f, 0.0f);
    constexpr float apo = 130.0f, peri = 100.0f;
    const float a = 0.5f * (apo + peri);
    const float speed = std::sqrt(kTerrainGravitationalParameter * (2.0f / apo - 1.0f / a));
    const glm::vec3 shipPos = pass ? C + orbitRadial * apo : C + R * shipSurfaceLocal;
    const glm::quat shipRot = pass ? glm::quatLookAt(orbitTangent, orbitRadial)
                                   : R * RotationAligningUpTo(surface->Sample(shipSurfaceLocal).outwardNormal);
    const glm::vec3 shipVel = pass ? orbitTangent * speed : glm::vec3(0.0f);
    const glm::vec3 playerSpawn = pass ? shipPos + shipRot * glm::vec3(0.0f, 1.25f, 0.0f)
        : C + R * TerrainDemo::LocalPointAbove(*surface, TerrainDemo::kBasinAX, -7.0f, 3.0f);
    {
        SceneObject& o = Add(s, "Player start", playerSpawn);
        ScenePlayerStartComponent p; p.yawDegrees = pass ? 0.0f : kPlayerSpawnYawDegrees; p.view = ScenePlayerView::FirstPerson;
        o.playerStart = p;
    }
    {
        const glm::vec3 pos = C + R * TerrainDemo::LocalPointAbove(*surface, TerrainDemo::kBasinAX - 2.0f, -5.5f, 1.4f);
        SceneObject& o = Add(s, "Dense block", pos);
        o.render = RBox(kTerrainPickupHalfExtents, kTerrainPickupColor);
        o.body = BDynBox(kTerrainPickupHalfExtents, kTerrainPickupMass, 0.9f, kDynamicObjectRestitution);
        o.body->pickable = true;
    }
    if (!pass) {
        const char* names[3] = {"Fuel A", "Fuel B", "Far fuel"};
        for (int i = 0; i < 3; ++i) {
            const glm::vec3 pos = shipPos + shipRot * kFireBlockShipLocalPositions[i];
            SceneObject& o = Add(s, names[i], pos, shipRot);
            o.render = RBox(kFireBlockHalfExtents, kFireBlockColors[i]);
            o.body = BDynBox(kFireBlockHalfExtents, kFireBlockMass, kDynamicObjectFriction, kDynamicObjectRestitution);
            o.body->pickable = true;
            SceneCombustibleComponent c; c.heatCapacityJPerK = 150.0f; c.initialFuelMassKg = 0.12f;
            c.ignitionTemperatureK = 550.0f; c.maximumFuelRateKgPerSecond = 0.003f;
            c.radiativeAreaSquareMeters = 1.5f; c.retainedCombustionHeatFraction = 0.75f;
            o.combustible = c;
        }
    }
    {
        SceneObject& o = Add(s, "Spacecraft", shipPos, shipRot);
        SceneRenderComponent r; r.shape = SceneShape::Mesh; r.meshAsset = "9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5"; r.color = kFlyingPrimitiveColor;
        o.render = r;
        o.body = BDynBox(kFlyingPrimitiveHalfExtents, kFlyingPrimitiveMass, kFlyingPrimitiveFriction, kFlyingPrimitiveRestitution);
        o.body->initialLinearVelocity = shipVel;
        SceneVehicleComponent v; v.gravity = SceneVehicleGravity::Celestial; v.headlight = true; v.navigationLights = true;
        v.dragCoefficient = kSpacecraftDragCoefficient; v.initialPilotAttached = pass;
        o.vehicle = v;
    }
    {
        const glm::vec3 localCenter = TerrainDemo::LocalPointAbove(*surface, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 0.65f);
        const glm::vec3 source = TerrainDemo::LocalPointAbove(*surface, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 2.8f);
        SceneObject& o = Add(s, "Lake", C + R * localCenter, R);
        SceneFluidVolumeComponent f; f.spacing = TerrainDemo::kWaterSpacing; f.countX = f.countY = f.countZ = 5;
        f.emitter = true; f.emitterLocalOffset = source - localCenter; f.maxParticles = static_cast<int>(kTerrainMaxWaterParticles);
        o.fluidVolume = f;
    }
    return s;
}

// Milestone 29 demonstration: a flat world with a distance policy, a row
// of managed crates stretching away from the start, and two free-flying
// managed spheres above the gravity region (zero g) whose inertial motion
// continues coarsely when they are far away.
Scene MakeFidelityDemo() {
    Scene s;
    s.Settings().name = "Fidelity demonstration";
    s.Settings().fidelityPolicy = SceneFidelityPolicy::Distance;
    s.Settings().fidelityFullRadius = 25.0f;
    s.Settings().fidelityCoarseRadius = 70.0f;
    {
        SceneObject& g = Add(s, "Ground", glm::vec3(0.0f, -1.0f, 100.0f));
        g.render = RBox(glm::vec3(60.0f, 1.0f, 160.0f), glm::vec3(0.36f, 0.42f, 0.30f));
        g.body = BStaticBox(glm::vec3(60.0f, 1.0f, 160.0f), 0.8f, 0.05f);
        SceneGravityComponent grav; grav.kind = SceneGravityKind::Uniform; grav.magnitude = 9.81f;
        grav.regionShape = SceneRegionShape::Box; grav.regionHalfExtents = glm::vec3(80.0f, 30.0f, 180.0f);
        g.gravity = grav;
    }
    {
        SceneObject& o = Add(s, "Player start", glm::vec3(0.0f, 1.0f, -3.0f));
        ScenePlayerStartComponent p; p.yawDegrees = 180.0f; p.view = ScenePlayerView::ThirdPerson;
        o.playerStart = p;
    }
    for (int i = 0; i < 60; ++i) {
        const float z = 6.0f + static_cast<float>(i) * 4.0f;
        const float x = (i % 3 - 1) * 4.0f;
        SceneObject& c = Add(s, "Managed crate " + std::to_string(i + 1), glm::vec3(x, 0.5f, z));
        const float t = static_cast<float>(i) / 59.0f;
        c.render = RBox(glm::vec3(0.5f), glm::vec3(0.85f - 0.5f * t, 0.35f + 0.4f * t, 0.2f + 0.6f * t));
        c.body = BDynBox(glm::vec3(0.5f), 5.0f, 0.6f, 0.15f);
        c.body->pickable = true;
        c.body->managed = true;
    }
    for (int i = 0; i < 4; ++i) {
        SceneObject& c = Add(s, "Local crate " + std::to_string(i + 1), glm::vec3(-6.0f + i * 1.5f, 0.45f, 2.0f));
        c.render = RBox(glm::vec3(0.45f), glm::vec3(0.75f, 0.75f, 0.8f));
        c.body = BDynBox(glm::vec3(0.45f), 5.0f, 0.6f, 0.15f);
        c.body->pickable = true;
    }
    {
        SceneObject& a = Add(s, "Satellite A", glm::vec3(-20.0f, 45.0f, 20.0f));
        a.render = RSphere(1.2f, glm::vec3(0.25f, 0.7f, 0.95f));
        a.body = BDynSphere(1.2f, 40.0f, 0.5f, 0.1f);
        a.body->initialLinearVelocity = glm::vec3(0.8f, 0.0f, 0.5f);
        a.body->managed = true;
        SceneObject& b = Add(s, "Satellite B", glm::vec3(20.0f, 50.0f, 30.0f));
        b.render = RSphere(1.2f, glm::vec3(0.95f, 0.55f, 0.2f));
        b.body = BDynSphere(1.2f, 40.0f, 0.5f, 0.1f);
        b.body->initialLinearVelocity = glm::vec3(-0.6f, 0.0f, 0.9f);
        b.body->managed = true;
    }
    {
        SceneObject& l = Add(s, "Lamp", glm::vec3(0.0f, 6.0f, 4.0f));
        SceneLightComponent light; light.kind = SceneLightKind::Point; light.color = glm::vec3(3.0f, 2.8f, 2.4f); light.range = 25.0f;
        l.light = light;
    }
    return s;
}

// Milestone 29: the "tiny conventional game" case — one small flat map,
// fifty entities, no planets, no policy, nothing managed.
Scene MakeFlatPlayground() {
    Scene s;
    s.Settings().name = "Flat playground";
    {
        SceneObject& g = Add(s, "Ground", glm::vec3(0.0f, -1.0f, 0.0f));
        g.render = RBox(glm::vec3(25.0f, 1.0f, 25.0f), glm::vec3(0.40f, 0.44f, 0.36f));
        g.body = BStaticBox(glm::vec3(25.0f, 1.0f, 25.0f), 0.8f, 0.05f);
        SceneGravityComponent grav; grav.kind = SceneGravityKind::Uniform; grav.magnitude = 9.81f;
        grav.regionShape = SceneRegionShape::Box; grav.regionHalfExtents = glm::vec3(40.0f, 30.0f, 40.0f);
        g.gravity = grav;
    }
    {
        SceneObject& o = Add(s, "Player start", glm::vec3(0.0f, 1.0f, -8.0f));
        ScenePlayerStartComponent p; p.yawDegrees = 180.0f; p.view = ScenePlayerView::ThirdPerson;
        o.playerStart = p;
    }
    for (int i = 0; i < 50; ++i) {
        const float x = static_cast<float>(i % 10) * 2.2f - 9.9f;
        const float z = static_cast<float>(i / 10) * 2.2f - 2.0f;
        if (i % 3 == 0) {
            SceneObject& b = Add(s, "Ball " + std::to_string(i + 1), glm::vec3(x, 0.4f, z));
            b.render = RSphere(0.4f, glm::vec3(0.9f, 0.8f, 0.2f));
            b.body = BDynSphere(0.4f, 3.0f, 0.6f, 0.2f);
            b.body->pickable = true;
        } else {
            SceneObject& c = Add(s, "Crate " + std::to_string(i + 1), glm::vec3(x, 0.45f, z));
            c.render = RBox(glm::vec3(0.45f), glm::vec3(0.85f, 0.35f, 0.2f));
            c.body = BDynBox(glm::vec3(0.45f), 5.0f, 0.6f, 0.15f);
            c.body->pickable = true;
        }
    }
    {
        SceneObject& l = Add(s, "Lamp", glm::vec3(0.0f, 5.0f, 0.0f));
        SceneLightComponent light; light.kind = SceneLightKind::Point; light.color = glm::vec3(2.5f, 2.3f, 2.0f); light.range = 20.0f;
        l.light = light;
    }
    return s;
}

// Milestone 30: the tiny flat game project — ground, player, a few boxes,
// one light, one door. No planets, no fluid, no policy: what a small
// conventional game made with Judas looks like.
Scene MakeTinyGame() {
    Scene s;
    s.Settings().name = "Tiny game";
    {
        SceneObject& g = Add(s, "Ground", glm::vec3(0.0f, -0.5f, 0.0f));
        g.render = RBox(glm::vec3(20.0f, 0.5f, 20.0f), glm::vec3(0.40f, 0.44f, 0.36f));
        g.body = BStaticBox(glm::vec3(20.0f, 0.5f, 20.0f), 0.8f, 0.05f);
        SceneGravityComponent grav; grav.kind = SceneGravityKind::Uniform; grav.magnitude = 9.81f;
        grav.regionShape = SceneRegionShape::Box; grav.regionHalfExtents = glm::vec3(30.0f, 30.0f, 30.0f);
        g.gravity = grav;
    }
    {
        SceneObject& o = Add(s, "Player start", glm::vec3(0.0f, 1.0f, 6.0f));
        ScenePlayerStartComponent p; p.yawDegrees = 0.0f; p.view = ScenePlayerView::ThirdPerson;
        o.playerStart = p;
    }
    const glm::vec3 boxPositions[3] = {{-2.0f, 0.5f, 0.0f}, {0.0f, 0.5f, -1.0f}, {2.0f, 0.5f, 0.5f}};
    const glm::vec3 boxColors[3] = {{0.85f, 0.35f, 0.2f}, {0.2f, 0.55f, 0.85f}, {0.85f, 0.75f, 0.2f}};
    for (int i = 0; i < 3; ++i) {
        SceneObject& b = Add(s, "Box " + std::to_string(i + 1), boxPositions[i]);
        b.render = RBox(glm::vec3(0.5f), boxColors[i]);
        b.body = BDynBox(glm::vec3(0.5f), 5.0f, 0.6f, 0.15f);
        b.body->pickable = true;
    }
    {
        SceneObject& w = Add(s, "Wall", glm::vec3(0.0f, 1.0f, -6.0f));
        w.render = RBox(glm::vec3(4.0f, 1.0f, 0.2f), glm::vec3(0.5f, 0.5f, 0.55f));
        w.body = BStaticBox(glm::vec3(4.0f, 1.0f, 0.2f), 0.8f, 0.05f);
    }
    {
        SceneObject& d = Add(s, "Door", glm::vec3(4.0f, 1.0f, -6.0f));
        d.render = RBox(glm::vec3(1.0f, 1.0f, 0.1f), glm::vec3(0.55f, 0.38f, 0.22f));
        SceneDoorComponent door; door.localHingeAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        door.openAngleDegrees = 90.0f; door.angularSpeedDegreesPerSecond = 120.0f;
        d.door = door;
    }
    {
        SceneObject& l = Add(s, "Lamp", glm::vec3(0.0f, 4.0f, 0.0f));
        SceneLightComponent light; light.kind = SceneLightKind::Point; light.color = glm::vec3(2.5f, 2.3f, 2.0f); light.range = 18.0f;
        l.light = light;
    }
    return s;
}

bool Write(const Scene& s, const std::string& path) {
    std::string error;
    if (!SaveSceneToFile(s, path, error)) { std::fprintf(stderr, "%s\n", error.c_str()); return false; }
    std::printf("wrote %s (%zu objects)\n", path.c_str(), s.Objects().size());
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "assets/scenes";
    bool ok = true;
    ok &= Write(MakeClassic(0), dir + "/classic.judas");
    ok &= Write(MakeClassic(1), dir + "/classic_fluid_rotated.judas");
    ok &= Write(MakeClassic(2), dir + "/classic_fluid_zero.judas");
    ok &= Write(MakeTerrain(false, false), dir + "/terrain.judas");
    ok &= Write(MakeTerrain(true, false), dir + "/terrain_rotated.judas");
    ok &= Write(MakeTerrain(false, true), dir + "/terrain_atmospheric_pass.judas");
    ok &= Write(MakeTerrain(true, true), dir + "/terrain_atmospheric_pass_rotated.judas");
    ok &= Write(MakeFidelityDemo(), dir + "/fidelity_demo.judas");
    ok &= Write(MakeFlatPlayground(), dir + "/flat_playground.judas");
    if (argc > 2) ok &= Write(MakeTinyGame(), std::string(argv[2]) + "/main.judas");
    return ok ? 0 : 1;
}
