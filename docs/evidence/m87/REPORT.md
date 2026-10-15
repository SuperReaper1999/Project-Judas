# M87 — Judas Learns to Scale Physics

## Status and scope

Linux-validated Experimental candidate; **human acceptance pending**. Main remains
at the accepted M86 checkpoint. Windows M87 compilation/runtime was not performed;
Windows remains informational and nonblocking. This is not a full physics audit or
a blanket promise that every thousand-body scene runs at 60 Hz.

Starting source: `4fada67c3228d37301faffa33e79fff65c695461`.
Baseline captured **2026-10-15 17:56 UTC**, before physics changes. Release,
GCC, `-O3 -DNDEBUG`, C++17, Linux x86-64, Intel i7-4790 (4 cores / 8 threads).
The existing desktop GPU is a GTX 970; CPU benchmark timings do not measure it.
Exact hardware/configuration/time/source identities are in
[baseline provenance](baseline/provenance.json) and the raw run receipts.
`baseline/frozen-binaries.json` identifies the actually measured, freshly linked
baseline executables separately from the initial inspection hashes.

Two independent CPU-bound fluid-research processes were left entirely untouched.
Timed benchmark processes inherited affinity to logical CPU 3; builds, validation
and timed runs were serialized. Frequency governor remained `schedutil` and host
scheduling was not controlled. Treat small differences and single maxima as noisy;
do not infer portable hardware budgets from this machine.

## What profiling established

The old sleep assessment created map/member-vector storage each step and rescanned
all cached contacts, touch records and joints for each independent island.
Its cost grew with **islands × global edges** even when everything was asleep.
In the 1,024-body diagnostic captures, mean sleep assessment consumed:

| Workload | Baseline sleep assessment | Final sleep assessment |
|---|---:|---:|
| Cooked terrain | 41.016 ms | 0.123 ms |
| Active mechanisms | 18.375 ms | 0.118 ms |
| Naturally sleeping mechanisms | 18.502 ms | 0.122 ms |

These are inclusive M56 retained-frame diagnostics, separate from normal-allocator,
collection-off acceptance timings. Full attribution is in
[profile-attribution.json](profile-attribution.json).

Independent joint preparation also did growing linear membership searches for
every body/state. A separate unchanged-input probe showed quadratic duplicate
append work. Cooked box queries rebuilt the same twelve edges with a `std::set`
and repeatedly grew point/normal vectors. In Tuppence, CCD geometry/search and
impact-island discovery dominate remaining spikes; iteration arithmetic alone
was not the main cause of those spikes.

## Generic corrections

1. **Sleep graph aggregation.** Reuse slot-indexed DSU/member/island scratch and
   classify support/joints/body motion in passes. Preserve union order, ascending
   roots and member order, generation checks, quiet clocks, thresholds, asymmetric
   world-anchor semantics and the exact deep-contact neighbourhood rule. Derived
   scratch is never serialized. Actual support removal and impulses still wake
   connected physical bodies; sharing a world does not wake unrelated assemblies.
2. **Ordered joint membership.** Retained open-addressed pointer tables replace
   quadratic membership scans. They never order a solve. First-encounter body
   vectors, input/row order, duplicate non-append semantics and appended warm-start
   protection remain unchanged.
3. **Cooked box allocation.** Reserve eight vertices/six normals; assign precisely
   the previous lexicographically ordered twelve box edges. Preserve face order,
   transformed arithmetic, cross-product normals, SAT and witnesses. Hull topology,
   triangle sidedness and the accepted triangle-normal guard are unchanged.
4. **Bounded profiling.** Separate prediction/wake, integration, candidates, contact
   preparation and history/endpoint publication. Aggregate joint preparation and
   CCD search/geometry/island/solve time in constant per-world storage. No event
   per predicate or impact island. [Profiler semantics](../../PROFILER.md) explain
   overlapping inclusive counters and legacy StepStats boundaries.

No timestep, iteration count, CCD budget, contact margin, material, sleep threshold,
collision filter, body population or benchmark input changed. No distance freezing,
physics bypass, game-name branch, new middleware or parallel solver was added.
No numerical change is intended; matched trajectory evidence supports exact
same-platform physical behavior within the tested workloads.

