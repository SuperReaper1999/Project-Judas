#pragma once

class RigidBody;
class GravityField;

// The one place gravity enters the new Judas-owned physics stack: sample
// `gravity` at `body`'s own current position and turn that acceleration
// into a force on the body's accumulator, exactly the "Judas samples,
// physics obeys" ordering every existing gravity consumer already uses
// (PlayerController::FixedUpdate, DynamicBody's PrepareDynamicBodiesForStep).
// RigidBody itself stays ignorant of GravityField — this free function is
// the seam, not a method on the body — so the low-level body-state/
// integration layer (RigidBody.h) never needs to know gravity exists at
// all, matching "Judas owns gravity, never the physics middleware" now
// applied to Judas's own middleware too.
void ApplyGravity(RigidBody& body, const GravityField& gravity);
