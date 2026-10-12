# Fast, small, spinning bodies: two fixes

These are the two pre-existing issues reported in `docs/evidence/ccd-mesh-contacts/FOUND_ISSUES.md`:

1. `PhysicsWorld::Step` throws `angular midpoint did not converge`.
2. Fast thin bodies cross sloped triangle meshes, and sink deep into box slabs.

Issue 2 is fixed for planar slopes, meaning tilted sheets and slabs. It is **not** fixed for
the curved troughs in `judas_impact_mesh_cost_tests`; that case has separate causes, described
at the end. All numbers come from Linux, a Release build, 60 Hz fixed steps, and base `2272f288`
(main with PR 13).

## What changed

### `AngularDriftGenerator` (`src/RigidBody.cpp`): the throw

- **Cause.** The implicit-midpoint Newton solve starts cold from `m0`. When `|ω|·Δt` is several
  radians, as for a 0.2 kg box spun to 250–1,000 rad/s by impacts, the 12 iterations can land
  outside Newton's basin. The solve then throws out of `Step`.
- **Common path.** Unchanged: same iterations, same tolerance, same result, bit for bit.
- **New fallback.** It runs only where the solve used to throw. It solves the same midpoint
  equation by *continuation* in the interval:
  - It splits the half-interval into 8, then 64, 512 and 4,096 stages.
  - Each stage starts from the converged root of the slightly shorter interval before it, so it
    follows the root that grows continuously from `m0` at `h=0`.
  - The answer meets the same residual tolerance, so the stated invariants still hold: momentum
    norm and kinetic energy are conserved to solve/rounding error. Only if every stage count
    fails does it still throw.
- **Evidence.** 9 inputs were captured from real throws, as hexfloats in
  `tests/ImpactRobustnessTests.cpp`. All 9 now solve, and keep the momentum norm and energy within
  1e-5 relative.

### Spinning support pairs (`src/PhysicsWorld.cpp`): crossing planar slopes

- **Cause.** A pair touching at the start of the interval is a *support pair* and leaves the
  continuous search to the contact solver. That is right for a body resting or sliding. It is
  wrong for a light box spun up by an off-centre impact: it can turn another corner through the
  support within the same step, and once its centre is behind a one-sided mesh triangle the mesh
  never catches it again.
- **What a pair now does instead.** A touching pair whose rotation can sweep a point further than
  half the thinner primitive's thickness within the remaining interval now stays under
  continuous search:
  - The sweep is `Σ|ω|·boundingRadius·remaining` over non-sphere moving bodies.
  - The pair goes into `spinningPairs` instead of `supportPairs`.
  - It is searched with a penetration *target*: the first time its deepest penetration exceeds
    its start depth by a quarter of that thickness.
  - The search goes straight to the bounded temporal sampling. Conservative advancement needs a
    separating plane, which a touching pair does not have.
  - Spinning pairs still count as supports for the restitution choice.
  - After an inelastic impact event, each event pair is filed as spinning or support by the same
    test.
- **`PrimitiveFeature`.** This is the existing thinnest-half-dimension helper, moved out of the
  sampling lambda so the new test can use it too.

### Deep-overlap normal against a triangle (`src/CollisionGeometry.cpp`)

- **Cause.** Deep inside a thin body, `sat()`'s least-overlap axis against a mesh triangle can be
  one of the *body's* own faces pointing away from the triangle's front. The contact then pushes
  the body further through the sheet.
- **Fix.** When the SAT normal opposes the triangle's front (dot < −1e-3), the triangle's face
  axis is used, with its own overlap. Lateral edge axes are perpendicular to the face normal, so
  they are unaffected.

## New test: `judas_impact_robustness_tests`

The bodies are a 4 × 2 × 6 cm, 0.2 kg box, against either a one-sided 3 × 3 m mesh sheet (60 × 60
cells) or a 1 m thick box-primitive slab of the same size. The test covers:

- the 9 captured drift inputs above;
- **spin-up drops**: horizontal support, falling at 20 or 40 m/s, with 0 or 30 m/s sideways and
  0, 15 or 45 rad/s initial spin, 90 steps;
- **tilted supports**: 0°, 5°, 15°, 30° and 45°, falling at 10, 20 or 40 m/s, 60 steps;
- **gentle slides**: the box set down at rest on a 15°, 30° or 45° slope, 180 steps.

