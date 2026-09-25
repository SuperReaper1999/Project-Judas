#include "Simulation.h"

#include <chrono>
#include <vector>

#include "CelestialGravity.h"
#include "CoarseSimulation.h"
#include "GameSession.h"
#include "PilotControl.h"
#include "RuntimeWorld.h"
#include "Window.h"

namespace {
using Clock = std::chrono::steady_clock;

double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// The fluid solver returns each particle/solid contact impulse after the
// rigid step. A coarse lake particle (M25: 125 kg) cannot hand its delayed
// reaction stably to a much lighter rigid body — the measured 80 kg
// spacecraft and 5 kg fuel blocks went unstable — while the accepted heavy
// couplings (2,000 kg prop, 120 kg cups against 0.125 kg cup water) are
// fine. The rule that reproduces exactly those accepted cases: a body
// lighter than four particle masses still displaces water (it remains a
// fluid boundary) but receives no reaction impulse.
constexpr float kFluidCouplingMassRatio = 4.0f;
}  // namespace

void StepPlayedWorld(GameSession& session, const Window& window, float fixedDeltaTime,
                     FixedStepMeasurements* measurements) {
    RuntimeWorld& world = session.World();
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
                                excludedFromLocalGravity);
    world.Celestial().ApplyForces(physics);
    // Milestone 29: a Coarse celestial entity still pulls on the live ones
    // (CoarseSimulation applies the reciprocal pull to it), so a pair split
    // across fidelities keeps attracting each other.
    for (const EntityRecord& coarse : world.Entities()) {
        if (coarse.lifecycle != EntityLifecycle::Active || coarse.fidelity != SimulationFidelity::Coarse ||
            !coarse.definition.celestial || !coarse.definition.body) continue;
        for (const BodyHandle live : world.CelestialParticipants()) {
            if (!physics.IsDynamicBody(live)) continue;
            physics.ApplyForce(live, CelestialGravity::ForceOnB(coarse.state.position, coarse.definition.body->mass,
                                                                physics.GetTransform(live).position,
                                                                physics.GetMass(live)));
        }
    }

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
    if (!world.OperatorThrusts().empty() && world.CelestialParticipants().size() >= 2) {
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
    if (manipulation.IsHolding() && !session.IsPiloting()) {
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
    for (Door& door : world.Doors()) door.FixedUpdate(physics, fixedDeltaTime);
    for (LightSwitch& lightSwitch : world.LightSwitches()) lightSwitch.FixedUpdate(fixedDeltaTime);
    physics.Step(fixedDeltaTime);

    if (!world.Combustibles().empty() && world.GetAtmosphere()) {
        const auto start = measurements && measurements->measureFire ? Clock::now() : Clock::time_point{};
        RadiantHeater heater;
        if (session.IgniterPowered()) {
            heater.worldPosition = ComputeCarryTarget(player.GetPosition(), player.GetOrientation(),
                                                      player.GetLookDirection(), 0.7f, 1.5f);
            heater.powerWatts = 18000.0f;
        }
        world.Combustion().Step(fixedDeltaTime, physics, world.GetAtmosphere()->field,
                                world.GetAtmosphere()->frame,
                                session.IgniterPowered() ? &heater : nullptr);
        if (measurements && measurements->measureFire) {
            measurements->fireMilliseconds = MillisecondsSince(start);
            measurements->fireMeasured = true;
        }
    }

    if (world.HasFluid()) {
        if (!session.IsPiloting() && window.IsActionActive(Action::AddTerrainWater)) {
            world.EmitFluidParticle();
        }
        if (!world.Fluid().Particles().empty()) {
            const auto start = measurements && measurements->measureFluid ? Clock::now() : Clock::time_point{};
            std::vector<FluidBoxCollider> boxes;
            std::vector<FluidSphereCollider> spheres;
            std::vector<FluidTerrainCollider> terrains;
            const auto appendBoxes = [&](BodyHandle handle) {
                const std::vector<BodyBox> previous = physics.GetPreviousBodyBoxes(handle);
                const std::vector<BodyBox> current = physics.GetBodyBoxes(handle);
                for (std::size_t part = 0; part < current.size(); ++part) {
                    boxes.push_back(FluidBoxCollider{handle,
                        BodyTransform{previous[part].center, previous[part].rotation},
                        BodyTransform{current[part].center, current[part].rotation},
                        current[part].halfExtents});
                }
            };
            for (const RuntimeWorld::StaticBody& body : world.StaticBodies()) {
                if (body.shape == SceneShape::Sphere) {
                    spheres.push_back({body.handle, physics.GetPreviousTransform(body.handle),
                                       physics.GetTransform(body.handle), body.radius});
                } else {
                    appendBoxes(body.handle);
                }
            }
            for (const DynamicBody& body : world.DynamicBodies()) {
                if (!body.IsLive()) continue;
                if (body.GetVisual().shape == DynamicBody::Shape::Sphere) {
                    spheres.push_back({body.Handle(), physics.GetPreviousTransform(body.Handle()),
                                       physics.GetTransform(body.Handle()), body.GetVisual().radius});
                } else {
                    appendBoxes(body.Handle());
                }
            }
            for (const RuntimeWorld::Terrain& terrain : world.Terrains()) {
                terrains.push_back({terrain.handle, physics.GetPreviousTransform(terrain.handle),
                                    physics.GetTransform(terrain.handle), terrain.surface.get()});
            }
            std::vector<FluidContactImpulse> impulses;
            world.Fluid().Step(fixedDeltaTime, gravity, boxes, spheres, terrains, &impulses);
            float particleMass = 0.0f;
            for (const RuntimeWorld::FluidVolume& volume : world.FluidVolumes()) {
                particleMass = std::max(particleMass, volume.particleMass);
            }
            for (const FluidContactImpulse& contact : impulses) {
                if (!physics.IsDynamicBody(contact.owner)) continue;
                if (physics.GetMass(contact.owner) < kFluidCouplingMassRatio * particleMass) continue;
                physics.ApplyImpulseAtPoint(contact.owner, contact.impulse, contact.point);
            }
            if (measurements && measurements->measureFluid) {
                measurements->fluidMilliseconds = MillisecondsSince(start);
                measurements->fluidMeasured = true;
            }
        }
    }

    // Milestone 29: reduced-fidelity entities advance from their records,
    // never through PhysicsWorld.
    StepCoarseEntities(world, fixedDeltaTime);

    AdvancePlayerForPiloting(vehicleControl, session.Attachment(), player, physics, window, gravity,
                             fixedDeltaTime);
    SyncDynamicBodiesFromPhysics(world.DynamicBodies(), physics);
    world.AdvanceSimulationTime(fixedDeltaTime);

    // Milestone 29: the scene's fidelity policy runs last, over the settled
    // step, with gameplay's pins (a held object, the player's support) kept
    // Full whatever the policy says.
    if (world.GetFidelityPolicy()) {
        FidelityPolicyContext context;
        context.focus = player.GetPosition();
        context.simulationTimeSeconds = world.SimulationTimeSeconds();
        world.EvaluateFidelityPolicy(context, session.PinnedEntities());
    }
    session.RefreshEntityBindings();
}
