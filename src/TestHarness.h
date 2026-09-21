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
// interactive path) through a fixed number of physics steps as fast as
// possible — no real-time pacing, no vsync wait — driving `window`'s input
// from a scripted sequence read from `scriptPath` instead of a real
// keyboard/mouse. Prints one CSV line of player state to stdout per
// logged step, and, at any step the script requests, renders the scene
// (via `drawScene`, since this harness has no idea what a "sphere" or a
// "cube" is — that stays the caller's business) and writes it to a PNG
// file, so the actual rendered output can be inspected without a way to
// see the window.
//
// Returns a process exit code (0 on success, non-zero if the script
// couldn't be read).
int RunTestHarness(Window& window, Renderer& renderer, PhysicsWorld& physicsWorld,
                    PlayerController& player, const GravityField& gravity,
                    const std::function<void(Renderer&)>& drawScene,
                    const std::string& scriptPath);
