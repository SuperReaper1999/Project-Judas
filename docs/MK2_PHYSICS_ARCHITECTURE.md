# Judas Physics MK2 — current production contract

MK2 is Judas's only rigid physics solver. The operator human-accepted
`c859901cc9a52cf5b7104887f56b5e7ad4269b1d`; main and Experimental aligned there.
Subsequent Experimental corrections require their own acceptance. There is no
MK1 selector, fallback solver or requirement to reproduce MK1 trajectories.

The [unaltered development record](MK2_PHYSICS_DEVELOPMENT.md) preserves the
experiments, rejected approaches, original failures, measurements and stage-time
acceptance statements. Its old “not promoted”, alternate-solver flags and interim
open gaps are historical, not the current runtime contract.

## Ownership and update order

`src/PhysicsWorld.cpp` coordinates one world. The implementation map is
[src/physics/WorldModules.md](../src/physics/WorldModules.md).
`src/physics/WorldMk2Solver.cpp` owns the simultaneous solve and bounded continuous
pass. The same implementation handles ordinary steps, coupled intervals and
velocity-coupling participants; there is no old-solver path for coupled work.

A normal rigid step has four substeps. Contact blocks use two biased sweeps and
one unbiased relaxation sweep; joint-connected blocks receive the additional
joint sweeps. Contact geometry, inertia, anchors and concave contact geometry are
refreshed as required. Joint position error uses split-impulse projection, keeping
correction out of stored real velocity. Arrival-instant impulses/restitution and
the motion ledger represent travelled paths; presentation does not own physics.

A coupling receives `begin(dt)` once, exactly its requested `solve(dt,round)`
calls spread over substeps, then `finish(dt)` once. Bodies woken during coupling
join through the same owner, without a second simulation system. Gameplay forces
and per-entity gravity are engine primitives; input policy remains project JS.

## Contacts, persistence and determinism

Contact geometry, warm impulses, completed contact observations and sleep state
have distinct lifetimes. The solver publishes step-total contact impulses.
Joint continuation version 2 records its warm interval; older records have their
explicit per-step interpretation. Unknown historical continuation is not invented.

The owner solves in ordered candidate/manifold order. Parallel initial geometry
reads frozen inputs and joins before mutation. Public physics methods are not
generally thread-safe. Physical trajectories need not be bit-identical across
platforms/compilers. Relative float geometry operates inside the bounded physical
chart; origin services preserve absolute double positions separately.

## Supported envelope and limits

The fast-body pass sweeps ledger segments with the shared CCD search. It bounds
re-timing at four arrivals per body and 256 per step. A body can be held at an
arrival when response is unavailable or a bound is exhausted: this prevents
unbounded work/tunnelling, but can lose travelled distance in extreme cases.
Sidedness, deep initial overlap and bounded temporal search are not a promise of
exact unrestricted continuous dynamics. Legacy StepStats fallback fields do not
yet quantify every MK2 hold; do not interpret zero as proof no hold occurred.

Dense-contact cost depends on content, joints, contact representation and hardware.
The development record contains reproducible ordinary/severe room1024 and live
WASD measurements with exact stage identities. Those are not universal 60 Hz
certification or fresh measurements of every later checkpoint. The old Pre-Alpha
TOI-event/island hitch diagnoses describe the removed MK1 architecture.

## Validation

The focused manifest schedules MK2 foundation, physical balance, original physical
mesh arrival/continuation, contacts, movement and save/lifecycle regressions.
Water/default-off policy and fresh-process basic-water continuation have their own
entries. Synthetic and automated passes do not establish human acceptance.
See [Pre-Alpha status](PREALPHA.md) and
[the final correction work](PREALPHA_FINAL_CORRECTIONS.md).
