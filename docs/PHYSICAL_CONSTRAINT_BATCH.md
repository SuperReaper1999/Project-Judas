# Native physical constraint participants (Experimental)

This optional C++ interface supports development of the replacement pressure
backend. **It does not enable Advanced Fluids**, replace `FluidWorld`, or add a
JudasJS binding. Published Alpha packages retain their compiled fluid lock.

Include `PhysicsConstraintBatch.h` alongside `PhysicsWorld.h`. Set
`PhysicsWorld::VelocityCoupling::physicalBatch` to a participant that lives through
that `Step`/`StepCoupled` call. The field is appended so existing aggregate member
order remains intact. Native clients must rebuild for the changed structure size.
Unrelated projects do not include the batch header through `PhysicsWorld.h`.

## Ownership and timing

Register dynamic body handles in `PhysicsConstraintParticipant::bodies`. The
world checks world ownership, generation, active status, duplicates and declared
bounds before integration, then wakes registered owners before predictor and
contact construction. Contact/joint connectivity expands through dynamic bodies;
a shared static floor does not connect all the unrelated objects resting on it.

At each native chronological boundary, after force shares/arrival response but
before warm starts, `propose` receives copied poses, physical velocities, mass,
inertia, native contact Jacobians and prepared joint rows. Body and joint handles
retain their actual identities; array indices are local to that snapshot. A
world anchor has an invalid body handle and an explicit `worldAnchor` flag.
Never infer an owner from position, retain references into the batch, mutate the
world, or recursively step it from `propose`.

Return only material linear/angular impulses plus matching native contact and
joint impulses. The engine computes physical velocity from that complete impulse
sum. It validates dimensions, finite values, normal complementarity, joint
constraints and participant lifetimes/configuration before its own publication.
Unsupported bounds, friction or constraint types raise actionable errors; nothing
is truncated. Snapshot joint preparation operates on copied state and warm data.

`published` acknowledges an accepted transaction. Use it only for a nonthrowing
swap of already staged material state. Rejected proposals do not invoke it.
Returning false is a deliberate request for the ordinary native solve, **not** a
safe fallback for a failed live fluid solve. A failed live participant must throw
and stop the world. Earlier substeps, impacts and applied external force shares
are not rolled back; this interface does not promise whole-world transactions.

The usual contact/joint solve then continues with those already-applied impulses.
Warm starts are not applied a second time. Native history, joint observations,
contact publication and authoritative pose/CCD advancement retain their owners.
Existing users of `begin`/`solve`/`finish` retain their conventions. Combining
those velocity-changing callbacks with a physical batch is currently rejected;
`afterSubstep` observation remains available. This avoids pretending that a later
foreign impulse preserves the already-solved material boundary.

## Current support boundary

- Frictionless native contacts, including their actual speculative gap allowance
  through `Contact::minimumNormalSpeed`. A material solver must honor that target,
  rather than assuming all touching-looking points are stationary equalities.
- Rigid bilateral fixed, ball, hinge and slider rows.
- Static/prescribed boundary snapshots, connected dynamic bodies and world anchors.
- Explicit `StepCoupled` intervals with the same four native chronological phases.

Friction, arrival/deferred-impact coupling, joint limits, springs, drives,
rotational resistance and structural material joints are currently rejected when
they belong to the participating assembly. Unrelated assemblies keep their normal
native behaviour. Contact membership can change during later CCD motion; this is
not a universal material/CCD synchronization guarantee.

Defaults bound a batch to 64 bodies, 256 contacts and 256 joint rows. Native hard
bounds are 4096/16384/16384. Hitting a declared limit fails; it never clips an
island or silently lowers physical quality. Those are resource limits, not a
claim that every pressure solver scales to them. The existing profiler separately
labels `Physics material constraint batch` only when this path is used.

## Validation and remaining fluid work

`tests/PhysicalConstraintBatchTests.cpp` exercises physical free/support/joint
endpoints, rotated forcing, real sleeping owners, common-floor isolation,
speculative targets, scoped clocks, stale/foreign handles, malformed impulses,
mutation/lifetime rejection and deliberately omitted reactions. Existing native
physics, joints and cold saves must continue passing separately.

The fluid reference also exercises the native API with staged 3D pressure fields.
This is not yet the application backend. In particular, the old translating-piston
mesh silently loses body-boundary tags when actual native motion includes a tiny
rotation. The reference now detects this and fails closed; general moving owned
cut geometry must be completed before claiming that runtime case passes.

Detailed measured outcomes and preserved failures: [native interface checkpoint](ADVANCED_FLUID_NATIVE_INTERFACE.txt).
