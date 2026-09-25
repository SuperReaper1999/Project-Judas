#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "CollisionShapes.h"

// Milestone 28: Judas's authored scene representation.
//
// A Scene is AUTHORED content only — what an editor or a scene file says
// the world should look like before simulation starts. It is deliberately
// not the running world: no PhysicsWorld handle, no GPU mesh handle, no
// velocity read back from an integrator, no fluid particle position, no
// thermal temperature ever lives here. RuntimeWorld (src/RuntimeWorld.h)
// instantiates a Scene into those engine systems and owns everything
// transient; stopping Play simply discards that instance and the Scene is
// untouched. See docs/ARCHITECTURE.md, "Milestone 28, Authored vs runtime
// state."
//
// This is not an entity/component framework. A SceneObject is a plain
// struct with a fixed, small set of optional component structs — exactly
// the engine capabilities that exist today, no registry, no dynamic
// component types, no scripting. Names inside this file are engine
// concepts (a body, a light, a gravity region); nothing here knows that a
// particular sphere is "Planet A" or a particular box is "the plank" —
// those are scene content, authored in a scene file.

using SceneObjectId = std::uint64_t;
constexpr SceneObjectId kInvalidSceneObjectId = 0;

struct SceneTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    // Applies to rendering of mesh shapes only. Collision primitives and
    // box/sphere renderables carry explicit half-extents/radii, so a
    // physics body is never silently rescaled by a visual-only value.
    glm::vec3 scale{1.0f};
};

enum class SceneShape { Box, Sphere, Compound, Mesh, Terrain };

// What Renderer draws for the object. Compound and Terrain reuse the body
// component's geometry (there is nothing sensible to draw otherwise);
// Box/Sphere/Mesh are self-contained and need no body at all.
struct SceneRenderComponent {
    SceneShape shape = SceneShape::Box;
    glm::vec3 halfExtents{0.5f};   // Box
    float radius = 0.5f;           // Sphere
    glm::vec3 color{0.8f};
    float alpha = 1.0f;
    // Compound only: colour/alpha for every child box after the first
    // (an open container drawn with an opaque base and translucent walls).
    glm::vec3 secondaryColor{0.8f};
    float secondaryAlpha = 1.0f;
    // Mesh only: asset paths relative to the working directory, the same
    // convention every existing asset load already uses. Empty texture
    // path draws untextured.
    std::string meshPath;
    std::string texturePath;
};

enum class SceneBodyMotion { Static, Dynamic };

struct SceneBodyComponent {
    SceneBodyMotion motion = SceneBodyMotion::Static;
    SceneShape shape = SceneShape::Box;  // Box, Sphere, Compound or Terrain
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    std::vector<CompoundBox> compoundBoxes;
    // Terrain: identifier of a surface the engine can construct (see
    // src/TerrainLibrary.h). A scene never embeds the elevation function.
    std::string terrainSurface;
    float mass = 1.0f;  // Dynamic only
    float friction = 0.6f;
    float restitution = 0.1f;
    glm::vec3 initialLinearVelocity{0.0f};  // Dynamic only
    // Gameplay eligibility for the existing M18 pick-up/throw interaction.
    bool pickable = false;
};

enum class SceneGravityKind { Radial, Uniform };
enum class SceneRegionShape { Sphere, Box };

// A bounded local gravity context, centred on the object. Radial pulls
// toward the object's position (RadicalGravity); Uniform pulls along the
// object's local -Y (FaithfulGravity when that is world -Y at 9.81 m/s^2,
// UniformGravity otherwise). Regions are registered with GravityContextMap
// in scene order, so an earlier object wins where two regions overlap.
struct SceneGravityComponent {
    SceneGravityKind kind = SceneGravityKind::Radial;
    float magnitude = 9.81f;
    SceneRegionShape regionShape = SceneRegionShape::Sphere;
    float regionRadius = 10.0f;
    glm::vec3 regionHalfExtents{5.0f};
};

enum class SceneLightKind { Point, Spot };

// A standalone dynamic light at the object's transform; a spot light
// faces the object's local -Z.
struct SceneLightComponent {
    SceneLightKind kind = SceneLightKind::Point;
    glm::vec3 color{1.0f};
    float range = 10.0f;
    float innerConeDegrees = 15.0f;
    float outerConeDegrees = 25.0f;
};

// The M16 hinged door. Uses the render component's box half-extents and
// colour; Door owns its own static physics body, so no body component.
struct SceneDoorComponent {
    glm::vec3 localHingeAxis{0.0f, 1.0f, 0.0f};
    float openAngleDegrees = 90.0f;
    float angularSpeedDegreesPerSecond = 120.0f;
};

// The M16 lever that toggles one lamp. Lamp offset is in the object's
// local frame.
struct SceneLightSwitchComponent {
    glm::vec3 localHingeAxis{0.0f, 0.0f, 1.0f};
    float toggleAngleDegrees = 40.0f;
    float angularSpeedDegreesPerSecond = 220.0f;
    glm::vec3 lampLocalOffset{0.0f, 1.0f, 0.0f};
    glm::vec3 lampColor{3.0f};
    float lampRange = 10.0f;
};

enum class SceneVehicleGravity { Local, Celestial };

