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
3. Runs 2 biased iterations.
   - Rows are **rigid**: soft contacts were tried and sank about 1 cm per contact in a 20-stack.
   - Speculative rows (s > 0) target −s/h.
   - Penetration recovers at 0.02·(s + slop)/h, with a 0.5 mm slop, capped at 3 m/s.
4. Integrates poses through the motion ledger, one `RigidMotionSegment` per substep, so player
   sweeps and `GetBodyMotionSegments` see the travelled path.
5. Refreshes geometry from the new poses:
   - **Inertia:** world inverse inertia is recomputed from the new attitude.
   - **Lever arms:** they are rotated from the contact's precise local anchors to one common
     point: rA = dqA·rA0 − d/2 and rB = dqB·rB0 + d/2, where d is the anchor gap.
   - **Separation:** it is updated from displacements only, so it is safe in large worlds.
6. Runs 1 relax iteration with no bias.

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
2. The pass decides whether to clamp the pair:
   - A pair that is not touching at the start and has no row clamps on any hit.
   - A touching pair, or a pair with a row, clamps only if it **actually crossed**. Crossing
     means the end separation is below −depth, the contact point ended behind the first-touch
     plane, or the end pose overlaps by more than depth (depth = 0.25 × min feature).
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

**MK1 is unaffected.** The MK1 ordinary900 trajectory digests are identical to the previous
checkpoint on 900 of 900 steps.

| Workload | Metric | MK1 | MK2 |
|---|---|---|---|
| ordinary900 | mean / p95 / worst step | 1.73 / 5.77 / 14.9 | 1.97 / 9.61 / 16.7 |
| ordinary900 | sleeping at end | 1,152 | 1,152 |
| ordinary900 | deepest endpoint penetration | 3.1 mm | 0.8 mm |
| severe900 | mean step | 12.2 | 10.1–10.5 |
| severe900 | p95 step | 22.8 | 12.1–13.5 |
| severe900 | worst step | 40.8 | 16.7–37.4 (5 runs; 2 on the final code) |
| severe900 | steps over 16.67 ms | 130 | 0–11 |
| severe900 | impact events / TOI queries | 5,020 / 489k | 0 / 11.8k |
| severe900 | sleeping at end / deepest penetration | 140 / 5.0 mm | 134 / 6.8 mm |
| live120 (WASD) | worst visible frame | 212.8 ms (catch-up spiral) | 71 ms (disturbance onset) |
| live120 (WASD) | worst frame after the first 3 s | 212.8 ms | 23.4 ms |
| live120 (WASD) | worst step / steps over budget | 45.9 / 131 | 21.0 / 3 |
| live120 (WASD) | mean step | 10.4 | 11.1 |

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

### 4.3 Existing suites under MK2

MK1 passes all 56 physics programs. With `JUDAS_PHYSICS_MK2=1` the same 56 binaries give:

- **43 programs: the same outcome as MK1.** This includes:
  - wheels, joints and joint application;
  - terrain;
  - uniform and celestial gravity, and origin shifts;
  - kinematic lab;
  - curved-mesh support continuation;
  - rigid contact;
  - character, balance and powertrain labs;
  - ragdoll;
  - `save_streaming` and `physics_continuation`.

  The cold M70/M77 application runs were not repeated under MK2.
- **13 programs: a different outcome.** The FAIL counts below are lines in each log.

| Program | MK2 result | Classification |
|---|---|---|
| candidate_reuse | 1 FAIL | MK1 bookkeeping (TOI candidate reuse) |
| impact_batch_continuation | 4 FAIL | MK1 bookkeeping (event batches) |
| impact_discovery | 2 FAIL | MK1 bookkeeping (event discovery) |
| support_guess_ownership | 6+ FAIL | MK1 bookkeeping (support-guess owner) |
| contact_lifecycle | 3 FAIL | MK1 bookkeeping (TOI-solve observations) |
| impact_timing | 6+ FAIL | MK1 representation (2e-6 analytical endpoints; MK2 bound is one substep) |
| rigid_motion | aborts at check 10235 | MK1 bookkeeping: `impactEvents > 0` for a contact that MK2 solves without an event |
| kinematic | 3 FAIL | Partly bookkeeping (TOI evidence, owner event); **real gap 4** |
| impact_mesh_cost | 5 FAIL | **Real gap 1**: fast boxes cross dense and coarse meshes |
| impact_robustness | 2 FAIL | **Real gap 1**: box spinning at 45° falling onto a mesh |
| collision_geometry | 1 FAIL | **Real gap 2**: M42 mesh-triangle touch |
| collision_continuity | 6 FAIL | **Real gap 3**: arrival impulse timing |
| post_consumer_ragdoll | 1 FAIL | **Real gap 5**: passive joint resistance |

**MK2 therefore does not yet meet "no tunnelling" for fast bodies against triangle meshes.**
Primitive-versus-primitive CCD (test E) passes.

## 5. Known gaps (next stage)

1. **Thin fast boxes on meshes and slabs, and internal-edge hopping.**
   - Sliding on a 45° mesh hops at internal edges (`impact_robustness`, `impact_mesh_cost`).
   - Needs per-substep re-collision for concave pairs and internal-edge normal correction.
2. **M42 mesh-triangle touch events** (`collision_geometry`). Touch is observed per step, and
   short triangle contacts within a step are not reported.
3. **Same-step arrival impulse** (`collision_continuity`). A newly arriving separated body gets
   its impulse within the step, but the test expects it at the exact arrival instant.
4. **Kinematic rotation contact** (`kinematic`). Contact generated by a full rotation between
   equivalent endpoint attitudes is not detected; it needs swept kinematic rotation.
5. **Ragdoll passive resistance** (`post_consumer_ragdoll`). Joint passive resistance scales with
   substeps, so it needs the joint softness and relax review.
6. **MK1 event-bookkeeping tests** need MK2-appropriate restatements. These cover candidate
   reuse, impact batches and discovery, support-guess ownership and contact-lifecycle TOI
   observations.

## 6. Stages

1. ~~Prototype substepped block solver, restitution, sleep~~ (done).
2. ~~Fast-body bounded CCD pass and negative-control family~~ (done for primitives; meshes are
   gap 1).
3. ~~Joints inside the substep loop~~ (done). A softness and relax review remains (gap 5).
4. Concave per-substep re-collision and internal-edge normals (gaps 1-2).
5. Graph-coloured, deterministic parallel island solve, and narrowphase manifold reuse.
6. Restate the MK1 bookkeeping tests (gap 6).
7. Default switch only after human acceptance.
