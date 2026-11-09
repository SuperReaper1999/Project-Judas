#include "PerformanceProfiler.h"
#include "Simulation.h"

#include <chrono>
#include <stdexcept>
#include <vector>

#include "CelestialGravity.h"
#include "CoarseSimulation.h"
#include "RuntimeWorld.h"
#include "Window.h"

namespace {
using Clock = std::chrono::steady_clock;

double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void StepWorld(RuntimeWorld& world, const InputSystem& input,
               float fixedDeltaTime, FixedStepMeasurements* measurements) {
    JUDAS_PROFILE_SCOPE("Simulation systems");
    ProfileFixedStep profileFixedStep(fixedDeltaTime);
    // A replica world has no second authoritative physics solver. Scripts may
    // submit input and update presentation; remote resolved state is published
    // through the session's explicit snapshot path.
    if (world.NetworkClient()) {
        // Capture a replica step before invoking scripts. Closing the session
        // inside that callback resumes ordinary physics on the next step; it
        // cannot integrate a restored offline body midway through this one.
        world.CommitOrigin();
        world.NetworkBeforeFixed(fixedDeltaTime);
        world.FixedScripts(&input, fixedDeltaTime);
        // Fixed callbacks still own a local fixed clock; suppressing duplicate
        // physics must not freeze time.elapsed for replica scripts. This is
        // not a synchronized server clock.
        world.AdvanceSimulationTime(fixedDeltaTime);
        world.NetworkAfterFixed(fixedDeltaTime);
        return;
    }
    world.CommitOrigin();
    world.UpdateNavigation(fixedDeltaTime);
    world.NetworkBeforeFixed(fixedDeltaTime);
    world.FixedScripts(&input,fixedDeltaTime);
    // Joining is legal from a fixed callback (including its first start/restore
    // callback). Do not run a duplicate local solver merely because the role
    // was offline when this step began. Callbacks and the local clock still
    // advance exactly once.
    if (world.NetworkClient()) {
        world.AdvanceSimulationTime(fixedDeltaTime);
        world.NetworkAfterFixed(fixedDeltaTime);
        return;
    }
    world.UpdateWeather(fixedDeltaTime);
    // Only opted-in M70 instances advance references before dynamics. Legacy
    // animation stays in its established post-physics phase.
    world.UpdateCharacters(fixedDeltaTime,true,false);
    world.PrepareAnimationReferences(fixedDeltaTime);
    world.PreparePhysicalAnimations(fixedDeltaTime);
    world.PreparePhysicalBalances(fixedDeltaTime);
    PhysicsWorld& physics = world.Physics();
    const GravityField& gravity = world.Gravity();
    const unsigned structuralIntervals=world.MechanicalSubsteps();
    if(structuralIntervals==1)PrepareDynamicBodiesForStep(world.DynamicBodies(), gravity, physics, fixedDeltaTime,
                                BodyHandle{},[&world](size_t slot,glm::vec3 position){
                                    return world.SampleEntityGravity(world.DynamicVisuals()[slot].id,position);
                                });
    world.Celestial().ApplyForces(physics);
    // Cross-fidelity forces use the same pre-step geometry and one shared
    // force per pair; the coarse velocity kick precedes both pose advances.
    ApplyCoarseCelestialForces(world, fixedDeltaTime);


    const auto waterStart = measurements && measurements->measureFluid ? Clock::now() : Clock::time_point{};
    world.UpdateGridWater(fixedDeltaTime);
    if(measurements && measurements->measureFluid) {
        measurements->fluidMilliseconds=MillisecondsSince(waterStart);
        measurements->fluidMeasured=true;
    }
    // Mechanical sources advance once; wheel contacts then feed their actual
    // axle load back into the same rotational network before rigid integration.
    if(world.MechanicalIterations()==0){
        world.UpdatePowertrains(fixedDeltaTime);
        world.UpdateWheels(fixedDeltaTime);
        world.ResolvePowertrainLoads(fixedDeltaTime);
        physics.Step(fixedDeltaTime);
    }else {
        std::map<EntityId,WheelState> wheelPrevious;
        for(const auto& object:world.ScriptObjects(false))if(object.wheel)if(auto* wheel=world.RuntimeWheel(object.id))wheelPrevious.emplace(object.id,wheel->state);
        PhysicsWorld::VelocityCoupling materialPhase;materialPhase.iterations=world.MechanicalIterations();
        physics.StepCoupled(fixedDeltaTime,structuralIntervals,
        [&](float h,unsigned){
            if(structuralIntervals>1)PrepareDynamicBodiesForStep(world.DynamicBodies(),gravity,physics,h,
                BodyHandle{},[&world,&physics](size_t slot,glm::vec3){const auto position=physics.GetTransform(world.DynamicBodies()[slot].Handle()).position;return world.SampleEntityGravity(world.DynamicVisuals()[slot].id,position);});
            world.UpdatePowertrains(h);world.UpdateWheels(h);world.ResolvePowertrainLoads(h);
        },[&](float,unsigned){world.PublishPowertrainConnections();},&materialPhase);
        for(const auto& [id,state]:wheelPrevious)if(auto* wheel=world.RuntimeWheel(id))wheel->previous=state;
    }


    if (!world.Combustibles().empty()) {
        const auto start = measurements && measurements->measureFire ? Clock::now() : Clock::time_point{};
        world.Combustion().Step(fixedDeltaTime, physics,
                                world.GetAtmosphere() ? &world.GetAtmosphere()->field : nullptr,
                                world.GetAtmosphere() ? world.GetAtmosphere()->frame : ReferenceFrame{},
                                nullptr);
        if (measurements && measurements->measureFire) {
            measurements->fireMilliseconds = MillisecondsSince(start);
            measurements->fireMeasured = true;
        }
    }

    // Milestone 29: reduced-fidelity entities advance from their records,
    // never through PhysicsWorld.
    StepCoarseEntities(world, fixedDeltaTime);

    world.UpdateCharacters(fixedDeltaTime,false,true);
    SyncDynamicBodiesFromPhysics(world.DynamicBodies(), physics);
    world.PublishWheels();
    world.PublishPowertrains(fixedDeltaTime);
    world.UpdateAnimations(fixedDeltaTime);
    world.UpdateSpriteAnimations(fixedDeltaTime);
    world.UpdateRagdolls(fixedDeltaTime);
    world.UpdateSockets();
    world.UpdateDeformables(fixedDeltaTime);
    // Damage commits after material solving. Refresh only physical continuity;
    // do not advance shaft angles or repeat the mechanical impulse solve.
    world.PublishPowertrainConnections();
    world.UpdateVisualParticles(fixedDeltaTime);
    world.AdvanceSimulationTime(fixedDeltaTime);

    world.DispatchPhysicsEvents(&input,fixedDeltaTime);
    world.DispatchFractureEvents();
    if(world.Scripts())world.Scripts()->DrainSignals(&input,fixedDeltaTime,false);
    world.NetworkAfterFixed(fixedDeltaTime);

    // Milestone 29: the scene's fidelity policy runs last, over the settled
    // step, with gameplay's pins (a held object, the player's support) kept
    // Full whatever the policy says.
    if (world.GetFidelityPolicy()) {
        FidelityPolicyContext context;
        context.focus = world.view ? world.view->pose.position : glm::vec3(0);
        context.simulationTimeSeconds = world.SimulationTimeSeconds();
        world.EvaluateFidelityPolicy(context, {});
    }
}

}  // namespace

void StepPlayedWorld(RuntimeWorld& world, const Window& window, float fixedDeltaTime,
                     FixedStepMeasurements* measurements) {
    StepWorld(world, window.Input(), fixedDeltaTime, measurements);
}

void StepAuthoritativeWorld(RuntimeWorld& world, const InputSystem& input, float fixedDeltaTime,
                            FixedStepMeasurements* measurements) {
    StepWorld(world, input, fixedDeltaTime, measurements);
}