// The M8/M11/M12 pilotable rigid body. Requires a dynamic box body.
// `Local`: samples the scene's gravity contexts like any other body and
// also joins pairwise celestial gravity. `Celestial`: receives only the
// point-mass fields of static celestial sources (the M26 terrain planet).
struct SceneVehicleComponent {
    SceneVehicleGravity gravity = SceneVehicleGravity::Local;
    bool headlight = true;
    bool navigationLights = true;
    float dragCoefficient = 1.0f;
    // Start the run with the player already secured as pilot (M26 pass).
    bool initialPilotAttached = false;
};

// Newtonian gravitation participant. A dynamic body joins the pairwise
// N-body set using its own mass; a static body is a point-mass source with
// this gravitational parameter acting on Celestial-gravity vehicles.
struct SceneCelestialComponent {
    float gravitationalParameter = 0.0f;  // m^3/s^2, static sources only
    // Operator thrust demonstration (M20): force applied by the P/M/N
    // keys. Zero disables it.
    float operatorThrustForce = 0.0f;
};

// The M26 hydrostatic gas around a celestial object; the object's
// transform is the planet frame.
struct SceneAtmosphereComponent {
    float referenceRadius = 80.0f;
    float topRadius = 110.0f;
    float referenceDensity = 0.05f;
    float polytropicExponent = 1.4f;
    float oxidizerMassFraction = 0.21f;
    float referenceTemperatureKelvin = 300.0f;
};

// The M27 combustible coating on a dynamic body.
struct SceneCombustibleComponent {
    float heatCapacityJPerK = 150.0f;
    float initialFuelMassKg = 0.12f;
    float ignitionTemperatureK = 550.0f;
    float maximumFuelRateKgPerSecond = 0.003f;
    float radiativeAreaSquareMeters = 1.5f;
    float retainedCombustionHeatFraction = 0.75f;
};

// Authored liquid: a lattice of particles centred on the object. The
// solver settings live in SceneSettings; particle mass follows rest
// density and spacing. An emitter adds particles on demand at
// `emitterLocalOffset` until `maxParticles` (M25's held-B water source).
struct SceneFluidVolumeComponent {
    float spacing = 0.05f;
    int countX = 5;
    int countY = 5;
    int countZ = 5;
    bool emitter = false;
    glm::vec3 emitterLocalOffset{0.0f};
    int maxParticles = 0;
};

enum class ScenePlayerView { ThirdPerson, FirstPerson };

// Where the player starts. Exactly one object may carry this.
struct ScenePlayerStartComponent {
    float yawDegrees = 0.0f;
    ScenePlayerView view = ScenePlayerView::ThirdPerson;
};

struct SceneObject {
    SceneObjectId id = kInvalidSceneObjectId;
    std::string name;
    SceneTransform transform;
    std::optional<SceneRenderComponent> render;
    std::optional<SceneBodyComponent> body;
    std::optional<SceneGravityComponent> gravity;
    std::optional<SceneLightComponent> light;
    std::optional<SceneDoorComponent> door;
    std::optional<SceneLightSwitchComponent> lightSwitch;
    std::optional<SceneVehicleComponent> vehicle;
    std::optional<SceneCelestialComponent> celestial;
    std::optional<SceneAtmosphereComponent> atmosphere;
    std::optional<SceneCombustibleComponent> combustible;
    std::optional<SceneFluidVolumeComponent> fluidVolume;
    std::optional<ScenePlayerStartComponent> playerStart;
};

// Scene-wide authored settings.
struct SceneSettings {
    std::string name;
    // M23 absolute world origin of this local scene, metres.
    glm::dvec3 worldOrigin{0.0};
    glm::vec3 sunDirection{0.4f, 0.7f, 0.35f};
    glm::vec3 sunColor{1.0f, 0.98f, 0.92f};
    glm::vec3 ambientColor{0.16f, 0.17f, 0.19f};
    // Multiplies the fluid solver's default length scales (M25 used 10x
    // for the lake). 1 is the M24 cup resolution.
    float fluidScale = 1.0f;
};

class Scene {
public:
    SceneSettings& Settings() { return m_settings; }
    const SceneSettings& Settings() const { return m_settings; }

    const std::vector<SceneObject>& Objects() const { return m_objects; }
    std::vector<SceneObject>& Objects() { return m_objects; }

    // Creates an empty object with a fresh, never-reused id.
    SceneObject& CreateObject(const std::string& name);
    // Inserts an object carrying an explicit id (loading, undo). Fails if
    // the id is invalid or already present.
    bool InsertObject(const SceneObject& object);
    bool DestroyObject(SceneObjectId id);
    SceneObject* Find(SceneObjectId id);
    const SceneObject* Find(SceneObjectId id) const;
    // Reorders `id` one slot earlier/later; order matters for gravity
    // region priority and is therefore authored data.
    bool MoveObject(SceneObjectId id, int delta);

    // The next id CreateObject would hand out; serialized so ids stay
    // unique across save/load even after deletions.
    SceneObjectId NextId() const { return m_nextId; }
    void SetNextId(SceneObjectId nextId);

    void Clear();

private:
    SceneSettings m_settings;
    std::vector<SceneObject> m_objects;
    SceneObjectId m_nextId = 1;
};

// Structural equality of authored state, used by tests and the editor's
// "unsaved changes" tracking. Floats compare exactly.
bool SceneObjectsEqual(const SceneObject& a, const SceneObject& b);
bool ScenesEqual(const Scene& a, const Scene& b);
