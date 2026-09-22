#pragma once

#include "FlyingPrimitiveControl.h"
#include "PilotAttachment.h"

class PlayerController;
class PhysicsWorld;
class Window;
class GravityField;

// Milestone 11: the "press F" acquisition/release policy and the "how does
// the player advance this fixed step" decision, factored into shared free
// functions specifically so the interactive game loop (Application.cpp)
// and BOTH JUDAS_TEST_SCRIPT harness modes (src/TestHarness.cpp) drive
// piloting identically — Milestone 8's simpler toggle-only version of this
// logic was small enough to tolerate being copy-pasted three times, but
// Milestone 11's acquisition (capturing an attachment) and per-step
// handling (attachment vs. ordinary FixedUpdate) are not, and three
// independently-maintained copies is exactly the kind of drift risk that
// produces "it works in the harness but not interactively" bugs. See
// docs/ARCHITECTURE.md, "Milestone 11."

// Call once per render frame, immediately after
// Window::ConsumeControlToggleRequest() reports a press, BEFORE that
// frame's fixed-step loop runs — the same timing Milestone 8's inline
// toggle check already used. Acquisition is gated on the player's own
// CURRENT support state exactly as Milestone 8 required (F from anywhere
// else is a no-op); release is always available regardless of orientation,
// support, or gravity context. Reads the spacecraft's current authoritative
// transform/velocities via `physics` to capture the attachment (see
// BeginPilotAttachment) or compute the player's inherited release velocity
// (see ComputePilotReleaseVelocity) — never a predicted or presented pose.
void HandlePilotToggleRequest(FlyingPrimitiveControl& control, PilotAttachment& attachment,
                               PlayerController& player, PhysicsWorld& physics);

// Call once per fixed step, AFTER PhysicsWorld::Step, in place of calling
// PlayerController::FixedUpdate directly. While attached, drives the
// player's authoritative pose entirely from the attachment (using the
// spacecraft's just-resolved, real authoritative transform — see
// PlayerController::FixedUpdateAttached) instead of ordinary gravity/
// support/locomotion; PlayerController::FixedUpdate is not called at all
// that step. Otherwise, calls PlayerController::FixedUpdate normally with
// input enabled — Milestone 11 only ever disables player input WHILE
// attached, so the old always-present `inputEnabled` parameter collapses
// into this one attached/not-attached branch.
void AdvancePlayerForPiloting(const FlyingPrimitiveControl& control, PilotAttachment& attachment,
                               PlayerController& player, PhysicsWorld& physics, const Window& window,
                               const GravityField& gravity, float fixedDeltaTime);