It checks three things in every case: `Step` does not throw; the centre never gets more than
5 mm beneath the support while over it; and no corner gets more than 3 cm beneath it.

| | Unmodified `2272f288` | This change |
|---|---:|---:|
| checks / failures | 219 / **81** | 237 / **0** |
| captured drift inputs that throw | 9 of 9 | 0 |
| drops/slides where `Step` throws | 10 | 0 |
| centre passes through | 31 | 0 |
| corner sinks more than 3 cm | 31 | 0 |
| deepest corner below any support | 3.88 m | 1.8 cm |

The unmodified run records fewer checks because a captured input that throws skips its
follow-up checks.

Selected cases. Each cell is the deepest centre / deepest corner beneath the surface, in cm;
"threw" means `Step` threw part-way through.

| Case | Unmodified | This change |
|---|---|---|
| mesh, 5° tilt, 20 m/s | threw, 384.8 / 387.8 | 0 / 0.3 |
| mesh, 15° tilt, 10 m/s | 154.6 / 157.5 | 0 / 1.4 |
| mesh, 30° tilt, 40 m/s | 60.8 / 63.9 | 0 / 0.8 |
| slab, 30° tilt, 40 m/s | 28.7 / 31.5 | 0 / 0.5 |
| mesh, flat, down 40, spin 15 | threw, 127.4 / 131.1 | 0 / 1.6 |
| mesh, **gentle slide** 45° | 85.2 / 88.2 | 0 / 1.4 |
| mesh, **gentle slide** 30° | 17.8 / 20.6 | 0 / 1.8 |

The gentle slides are worth noting. The unmodified engine lets a box that is merely *set down*
on a 30° or 45° mesh slide through it, with no fast impact involved at all. The deep-overlap
normal fix covers that case.

## Regression check: 60 runs, unmodified vs this change

The comparison covered:

- the 42 physics, collision, ragdoll, character, motor, fracture and deformable suites;
- the balance, character, motor-tag, world-building, collision, fracture and deformable
  application runs, each with a fresh fixture directory;
- `judas_impact_mesh_cost_tests` at throw speeds of 1.0, 1.2 and 1.4 m/s.

Outcomes:

- Every run passes with this change.
- The only failure on unmodified main is the new robustness test.
- Outputs were compared with timings masked. The differences are:

| Suite | Difference | Why |
|---|---|---|
| `judas_kinematic_tests` | full-turn bar: 4 impact events instead of 1, crate leaves at 385 m/s instead of 235 m/s | A kinematic bar sweeping a full turn in one step. The old engine stopped after the first touch, when the pair became a support. Now the bar keeps striking, which is the intended behaviour. |
| `judas_collision_geometry_tests` | one unassisted-curve energy value: 3.59 J → 4.11 J | Assertions pass; a chaotic trajectory. |
| `judas_rigid_contact_tests` | final energy of two 50-body piles shifted by about 1–4% | Chaotic. All energy-gain checks still pass, with the largest one-step gain at most 0.012 J. |
| `judas_broadphase_tests` | touching pair-steps 15,661 → 15,918 | 0 missed and 0 spurious in both. |

### A ragdoll regression found and fixed during verification

The first version used a lower threshold: a sweep of a *quarter* of the thickness. With it,
`judas_post_consumer_ragdoll_tests` failed "physical disturbance resettles":

- After the hips were shoved, one leg kept swinging with an 11 s period and never slept.
- This happened at 6 of 8 shove strengths, against 0 of 8 for the unmodified engine.
- The limbs involved only just crossed the threshold, with a sweep of 2.5–4.6 cm against a
  2 cm limit.

The threshold is now half the thickness. At that setting:

- the ragdoll suite passes at all 19 shove strengths tried, from 0.5× to 2× (the unmodified
  engine also passes all 19);
- the robustness test keeps all 237 checks;
- a full thickness would lose 2 of the robustness checks, and twice that would lose 8.

## Cost

### `judas_impact_mesh_cost_tests` (ragdoll thrown into a mesh, 300 steps × 3 speeds)

