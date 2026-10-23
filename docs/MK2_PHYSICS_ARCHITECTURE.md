# Judas Physics MK2: architecture

Status: **working prototype** (Experimental, opt-in). MK1 stays the default, the reference and
the fallback.

- Opt-in: set `JUDAS_PHYSICS_MK2=1` in the environment. It is read at `PhysicsWorld::Init`.
- No human validation has been done. Everything below is automated tests, probes and benchmarks
  on one Linux machine.

## 1. What MK1 taught us (evidence, not opinion)

Sources:

- `docs/PREALPHA_SPECULATIVE_GATE.txt` §1-8;
- `docs/PREALPHA_PHYSICS_INVESTIGATION_ACCOUNT.txt`.

Findings:

- **The cascade is structural.** MK1 sends every newly encountered separated pair to an exact,
  time-ordered TOI event loop (`AdvanceImpacts`). Each event solves a growing domain at its own
  instant, then re-queries the neighbours of every body whose velocity changed.
  - In the severe room1024 burst that is about 5,000 events and 1.4M TOI queries per 900 steps.
  - The worst step is about 40 ms, and the live worst frame is about 200 ms.
- **Local cuts barely help.** Removing 16-66% of queries saved only about 1-8% of time.
  - The exact-time events re-solve the same bodies 2-3× in the worst steps.
  - Each re-solve starts from a genuinely different state: only 0.03% of repeats had identical
    poses, so nothing can be reused exactly.
- **Speculative rows bolted onto MK1's single-pass solver are one-step-lagged impacts.**
  - They broke exact-arrival contracts.
  - MK1 also lacked per-substep separation, a restitution pass and symmetric patch handling.
- **Settling tests need jitter.** "All 1,152 asleep" depends on the authored lattice being
  perfectly aligned. Stacking quality must also be judged on jittered and rotated fixtures.

## 2. The MK2 step (as built)

`Impl::Mk2Solve(dt)` lives in `src/physics/WorldMk2Solver.cpp`. It replaces four MK1 stages:

- `ResolveInitialImpacts`;
- `ContactSolver::SolveVelocities`;
- `AdvanceImpacts`;
- `SolvePositions`.

It runs on every step except those with velocity coupling or a coupled interval, which still go
to MK1.

Everything else is unchanged:

- broadphase;
- narrowphase;
- touch observation;
- sleep islands;
- proxies;
- saves;
- the motion ledger;
- all public APIs.

### 2.1 Substepped rigid solve

Each step runs N = 4 substeps of h = dt/4. Each substep:

1. Adds the step's external velocity change / N. `Body::stepVelocityDelta` collects gravity and
   `ApplyLinearAcceleration`, so arbitrary and per-body gravity fields are redistributed exactly.
2. Warm-starts each manifold block from the cached impulse / N.
   - From substep 1 on, the whole block is scaled by min(1, Σsteady / Σcurrent).
   - An impact spike is therefore never re-applied, and because the whole block is scaled
     uniformly the cap creates no torque.
3. Runs 2 biased iterations. Each sweep solves the joints first and the contacts last, so its
   final velocity never approaches a contact.
   - Rows are **rigid**: soft contacts were tried and sank about 1 cm per contact in a 20-stack.
   - Speculative rows (s > 0) target −s/h.
   - Penetration recovers at 0.02·(s + slop)/h, with a 0.5 mm slop, capped at 3 m/s. A
     stronger tier for overlap beyond 2.5 mm was tried and rejected: it toppled the 20-stack.
   - Joints correct 0.1 of their position error per substep (MK1: 0.2). The ratio of the joint
     and contact rates decides how a friction-locked pose conflict splits into joint error versus
     penetration; at 0.2, a ragdoll's head sat 10 mm into a flat floor for seconds.
4. Integrates poses through the motion ledger, one `RigidMotionSegment` per substep, so player
   sweeps and `GetBodyMotionSegments` see the travelled path.
