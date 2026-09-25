#pragma once

class RuntimeWorld;

// Milestone 29: the reduced-fidelity simulation path.
//
// Advances every Active, Coarse entity by one fixed step from its retained
// EntityPhysicalState, with no PhysicsWorld body:
//
//   Settled  — nothing moves. An entity demoted while at rest (below the
//              settle thresholds) is assumed to remain supported by the
//              static geometry it rested on; it costs nothing per step.
//   Inertial — free flight. The same symplectic Euler update the rigid-body
//              integrator uses (velocity, then position; quaternion
//              integrated from angular velocity), driven by the same
//              acceleration sources a live body receives: the scene's local
//              gravity contexts, static point-mass sources, and pairwise
//              Newtonian gravity with the other celestial participants
//              (live ones read from PhysicsWorld, coarse ones from their
//              records). No contacts, no friction, no drag.
//
// This is real reduced physics, not an animation: the state it evolves is
// exactly the state reconstruction needs, so promoting back to Full hands
// the live body the coarse pose and velocities with no discontinuity
// beyond what the missing contact response would have produced. That
// missing response is the documented limit: an Inertial coarse entity
// passes through solid geometry. A policy that lets an entity go coarse
// while heading into terrain is choosing to accept that; the M29 demo
// keeps its inertial coarse entities in free space.
//
// Systems that cannot be reduced (fluid, atmosphere, combustion, the
// player, pilotable vehicles, compound bodies) keep their entities at Full
// by capability (RuntimeWorld::EntityRequiresFull); this file never sees
// them.
void StepCoarseEntities(RuntimeWorld& world, float fixedDeltaTime);
