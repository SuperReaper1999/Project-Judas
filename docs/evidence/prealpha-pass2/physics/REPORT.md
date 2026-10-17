# Pre-Alpha Pass 2: collision arrival, exact witnesses and impact scratch

## Scope and result

The baseline is Experimental `528cfb1b430d710ba5a4d2d13569aad106e3044e`.
This pass ships a **curved-mesh initial-arrival correction**, allocation reuse
with exact witness equivalence, and bounded impact-discovery scratch. It does
**not** close curved-mesh glancing/support continuation. The unchanged full test
still fails and remains a release limitation. No physics cadence, material,
solver budget, fixture, acceptance threshold or fluid code was changed.

Physics changes are in:

- `src/PhysicsWorld.cpp`: static-mesh advancement and event scratch reuse.
- `src/CollisionGeometry.cpp/.h`: finite-patch advancement and reusable witness traversal.
- `src/ImpactPairVisitSet.h`: exact, bounded pair membership for impact discovery.
- `src/ReentrantQueryScratch.h`: stable nested query leases and deep-recursion spill.
- `tests/CollisionWitnessTests.cpp`: positive/negative witness, pair-membership and unwind checks.

The parent integration change in `src/RuntimeWorld.cpp` also removes redundant
per-body destruction during final whole-world teardown. Ordinary entity deletion
retains its wake-up behaviour. It is measured separately below.

## Established geometry mechanism and correction

A local proximity/response plane of a concave mesh is not a separating plane for
the union of its finite patches. The old advance could skip first contact while
its chosen local normal changed toward a tangential axis. Pass 1's independent
oracle already distinguishes actual tessellated-sheet crossing from legal upper
flight; that oracle and its original 5 cm threshold are unchanged here.

The new `MeshSeparationAdvance` performs one swept BVH candidate traversal and
intersects each finite triangle's SAT projection intervals. For pure translation,
axis interval entry **and exit** times are intersected, retaining finite-edge
misses. The minimum possible arrival over all candidate triangles bounds arrival
at the union. It does not launch a separate iterative TOI solve for each triangle.
For rotation, a fixed unit axis `n` has projected closing speed bounded by
`|n × omega_g| R`, plus projected translation. A patch's maximum fixed-axis bound
is conservative; the union uses the minimum over patches.

Here `omega_g` is the **stored geometric drift generator**, not the transported
physical endpoint angular velocity. `RigidMotionSegment::EvaluateGeometry`
uses `q(t) = normalize(q0 + (omega_g*q0)t/2)`, with fixed world axis and instantaneous
rate `omega_g / (1 + |omega_g|² t²/4)`. Prescribed motion uses a constant-axis
exponential. `DriftGenerator` returns this same stored vector. This holds for the
existing anisotropic-body drift too; physical angular momentum transport does
not replace the geometric ledger. Existing kinematic interval splitting remains.

Only box/convex-hull versus **stationary triangle mesh** uses this path. Existing
actual accepted-contact truth, response, 64 advancement iterations, stall policy,
32 bisection updates and bounded sampling fallback remain authoritative. No new
claim of exact/universal continuous CCD is made.

### Unchanged tests and controls

| Contract | Baseline crossings coarse/medium/dense | Candidate | Result |
|---|---:|---:|---|
| Original two-step arrival | 20 / 16 / 16 | 0 / 0 / 0 | **193 checks, zero failures** |
| Original full 90-step continuation | 24 / 24 / 24 | 11 / 20 / 15 | **193 checks, four failures** |

Controls exercise the missing collider (all 24 boxes must cross), upper flight,
actual lower-sheet penetration, finite edges, one-sided rear entry and arbitrary
rotation. The controls would fail if collision were merely removed, an infinite
plane substituted, or rear entry incorrectly blocked.

### Remaining continuation mechanism

The rejected patch-support experiments narrowed but did not remove crossings.
Some bodies finish an earlier step with their centre already behind a one-sided
sheet by less than the unchanged 5 cm reporting gate. At the next step the old
centre-sided contact policy can exclude them and their small body bound no
longer overlaps the thin sheet: preserved traces show **zero CCD queries** at
that later crossing. Reliable repair requires a justified contact/feature episode
across fixed boundaries and compatible broadphase/publication rules. A local TOI
change alone is insufficient. No missing historical state was invented.

