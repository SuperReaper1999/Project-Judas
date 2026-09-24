#include "AerodynamicDrag.h"
#include "AtmosphereField.h"
#include "CelestialGravity.h"
#include "DynamicBody.h"
#include "GravityField.h"
#include "FlyingPrimitiveControl.h"
#include "PhysicsWorld.h"
#include "Window.h"
#include "WorldCoordinates.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include <glm/gtc/quaternion.hpp>

namespace {
constexpr float kDt = 1.0f / 60.0f;
constexpr float kShipMass = 80.0f;
constexpr float kOrbitApoapsis = 130.0f;
constexpr float kOrbitPeriapsis = 100.0f;
const glm::vec3 kShipHalfExtents(2.0f, 0.25f, 3.0f);
int gFailures = 0;

void Check(bool condition, const char* message) {
    if (condition) std::printf("  OK   %s\n", message);
    else {
        std::fprintf(stderr, "  FAIL %s\n", message);
        ++gFailures;
    }
}

void CheckNear(float actual, float expected, float tolerance, const char* message) {
    Check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

AtmosphereField MakeAtmosphere() {
    return AtmosphereField(AtmosphereParameters{});
}

float SpecificEnergy(const glm::vec3& relativePosition, const glm::vec3& relativeVelocity,
                     float gravitationalParameter) {
    return 0.5f * glm::dot(relativeVelocity, relativeVelocity) -
           gravitationalParameter / glm::length(relativePosition);
}

float OsculatingApoapsis(const glm::vec3& relativePosition,
                         const glm::vec3& relativeVelocity, float gravitationalParameter) {
    const float energy = SpecificEnergy(relativePosition, relativeVelocity,
                                        gravitationalParameter);
    const glm::vec3 angularMomentum = glm::cross(relativePosition, relativeVelocity);
    const float angularMomentumSquared = glm::dot(angularMomentum, angularMomentum);
    const float eccentricitySquared = 1.0f + 2.0f * energy * angularMomentumSquared /
                                     (gravitationalParameter * gravitationalParameter);
    if (!(energy < 0.0f) || eccentricitySquared < 0.0f) {
        return std::numeric_limits<float>::infinity();
    }
    return -gravitationalParameter / (2.0f * energy) *
           (1.0f + std::sqrt(eccentricitySquared));
}

void TestDragAndOrientation() {
    std::printf("Section A: box drag uses measured gas and orientation\n");
    const AtmosphereField atmosphere = MakeAtmosphere();
    const ReferenceFrame planet;
    PhysicsWorld world;
    Check(world.Init(), "physics world initialized");
    const glm::vec3 position(90.0f, 0.0f, 0.0f);
    const glm::vec3 planetaryAcceleration = CelestialGravity::AccelerationFromPointMass(
        planet.originPosition, atmosphere.Parameters().gravitationalParameter, position);
    CheckNear(planetaryAcceleration.x,
              -atmosphere.Parameters().gravitationalParameter / (90.0f * 90.0f),
              1.0e-5f, "static massive planet supplies the inverse-square test-body pull");
    CheckNear(planetaryAcceleration.y, 0.0f, 1.0e-6f,
              "planetary test-body pull has no preferred transverse axis");
    const BodyHandle ship = world.CreateDynamicBox(
        position, kShipHalfExtents, kShipMass, 0.0f, 0.0f);
    const glm::vec3 velocity(0.0f, 30.0f, 0.0f);
    world.SetLinearVelocity(ship, velocity);

    const AtmosphereSample gas = atmosphere.Sample(position, planet);
    const AerodynamicDragResult broad = ApplyAerodynamicDrag(
        world, ship, atmosphere, planet);
    const float expectedBroadArea = 4.0f * 6.0f;
    const float expectedForce = -0.5f * gas.density * expectedBroadArea *
                                glm::dot(velocity, velocity);
    CheckNear(broad.density, gas.density, 1.0e-7f,
              "aerodynamic coupling reads gas density");
    CheckNear(broad.pressure, gas.pressure, 1.0e-6f,
              "pressure remains an independently sampled gas property");
    CheckNear(broad.projectedArea, expectedBroadArea, 1.0e-4f,
              "broad-side projected area is the box's true 4 x 6 m face");
    CheckNear(broad.relativeAirspeed, 30.0f, 1.0e-5f,
              "relative airspeed comes from body minus gas velocity");
    CheckNear(broad.force.y, expectedForce, std::abs(expectedForce) * 1.0e-5f,
              "drag magnitude follows one-half rho Cd A speed squared");
    Check(glm::dot(broad.force, broad.relativeAirVelocity) < 0.0f &&
              std::abs(broad.force.x) < 1.0e-5f && std::abs(broad.force.z) < 1.0e-5f,
          "drag strictly opposes motion through the gas");
    CheckNear(world.GetLinearVelocity(ship).y, velocity.y, 1.0e-6f,
              "aerodynamics does not directly overwrite velocity");
    world.Step(kDt);
    CheckNear(world.GetLinearVelocity(ship).y,
              velocity.y + expectedForce / kShipMass * kDt, 1.0e-4f,
              "ordinary PhysicsWorld integration turns drag force into acceleration");
    CheckNear(glm::length(world.GetAngularVelocity(ship)), 0.0f, 1.0e-6f,
              "the symmetric centred box receives no invented torque");

    const glm::quat quarterTurn = glm::angleAxis(glm::radians(90.0f),
                                                 glm::vec3(0.0f, 0.0f, 1.0f));
    world.ResetBody(ship, position, quarterTurn);
    world.SetLinearVelocity(ship, velocity);
    const AerodynamicDragResult side = ApplyAerodynamicDrag(
        world, ship, atmosphere, planet);
    CheckNear(side.projectedArea, 0.5f * 6.0f, 1.0e-4f,
              "turning the craft exposes its 0.5 x 6 m side");
    CheckNear(glm::length(side.force) / glm::length(broad.force),
              side.projectedArea / broad.projectedArea, 1.0e-5f,
              "orientation changes drag through actual exposed area");
    world.Shutdown();
}

void TestVacuumFrameAndExit() {
    std::printf("Section B: vacuum, moving gas frame, and continuous exit\n");
    const AtmosphereField atmosphere = MakeAtmosphere();
    PhysicsWorld world;
    Check(world.Init(), "frame/vacuum physics world initialized");
    const BodyHandle ship = world.CreateDynamicBox(
        glm::vec3(90.0f, 0.0f, 0.0f), kShipHalfExtents, kShipMass, 0.0f, 0.0f);
    ReferenceFrame movingPlanet;
    movingPlanet.linearVelocity = glm::vec3(7.0f, 30.0f, -4.0f);
    world.SetLinearVelocity(ship, movingPlanet.linearVelocity);
    const AerodynamicDragResult comoving = ApplyAerodynamicDrag(
        world, ship, atmosphere, movingPlanet);
    CheckNear(comoving.relativeAirspeed, 0.0f, 1.0e-6f,
              "body and translating atmosphere with equal world velocity have no airspeed");
    CheckNear(glm::length(comoving.force), 0.0f, 1.0e-6f,
              "co-motion applies no aerodynamic force");

    movingPlanet.angularVelocity = glm::vec3(0.0f, 0.0f, 0.2f);
    world.SetLinearVelocity(ship, movingPlanet.linearVelocity + glm::vec3(0.0f, 18.0f, 0.0f));
    const AerodynamicDragResult rotatingComotion = ApplyAerodynamicDrag(
        world, ship, atmosphere, movingPlanet);
    CheckNear(rotatingComotion.relativeAirspeed, 0.0f, 1.0e-4f,
              "co-motion includes the rotating planet frame's omega cross r velocity");

    world.ResetBody(ship, glm::vec3(140.0f, 0.0f, 0.0f),
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const glm::vec3 vacuumVelocity(0.0f, 30.0f, 0.0f);
    world.SetLinearVelocity(ship, vacuumVelocity);
    const AerodynamicDragResult vacuum = ApplyAerodynamicDrag(
        world, ship, atmosphere, movingPlanet);
    Check(vacuum.density == 0.0f && vacuum.pressure == 0.0f &&
              vacuum.relativeAirspeed == 0.0f && glm::length(vacuum.force) == 0.0f,
          "distant vacuum has no gas pressure, airspeed, or aerodynamic force");
    world.Step(kDt);
    CheckNear(glm::length(world.GetLinearVelocity(ship) - vacuumVelocity), 0.0f, 1.0e-6f,
              "vacuum preserves inertial linear motion");

    const float top = atmosphere.Parameters().topRadius;
    const float sampleRadii[] = {top - 2.0f, top - 1.0f, top - 0.1f, top};
    float priorForce = std::numeric_limits<float>::infinity();
    for (float radius : sampleRadii) {
        world.ResetBody(ship, glm::vec3(radius, 0.0f, 0.0f),
                        glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        world.SetLinearVelocity(ship, glm::vec3(0.0f, 20.0f, 0.0f));
        const AerodynamicDragResult value = ApplyAerodynamicDrag(
            world, ship, atmosphere, ReferenceFrame{});
        const float force = glm::length(value.force);
        Check(force <= priorForce && std::isfinite(force),
              "aerodynamic force decreases as the craft approaches vacuum");
        priorForce = force;
    }
    CheckNear(priorForce, 0.0f, 1.0e-8f,
              "aerodynamic force reaches zero at the smooth gas boundary");

    // Cross the top in one real physics step. The next step must retain
    // exactly the resulting velocity: the force accumulator cannot carry a
    // stale atmospheric load into vacuum.
    world.ResetBody(ship, glm::vec3(top - 1.0f, 0.0f, 0.0f),
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    world.SetLinearVelocity(ship, glm::vec3(100.0f, 0.0f, 0.0f));
    const AerodynamicDragResult leaving = ApplyAerodynamicDrag(
        world, ship, atmosphere, ReferenceFrame{});
    Check(glm::length(leaving.force) > 0.0f,
          "the outward step starts with a real atmospheric load");
    world.Step(kDt);
    Check(glm::length(world.GetTransform(ship).position) > top,
          "the body physically crosses the gas boundary");
    const glm::vec3 velocityAfterExit = world.GetLinearVelocity(ship);
    const AerodynamicDragResult afterExit = ApplyAerodynamicDrag(
        world, ship, atmosphere, ReferenceFrame{});
    world.Step(kDt);
    Check(glm::length(afterExit.force) == 0.0f &&
              glm::length(world.GetLinearVelocity(ship) - velocityAfterExit) < 1.0e-6f,
          "no atmospheric force lingers on the next vacuum physics step");
    world.Shutdown();
}

struct OrbitalPass {
    glm::vec3 relativePosition{0.0f};
    glm::vec3 relativeVelocity{0.0f};
    float initialEnergy = 0.0f;
    float finalEnergy = 0.0f;
    float finalApoapsis = 0.0f;
    float dragWorkPerMass = 0.0f;
    float peakDrag = 0.0f;
    float peakDensity = 0.0f;
    float minimumRadius = std::numeric_limits<float>::infinity();
    int steps = 0;
    bool entered = false;
    bool exited = false;
};

OrbitalPass SimulatePass(const AtmosphereField& atmosphere, bool enableDrag,
                         const glm::quat& rotation, const glm::vec3& offset,
                         int fixedStepCount = 0) {
    const float mu = atmosphere.Parameters().gravitationalParameter;
    const float semiMajorAxis = 0.5f * (kOrbitApoapsis + kOrbitPeriapsis);
    const float apoapsisSpeed = std::sqrt(mu *
        (2.0f / kOrbitApoapsis - 1.0f / semiMajorAxis));
    ReferenceFrame planet;
    planet.originPosition = offset;
    planet.orientation = rotation;
    PhysicsWorld world;
    Check(world.Init(), "orbital-pass physics world initialized");
    world.CreateStaticSphere(offset, atmosphere.Parameters().referenceRadius, 0.0f, 0.0f);
    const glm::vec3 startPosition = offset + rotation *
        glm::vec3(kOrbitApoapsis, 0.0f, 0.0f);
    const glm::vec3 startVelocity = rotation * glm::vec3(0.0f, apoapsisSpeed, 0.0f);
    const BodyHandle ship = world.CreateDynamicBox(
        startPosition, kShipHalfExtents, kShipMass, 0.0f, 0.0f);
    // The same box that had 24 m^2 broad-side area in Section A is flown
    // approximately nose-first for this shallow pass (2 m^2 at apoapsis).
    // Its orientation is fixed here; no attitude/orbit path is prescribed.
    const glm::quat noseAlongVelocity = glm::angleAxis(
        glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    world.ResetBody(ship, startPosition, rotation * noseAlongVelocity);
    world.SetLinearVelocity(ship, startVelocity);

    OrbitalPass result;
    result.initialEnergy = SpecificEnergy(startPosition - offset, startVelocity, mu);
    bool wasInsideGas = false;
    const int maximumSteps = fixedStepCount > 0 ? fixedStepCount : 3000;
    for (int step = 0; step < maximumSteps; ++step) {
        const BodyTransform pose = world.GetTransform(ship);
        const glm::vec3 position = pose.position - offset;
        const float radius = glm::length(position);
        result.minimumRadius = std::min(result.minimumRadius, radius);
        const glm::vec3 acceleration = CelestialGravity::AccelerationFromPointMass(
            planet.originPosition, mu, pose.position);
        world.ApplyForce(ship, acceleration * kShipMass);
        if (enableDrag) {
            const AerodynamicDragResult drag = ApplyAerodynamicDrag(
                world, ship, atmosphere, planet);
            result.peakDrag = std::max(result.peakDrag, glm::length(drag.force));
            result.peakDensity = std::max(result.peakDensity, drag.density);
            result.dragWorkPerMass += glm::dot(drag.force,
                world.GetLinearVelocity(ship)) * kDt / kShipMass;
            if (drag.density > 0.0f) {
                result.entered = true;
                wasInsideGas = true;
            }
        } else if (atmosphere.Sample(pose.position, planet).density > 0.0f) {
            result.entered = true;
            wasInsideGas = true;
        }
        world.Step(kDt);
        result.steps = step + 1;
        if (fixedStepCount == 0 && wasInsideGas) {
            const glm::vec3 afterPosition = world.GetTransform(ship).position - offset;
            const glm::vec3 afterVelocity = world.GetLinearVelocity(ship);
            const float afterRadius = glm::length(afterPosition);
            if (afterRadius >= atmosphere.Parameters().topRadius &&
                glm::dot(afterPosition, afterVelocity) > 0.0f) {
                result.exited = true;
                break;
            }
        }
    }
    result.relativePosition = world.GetTransform(ship).position - offset;
    result.relativeVelocity = world.GetLinearVelocity(ship);
    result.finalEnergy = SpecificEnergy(result.relativePosition, result.relativeVelocity, mu);
    result.finalApoapsis = OsculatingApoapsis(
        result.relativePosition, result.relativeVelocity, mu);
    if (fixedStepCount > 0) result.exited = glm::length(result.relativePosition) >=
        atmosphere.Parameters().topRadius;
    world.Shutdown();
    return result;
}

void TestOrbitalAtmosphericPass() {
    std::printf("Section C: gravity plus real drag changes an orbital trajectory\n");
    const AtmosphereField atmosphere = MakeAtmosphere();
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const OrbitalPass dragged = SimulatePass(
        atmosphere, true, identity, glm::vec3(0.0f));
    const OrbitalPass vacuum = SimulatePass(
        atmosphere, false, identity, glm::vec3(0.0f), dragged.steps);
    Check(dragged.entered && dragged.exited && dragged.minimumRadius < 100.0f,
          "the ship crosses from vacuum into gas and exits after periapsis");
    Check(dragged.peakDrag > 1.0f && dragged.peakDensity > 0.0f,
          "the pass encounters measurable gas and aerodynamic load");
    Check(dragged.dragWorkPerMass < -1.0f,
          "aerodynamic force performs negative work on the ship");
    Check(dragged.finalEnergy < vacuum.finalEnergy - 1.0f,
          "atmospheric pass loses orbital energy compared with gravity-only flight");
    Check(dragged.finalApoapsis < vacuum.finalApoapsis - 1.0f,
          "lower post-pass apoapsis emerges from the changed position and velocity");
    Check(glm::length(dragged.relativePosition - vacuum.relativePosition) > 0.5f,
          "the post-pass trajectory differs continuously from the unpowered orbit");
    std::printf("  Measured pass: %d steps (%.2f s), min r %.3f m, peak rho %.6f kg/m^3, "
                "peak drag %.3f N\n",
                dragged.steps, dragged.steps * kDt, dragged.minimumRadius,
                dragged.peakDensity, dragged.peakDrag);
    std::printf("  Specific energy: initial %.5f, vacuum %.5f, drag %.5f m^2/s^2; "
                "drag work %.5f m^2/s^2; apoapsis vacuum %.3f, drag %.3f m\n",
                dragged.initialEnergy, vacuum.finalEnergy, dragged.finalEnergy,
                dragged.dragWorkPerMass, vacuum.finalApoapsis, dragged.finalApoapsis);

    const glm::quat rotation = glm::angleAxis(0.81f,
        glm::normalize(glm::vec3(1.0f, -2.0f, 3.0f)));
    const glm::vec3 localOffset(600.0f, -400.0f, 250.0f);
    const OrbitalPass transformed = SimulatePass(
        atmosphere, true, rotation, localOffset, dragged.steps);
    Check(glm::length(glm::inverse(rotation) * transformed.relativePosition -
                      dragged.relativePosition) < 0.15f,
          "rotated and translated orbital position maps back to the same result");
    Check(glm::length(glm::inverse(rotation) * transformed.relativeVelocity -
                      dragged.relativeVelocity) < 0.08f,
          "rotated and translated orbital velocity maps back to the same result");
    CheckNear(transformed.finalEnergy, dragged.finalEnergy, 0.2f,
              "energy loss is invariant to local frame rotation and translation");

    // M23 represents a far absolute translation in double precision while
    // keeping this same local physics state. No large float subtraction is
    // allowed into the gravity, gas, collision, or drag calculation.
    const WorldCoordinates farWorld(glm::dvec3(1.0e9, -2.0e9, 3.0e9));
    const glm::vec3 recoveredLocal = farWorld.ToLocal(
        farWorld.ToGlobal(dragged.relativePosition));
    Check(glm::length(recoveredLocal - dragged.relativePosition) < 1.0e-5f,
          "M23 far absolute origin preserves the local atmospheric-pass result");
}

void TestSasComposesWithAtmosphere() {
    std::printf("Section D: SAS torque composes with gas drag and orbital translation\n");
    const AtmosphereField atmosphere = MakeAtmosphere();
    const ReferenceFrame planet;
    Window window;
    window.SetTestInputMode(true);

    // First build a real angular rate from pilot torque while moving through
    // nonzero-density gas. The craft simultaneously receives aerodynamic
    // force and SAS counter-torque through ordinary physics accumulators.
    PhysicsWorld tumbleWorld;
    Check(tumbleWorld.Init(), "atmospheric tumble world initialized");
    const glm::vec3 gasPosition(90.0f, 0.0f, 0.0f);
    const BodyHandle tumblingShip = tumbleWorld.CreateDynamicBox(
        gasPosition, kShipHalfExtents, kShipMass, 0.0f, 0.0f);
    tumbleWorld.SetLinearVelocity(tumblingShip, glm::vec3(0.0f, 5.0f, 0.0f));
    FlyingPrimitiveControl tumbleControl;
    tumbleControl.handle = tumblingShip;
    tumbleControl.controlled = true;
    window.SetTestActionState(Action::YawLeft, true);
    window.SetTestActionState(Action::PitchUp, true);
    for (int i = 0; i < 45; ++i) {
        ApplyAerodynamicDrag(tumbleWorld, tumblingShip, atmosphere, planet);
        ApplyFlyingPrimitiveControl(tumbleControl, window, tumbleWorld);
        tumbleWorld.Step(kDt);
    }
    window.SetTestActionState(Action::YawLeft, false);
    window.SetTestActionState(Action::PitchUp, false);
    const float angularRateBeforeCoast = glm::length(
        tumbleWorld.GetAngularVelocity(tumblingShip));
    for (int i = 0; i < 30; ++i) {
        ApplyAerodynamicDrag(tumbleWorld, tumblingShip, atmosphere, planet);
        ApplyFlyingPrimitiveControl(tumbleControl, window, tumbleWorld);
        tumbleWorld.Step(kDt);
    }
    Check(angularRateBeforeCoast > 0.1f &&
              std::abs(glm::length(tumbleWorld.GetAngularVelocity(tumblingShip)) -
                       angularRateBeforeCoast) < 1.0e-4f,
          "manual torque builds a tumble that persists with SAS off in gas");
    SetSpacecraftSasEnabled(tumbleControl, true, tumbleWorld);
    const glm::quat sasTarget = tumbleWorld.GetTransform(tumblingShip).rotation;
    const glm::vec3 velocityAtSasStart = tumbleWorld.GetLinearVelocity(tumblingShip);
    glm::vec3 dragVelocityChange(0.0f);
    float peakDragDuringSas = 0.0f;
    bool remainedInGas = true;
    for (int i = 0; i < 300; ++i) {
        const AerodynamicDragResult drag = ApplyAerodynamicDrag(
            tumbleWorld, tumblingShip, atmosphere, planet);
        remainedInGas &= drag.density > 0.0f;
        peakDragDuringSas = std::max(peakDragDuringSas, glm::length(drag.force));
        dragVelocityChange += drag.force * (kDt / kShipMass);
        ApplyFlyingPrimitiveControl(tumbleControl, window, tumbleWorld);
        tumbleWorld.Step(kDt);
    }
    Check(remainedInGas && peakDragDuringSas > 0.01f,
          "aerodynamic force acts while SAS counter-torque settles the tumble");
    Check(glm::length(tumbleWorld.GetAngularVelocity(tumblingShip)) < 0.01f,
          "SAS counter-torque settles the atmospheric tumble");
    Check(std::abs(glm::dot(tumbleWorld.GetTransform(tumblingShip).rotation, sasTarget)) > 0.999f,
          "SAS holds its captured attitude while atmosphere is present");
    Check(glm::length(tumbleWorld.GetLinearVelocity(tumblingShip) -
                      velocityAtSasStart - dragVelocityChange) < 1.0e-4f,
          "drag accounts for the full linear velocity change while SAS acts");
    tumbleWorld.Shutdown();

    // With matching pose and zero angular rate, both craft expose exactly
    // the same area to the same gas and gravity. This isolates SAS's direct
    // effect on translation: any difference would be invented linear force.
    // A genuinely tumbling SAS-off craft would expose a changing area, so
    // demanding the same dragged orbit in that case would be a false law.
    PhysicsWorld heldWorld;
    PhysicsWorld freeWorld;
    Check(heldWorld.Init() && freeWorld.Init(), "paired atmospheric worlds initialized");
    const glm::vec3 startPosition(100.0f, 0.0f, 0.0f);
    const glm::vec3 startVelocity(0.0f, 25.0f, 0.0f);
    const BodyHandle heldShip = heldWorld.CreateDynamicBox(
        startPosition, kShipHalfExtents, kShipMass, 0.0f, 0.0f);
    const BodyHandle freeShip = freeWorld.CreateDynamicBox(
        startPosition, kShipHalfExtents, kShipMass, 0.0f, 0.0f);
    heldWorld.SetLinearVelocity(heldShip, startVelocity);
    freeWorld.SetLinearVelocity(freeShip, startVelocity);
    FlyingPrimitiveControl heldControl;
    heldControl.handle = heldShip;
    SetSpacecraftSasEnabled(heldControl, true, heldWorld);
    FlyingPrimitiveControl freeControl;
    freeControl.handle = freeShip;
    float accumulatedDrag = 0.0f;
    bool matchedAerodynamicForces = true;
    for (int i = 0; i < 120; ++i) {
        const glm::vec3 heldPosition = heldWorld.GetTransform(heldShip).position;
        const glm::vec3 freePosition = freeWorld.GetTransform(freeShip).position;
        heldWorld.ApplyForce(heldShip, kShipMass *
            CelestialGravity::AccelerationFromPointMass(
                planet.originPosition, atmosphere.Parameters().gravitationalParameter,
                heldPosition));
        freeWorld.ApplyForce(freeShip, kShipMass *
            CelestialGravity::AccelerationFromPointMass(
                planet.originPosition, atmosphere.Parameters().gravitationalParameter,
                freePosition));
        const AerodynamicDragResult heldDrag = ApplyAerodynamicDrag(
            heldWorld, heldShip, atmosphere, planet);
        const AerodynamicDragResult freeDrag = ApplyAerodynamicDrag(
            freeWorld, freeShip, atmosphere, planet);
        accumulatedDrag += glm::length(heldDrag.force);
        matchedAerodynamicForces &=
            glm::length(heldDrag.force - freeDrag.force) < 1.0e-5f;
        ApplyFlyingPrimitiveControl(heldControl, window, heldWorld);
        ApplyFlyingPrimitiveControl(freeControl, window, freeWorld);
        heldWorld.Step(kDt);
        freeWorld.Step(kDt);
    }
    Check(matchedAerodynamicForces,
          "matching attitudes receive matching aerodynamic force with SAS on/off");
    Check(accumulatedDrag > 1.0f,
          "paired flight experiences nonzero aerodynamic resistance");
    Check(glm::length(heldWorld.GetTransform(heldShip).position -
                      freeWorld.GetTransform(freeShip).position) < 1.0e-5f &&
              glm::length(heldWorld.GetLinearVelocity(heldShip) -
                          freeWorld.GetLinearVelocity(freeShip)) < 1.0e-5f,
          "SAS does not change gravity-plus-drag orbital translation");
    heldWorld.Shutdown();
    freeWorld.Shutdown();
}

void TestDistinctGravityConsumers() {
    std::printf("Section E: the ship has one celestial source while props keep local gravity\n");
    struct LocalGravity final : GravityField {
        glm::vec3 Sample(const glm::vec3&) const override {
            return glm::vec3(0.0f, -9.81f, 0.0f);
        }
    } localGravity;
    PhysicsWorld world;
    Check(world.Init(), "distinct-gravity physics world initialized");
    const glm::vec3 shipStart(90.0f, 0.0f, 0.0f);
    const glm::vec3 propStart(90.0f, 0.0f, 20.0f);
    const BodyHandle ship = world.CreateDynamicBox(shipStart, kShipHalfExtents,
                                                   kShipMass, 0.0f, 0.0f);
    const BodyHandle prop = world.CreateDynamicBox(propStart, glm::vec3(0.5f),
                                                   5.0f, 0.0f, 0.0f);
    DynamicBody::Visual shipVisual;
    shipVisual.halfExtents = kShipHalfExtents;
    DynamicBody::Visual propVisual;
    std::vector<DynamicBody> bodies;
    bodies.emplace_back(ship, shipVisual, shipStart, glm::quat(1, 0, 0, 0));
    bodies.emplace_back(prop, propVisual, propStart, glm::quat(1, 0, 0, 0));
    PrepareDynamicBodiesForStep(bodies, localGravity, world, kDt, ship);
    const glm::vec3 celestialAcceleration = CelestialGravity::AccelerationFromPointMass(
        glm::vec3(0.0f), 62784.0f, shipStart);
    world.ApplyForce(ship, kShipMass * celestialAcceleration);
    world.Step(kDt);
    Check(glm::length(world.GetLinearVelocity(ship) - celestialAcceleration * kDt) < 2.0e-6f,
          "spacecraft receives exactly one Newtonian source without local-gravity addition");
    Check(glm::length(world.GetLinearVelocity(prop) - glm::vec3(0.0f, -9.81f, 0.0f) * kDt) <
              2.0e-6f,
          "ordinary nearby body still receives its own local gameplay gravity");
    world.Shutdown();
}
}  // namespace

int main() {
    TestDragAndOrientation();
    TestVacuumFrameAndExit();
    TestOrbitalAtmosphericPass();
    TestSasComposesWithAtmosphere();
    TestDistinctGravityConsumers();
    std::printf("Atmospheric flight tests: %s (%d failures)\n",
                gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
