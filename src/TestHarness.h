#pragma once

#include <functional>
#include <string>

class Window;
class Renderer;
class GameSession;

// Opt-in developer/automation tooling — not part of the game itself, and
// not something the normal interactive game loop ever touches. See
// docs/ARCHITECTURE.md, "Automated testing," for why this exists (Claude
// Code cannot see or interact with Judas's real window in this
// environment, and there is no desktop-control tool available) and for
// the script file format.
//
// Runs an already-begun GameSession (the same RuntimeWorld/PlayerController
// construction the interactive path uses, so setup stays identical)
// through a scripted sequence, driving `window`'s input instead of a real
// keyboard/mouse. Two modes, chosen by the script itself (`REALTIME <n>`
// present or absent — see TestHarness.cpp for the full directive list):
//
//   - Fixed-step mode (default): runs a fixed number of physics steps as
//     fast as possible — no real-time pacing, no vsync wait.
//   - Real-time mode (`REALTIME <renderFrameCount>`, added in Milestone 6):
//     mirrors Application::Run's own interactive accumulator loop exactly,
//     so it reproduces the true render-frame-to-fixed-step relationship
//     headlessly.
//
// Both modes print one CSV line to stdout per logged row, and, at any
// step/frame the script requests, render the scene (via `drawScene`) and
// write it to a PNG file, so rendered output can be inspected without a
// way to see the window.
//
// Milestone 28: every fixed step goes through StepPlayedWorld (src/
// Simulation.h) — the identical ordering the interactive loop runs — and
// TAP R/F go through GameSession's own input handling, so the harness can
// never drift from real play. The CSV's per-body columns follow the
// scene's dynamic bodies in scene order.
//
// Returns a process exit code (0 on success, non-zero if the script
// couldn't be read).
int RunTestHarness(Window& window, Renderer& renderer, GameSession& session,
                   const std::function<void(Renderer&, float)>& drawScene,
                   const std::string& scriptPath);