## Independent exact witness and scratch work

The cooked evaluator reuses polyhedron, triangle and clipping buffers. Boolean
queries visit the **same accepted witnesses**, stopping only after a witness is
accepted. They do not replace contact truth with SAT overlap. Full response
retains witness order, clipping, margins, finite edges, sidedness and crease rules.

The matched 12,000-case fixture tests 36,000 margin queries in both orders, hulls,
one-/two-sided finite triangles, arbitrary poses, offsets, front/back, deep and
edge cases. It includes 9,574 positives and 26,426 negatives. Baseline and candidate
produce the identical complete witness digest `bcda068c12557e44`; each Boolean
result and depth threshold is also compared directly against its full manifold.

Per-thread scratch frames have stable addresses, retained to depth 32. Deeper
queries use lease-owned spill frames, with no new query-depth rejection.
65-deep nesting and injected failures at depths 0/31/32/33/65 prove caller storage
and exception unwind remain correct. Final-source narrow checks pass
**120,283 checks**, including exact compact/fallback pair-set comparisons.

Impact discovery retains its original encounter order and uncertain-miss policy.
A compact exact bitset replaces individual hash nodes for slot spaces up to 4096,
using at most 1 MiB. Larger/sparse spaces retain the hash fallback. Event vectors
reuse capacity only within the fixed-step event pass; no cross-time manifold or
physical state is cached. Quiet/free-flight worlds allocate none of this event
scratch. Pair-set tests compare against an independent ordered set, including
4096/4097 boundaries and clearing between events.

## Rejected experiments, preserved rather than shipped

Complete sources, build receipts and genuine failures are retained under
`experiments/` and `raw/`:

- Batch arrival alone repaired entry with practical cost and is retained.
- Per-patch support continuation reduced crossings to zero in some cases but
  produced actual `impact bracket lost contact geometry` exceptions.
- Coplanar support exclusions and per-step front references reduced exceptions,
  but the strongest variant still had 2/10/7 crossings and severe mesh cost.
  These continuation/selection changes are **absent from production**.
- A full exact-pose geometry memo preserved all 270 dense-room trajectories, but
  produced only 2352 hits against 170326 stores, retained 3.4 MB, and did not improve
  the worst step (2123→2140 ms). It was rejected and removed.
- A first memo compile used a nonexistent `RigidBody::localCenter`; the genuine
  build log is preserved. The supported simple bodies have zero pivot offset.

## Matched measurements

Linux Intel Core i7-4790, 8 logical CPUs, GCC Release `-O3 -DNDEBUG`, 60 Hz.
`HARDWARE.txt`, build commands, timestamps, source/binary hashes and raw CSV/JSON
are retained. CPU-intensive builds and Windows compilation were not run during
these timing windows. The Windows VM was paused at 23:20:19 UTC, before the
matched dense run began at 23:21:11 UTC (coordinator platform receipt). These are host CPU physics
observations, not GPU or Windows performance claims.

### Curved articulation (three alternating-order trials)

Same 13-body/12-joint ragdoll, original mesh, 60 fixed steps. Values are the median
of each trial's reported result. These are actual changed collision trajectories,
not a claim of bit-identical mesh simulation. Maximum measured penetration stays
unchanged in these prefixes; every run remains deterministic internally.

| Mesh triangles | Baseline total / worst ms | Candidate total / worst ms | Max penetration baseline / candidate |
|---:|---:|---:|---:|
| 104 | 292.99 / 17.13 | 224.13 / 12.46 | 9.92 /9.92 mm |
| 1800 | 397.56 / 26.84 | 300.94 / 20.76 | 10.80 /10.80 mm |
| 7200 | 844.78 / 45.48 | 604.29 / 29.65 | 21.48 /21.48 mm |

Pure matched witness-fixture median: **290.189→209.990 ms**, identical witness
digest. This measures this query mix, not whole-game FPS.

### Exact M91 dense-room workload