5. Re-collides concave pairs (triangle mesh, terrain) at the new poses, then refreshes geometry:
   - **Re-collision:** a fixed row normal is exact only for convex pairs. A body sliding along a
     curved mesh would follow the tangent plane of the face it started on while the surface rises
     ahead; it would also miss every crease it crossed until the next step. A ragdoll's head
     reached 47 mm in the coarse trough this way. Each row of such a pair moves to the nearest new
     contact of the pair, keeping its impulse and re-projecting its friction; the anchor gap at
     re-collision becomes its new baseline. A row with no new contact becomes far speculative,
     so the solver releases its impulse.
   - **Inertia:** world inverse inertia is recomputed from the new attitude.
   - **Lever arms:** they are rotated from the contact's precise local anchors to one common
     point: rA = dqA·rA0 − d/2 and rB = dqB·rB0 + d/2, where d is the anchor gap.
   - **Separation:** it is updated from displacements only, so it is safe in large worlds.
6. Runs 1 relax iteration with no bias. The joints' rigid positional bias is dropped here too
   (springs, resistance and motors keep their targets), so no positional correction outlives
   its substep as motion.

Two further rules apply across the step:

- **Joints** are solved inside the same substep loop by the existing `jointSolver`.
- **Unconstrained bodies** use MK1's exact single-segment free flight.

**Manifolds are block-solved.** Each manifold (2–4 points) is a regularised active-set LCP:
Cholesky with a 1e-9 diagonal, then friction on an isotropic cone. This symmetric response keeps
box-on-sphere columns upright. MK1's `NormalPatch` failed on rank-3 four-point patches.

### 2.2 Restitution pass

After the substeps, rows whose pre-solve approach speed exceeds 0.5 m/s get a block restitution
solve, iterated over up to 8 sweeps.

- The target is e × the measured approach velocity.
- Iterating propagates impulses through simultaneous chains. In a three-body elastic line the
  result is symmetric, momentum is exact and the energy matches (9.000 → 8.999).

### 2.3 Bounded CCD pass (`Mk2Continuous`)

The pass runs only for **fast** bodies: those whose largest ledger segment reach, (|v| + |ω|R)·h,
exceeds 0.25 × their smallest feature.

1. Each candidate pair is swept with MK1's exact `ImpactTime` on the recorded ledger.
   - A pair **touching at the start** gets MK1's own continuation search: for mesh pairs and
     spinning pairs, the first time the overlap exceeds a quarter of the smallest feature
     (depth). Other touching pairs belong to their rows.
   - Otherwise the result is the first-touch time.
2. The pass decides whether to clamp the pair:
   - A pair with no row clamps on any hit.
   - A touching pair clamps at its depth-search hit.
   - A non-touching pair with a row clamps only if it **actually went deep**. That means any of:
     - its end separation is below −depth;
     - the contact point ended behind the first-touch plane;
     - the end pose overlaps by more than depth;
     - the same depth search from first touch hits before the step ends.
   - Before this stage every touching pair was treated as hit at t = 0. Rigidly carried anchors
     then misjudged fast-spinning boxes: some were frozen in place every step while their
     velocity grew, and others were never re-swept and passed through the trough.
3. On a clamp:
   - The fast body's ledger is held at the hit pose.
   - `Mk2ResolveArrival` runs MK1's `ImpactSolver` on private copies at that pose, using manifold
     margins of 1 mm, 5 mm and 1 cm, and copies the velocities back.

Each clamp counts as `stats.impactEvents`. The pass runs at most once per fast body per step,
so it cannot cascade by construction.

### 2.4 Narrowphase options (MK2-only; MK1 keeps its exact behaviour)

- **`facePreference`:** a separated box-box edge axis must beat the best face axis by 1e-6.
  Without it, a degenerate edge axis made rotated stacks land on a single corner.
- **Cooked-mesh `nearest` reduction:** only candidates within 2 cm of the closest feature are
  kept. Without it, large margins chose far edges whose normals pointed sideways.
- **Triangle normal cone** (same flag). An axis other than the triangle's face normal is a
  solver normal only at an **active** (convex) edge or vertex of that triangle. This applies to
  the query box's own face axes too, which MK1 always allows. Across an inactive (coplanar or
  concave) crease the neighbour owns that direction, so the triangle answers along its face
  normal, with the gap measured along it.
  - Without this, a spinning box on a perfectly flat sheet received normals such as
    (−0.13, 0.28, 0.95) from neighbouring triangles. It tipped, then dropped through the
    one-sided sheet.
  - The cooker's convention is that convex creases are active, which matches this rule.
