#pragma once

#include <glm/glm.hpp>

#include "PhysicsWorld.h"

class Window;

// Milestone 8's control-ownership mechanism, upgraded in Milestone 11 to
// full 6-degree-of-freedom spacecraft control, and in Milestone 12 to
// genuine force/torque-driven inertia — the underlying object is
// repurposed, not replaced, each time: still one ordinary DynamicBody
// (src/DynamicBody.h) in Application.cpp's dynamicBodies list, still
// local/celestial gravity through the composition root, ordinary collision
// via PhysicsWorld::Step, ordinary presentation interpolation, ordinary
// ResetToSpawn. While `controlled` is true, pilot input is turned into a
// spacecraft-local force and torque applied to its body's accumulator
// (PhysicsWorld::ApplyForce/ApplyTorque) — added on top of gravity and
// celestial forces already accumulated that step. Its optional SAS mode
// separately applies attitude-hold torque, including after pilot release.
// Through Milestone 11, pilot input
// instead commanded linear/angular velocity directly (SetLinearVelocity/
// SetAngularVelocity), the same "Judas commands the velocity outright,
// physics obeys" idiom PlayerController's own grounded locomotion still
// uses; Milestone 12 deliberately replaces that with real F=ma physics for
// the spacecraft specifically — see docs/ARCHITECTURE.md, "Milestone 12,"
// for why (releasing input must coast, not stop). Still deliberately not a
// vehicle-physics framework: no thrust/fuel/engine model or generalized
// possession mechanism for multiple objects. See
// src/PilotAttachment.h for the secured-pilot relationship this struct
// pairs with, unaffected by this milestone's change (it reads whatever
// velocity the spacecraft actually has, however that velocity got there).
struct FlyingPrimitiveControl {
    BodyHandle handle;
    bool controlled = false;
    bool sasEnabled = false;
    glm::quat sasTargetOrientation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Enables/disables the spacecraft's rate-zero attitude hold. Enabling
// captures the current authoritative orientation as the hold target.
void SetSpacecraftSasEnabled(FlyingPrimitiveControl& control, bool enabled,
                              const PhysicsWorld& physics);

// Called once per fixed step, after gravity is accumulated and before
// PhysicsWorld::Step. Pilot input contributes only while `controlled` is
// true; enabled SAS torque remains active after pilot release.
//
// Milestone 12: applies a constant-magnitude force along the combined held
// translation directions, and a constant-magnitude torque about each held
// rotation axis, via PhysicsWorld::ApplyForce/ApplyTorque — NOT a directly
// commanded velocity/angular velocity. Must be called fresh every fixed
// step held input should still be contributing force/torque: nothing here
// persists a force or torque across steps on its own (PhysicsWorld::Step
// clears both accumulators every step after integrating them — see
// PhysicsWorld::Step's own comment). Releasing every key simply means this
// function contributes nothing that step; it does NOT zero the
// spacecraft's existing velocity/angular velocity — see "no automatic
// braking" in docs/ARCHITECTURE.md, "Milestone 12."
//
// Every control axis (translation AND rotation) is still derived from the
// spacecraft's OWN current orientation — never gravity, never a fixed
// world axis (unchanged from Milestone 11; this function still takes no
// GravityField). Gravity remains a completely independent input to this
// same body via PrepareDynamicBodiesForStep/ApplyLinearAcceleration,
// called separately by the caller; the force/torque applied here compose
// with gravity's own contribution inside the same Step() integration, they
// never override it.
void ApplyFlyingPrimitiveControl(FlyingPrimitiveControl& control, const Window& window,
                                  PhysicsWorld& physics);