## Matched scaling measurements

The same frozen C++ driver links the ordinary engine before/after. Five workloads,
64/256/1,024 dynamic bodies, two processes per case, 60 warm-up / 120 measured
steps (300 warm-up for naturally sleeping mechanisms), fixed 60 Hz. Timing uses
ordinary allocation with M56 collection **off**. Gravity submission plus Step is
timed; inspection/CSV/report export and per-machine motor-policy updates are
outside that interval. The counters and separate application runs expose those
boundaries; these are not displayed FPS measurements.

[Complete 15-row table](SCALING_TABLE.md) includes medians, p95, maxima and peak RSS.
[Machine-readable summary](scaling-summary.json) pools the two 120-step samples per
case using lower empirical quantiles. Representative 1,024-body results:

| Workload | Median before → after | p95 before → after | Worst before → after |
|---|---:|---:|---:|
| Active mechanisms | 21.711 → 2.565 ms | 24.741 → 2.839 ms | 31.183 → 5.971 ms |
| Sleeping mechanisms | 20.017 → 0.903 ms | 22.024 → 0.976 ms | 31.778 → 1.025 ms |
| Cooked terrain | 46.603 → 2.660 ms | 54.093 → 9.428 ms | 75.726 → 25.983 ms |
| Load-bearing stacks | 11.353 → 1.938 ms | 26.222 → 15.886 ms | 28.003 → 16.655 ms |

All **30 positive trajectory digests match**, as do all four negative-control
trajectories. Physical acceptance and contacts/anchors/actual wake endpoints pass.
Negative controls genuinely fail when contacts/body-body support/stepping are
removed; a timeout/crash is not accepted as a successful negative control.
Activity/sleep counts remain in raw CSV: 1,024 dynamic bodies does not imply all
1,024 remain awake. The free-motion case keeps all moving; active machinery has
512 naturally sleeping loose/support bodies at the measured endpoint.

### Allocation and memory evidence

The separate diagnostic executable intercepts normal/aligned/nothrow C++ new on
only the Step owner thread. It measures requested allocations/bytes, not C malloc,
deallocations, live bytes, worker allocation or VRAM. Its timing is not substituted
for the ordinary executable. The sleeping 1,024-body case falls from **2,563 to
594 new calls/step**, requested bytes **199,172 → 130,240**. Active mechanisms:
**2,585.79 → 616.79 calls/step**. Remaining contact/event/history work still allocates.

The isolated all-awake 256-body cooked case (sleeping disabled identically in both
allocation-instrumented ablation runs, M56 collection off) keeps exact trajectories
and reduces **24,378.47 → 19,770.47 new
calls/step**, requested bytes **3,103,698.6 → 2,882,514.6**. Median diagnostic time
6.157 → 6.081 ms is a small difference; it is not presented as a large speedup.

Peak process RSS in the normal sleeping 1,024 case is **25.1 → 25.3 MiB**; cooked
terrain **29.2 → 29.1 MiB**. Reduced allocation churn does not imply reduced RSS.
Retained scratch/hash capacity deliberately trades storage for fewer scans and
allocations. Process RSS includes allocator/cooking/profiler storage. Reserved
profiler capacity is not resident memory. See the complete scaling table.

### Joint isolation

At 128 / 512 / 2,048 independent joints, preparation medians are
0.0552 → 0.0525 / 0.2468 → 0.2114 / **1.4465 → 0.8646 ms**. Duplicate append:
0.00214 → 0.00055 / 0.02990 → 0.00239 / **0.43855 → 0.01617 ms**.
Ten solve rounds have unchanged arithmetic; small solve-time variation is not
credited as an optimization. Every physical digest matches. Raw samples and exact
source/object/binary identities are under [joint-isolation](joint-isolation/).

## Real consumer and articulated collision

Tuppence's current, unchanged ball/coin games exercise finite-force pusher, actual
convex tokens, drops, scoring, pause, save/load and scene changes. Their existing
application fixture records 1,048 fixed-frame samples per mode; it inspects script
state and renders selected screenshots, so frame-work is not displayed FPS.
Final collection-off comparison and collection-on attribution are recorded in
`consumer-summary.json` and the raw consumer receipts. Small/unchanged consumer
results must not be replaced with the much larger independent-island speedups.

