#pragma once

#include <functional>
#include <string>

class Window;
class Renderer;
class PhysicsWorld;
class PlayerController;
class GravityField;

// Opt-in developer/automation tooling — not part of the game itself, and
// not something the normal interactive game loop ever touches. See
// docs/ARCHITECTURE.md, "Automated testing," for why this exists (Claude
// Code cannot see or interact with Judas's real window in this
// environment, and there is no desktop-control tool available) and for
// the script file format.
//
// Runs `Window`/`Renderer`/`PhysicsWorld`/`PlayerController` already
// constructed by the caller (so setup stays identical to the normal
// interactive path) through a scripted sequence, driving `window`'s input
// instead of a real keyboard/mouse. Two modes, chosen by the script itself
// (`REALTIME <n>` present or absent — see TestHarness.cpp for the full
// directive list):
//
//   - Fixed-step mode (default): runs a fixed number of physics steps as
//     fast as possible — no real-time pacing, no vsync wait. What
//     Milestone 5 used to verify gameplay/physics logic.
//   - Real-time mode (`REALTIME <renderFrameCount>`, added in Milestone 6):
//     mirrors Application::Run's own interactive accumulator loop exactly,
//     so it reproduces the true render-frame-to-fixed-step relationship
//     headlessly — used to diagnose whether visible motion issues come
//     from the simulation itself or from how fixed-step state gets
//     presented across render frames. See docs/ARCHITECTURE.md, "Diagnosis."
//
// Both modes print one CSV line to stdout per logged row, and, at any
// step/frame the script requests, render the scene (via `drawScene`, since
// this harness has no idea what a "sphere" or a "cube" is — that stays the
// caller's business) and write it to a PNG file, so rendered output can be
// inspected without a way to see the window.
//
// Returns a process exit code (0 on success, non-zero if the script
// couldn't be read).
// `drawScene`'s float parameter is the presentation interpolation factor
// (see PlayerController::GetPresentedPosition/Orientation and
// docs/ARCHITECTURE.md, "Simulation/presentation boundary") — fixed-step
// mode always passes 1.0 (there is no "in between" a back-to-back fixed
// step), real-time mode passes the same accumulator-derived value used for
// that frame's own camera, so a requested screenshot shows exactly what
// that render frame actually presented.
int RunTestHarness(Window& window, Renderer& renderer, PhysicsWorld& physicsWorld,
                    PlayerController& player, const GravityField& gravity,
                    const std::function<void(Renderer&, float)>& drawScene,
                    const std::string& scriptPath);