- **Minimum speculative margin of 2 mm.** Two bodies resting together have no relative motion,
  so a motion-only margin is about zero, and a corner 1e-10 m apart can be culled by rounding.
  That culled one side of a box-on-box patch and toppled the authored box-on-sphere column.
  Speculative rows are exact, so the floor costs nothing in accuracy.
- The continuous pass's `PairAt` uses the same MK2 options, so it judges the same manifolds
  as the rows.

### 2.5 Persistence, sleep, determinism, precision

- **Contact identity and cache format** are MK1's (`ContactKey`, local anchors).
  - The cache stores the final relaxed substep load × N, before restitution.
  - Touch events receive step totals.
  - Saves, `ReadBodyContactSample`, the sleep graph and cold continuation therefore read the same
    data as MK1.
- **Determinism:** the solve is serial in a fixed order (sorted candidate pairs, then manifold
  order). There are no wall-clock budgets.
- **Precision:** solver state is float in relative form (anchor offsets, pose deltas). Large-world
  coordinates never enter the iteration.

## 3. Contract mapping

Physical contracts MK2 must meet:

- no tunnelling;
- non-penetration within slop;
- linear and angular momentum conservation;
- no energy gain;
- rebound equal to the restitution coefficient;
- Coulomb friction;
- stable stacks that sleep;
- deterministic replay;
- cold-save continuation.

MK1-specific representation contracts are not MK2 targets:

- analytical endpoints to 2e-6 at the exact arrival instant;
- per-event observability;
- exact-TOI ledger anchors.

MK2 states its own bound instead: arrival timing is within one substep.

## 4. Evidence

### 4.1 Focused acceptance tests (`judas_mk2_physics_tests`, all PASS)

- **A. Authored columns stay upright and sleep.** The SBBS, BBSB and BSBB columns at the room1024
  coordinates.
- **B. Stacks hold.**
  - A rotated 5-stack stays aligned and sleeps.
  - A 20-stack's height error is under 2 cm.
- **C. Momentum is conserved in a fast spinning impact.** Linear and angular momentum error is
  about 1e-6.
- **D. Simultaneous arrivals are correct at e = 0 and e = 1.**
  - The response is symmetric.
  - Momentum is conserved.
  - No energy is created.
- **E. No crossings.** 135 fast or spinning box and sphere cases against a 2 cm wall gave 0
  crossings, checked every substep.
- **F. Determinism.** Two runs of a 30-body pile are bitwise identical.

### 4.2 room1024 (same machine, release build)

Both engines were run with the same `stress-v3` driver and binary. MK1 runs at the default; MK2
runs with `JUDAS_PHYSICS_MK2=1`. Times are fixed-step milliseconds.

**MK1 is unaffected.** After stage 2 the MK1 ordinary900 trajectory digests are still
identical to the previous checkpoint on 900 of 900 steps.

The first MK2 column is the stage-1 code; the second is the stage-2 code, run once with the
machine otherwise idle.

| Workload | Metric | MK1 | MK2 stage 1 | MK2 stage 2 |
|---|---|---|---|---|
| ordinary900 | mean / p95 / worst step | 1.73 / 5.77 / 14.9 | 1.97 / 9.61 / 16.7 | 1.95 / 9.24 / 16.8 |
| ordinary900 | sleeping at end | 1,152 | 1,152 | 1,152 |
| ordinary900 | deepest endpoint penetration | 3.1 mm | 0.8 mm | 0.8 mm |
| severe900 | mean step | 12.2 | 10.1–10.5 | 10.9 |
| severe900 | p95 step | 22.8 | 12.1–13.5 | 13.1 |
| severe900 | worst step | 40.8 | 16.7–37.4 (5 runs) | 20.5 |
| severe900 | steps over 16.67 ms | 130 | 0–11 | 4 |
| severe900 | impact events / TOI queries | 5,020 / 489k | 0 / 11.8k | 0 / 12.4k |
| severe900 | sleeping at end / deepest penetration | 140 / 5.0 mm | 134 / 6.8 mm | 137 / 5.8 mm |
| live120 (WASD) | worst visible frame | 212.8 ms (catch-up spiral) | 71 ms (onset) | 71.5 ms (onset) |
| live120 (WASD) | worst frame after the first 3 s | 212.8 ms | 23.4 ms | 41.7 ms |
| live120 (WASD) | worst step / steps over budget | 45.9 / 131 | 21.0 / 3 | 24.3 / 7 |
| live120 (WASD) | mean step | 10.4 | 11.1 | 11.6 |

