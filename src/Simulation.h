#pragma once

#include <chrono>
#include <cstddef>


class RuntimeWorld;
class InputSystem;
class Window;

// Milestone 28: optional per-step timing/measurement output. Filled only
// when the caller asks (the JUDAS_*_DIAGNOSTICS modes); zero cost otherwise.
struct FixedStepMeasurements {
    bool measureFire = false;
    bool measureFluid = false;
    double fireMilliseconds = 0.0;
    double fluidMilliseconds = 0.0;
    bool fireMeasured = false;
    bool fluidMeasured = false;
};

// One authoritative fixed step shared by standalone, editor and dedicated
// hosts: scripts/network, weather, animation references, water, gravity,
// mechanical/MK2 solve, combustion, characters, presentation and events.
// Input policy and pausing belong to the caller; gameplay belongs to scripts.
void StepPlayedWorld(RuntimeWorld& world, const Window& window, float fixedDeltaTime,
                     FixedStepMeasurements* measurements = nullptr);

// The same simulation path with logical input and no native window.
void StepAuthoritativeWorld(RuntimeWorld& world, const InputSystem& input, float fixedDeltaTime,
                            FixedStepMeasurements* measurements = nullptr);
