#pragma once

#include <glm/glm.hpp>

#include "Contacts.h"

class RigidBody;

// Judas-owned contact resolution: sequential impulses (velocity-level,
// resolves the normal and Coulomb-friction constraints) plus direct
// positional correction (a small, penetration-proportional position nudge
// — the standard cheap alternative to a full position-level solve, kept
// deliberately simple for this engine's scale). Nothing here reads or
// assumes any world-space axis: every quantity is derived from the
// contact's own normal/point and each body's own state.
//
// `bodyA`/`bodyB` follow Contact's own convention (the normal separates A
// from B, pointing toward A). Either may be static (RigidBody::IsStatic())
// — ApplyLinearImpulse/ApplyAngularImpulse are already no-ops for a static
// body, so no special-casing is needed here.
void ResolveContact(RigidBody& bodyA, RigidBody& bodyB, const Contact& contact, float friction,
                     float restitution);