| Scenario | Total ms (3 speeds), main → fix | Worst step ms | Max penetration mm | Impact events | Fast boxes through (per speed) |
|---|---:|---:|---:|---:|---|
| box primitive floor | 1,174 → 1,344 | 7 → 8 | 42.0 → 14.9 | 2,575 → 2,736 | 0/0/0 → 0/0/0 |
| flat sheet 6×6 | 1,722 → 2,639 | 11 → 11 | 29.4 → 11.3 | 3,356 → 4,952 | 0/0/0 → 0/0/0 |
| flat sheet 60×60 | 5,277 → 10,290 | 53 → 61 | 45.1 → 15.6 | 2,433 → 5,378 | 3/3/3 → 0/0/0 |
| trough 2×26 coarse | 3,746 → 3,705 | 43 → 37 | 13.1 → 17.2 | 2,168 → 2,034 | 24/24/24 → 24/24/24 |
| trough 30×30 | 3,654 → 6,506 | 62 → 103 | 14.1 → 15.3 | 1,660 → 2,769 | 24/24/24 → 24/24/24 |
| trough 60×60 dense | 6,583 → 5,220 | 141 → 134 | 15.8 → 12.6 | 1,543 → 986 | 24/24/24 → 22/22/22 |
| **all** | **22,155 → 29,703** | | | | |

The ragdoll now sinks about a third as deep into the floor and flat sheets: 42 → 15 mm and
29 → 11 mm. It pays for that with more impact events while it tumbles, which shows most on the
dense flat sheet, at about 2× the total. The trough results are chaotic and go both ways. The
worst single step is about the same overall.

### Steady state and settling

In `judas_post_consumer_ragdoll_tests`, both binaries were built from the same tree and run
alternately three times:

- **Per-step cost while resettling:** within 2%. For 20 rigs, 1.39 ms on the unmodified engine
  and 1.41 ms with this change.
- **Settle time after activation:** 10 and 20 rigs fall asleep after 7 s instead of 11 s. As a
  result, the whole suite runs in 24.5 s instead of 33.3 s.
- **`judas_rigid_contact_tests`:** 2.03 s instead of 2.74 s. Almost all of the difference is in
  one chaotic 50-body pile scene.

### Game: Skate & Parkour half-pipe bail (real time, offscreen, `JUDAS_PROFILE`)

The game quits 2 s after a forced bail into the art half-pipe, so the profiler's 120-frame
history covers the ragdoll. The game ran 2 times on each engine:

| | Unmodified | This change |
|---|---|---|
| ragdoll-awake frames in the history | 39, 37 | 28, 42 |
| rigid physics per ragdoll frame, median | 9.8, 3.0 ms | 4.8, 3.9 ms |
| worst rigid physics frame | 93.6, 167.0 ms | 14.4, 82.4 ms |

## Not fixed: fast boxes crossing the curved troughs

The `fastBoxesThrough` count for the troughs is still 22–24 of 24. Instrumenting
`ImpactTime` for these boxes showed two causes. Neither is related to spin-up or to the changes
above.

1. **The conservative-advancement step bound assumes a convex pair.**
   - The bound is `gap/closing`, with the closing speed taken along the nearest contact's normal.
     On a concave mesh, a steeper triangle ahead of a box moving across the slope closes faster
     than that.
   - So the advance overshoots into a pose where the box centre is already behind that triangle.
     One-sided culling then drops the triangle, and the query reports no contact
     (`count 0`, `return finish+1`).
   - The box passes in its first step with its velocity unchanged: (0, −40.3, 12…30) m/s.
   - Taking the minimum over all manifold points did not help, because `reduce()` keeps only 4
     points. Bounding mesh pairs by `gap/speed` (`speed` is the full relative speed plus `|ω|R`)
     catches every first-step hit, but it costs 1.5–2.5× on the troughs.
2. **Centroid-based one-sided culling loses a thin box sliding fast with spin.**
   - With the first-step hit caught, a box lands and slides down the curve at about 13 m/s,
     spinning at 90–140 rad/s.
   - Within a step or two its centroid dips behind the faceted surface. From then on the culling
     rule (`side < −eps → continue`) ignores those triangles, and the box drops through by
     step 4.
   - Fixing this means deciding which side of a one-sided mesh a partially overlapping body
     belongs to, for example from its pose at the start of the step instead of its current
     centroid. That is a design decision for the engine owner.

The flat-sheet case of the same counter (3 per speed on the 60×60 sheet) is fixed by this
change.

## Note: penetration slop against thin bodies

`kPenetrationSlop` is 5 mm, so a 2 cm thick box can rest a quarter of its thickness inside a
support. That is harmless here, since the robustness test allows a 3 cm corner depth. Thin
bodies are, however, where a fixed slop matters most.