| Mode | Median before → after (ms) | p95 before → after | Worst before → after |
|---|---:|---:|---:|
| play | 13.765 → 13.973 | 34.621 → 34.649 | 84.694 → 73.225 |
| coins | 6.816 → 6.614 | 29.809 → 30.656 | 117.979 → 131.095 |

These are matched collection-off fixed-step costs. Ball-mode median/p95 are
roughly unchanged; coin-mode median improves slightly while p95/worst increase.
All physical counter traces (population, awake/pairs/contacts/events/queries)
match exactly. Both game modes pass their original 30/31 checks. No substantial
consumer-game speedup is claimed. In the final collection-on coin capture,
retained spike-frame mean CCD geometry is 26.19 ms, search 19.01 ms and impact-island
discovery 6.99 ms (inclusive/overlapping). These remain meaningful optimization
limits; do not sum them.

The unchanged 13-body / 12-joint dense trough fixture retains digest
`c007331aa912b5d9`, **21.48 mm** maximum penetration, identical search/event counts
and its original **24 fast-box crossings / 0 exceptions**. Those crossings are a
known unresolved fixture outcome, not a passing no-tunnelling guarantee. Matched
fresh processes with the same `300 'trough 60x60 dense'` arguments report total
1,162.03 → 1,108.45 ms and worst 47.16 → 44.87 ms. This existing driver reports
aggregate/worst costs, not per-step median/p95. The separate six-scenario core
regression recorded 1,333.28 ms / 85.01 ms for its dense case after earlier
scenarios; that different process history is not a matched speed comparison and
still illustrates variable tails. All six existing scenarios retain their
support/determinism checks. This milestone does not solve every CCD spike.

## Editable Mechanical Arcade

[Project](../../../projects/physics_stress_lab/README.md): four normally registered
scenes, 32 independent cabinets, 96 native hinges/sliders, three articulated bodies
and one loose ball per cabinet; 128/512/1,024 loose pieces give **256/640/1,152**
total dynamic bodies. Ordinary static walls/floor/pit remain solid. A shared JS
controller provides finite motor intent, real impulses, camera, input, UI and tour;
physics owns all motion and natural sleeping. Runtime prefab additions are bounded
to 256 to prevent an unattended content policy allocating forever.

F5 watches drive → power-off → impulse; M toggles power, J/K impulse one/all,
1–3 choose population, 4 rotates the entire setup/gravity, P drops 32 bodies,
F6/F7 save/load, R reload, Escape pause, WASD/mouse/QE inspect.

Application validation caught real **new-content** bad clearance: the original
1.05-m shelf half-length stalled against cabinet walls at ~0.10 m, before its
0.225-m reversal point. The shelf is now 0.80 m, leaving room for its unchanged
±0.26-m stroke. Mass/drive force/joints/populations were retained. This has no
relation to the unchanged before/after benchmark or consumer geometry.

[Final application evidence](application/README.md): **51 checks / 0 failures**.
Loaded machinery genuinely travels 0.186691 m in the original 40-frame interval;
normal sleep arrives at 7.816667 s after unloading. Tour impulse moves the actual
selected mechanism 0.168497 m; an unrelated control remains still and asleep.
All 288 saved dynamic endpoints, native continuation bytes, pause, prefab spawning,
scale/rotated scene changes, reload and stale handles are checked.

## Focused validation and preserved failures

- Release build of changed mechanisms, lab, runtime/editor/export and relevant
  regressions: zero new-build warnings. The unchanged historical cold observer
  has two preserved pre-existing warnings; it was not edited for warning cleanup.
- New scaling semantics: **130/0**; ordered-joint checks: **25/0**.
- Combined final evidence: **29/30 focused core targets passing**. The remaining
  unchanged M77 foundation test is **34 checks / 1 failure** both on original
  physics and candidate: it expects participant 18 to reject, while the accepted
  starting save implementation supports 21. Its assertion and failure are retained.
