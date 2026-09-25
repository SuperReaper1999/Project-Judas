#include "GameplayHud.h"

#include "AerodynamicDrag.h"
#include "CelestialGravity.h"
#include "GameSession.h"
#include "Interactable.h"
#include "ObjectManipulation.h"
#include "ReferenceFrame.h"
#include "RuntimeWorld.h"
#include "WorldCoordinates.h"

namespace {
ReferenceFrame FrameFromBody(const PhysicsWorld& physics, BodyHandle handle) {
    const BodyTransform transform = physics.GetTransform(handle);
    return ReferenceFrame{transform.position, transform.rotation, physics.GetLinearVelocity(handle),
                          physics.GetAngularVelocity(handle)};
}
}  // namespace

HUDViewData BuildHudView(const GameSession& session, const WorldCoordinates& worldCoordinates,
                         const AerodynamicDragResult& lastAerodynamicDrag) {
    const RuntimeWorld& world = session.World();
    const PhysicsWorld& physics = world.Physics();
    const PlayerController& player = session.Player();
    HUDViewData hud;
    hud.grounded = player.IsGrounded();
    hud.gravityMagnitude = glm::length(world.Gravity().Sample(player.GetPosition()));
    hud.controllingSpacecraft = session.IsPiloting();
    hud.pilotAttached = session.Attachment().attached;
    hud.spacecraftSasEnabled = session.VehicleControl().sasEnabled;
    hud.worldOrigin = worldCoordinates.Origin();
    hud.absolutePlayerPosition = worldCoordinates.ToGlobal(player.GetPosition());

    glm::vec3 shipPosition(0.0f);
    glm::vec3 shipVelocity(0.0f);
    if (world.GetVehicle()) {
        const BodyHandle ship = world.GetVehicle()->handle;
        shipPosition = physics.GetTransform(ship).position;
        shipVelocity = physics.GetLinearVelocity(ship);
        const ReferenceFrame shipFrame = FrameFromBody(physics, ship);
        hud.spacecraftLinearSpeed = glm::length(shipVelocity);
        if (session.IsPiloting() && world.GetVehicle()->component.gravity == SceneVehicleGravity::Celestial) {
            glm::vec3 acceleration(0.0f);
            for (const RuntimeWorld::PointMassSource& source : world.PointMassSources()) {
                acceleration += CelestialGravity::AccelerationFromPointMass(
                    source.position, source.gravitationalParameter, shipPosition);
            }
            hud.gravityMagnitude = glm::length(acceleration);
        }
        const glm::vec3 pilotVelocity = session.Attachment().attached
            ? VelocityToWorld(shipFrame, session.Attachment().localOffset, glm::vec3(0.0f))
            : player.GetVelocity();
        hud.pilotWorldSpeed = glm::length(pilotVelocity);
        hud.pilotRelativeSpacecraftSpeed = glm::length(
            RelativeVelocityToFrame(shipFrame, player.GetPosition(), pilotVelocity));
    } else {
        hud.pilotWorldSpeed = glm::length(player.GetVelocity());
    }

    // M22 reference readouts: the first dynamic Newtonian body, else the
    // atmosphere's planet.
    if (!world.CelestialParticipants().empty() && physics.IsDynamicBody(world.CelestialParticipants()[0])) {
        const BodyHandle reference = world.CelestialParticipants()[0];
        const ReferenceFrame frame = FrameFromBody(physics, reference);
        hud.celestialReferenceAvailable = true;
        hud.celestialReferenceLabel = world.NameOfBody(reference);
        hud.celestialBodyWorldSpeed = glm::length(frame.linearVelocity);
        hud.spacecraftRelativeCelestialSpeed = glm::length(RelativeVelocityToFrame(frame, shipPosition, shipVelocity));
    }
    if (world.GetAtmosphere()) {
        const RuntimeWorld::Atmosphere& atmosphere = *world.GetAtmosphere();
        const AtmosphereSample gas = atmosphere.field.Sample(shipPosition, atmosphere.frame);
        hud.celestialReferenceAvailable = true;
        hud.celestialReferenceLabel = atmosphere.name;
        hud.celestialBodyWorldSpeed = glm::length(atmosphere.frame.linearVelocity);
        hud.spacecraftRelativeCelestialSpeed = glm::length(
            RelativeVelocityToFrame(atmosphere.frame, shipPosition, shipVelocity));
        hud.atmosphereAvailable = true;
        hud.atmosphereDensity = gas.density;
        hud.atmospherePressure = gas.pressure;
        hud.spacecraftRelativeAirspeed = gas.density > 0.0f ? glm::length(shipVelocity - gas.velocity) : 0.0f;
        hud.spacecraftDynamicPressure = 0.5f * gas.density * hud.spacecraftRelativeAirspeed *
                                        hud.spacecraftRelativeAirspeed;
        hud.spacecraftAerodynamicForce = glm::length(lastAerodynamicDrag.force);
    }
    if (!world.Combustibles().empty() && world.GetAtmosphere()) {
        hud.thermalAvailable = true;
        hud.igniterPowered = session.IgniterPowered();
        hud.oxidizerMassDensity = world.GetAtmosphere()->field.Sample(
            player.GetPosition(), world.GetAtmosphere()->frame).oxidizerMassDensity;
        for (const RuntimeWorld::Combustible& c : world.Combustibles()) {
            const ThermalBodyState* state = world.Combustion().State(c.handle);
            if (!state) continue;
            hud.thermalBodies.push_back({c.name, state->temperatureKelvin, state->remainingFuelMassKg,
                                         state->burnRateKgPerSecond});
        }
    }

    // Milestone 29: lifecycle counts whenever a policy runs or anything has
    // left the plain all-Full state.
    const RuntimeWorld::LifecycleCounts counts = world.CountLifecycle();
    if (world.GetFidelityPolicy() || counts.coarse + counts.dormant + counts.destroyed > 0) {
        hud.lifecycleAvailable = true;
    }
    hud.entitiesFull = counts.full;
    hud.entitiesCoarse = counts.coarse;
    hud.entitiesDormant = counts.dormant;
    hud.entitiesDestroyed = counts.destroyed;
    hud.physicsBodies = counts.physicsBodies;

    const Interactable* target = session.InteractionTarget();
    if (session.Manipulation().IsHolding()) {
        hud.interactPrompt = target ? target->GetPromptText() + " | H Throw" : "G Drop | H Throw";
        if (physics.GetBodyBoxes(session.Manipulation().HeldBody()).size() > 1) hud.interactPrompt += " | Look to tip";
    } else {
        hud.interactPrompt = target ? target->GetPromptText() : std::string();
    }
    if (!session.IsPiloting()) {
        bool emitter = false;
        for (const RuntimeWorld::FluidVolume& v : world.FluidVolumes()) emitter = emitter || v.component.emitter;
        if (emitter) {
            if (!hud.interactPrompt.empty()) hud.interactPrompt += " | ";
            hud.interactPrompt += "Hold B: add water";
        }
        if (!world.Combustibles().empty()) {
            if (!hud.interactPrompt.empty()) hud.interactPrompt += " | ";
            hud.interactPrompt += "Hold C: radiant heater";
        }
        if (target && dynamic_cast<const PickupInteractable*>(target)) {
            hud.interactPrompt += " | Y Destroy permanently";
        }
    }
    return hud;
}