The unchanged `projects/physics_stress_lab/Scenes/room1024.judas` has SHA-256
`a2caf62a295243d301149202c8a0a5eedafff256d68de0e3cda988846b2e05bf`.
Same 270 requested steps, synthetic 60 Hz cadence, hidden renderer, profiler on,
tour off, endpoint inspection outside timing. All 270 physical digests and listed
physical counters match exactly; final digest is `192f0f4bed728953`. Both endpoints
have deepest observed penetration **7.2171085 mm**.

| Fixed-step measurement | Baseline | Candidate |
|---|---:|---:|
| Median |27.070 ms |25.937 ms |
| p95 (floor order statistic) |413.328 ms |416.695 ms |
| Maximum |2123.409 ms |1970.799 ms |

Worst-step bounded profiler counters show island discovery **723.626→630.182 ms**,
geometry **932.147→899.518 ms**, TOI **331.157→319.315 ms**. Categories are inclusive
and overlap; **do not sum them**. The expensive case still has 51 impact events,
501599 TOI queries, and a 973-body/2517-row island. This is still far outside a
real-time frame budget. Mixed p95/worst observations do not establish a universal
dense-contact speedup or a predictable upper bound; the extreme release limit
remains open.

The baseline and rejected-memo processes completed 270 steps and valid exports,
then hit their 65 s watchdog during teardown. The candidate with the parent's
whole-world teardown correction **exited successfully in 54.731 s**, including
all 270 steps and cleanup. Source tracing establishes the separate cause:
individual final `DestroyBody` calls repeatedly ran a wake BFS through the dense
contact graph immediately before deleting the entire physics world.

### General regression workload controls

Matched free-flight 1024 and pile/active-/sleeping-assembly 256 workloads all pass
physical acceptance on both archives. Every trajectory digest matches. Median
step costs are approximately 0.67–0.70 /0.37–0.38 /0.59 /0.19–0.20 ms respectively;
small timing differences are not promoted into performance claims. Raw summaries
include p95, maxima, memory, allocation measurements and wake controls.

## Focused validation

The isolated combined physics archive passes:

- Collision geometry 1303; rigid-contact component checks.
- Impact timing 3197/89 cases; impact solver 267/46 cases; robustness 4412.
- Kinematic 96; ragdoll 36; imported-ragdoll 93.
- Sphere bounds 16; physics continuation 112; CharacterMotor 32.
- Final spill-source witness/pair-set/unwind 120283; initial mesh arrival 193.

Full curved continuation remains four asserted failures. No full production suite
was rerun by this workstream. The parent's final shared build/gates cover the
exact combined candidate. Windows functional coverage is reported separately by
the parent; these receipts do not certify Windows physics or hardware.

The dense v8 receipt predates only the late scratch overflow-policy refinement;
its recorded source hashes identify that precise binary. `raw/final-narrow/`
checks the exact final overflow source. The refinement changes neither ordinary
query arithmetic nor collision policy. `FINAL_SOURCE.json` identifies final
physics inputs separately, avoiding a false exact-binary provenance claim.

## Reproduction and evidence

```
cmake --build build --target judas_collision_witness_tests judas_impact_mesh_cost_tests -j2
build/judas_collision_witness_tests
build/judas_impact_mesh_cost_tests 1 trough 1.2 entry
build/judas_impact_mesh_cost_tests 1 trough
build/judas_impact_mesh_cost_tests 60 trough
build/judas_physics_stress_live_benchmark projects/physics_stress_lab/Scenes/room1024.judas OWNED_OUTPUT 105 270 0 0 1 0.016666667 1 0 1
```

The full/default curved commands must still fail; `entry` is a distinct contract.
`MEASUREMENTS.json` provides machine-readable dense comparison and physical-field
identity. `MANIFEST.json` hashes each retained receipt and its original
uncompressed bytes. Research scripts are execution records using original
scratch paths: copy/adapt them into fresh owned output before reproducing.
Generated binaries/archives remain outside tracked evidence; their hashes and
commands are retained. Protected history and independent fluid research were not
modified. No human acceptance is inferred from these checks.
