# Ragdoll in a curved triangle mesh: continuous-impact cost

Consumer report (Skate & Parkour, a game built on Judas): after a bail into the arena's art
half-pipe, single frames took 0.2–14 s. In the M56 profile, almost all of that time was in
`Physics continuous impacts`. This note records how the cause was confirmed and what the
accompanying change does. It also gives measured options for further improvement. All numbers
are from Linux, a Release build on the developer machine, and 60 Hz fixed steps.

## Reproduction without game content

`judas_impact_mesh_cost_tests [steps] [scenario-filter] [throw-speed]` drops a 12-body jointed box ragdoll
into an open, procedurally cooked surface:
- The ragdoll is rider-sized. The head is a sphere, and pair collisions are disabled like
  `WorldRagdoll` without self-collision.
- It is thrown at 1.2 m/s with a little spin, then simulated for 300 steps.
- Surfaces: a flat sheet, or a curved trough (radius 1.6 m, 3 m × 3 m, like a skate
  half-pipe), at several densities, plus a box-primitive floor for reference.

For each scenario it prints:
- step times;
- impact and geometry counters;
- the analytic penetration;
- a bit-exact trajectory digest: FNV-1a over every body's position, rotation and velocities
  after every step;
- from the second commit on, search stalls and a fast-box tunnelling count (see
  `FOUND_ISSUES.md`).

Timings are reported, never asserted. The checks are determinism from run to run and that
nothing falls through. Unmodified engine, `3bdf09e9`:

| Scenario | Triangles | Total ms | Worst step ms | Searches | Tries | 64-try limits | Convex tests | Digest |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| box primitive floor | 0 | 373 | 6.4 | 5,564 | 12,819 | 8 | 0 | `8b4d456809cb0517` |
| flat sheet 6×6 | 72 | 919 | 15.7 | 7,236 | 18,639 | 7 | 308,032 | `c5d8d90b43b94462` |
| flat sheet 60×60 | 7,200 | 11,950 | 162.6 | 10,209 | 28,471 | 15 | 5,735,471 | `7883ad10d5732411` |
| trough 2×26 (coarse) | 104 | 13,936 | 546.7 | 6,045 | 110,017 | 864 | 6,779,528 | `fc84cd37e28527e6` |
| trough 30×30 | 1,800 | 16,486 | 914.6 | 4,291 | 89,255 | 850 | 8,169,973 | `7c3530fd2305805c` |
| trough 60×60 (dense) | 7,200 | 27,644 | 1,392.1 | 2,606 | 60,996 | 556 | 13,847,148 | `6a99fee5d082cb9a` |

## Confirmed cause

There are two multiplying factors, both measured with temporary instrumentation that is not
part of the change.

**1. The search crawls on a curved surface.** A limb sliding along a concave surface stays
about 0.2–0.3 mm from it and is always closing on the next triangle, because the surface curves
up ahead. Each conservative-advancement try moves only `gap / closing ≈ 0.25 mm / 1.8 m/s`, about
0.14 ms of the 16.7 ms step, so the pair runs into the 64-try cap. It then pays the sampling
fallback: at least 16 samples, then a 32-step bisection.

| Dense mesh | Searches hitting the 64-try cap | Median tries, costliest 10% | Median start gap |
|---|---:|---:|---:|
| Flat sheet 60×60 | 15 of 10,209 (0.15%) | 6 | 0.14 mm |
| Trough 60×60 | 556 of 2,606 (21%) | 64 | 0.29 mm |

About 60% of the closing speed bound comes from rotation (`|ω|·boundingRadius`).

**2. Every try is a full mesh query.** Each try is `PairAt`, a margin manifold, or
`PairTouches`. Both test the limb box against every BVH candidate triangle with a 43-axis SAT
and allocate a `Poly` for each triangle. Most of those tests are rejections that a face axis
alone proves: 91% in the dense trough and 88% on the dense flat sheet.

Stack samples (gdb, 39 samples) taken during the in-game stall agree: about 80% of the time is
in `sat(Poly,Poly)`, reached from `ImpactTime → PairAt → ComputeContacts → CookedContacts` and
from `ImpactTime → PairTouches → CookedHasContacts`.

## Change: same results, fewer operations (`src/CollisionGeometry.cpp`)

- In `visitCookedPolyContacts`, each candidate triangle's face axes are projected first, using
  sat()'s exact projection (`faceAxesSeparate`).
  - When one of them separates the pair by more than the margin, sat() would also return
    `gap > margin`: face axes are always allowed axes, so `best.gap` is at least that value. The
    triangle is therefore rejected exactly as before, without evaluating the 36 edge-pair axes.
  - The skipped triangle still counts one `convexTests`, so the geometry counters do not change.
  - Every other triangle goes through the unchanged sat(), witness and margin path.
- Candidate triangles are written into one reused `Poly` (`triangleInto`) instead of a new
  allocation per triangle. The arithmetic is the same as `triangle()`, which now uses it.
- `test` takes the `Poly` by const reference.