How to read these numbers:

- **The severe worst step is wall-clock noise in a deterministic run.**
  - The worst-step cluster fell at different steps in different runs (for example 287, 409 and 710).
  - At step 287 the candidate and contact counts matched (about 12.2k and 3.25k), but the
    narrowphase took twice as long.
  - The robust gains are the p95, the over-budget count and the visible-frame figures.
- **The live repeat run is excluded.** That run had an 8-step catch-up stretch, but it overlapped
  a git commit and the harness flagged it as having changed source inputs.
- **The quiet case costs more.** MK2's ordinary mean is about 14% higher, which is the price of
  4 substeps on settled piles.
- **Remaining cost is split evenly.** The narrowphase and the solver each take about half of a
  worst MK2 step.
- **Stage 2 costs about 4–5% in the burst.** The 2 mm margin floor adds about 4% more contact
  rows to severe900 (2,983 against 2,856 on average); the ordinary workload is unchanged.
  - room1024 has no meshes, so re-collision does not run there.
  - The live run's 41.7 ms frame is a short stretch of two-step frames of 12–15 ms each at
    5.2 s. It is not an 8-step catch-up spiral: there were none.

### 4.3 Existing suites under MK2 (stage 2 code)

The runner calls each of the 56 physics programs without arguments.

- **MK1** passes 53 of them.
- **The other three exit with usage code 2:** `balance_lab`, `character_lab` and
  `save_streaming` need arguments. An earlier version of this section wrongly counted them as
  passes. They are run with arguments in the cold-continuation table below.

With `JUDAS_PHYSICS_MK2=1`, 45 programs give the same outcome as MK1. They include:

- wheels, joints, joint application and ragdoll;
- terrain;
- uniform and celestial gravity, and origin shifts;
- kinematic lab;
- curved-mesh support continuation;
- rigid contact;
- `physics_continuation`;
- **`impact_robustness`** (4,412 checks, 0 failures);
- **`impact_mesh_cost`** (372 checks, 0 failures).

The last two were the target of stage 2. Eleven programs differ; FAIL counts are lines in each
log.

| Program | MK2 result | Classification |
|---|---|---|
| candidate_reuse | 1 FAIL | MK1 bookkeeping (TOI candidate reuse) |
| impact_batch_continuation | 4 FAIL | MK1 bookkeeping (event batches) |
| impact_discovery | 2 FAIL | MK1 bookkeeping (event discovery) |
| support_guess_ownership | 6+ FAIL | MK1 bookkeeping (support-guess owner) |
| contact_lifecycle | 3 FAIL | MK1 bookkeeping (TOI-solve observations) |
| impact_timing | 6+ FAIL | MK1 representation (2e-6 analytical endpoints; MK2 bound is one substep) |
| rigid_motion | aborts at check 10235 | MK1 bookkeeping: `impactEvents > 0` for a contact that MK2 solves without an event |
| kinematic | 3 FAIL | Partly bookkeeping (TOI evidence, owner event); **real gap 3** |
| collision_geometry | 1 FAIL | **Real gap 1**: M42 mesh-triangle touch |
| collision_continuity | 6 FAIL | **Real gap 2**: arrival impulse timing |
| post_consumer_ragdoll | 1 FAIL | **Real gap 4**: passive joint resistance |

**Programs run with their arguments** (absolute XDG paths):

| Run | MK1 | MK2 |
|---|---|---|
| save_streaming write, then cold read | pass | pass |
| character_lab, cold-write, cold-read | pass | pass |
| balance_lab supports, radial | pass | pass |
| balance_lab standing, recovery, cold-write, cold-read | pass | **fail (real gap 5)** |

The balance-lab failure is not new: MK2 at `af0b46aa`, built separately, fails the same checks.
It is also unaffected by this stage's joint changes, which were reverted for the test.

