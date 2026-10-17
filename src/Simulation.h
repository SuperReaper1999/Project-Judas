#pragma once

#include <chrono>
#include <cstddef>

#include "AerodynamicDrag.h"

class GameSession;
class InputSystem;
class Window;

// Milestone 28: optional per-step timing/measurement output. Filled only
// when the caller asks (the JUDAS_*_DIAGNOSTICS modes); zero cost otherwise.
struct FixedStepMeasurements {
    bool measureAtmosphere = false;
    bool measureFire = false;
    bool measureFluid = false;
    double atmosphereMilliseconds = 0.0;
    double fireMilliseconds = 0.0;
    double fluidMilliseconds = 0.0;
    bool atmosphereMeasured = false;
    bool fireMeasured = false;
    bool fluidMeasured = false;
    AerodynamicDragResult lastAerodynamicDrag;
};

// Advances one authoritative fixed step of a played scene, in the exact
// order used by the interactive runtime and editor Play:
//
//   dynamic bodies sample local gravity  ->  pairwise celestial forces  ->
//   vehicle point-mass gravity + drag    ->  operator thrust            ->
//   carried-object forces                ->  vehicle pilot forces        ->
//   door/switch animation                ->  fluid emission/cadence/coupling ->
//   PhysicsWorld::Step                    ->  combustion                  ->
//   player (attached or walking)         ->  presentation sync
//
// `window` supplies held-key state (thrust, water, vehicle controls); the
// caller decides whether gameplay owns input this step by simply not
// calling this while a menu is open. Nothing here knows about pausing.
void StepPlayedWorld(GameSession& session, const Window& window, float fixedDeltaTime,
                     FixedStepMeasurements* measurements = nullptr);

// The same fixed simulation path, without a native window or legacy input
// policy. Dedicated hosts use ordinary project scripts and logical input.
// Rejects a legacy-gameplay session before advancing any simulation state.
void StepAuthoritativeWorld(GameSession& session, const InputSystem& input, float fixedDeltaTime,
                            FixedStepMeasurements* measurements = nullptr);
