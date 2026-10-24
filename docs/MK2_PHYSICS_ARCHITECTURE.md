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

A normal rigid step has four chronological substeps. Contact blocks use two
biased sweeps and one unbiased relaxation sweep; joint-connected blocks receive
additional joint sweeps. Relaxation uses the configuration at which its impulses
were applied, before geometry/inertia/Jacobians advance. Position recovery has a
separate geometric channel; it is not stored as physical velocity. Arrival response
samples pose and velocity at the same time, publishes both affected paths together,
and invalidates affected dynamic dependencies without revisiting committed history.

`src/RigidRotation.cpp` constructs adaptive implicit-midpoint/Cayley trajectories
for asymmetric dynamic rotation and analytic exponential trajectories for principal
rotation. Integration, CCD and public motion sampling use the same curve. Adaptive
knots preserve a common linear episode origin/time. The local angle tolerance is
1e-6 radians, with explicit bounded refinement failure; energy is not clamped.
CCD contact depth allowances span the episode, rather than restarting at each knot.
A conservative point-speed bound preserves ordinary speculative support when the
existing restitution law cannot bounce; the 0.5 m/s cutoff is unchanged.

A coupling receives `begin(dt)` once, exactly its requested `solve(dt,round)`
calls spread over substeps, then `finish(dt)` once. Bodies woken during coupling
join through the same owner, without a second simulation system. Gameplay forces
and per-entity gravity are engine primitives; input policy remains project JS.
All awake bodies share the chronological intervals. Pre-applied external kicks
are redistributed only for actual constraints/velocity couplings; free flight
retains its public pre-step impulse timing. The [qualification follow-up](PREALPHA_FINAL_QUALIFICATION.md)
records the suspension regression and balanced-kick proof for this correction.

## Contacts, persistence and determinism

Contact geometry, warm impulses, completed contact observations and sleep state
have distinct lifetimes. The solver publishes step-total contact impulses. Repeated continuous observations
coalesce only matching body revisions, nearby anchors and parallel normals, summing
all reaction impulses independently of the warm cache. CCD-awakened bodies join
the next solver boundary and retain a stationary prefix in their motion ledger.
Joint continuation version 2 records its warm interval; older records have their
explicit per-step interpretation. Unknown historical continuation is not invented.

The owner solves in ordered candidate/manifold order. Parallel initial geometry
reads frozen inputs and joins before mutation. Public physics methods are not
generally thread-safe. Physical trajectories need not be bit-identical across
platforms/compilers. Relative float geometry operates inside the bounded physical
chart; origin services preserve absolute double positions separately.

## Supported envelope and limits

The fast-body pass sweeps ledger segments with the shared CCD search. It bounds
re-timing at four distinct arrivals per body and 256 response iterations per step.
Same-instant iterations count against the global bound without exhausting distinct
future-arrival slots. A certified static-planar sphere fallback can retain tangential
travel, subject to ordinary CCD against other colliders. A body can be held at an
arrival when response is unavailable or a bound is exhausted: this prevents
unbounded work/tunnelling, but can lose travelled distance in extreme cases.
Sidedness, deep initial overlap and bounded temporal search are not a promise of
exact unrestricted continuous dynamics. StepStats and the profiler now count actual path-hold operations, partitioned into
per-body arrival limit, per-step arrival limit and unavailable response. They are
not unique-body counts; coupled intervals sum operations. `impactSafetyFallback`
is the total, and `impactEventCapFallback` is the body/step-cap subset. These
counters describe actual safety operations. They do not imply perfectly accurate
unrestricted continuous dynamics.

Continuous response attempts are partitioned into applied responses, unavailable
or uncertain manifolds, and rejected energy budgets. A rejected kinematic response
need not produce a path hold. The older `impactEvents` includes both substep
restitution work and continuous arrival attempts; it is not a unique collision
count. Native counts reset each step. The matching CCD profiler counters use the
completed public step, including StepCoupled totals, rather than only its last
interval. Restitution frictionless retries, inelastic fallbacks and coupled-impact
fallbacks are separate profiler **frame sums**, batched once per solve instead of
emitting an event per contact/retry. These diagnostics are observational; they do not select gameplay policy.

Dense-contact cost depends on content, joints, contact representation and hardware.
The development record contains reproducible ordinary/severe room1024 and live
WASD measurements with exact stage identities. Those are not universal 60 Hz
certification or fresh measurements of every later checkpoint. The old Pre-Alpha
TOI-event/island hitch diagnoses describe the removed MK1 architecture.

The [rotation/contact correction](PREALPHA_MK2_ROTATION_CORRECTION.md) repairs the
moderate-rate elastic energy-gain reproduction through consistent rotation and
chronological response. Its measured energy, collision, momentum, cold-save and
performance evidence is separate from the preserved original failure record.
It was human accepted at `c76e92dc20b5765f61d6077bf618a8b94ac22462`, not a universal
no-energy-error guarantee: nominal e=1 still loses some energy over long runs,
extreme spin has measured phase error, and bounded arrivals can truncate motion.

## Validation

The focused manifest schedules MK2 foundation, physical balance, original physical
mesh arrival/continuation, contacts, movement and save/lifecycle regressions.
Water/default-off policy and fresh-process basic-water continuation have their own
entries. The rotation and process-separated rotation-cold suites cover the new
trajectory and continuation contracts. Synthetic and automated passes do not establish human acceptance.
See [Pre-Alpha status](PREALPHA.md) and
[the final correction work](PREALPHA_FINAL_CORRECTIONS.md).