Contacts, their order, sidedness, crease handling and the counters are unchanged. The distance
query (`PolyGeometry`, which needs the exact minimum gap) and the box/convex paths are untouched.

## Verification

**Repro, before and after.** Digests and every counter are identical in all six scenarios:

| Scenario | Total ms before → after | Worst step ms before → after |
|---|---:|---:|
| box primitive floor | 373 → 370 | 6.4 → 6.3 |
| flat sheet 6×6 | 919 → 558 (1.6×) | 15.7 → 8.4 |
| flat sheet 60×60 | 11,950 → 2,970 (4.0×) | 162.6 → 62.4 |
| trough 2×26 (coarse) | 13,936 → 3,095 (4.5×) | 546.7 → 112.5 |
| trough 30×30 | 16,486 → 3,383 (4.9×) | 914.6 → 228.3 |
| trough 60×60 (dense) | 27,644 → 5,571 (5.0×) | 1,392.1 → 236.8 |

**Existing suites, before and after.** I built and ran each suite with the original and the
changed `CollisionGeometry.cpp`, then compared the complete output. Apart from timing values
and temporary paths, every output is identical, and every exit code is the same.
- 42 suites with no arguments: `judas_*_tests` for physics, impact solver and timing,
  collision, collision geometry, contact (cache, geometry, lifecycle, storage), rigid contact,
  broadphase, joint, ragdoll, post-consumer ragdoll, physical animation, physical balance
  (controller and foundation), kinematic and kinematic lab, character motor and its
  application, gravity selection, uniform-gravity scene, celestial gravity, terrain (physics,
  pipeline, authoring, fluid), fracture, deformable (and their performance suites), particle
  contact batch, fluid boundary contact, fluid cavity impact, physics cast and its application,
  physics continuation, physics batch script, and joint authoring and application.
- With arguments: balance lab (standing, supports, recovery, radial), character lab, character
  publication, motor runtime tags, world building, collision application (`collision_lab`,
  probe), fracture application (`fracture_lab`: probe, mechanics, support, radial), and
  deformable application (`deformable_lab`, probe).
- Not covered: `judas_terrain_application_tests` exits with a usage error in both builds.
- `judas_physics_batch_script_tests` writes a fixed fixture folder (`build/m69-query-fixture`),
  and a second run in the same tree fails at "normal registered script asset" whatever the
  engine. It passes, with the same output, when it is given a fresh folder.

**Game, real time.** The Skate & Parkour half-pipe bail, with the game's bail watchdog off,
14 s of game time, three runs per build. Real-time runs differ from run to run, because the
number of steps per frame follows wall time.

| Collision under the ragdoll | Worst frame before | Worst frame after | Wall time before | Wall time after |
|---|---|---|---|---|
| Art half-pipe, 20k-triangle closed remesh | 873 / 481 / 1,285 ms | 222 / 355 / 383 ms | 24.7 / 16.8 / 23.8 s | 14.3 / 14.6 / 14.5 s |
| Open profile ramp, about 100 triangles | 878 / 1,000 / 1,915 ms | 932 / 232 / 110 ms | 15.8 / 15.9 / 18.2 s | 15.4 / 14.4 / 14.2 s |

The game's deterministic merge-line acceptance run produces identical script output with both
builds.

## Second commit: finish stalled searches by sampling (behaviour change)

This is commit `b4c888a2`, kept separate so it can be taken or dropped on its own.

**What changed.** `ImpactTime` leaves the conservative-advancement loop after **8 consecutive
advances that each cover less than 5% of the interval still to search**. It then finishes with
the existing bounded sampling, exactly as it already does when it reaches the 64-iteration
limit. Searches that progress normally are untouched. Stalls are counted separately, in
`StepStats::impactSearchStalls` and the profile counter `Physics impact search stalls`.

**How the thresholds were chosen.** First a single-throw sweep of 2, 4 and 8 consecutive
advances against 0.5, 1, 2 and 5%. Then the five most promising settings were rerun with three
throw speeds each (1.0, 1.2 and 1.4 m/s), because the trajectories are chaotic and single runs
are not comparable. In the table, totals are the mean of the three throws and worst steps are
the maximum of the three.

| Setting | Dense trough total / worst | Medium trough total / worst | Coarse trough total / worst | Dense flat total / worst |
|---|---|---|---|---|
| off (first commit only) | 6.62 s / 447 ms | 3.16 s / 231 ms | 2.30 s / 115 ms | 2.47 s / 61 ms |
| 4 advances under 2% | 2.54 s / 94 ms | 0.89 s / 57 ms | 1.08 s / 32 ms | **2.80 s** / 59 ms |
| 6 advances under 2% | 2.53 s / 131 ms | 1.34 s / 61 ms | 1.09 s / 38 ms | **2.78 s** / 60 ms |
| 8 advances under 1% | 3.20 s / 175 ms | 1.81 s / 78 ms | 1.31 s / 44 ms | 1.91 s / 57 ms |
| **8 advances under 5%** | **2.25 s / 145 ms** | **1.24 s / 65 ms** | **1.28 s / 42 ms** | **1.83 s** / 73 ms |
| 12 advances under 2% | 2.82 s / 149 ms | 1.57 s / 82 ms | 1.36 s / 52 ms | **2.82 s** / 65 ms |

