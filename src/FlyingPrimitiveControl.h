#pragma once

#include <glm/glm.hpp>

#include "PhysicsWorld.h"

class Window;
class GravityField;

// Milestone 8's entire control-ownership mechanism: input authority can
// move from the player to one other physical object and back. The flying
// primitive itself is an ordinary DynamicBody (src/DynamicBody.h) like any
// other test object in Application.cpp's dynamicBodies list — ordinary
// gravity via PrepareDynamicBodiesForStep, ordinary collision via
// PhysicsWorld::Step, ordinary presentation interpolation, ordinary
// ResetToSpawn. None of that is duplicated or special-cased here. This
// struct/function pair adds exactly one thing on top of it: while
// `controlled` is true, WASD (repurposed) and Q/E are turned into a
// directly-commanded linear/angular velocity for its body, overriding
// whatever PrepareDynamicBodiesForStep's ordinary gravity application
// contributed that step — the same "Judas commands the velocity outright,
// physics obeys" idiom PlayerController's own grounded locomotion already
// uses (see docs/ARCHITECTURE.md, "Locomotion"). This is deliberately not a
// vehicle-physics framework: no thrust/fuel/engine model, no generalized
// possession mechanism for multiple objects — one handle, one bool.
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
void ApplyFlyingPrimitiveControl(FlyingPrimitiveControl& control, const Window& window,
                                  PhysicsWorld& physics, const GravityField& gravity);