### 4.4 Mesh contact quality (stage 2)

`impact_mesh_cost` throws a 12-body jointed ragdoll into each surface; the 24-box fast family is
also run. The ragdoll's deepest bone penetration is reported, not asserted, and these
trajectories are chaotic. The table therefore gives the range over throw speeds of 0.8, 1.0,
1.2, 1.4 and 1.6 m/s (300 steps each), as max / mean-per-step deepest, in mm.

| Surface | MK1 | MK2 before stage 2 (1.2 m/s only) | MK2 stage 2 |
|---|---|---|---|
| box primitive floor | max 10.1–34.9, mean 1.4–3.9 | 9.0 / 1.9 | max 6.5–7.8, mean 0.7–2.5 |
| flat sheet 6×6 | max 6.6–11.3, mean 1.0–1.8 | 293 / 128 (bones through the sheet) | max 2.4–6.2, mean 0.6–0.8 |
| flat sheet 60×60 | max 10.6–13.3, mean 1.1–2.1 | 191 / 6.5 (bones through the sheet) | max 7.1–11.7, mean 1.2–2.1 |
| trough 2×26 coarse | max 9.1–17.1, mean 1.3–3.3 | 8.8 / 3.9 | max 4.5–12.0, mean 0.2–1.3 |
| trough 30×30 | max 8.6–14.5, mean 1.7–3.8 | 251 / 60 (bones through the trough) | max 6.1–16.0, mean 1.3–1.7 |
| trough 60×60 dense | max 10.4–14.8, mean 3.1–4.8 | 15.8 / 3.6 | max 6.0–8.9, mean 1.3–4.9 |

All 30 MK2 runs pass every check of the program, including the 24-box fast family on each mesh.

- **Before stage 2:** fast boxes crossed the troughs (5, 9 and 15 of 24), and one 40 m/s
  spinning box fell through the flat mesh.
- **Per change**, as measured along the way:
  - The normal cone and the touching-pair depth search fixed the crossings.
  - Re-collision took the coarse-trough means from 7.7–9.2 mm to under 1.5 mm.
  - The unbiased joint relax and the 0.1 joint rate removed a 9–10 mm resting overlap on the
    flat floor.

## 5. Known gaps (next stage)

1. **M42 mesh-triangle touch events** (`collision_geometry`). Touch is observed per step, and
   short triangle contacts within a step are not reported. Per-substep re-collision now sees
   them, but does not publish them.
2. **Same-step arrival impulse** (`collision_continuity`). A newly arriving separated body gets
   its impulse within the step, but the test expects it at the exact arrival instant.
3. **Kinematic rotation contact** (`kinematic`). Contact generated by a full rotation between
   equivalent endpoint attitudes is not detected; it needs swept kinematic rotation.
4. **Ragdoll passive resistance** (`post_consumer_ragdoll`). Joint passive resistance scales with
   substeps.
5. **Balance lab** (`balance_lab` standing, recovery and cold continuation). The active-balance
   robot does not reach standing under MK2. Its controller was tuned against MK1's single-pass
   joint and contact response, and this needs its own investigation.
6. **MK1 event-bookkeeping tests** need MK2-appropriate restatements. These cover candidate
   reuse, impact batches and discovery, support-guess ownership and contact-lifecycle TOI
   observations.
7. **Probe-only crash, unexplained.** Probes linked by hand against the release test objects
   abort with a double free inside `PhysicsWorld::Step`, under MK1 as well. The same scenario
   passes in the real test binary, and an ASan+UBSan build at `-O3` reports nothing.

## 6. Stages

1. ~~Prototype substepped block solver, restitution, sleep~~ (done).
2. ~~Fast-body bounded CCD pass and negative-control family~~ (done).
3. ~~Joints inside the substep loop~~ (done): joints are solved first, and their relax is
   unbiased. Gaps 4 and 5 remain.
4. ~~Concave per-substep re-collision and internal-edge normals~~ (done; mesh CCD included).
5. Graph-coloured, deterministic parallel island solve, and narrowphase manifold reuse.
6. Restate the MK1 bookkeeping tests (gap 6).
7. Default switch only after human acceptance.