Runs of 4 or 6 slow advances are fastest in the troughs, but they also cut off ordinary short
stalls on the dense flat sheet, which made it slower than off (bold). 8 advances under 5% is
never slower than off.

**What it does to the result** (same three throws per setting):
- **Resting depth:** the mean depth stays within the spread between throws. For example:
  - dense trough: 3.1 mm (2.8–3.5) off, against 3.8 mm (2.2–4.6) with the chosen setting;
  - box floor: 2.3 (2.1–2.5) against 2.4 (2.3–2.4);
  - dense flat: 2.2 (1.5–3.5) against 2.9 (2.5–3.2).
- **Maximum depth:** identical in every scenario, for every setting except 6 advances.
- **Tunnelling:** the fast-box tunnelling counts are identical for every setting, including off.

**Existing suites.** The same 57 runs as the first commit, comparing the first commit alone
against both commits:
- Every run passes in both builds, with the same exit codes.
- 47 outputs are identical apart from timings.
- The other 10 show small numeric drift, as expected, but every check passes:
  - balance standing pose, at about 0.2 mm;
  - fracture bond forces: the **same bonds break**, 1, 3, 4, 7 and 10;
  - rigid-contact energy after settling: 268.7 → 272.3 J of 11,776 J;
  - ragdoll application frame count: 34 → 33;
  - broadphase touching pair-steps: 15,605 → 15,661, with 0 missed and 0 spurious;
  - balance-controller residuals and terrain-fluid contact counts.
- In the impact-timing suite, the engine's own `grazing_regression` case now ends after 8
  advances instead of at the 64-iteration limit. It still passes, with a momentum residual of
  8.4e-7 → 1.4e-6.

**Game, real time.** The same half-pipe bail as before, watchdog off, 14 s of game time, three
runs each. "Worst physics frame" is the worst frame that contains continuous-impact work.

| Collision under the ragdoll | First commit only | Both commits |
|---|---|---|
| Art half-pipe, 20k triangles | 137 / 308 / 291 ms | 94 / 68 / 103 ms |
| Open profile ramp | 285 / 568 / 797 ms | 94 ms, then 2 runs where no bail frame was among the 8 slowest (all were level-loading frames) |

With both commits, every run's wall time was 14.2 s for 14 s of game time. The game's
deterministic merge-line acceptance run passes with the same score.

## Further proposals (not implemented)

**A. Skip the search for pairs that cannot tunnel this step** (behaviour change). Measured on
top of the first commit only. In `ImpactTime`, skip the search when the pair's swept distance is
below a quarter of the smaller primitive feature:
`(|v_rel| + Σ|ω|·R) · Δt < 0.25 · min(feature)`, using the same feature size as the sampling
fallback. The step's speculative contacts already cover that closing distance: the narrowphase
speculative margin is `(|v_rel| + Σ|ω|·R) · Δt`.

| Scenario | Total ms | Worst step ms | Searches | Mean / max depth (mm) |
|---|---|---|---|---|
| trough 60×60 | 5,571 → 2,287 | 237 → 161 | 2,606 → 1,155 | 3.1 / 14.0 → 4.7 / 16.6 |
| flat sheet 60×60 | 2,970 → 609 | 62 → 21 | 10,209 → 1,107 | 1.5 / 24.6 → 2.4 / 24.6 |
| box floor | 370 → 87 | — | — | 2.6 → 4.7 mean |

This was measured with single throws, before the three-throw method above. On the box floor its
depth increase (2.6 → 4.7 mm mean) is well outside the 2.1–2.5 mm spread between throws there,
so it affects ordinary resting bodies, not only the stalled cases. The second commit is preferred
for that reason.

**C. Further same-result savings** (estimates from stack samples; not measured):
- Reuse one candidate list across all tries of a single `ImpactTime` call. The BVH query box
  differs per try, so the list must be refiltered per leaf to stay identical; BVH traversal is
  about 5% of the remaining samples.
- The advancement loop only uses the nearest contact of its `PairAt` manifold, but `reduce()`
  still builds and sorts the whole manifold.

**Consumer guidance.** For geometry that ragdolls can come to rest in, prefer open, coarse
collision surfaces. With this change the cost per try mostly follows the triangles near a limb,
and the number of tries follows the curvature.

**Observations, not investigated.** On a plain box-primitive floor, the ragdoll's first landing
reaches 42 mm of penetration at step 18 with the full continuous-impact path. The jointed limbs
appear to be driven into the support by the joint solve.

Two more serious pre-existing issues are written up in `FOUND_ISSUES.md`. Fast thin bodies
cross sloped triangle meshes, and impact spin can make `Step` throw `angular midpoint did not
converge`.