- Contacts/cache/storage/lifetime, broadphase oracle, geometry, kinematic motion,
  joints, rigid CCD/impacts, M70/M77, CharacterMotor, ragdoll support/wake,
  continuation, origin shifts, materials/mechanical connections, wheels,
  powertrains, deformable/fracture and profiler checks have their individual logs.
- Profiler off/on preserves every endpoint across **2,112 simultaneous independent
  impacts**, with bounded scope count and no lost/truncated/incomplete records.
- [Fresh-process cold proof](cold/README.md): M70 writer10/reader14 and M77
  writer24/reader13 checks pass. M77 actual physical endpoints are **exactly equal**,
  both genuinely standing with two supported feet; snapshot bytes stay unchanged.
- Initial new-test failures, independent original-source reproductions, rejected
  thin long-lever fixture, premature quiescence observation, wrong selected-tour
  observation and lab clearance failure are retained under [failures](failures/).
  No existing acceptance threshold or physics quality was weakened.
- Editor/export/moved-package results are listed in `delivery-results.json`.
  Automated UI/functional evidence is not human acceptance.

No full unrelated production/fluid/research-suite rerun. FLU-001, legacy missing
cold-save history, independent research and protected historical evidence remain
unchanged. No Windows M87 pass or Windows hardware performance claim is made.

## Remaining limits and engineering choices

- Serial owner-thread solving remains. Shared tree query scratch and row/body
  accumulators are not safe for an unqualified parallel loop. Removing measured
  quadratic bookkeeping was the lower-risk benefit; no scheduling determinism
  promise was traded for speculative parallel speedup.
- Dense connected impacts, repeated CCD geometry/search, impact-island discovery,
  wake graph scans and genuinely deep-contact neighbourhoods can still be costly.
  The retained deep-contact test remains O(deep contacts × bodies). Normal sleep
  scratch scales with allocated body-slot high-water capacity, including holes.
- No sparse/high-water-slot or arbitrary huge joint-graph performance guarantee.
  The rejected .1-m half-extent / 1-m-lever fixed assembly is unstable on the
  original and final solver; M87 does not change its conditioning or iterations.
- Fast curved-mesh CCD crossings and ragdoll/contact tails remain recorded limits.
  Identical trajectories preserve existing limitations too.
- Small game/mesh timing differences are noisy; some worst steps increased.
  M56-on counters add diagnostic overhead, and process memory is not VRAM.

## One human acceptance checklist

1. Open the Mechanical Arcade; use F5 and inspect actual shelf/paddle/ball motion.
2. Toggle M off, wait for natural sleep, then J/K; connected bodies wake while
   independent resting cabinets remain quiet. Nothing disappears or freezes by distance.
3. Try 1–3 and P; watch collisions at all populations and inspect responsiveness.
4. Try 4: gravity and support follow the rotated physical setup.
5. Save/load during motion; pause/resume, reload and editor Play/Stop.
6. Open View → Profiler; inspect new physics stages/counters, freeze/export capture
   while simulation continues, and distinguish inclusive times from sums.
7. Run the exported package from a different working directory; repeat controls.
8. Try current Tuppence ball and coin modes. Judge actual responsiveness honestly;
   this is not a promise that their remaining CCD spikes disappeared.

## Paths and reproduction

- [Mechanisms and exact commands](../../M87_PHYSICS_SCALING.md).
- `scripts/m87_benchmark.py`: ordinary/diagnostic matrix and before/after digests.
- `scripts/m87_validate.py`: focused serial correctness and graphics application.
- `scripts/m87_consumer_benchmark.py`: matched current consumer inputs, collection off/on.
- `scripts/m87_cold_validation.py`: unchanged historical observer/current archive.
- `FINAL_CHANGED_FILES.txt`, `FINAL_FINGERPRINTS.json`, `FINGERPRINT_VERIFICATION.json`:
  curated scope, exact final source/binary identities and checkpoint verification.
- Raw archives retain complete CSV series, bounded M56 JSON, commands, source
  patches, timestamps, host/configuration receipts and their entry SHA-256 inventory.
  Executables/build archives/caches/private project copies are not committed.
