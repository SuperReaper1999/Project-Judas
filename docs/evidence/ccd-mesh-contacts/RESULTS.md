# Ragdoll in a curved triangle mesh: continuous-impact cost

Consumer report (Skate & Parkour, a game built on Judas): after a bail into the arena's art
half-pipe, single frames took 0.2–14 s. In the M56 profile, almost all of that time was in
`Physics continuous impacts`. This note records how the cause was confirmed and what the
accompanying change does. It also gives measured options for further improvement. All numbers
are from Linux, a Release build on the developer machine, and 60 Hz fixed steps.

## Reproduction without game content

`judas_impact_mesh_cost_tests [steps] [scenario-filter]` drops a 12-body jointed box ragdoll
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
  after every step.

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

## Proposals for further improvement

None of these is in this change. The first two change results, so they are the engine owner's
call. The numbers below come from local, environment-gated experiments applied on top of this
change, measured with the same repro.

**A. Skip the search for pairs that cannot tunnel this step** (behaviour change). In
`ImpactTime`, skip the search when the pair's swept distance is below a quarter of the smaller
primitive feature:
`(|v_rel| + Σ|ω|·R) · Δt < 0.25 · min(feature)`, using the same feature size as the sampling
fallback. Such a pair cannot pass through the surface within the step. The step's speculative
contacts already cover the same closing distance: the narrowphase speculative margin is
`(|v_rel| + Σ|ω|·R) · Δt`.

| Scenario | Total ms | Worst step ms | Searches | Mean / max depth (mm) |
|---|---|---|---|---|
| trough 60×60 | 5,571 → 2,287 | 237 → 161 | 2,606 → 1,155 | 3.1 / 14.0 → 4.7 / 16.6 |
| flat sheet 60×60 | 2,970 → 609 | 62 → 21 | 10,209 → 1,107 | 1.5 / 24.6 → 2.4 / 24.6 |
| box floor | 370 → 87 | — | — | 2.6 → 4.7 mean |

Cost: about 1–2 mm more average sinking while resting, because the search also limits resting
penetration, not only tunnelling. A stricter factor (0.1) was worse on both time and depth.

**B. Fall back to sampling earlier when the search stops progressing** (behaviour change).
Lowering the 64-try cap to 8 in the dense trough:

| Measure | Cap 64 | Cap 8 |
|---|---|---|
| Total | 5,240 ms | 1,541 ms |
| Worst step | 237 ms | 66 ms |
| Mean / max depth | 3.1 / 14.0 mm | 4.6 / 12.2 mm |

In the coarse trough, cap 8 gives 2,927 → 1,651 ms, and mean depth 3.2 → 1.6 mm. A cleaner form
would trigger on lack of progress, for example several consecutive increments below 1% of the
remaining interval, rather than on a fixed count. That keeps flat and convex cases on the
proven path.

**C. Further same-result savings** (estimates from stack samples; not measured):
- Reuse one candidate list across all tries of a single `ImpactTime` call. The BVH query box
  differs per try, so the list must be refiltered per leaf to stay identical; BVH traversal is
  about 5% of the remaining samples.
- The advancement loop only uses the nearest contact of its `PairAt` manifold, but `reduce()`
  still builds and sorts the whole manifold.

**Consumer guidance.** For geometry that ragdolls can come to rest in, prefer open, coarse
collision surfaces. With this change the cost per try mostly follows the triangles near a limb,
and the number of tries follows the curvature.

**Observation, not investigated.** On a plain box-primitive floor, the ragdoll's first landing
reaches 42 mm of penetration at step 18 with the full continuous-impact path. The jointed limbs
appear to be driven into the support by the joint solve.
