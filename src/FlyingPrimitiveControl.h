#pragma once

#include <glm/glm.hpp>

#include "PhysicsWorld.h"

class Window;

// Milestone 8's control-ownership mechanism, upgraded in Milestone 11 to
// full 6-degree-of-freedom spacecraft control (translation along all three
// local axes, plus independent pitch/yaw/roll) — the underlying object is
// repurposed, not replaced: still one ordinary DynamicBody (src/DynamicBody.h)
// in Application.cpp's dynamicBodies list, still ordinary gravity via
// PrepareDynamicBodiesForStep, ordinary collision via PhysicsWorld::Step,
// ordinary presentation interpolation, ordinary ResetToSpawn. This
// struct/function pair still adds exactly one thing on top of that: while
// `controlled` is true, input is turned into directly-commanded linear and
// angular velocity for its body, overriding whatever
// PrepareDynamicBodiesForStep's ordinary gravity application contributed
// that step — the same "Judas commands the velocity outright, physics
// obeys" idiom PlayerController's own grounded locomotion uses. Still
// deliberately not a vehicle-physics framework: no thrust/fuel/engine
// model, no generalized possession mechanism for multiple objects — one
// handle, one bool. See docs/ARCHITECTURE.md, "Milestone 11," and
// src/PilotAttachment.h for the new secured-pilot relationship this
// milestone adds alongside it.
struct FlyingPrimitiveControl {
    BodyHandle handle;
    bool controlled = false;
};

// Called once per fixed step, AFTER PrepareDynamicBodiesForStep (so this
// body has already received ordinary gravity exactly like every other
// dynamic body) and BEFORE PhysicsWorld::Step (so the commanded velocity
// below is what actually gets integrated and checked for collisions this
// step). A no-op unless `control.controlled` is true. Takes no
// fixedDeltaTime — velocity is commanded directly (kinematic-style, the
// same idiom as PlayerController's grounded WASD control), never
// integrated from an acceleration here.
//
// Milestone 11: every control axis (translation AND rotation) is derived
// from the spacecraft's OWN current orientation — never gravity, never a
// fixed world axis (see docs/ARCHITECTURE.md, "Milestone 11," for the full
// mapping). Through Milestone 8/10, vertical control ("up") was gravity-
// relative; that coupling is deliberately removed here — this function no
// longer takes a GravityField at all, satisfying the brief's "spacecraft
// controls must not know which concrete gravity field is active" even more
// directly than by staying implementation-agnostic. Gravity remains a
// completely independent input to this same body via
// PrepareDynamicBodiesForStep, called separately by the caller; this
// function only ever OVERRIDES what that contributed this step, the same
// relationship as before — it never changes what gravity IS.
void ApplyFlyingPrimitiveControl(FlyingPrimitiveControl& control, const Window& window,
                                  PhysicsWorld& physics);
