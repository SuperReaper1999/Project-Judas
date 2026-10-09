#include "PerformanceProfiler.h"
#include "Simulation.h"

#include <chrono>
#include <vector>

#include "CelestialGravity.h"
#include "CoarseSimulation.h"
#include "GameSession.h"
#include "PilotControl.h"
#include "ProductionFluidCoupling.h"
#include "RuntimeWorld.h"
#include "Window.h"

namespace {
using Clock = std::chrono::steady_clock;

double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

}  // namespace

void StepPlayedWorld(GameSession& session, const Window& window, float fixedDeltaTime,
                     FixedStepMeasurements* measurements) {
    JUDAS_PROFILE_SCOPE("Simulation systems");
    ProfileFixedStep profileFixedStep(fixedDeltaTime);
    RuntimeWorld& world = session.World();
    const bool legacy = session.UsesLegacyGameplay();
    world.UpdateNavigation(fixedDeltaTime);
    world.Liquids().BeginStep();
    world.FixedScripts(&window.Input(),fixedDeltaTime);
    // Only opted-in M70 instances advance references before dynamics. Legacy
    // animation stays in its established post-physics phase.
    world.UpdateCharacters(fixedDeltaTime,true,false);
    world.PrepareAnimationReferences(fixedDeltaTime);
    world.PreparePhysicalAnimations(fixedDeltaTime);
    world.UpdateLiquids(fixedDeltaTime);
    PhysicsWorld& physics = world.Physics();
    const GravityField& gravity = world.Gravity();
    FlyingPrimitiveControl& vehicleControl = session.VehicleControl();
    PlayerController& player = session.Player();

    // A Celestial-gravity vehicle takes its planetary pull from point-mass
    // sources below instead of the local gameplay contexts, so it is never
    // counted twice.
    BodyHandle excludedFromLocalGravity;
    if (world.GetVehicle() && world.GetVehicle()->component.gravity == SceneVehicleGravity::Celestial) {
        excludedFromLocalGravity = world.GetVehicle()->handle;
    }
    PrepareDynamicBodiesForStep(world.DynamicBodies(), gravity, physics, fixedDeltaTime,
                                excludedFromLocalGravity,[&world](size_t slot,glm::vec3 position){
                                    return world.SampleEntityGravity(world.DynamicVisuals()[slot].id,position);
                                });
    world.Celestial().ApplyForces(physics);
    // Cross-fidelity forces use the same pre-step geometry and one shared
    // force per pair; the coarse velocity kick precedes both pose advances.
    ApplyCoarseCelestialForces(world, fixedDeltaTime);

    if (world.GetVehicle() && excludedFromLocalGravity.IsValid()) {
        const auto start = measurements && measurements->measureAtmosphere ? Clock::now()
                                                                            : Clock::time_point{};
        const BodyHandle ship = world.GetVehicle()->handle;
        const glm::vec3 shipPosition = physics.GetTransform(ship).position;
        for (const RuntimeWorld::PointMassSource& source : world.PointMassSources()) {
            physics.ApplyForce(ship, physics.GetMass(ship) *
                CelestialGravity::AccelerationFromPointMass(source.position,
                                                            source.gravitationalParameter, shipPosition));
        }
        if (world.GetAtmosphere()) {
            const AerodynamicDragResult drag = ApplyAerodynamicDrag(
                physics, ship, world.GetAtmosphere()->field, world.GetAtmosphere()->frame,
                world.GetVehicle()->component.dragCoefficient);
            if (measurements) measurements->lastAerodynamicDrag = drag;
        }
        if (measurements && measurements->measureAtmosphere) {
            measurements->atmosphereMilliseconds = MillisecondsSince(start);
            measurements->atmosphereMeasured = true;
        }
    }

    // M20 operator thrusters: real constant forces whose directions come
    // from the current barycentric state of the Newtonian set.
    if (legacy && !world.OperatorThrusts().empty() && world.CelestialParticipants().size() >= 2) {
        glm::vec3 barycentre(0.0f);
        glm::vec3 baryVelocity(0.0f);
        float totalMass = 0.0f;
        for (const BodyHandle handle : world.CelestialParticipants()) {
            if (world.GetVehicle() && handle.id == world.GetVehicle()->handle.id) continue;
            const float mass = physics.GetMass(handle);
            barycentre += physics.GetTransform(handle).position * mass;
            baryVelocity += physics.GetLinearVelocity(handle) * mass;
            totalMass += mass;
        }
        if (totalMass > 0.0f) {
            barycentre /= totalMass;
            baryVelocity /= totalMass;
            for (const RuntimeWorld::OperatorThrust& thrust : world.OperatorThrusts()) {
                const glm::vec3 position = physics.GetTransform(thrust.handle).position;
                const glm::vec3 radialOffset = position - barycentre;
                if (glm::length(radialOffset) <= 1.0e-6f) continue;
                const glm::vec3 radial = glm::normalize(radialOffset);
                const glm::vec3 relativeVelocity = physics.GetLinearVelocity(thrust.handle) - baryVelocity;
                const glm::vec3 tangentVelocity = relativeVelocity - radial * glm::dot(relativeVelocity, radial);
                glm::vec3 direction(0.0f);
                if (window.IsActionActive(Action::PlanetRadialThrust)) {
                    direction = radial;
                } else if (glm::length(tangentVelocity) > 1.0e-5f) {
                    const glm::vec3 tangent = glm::normalize(tangentVelocity);
                    if (window.IsActionActive(Action::PlanetProgradeThrust)) direction = tangent;
                    else if (window.IsActionActive(Action::PlanetRetrogradeThrust)) direction = -tangent;
                }
                if (glm::length(direction) > 0.0f) physics.ApplyForce(thrust.handle, direction * thrust.force);
            }
        }
    }

    ObjectManipulation& manipulation = session.Manipulation();
    if (legacy && manipulation.IsHolding() && !session.IsPiloting()) {
        const glm::vec3 carryTarget = ComputeCarryTarget(player.GetPosition(), player.GetOrientation(),
                                                         player.GetLookDirection(), 0.7f, 1.7f);
        manipulation.ApplyCarryForce(physics, carryTarget, player.GetVelocity());
        // A compound held body may need an attitude as well as a position.
        if (physics.GetBodyBoxes(manipulation.HeldBody()).size() > 1) {
            manipulation.ApplyCarryOrientationTorque(
                physics, ComputeCarryOrientation(player.GetOrientation(), player.GetLookDirection()));
        }
    }
    if (session.HasVehicle()) ApplyFlyingPrimitiveControl(vehicleControl, window, physics);

    // Doors write their pose to PhysicsWorld BEFORE the player's own
    // FixedUpdate so this step's move-and-slide sees the current panel.
    if (legacy) for (Door& door : world.Doors()) door.FixedUpdate(physics, fixedDeltaTime);
    if (legacy) for (LightSwitch& lightSwitch : world.LightSwitches()) lightSwitch.FixedUpdate(fixedDeltaTime);
    if (world.HasFluid()) {
        if (legacy && !session.IsPiloting() && window.IsActionActive(Action::AddTerrainWater)) world.EmitFluidParticle();
        world.FluidCoupling().PrepareRigidStep(world, fixedDeltaTime);
    } else player.SetFluidSample({});
    physics.Step(fixedDeltaTime);
    if (world.HasFluid()) {
        // Liquid walls follow the already resolved rigid trajectory, including
        // impacts and support correction, never a gravity-only endpoint guess.
        world.FluidCoupling().AdvanceResolvedLiquid(world, fixedDeltaTime);
        player.SetFluidSample(!legacy || world.view ? PlayerFluidSample{} : world.FluidCoupling().SamplePlayer(world, player));
        if (measurements && measurements->measureFluid) {
            measurements->fluidMilliseconds = world.FluidCoupling().Measurements().totalMilliseconds;
            measurements->fluidMeasured = true; // includes field/coupling cost even on particle hold frames
        }
    }

    if (!world.Combustibles().empty()) {
        const auto start = measurements && measurements->measureFire ? Clock::now() : Clock::time_point{};
        RadiantHeater heater;
        if (session.IgniterPowered()) {
            heater.worldPosition = ComputeCarryTarget(player.GetPosition(), player.GetOrientation(),
                                                      player.GetLookDirection(), 0.7f, 1.5f);
            heater.powerWatts = 18000.0f;
        }
        world.Combustion().Step(fixedDeltaTime, physics,
                                world.GetAtmosphere() ? &world.GetAtmosphere()->field : nullptr,
                                world.GetAtmosphere() ? world.GetAtmosphere()->frame : ReferenceFrame{},
                                session.IgniterPowered() ? &heater : nullptr);
        if (measurements && measurements->measureFire) {
            measurements->fireMilliseconds = MillisecondsSince(start);
            measurements->fireMeasured = true;
        }
    }

    // Milestone 29: reduced-fidelity entities advance from their records,
    // never through PhysicsWorld.
    StepCoarseEntities(world, fixedDeltaTime);

    if(world.view){const auto& pose=world.view->pose;
        // Compatibility observer for legacy diagnostics/interaction. Scripted
        // main-view projects do not run a second locomotion controller.
        player.ObserveExternalView(pose.position-pose.rotation*glm::vec3(0,.7f,0),pose.rotation);
    }else if (legacy) AdvancePlayerForPiloting(vehicleControl, session.Attachment(), player, physics, window, gravity,
                             fixedDeltaTime);
    world.UpdateCharacters(fixedDeltaTime,false,true);
    SyncDynamicBodiesFromPhysics(world.DynamicBodies(), physics);
    world.UpdateAnimations(fixedDeltaTime);
    world.UpdateRagdolls(fixedDeltaTime);
    world.UpdateSockets();
    world.UpdateDeformables(fixedDeltaTime);
    world.UpdateVisualParticles(fixedDeltaTime);
    world.AdvanceSimulationTime(fixedDeltaTime);

    world.DispatchPhysicsEvents(&window.Input(),fixedDeltaTime);
    world.DispatchFractureEvents();

    // Milestone 29: the scene's fidelity policy runs last, over the settled
    // step, with gameplay's pins (a held object, the player's support) kept
    // Full whatever the policy says.
    if (world.GetFidelityPolicy()) {
        FidelityPolicyContext context;
        context.focus = world.view ? world.view->pose.position : player.GetPosition();
        context.simulationTimeSeconds = world.SimulationTimeSeconds();
        world.EvaluateFidelityPolicy(context, session.PinnedEntities());
    }
    session.RefreshEntityBindings();
}
