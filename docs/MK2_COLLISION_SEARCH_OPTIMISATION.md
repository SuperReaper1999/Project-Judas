# MK2 collision-search optimisation — second pass

The operator human-accepted the first performance pass at
`f0990a9ea00e6a769853d392136776ea28b80682`. This second pass is **awaiting human
acceptance** on Experimental. Main, the Advanced Fluids lock, consumer SDK and
private original model files remain unchanged. No release is being published.

## Scope and evidence

Ryzen 5 5600, Release/GNU 15.2, the same corrected private Challenger, 24 structural
sections, 40 finite connections, 36 bonnet nodes, wheels and powertrain; 900 fixed
1/60-second steps per case. Executables and per-step CSVs are retained. Runs are
serial, without competing build/test jobs, on a normal desktop rather than an
isolated laboratory host. Profiled runs diagnose costs; **unprofiled** runs establish
performance. World-step time includes native materials; telemetry, preparation,
rendering, swap and save I/O are separate, not silently called physics time.

The immutable accepted executable SHA-256 is
`425b442ddb9d315246d803da9f9ec5b4886db5c93caf03fcbe936dee62f3e976`.
Raw evidence, trial results, command arrays and private model projects are under
`/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/Performance2-f0990a9e/`.
No downloaded geometry, textures or derivatives are committed or put into the SDK.

## What was expensive

The remaining problem is many small, repeatedly invalidated searches, not a
single 100-ms geometry query. At the worst profiled dense-debris step, root searches
totalled 184.253 ms within a 263.980-ms diagnostic world step. Conservative
advancement accounted for 86.427 ms, sampling 18.420 ms and motion-boundary
preparation 3.666 ms. The largest individual search was approximately 0.2 ms.
The heavy pairs were identity-child, offset compound **Box/Box** sections.

The same step visited 601,460 motion segments, with 35,311 interior knots and
29,026 vector-growth events. Geometry and root-search tables evicted 87,230 and
47,760 occupied entries respectively. Eviction alone does not imply a useful
result was lost: many keys belong to superseded trajectories. Direct experiments,
not those counts alone, justify changing retention.

The optional native `SetCollisionSearchObserver` reports body/primitive identity,
exact interval and target, participating trajectory revisions, phase timings,
start-contact classification, cache use/evictions, poses, orientation preparation
and motion-vector growth. The crash tool retains at most 4,096 pair aggregates and
32 slowest searches per step, plus all-query histograms. Cached or early-rejected
roots may have unknown start classification; touching/overlapping is not a proof
of strict penetration. Vector growth is not a comprehensive malloc trace.
Callback overhead is excluded from each query's clock but included in diagnostic
world-step time. Recursive phase scopes overlap; their sums are not a partition
of total CPU time. No diagnostic callback runs unless explicitly installed.

## Controlled changes and decisions

- **Retained:** 4,096 exact geometry observations and 4,096 exact
  root-search observations per world, two-way sets with least-recent replacement.
  Existing exact keys, pass epochs, participating trajectory revisions and
  owner-thread event ordering are unchanged. No interval widening, sampled-negative
  reuse, contact thresholds, solver iterations or physical budgets were changed.
- Descriptor-only Box/Sphere type/feature reads avoid constructing a posed owning
  primitive just to read immutable metadata. Cooked/unusual children retain the
  existing resolver. This is a small preparation reduction, not the main gain.
- **Rejected:** ordered merging of existing motion knots. It removed allocations
  and rescans but produced no meaningful net gain in these matched workloads;
  the established sorted-vector path remains. Experimental source/results are
  retained privately. No revision-keyed motion ownership system was introduced.
- Direct-mapped, two-way and four-way 4,096-entry trials were compared, as was a
  four-way 1,024-entry trial. More associativity was not automatically better.
- **Deferred, not claimed as failed implementations:** new rotating-offset sweep
  proofs and hierarchical sampling rejection. Existing instantaneous offset-box
  bounds remain; a rotating child must include its actual orbital offset and the
  represented parent trajectory. A centred-box shortcut is invalid. Sampling was
  a smaller cost than advancement; no unproved interval skip was substituted.
- Existing pair-search invalidation already uses only participating trajectory
  revisions. Unrelated bodies do not invalidate those exact keys. Wake adjacency
  was already much cheaper after the accepted pass; neither was rewritten.
- Worker comparisons cover the existing deterministic initial-manifold path.
  The continuous frontier remains owner-thread serial. Speculatively querying
  different intervals is not exact memoisation: interval-dependent refinement and
  subsequent trajectory changes would need a separate correctness design. No
  nondeterministic event resolution or speculative permanent damage was added.

## Memory and correctness contract

The two exact tables now occupy **3,538,944 bytes per world**, versus 868,352 bytes
in the accepted pass: +2,670,592 bytes, allocated once and bounded. The stamps are
derived cache state, not persistence. Native observer aggregates belong to the
profiling tool, not ordinary gameplay. Process RSS includes loading/cooking the
large private source model and must not be described as steady collision memory.
No cooked asset expansion, new workers by default or new material representation
is required.

The existing collision coverage, all 32 refinement updates, temporal sample
counts/limits, rotating trajectories, event ordering and physical response are
preserved. In particular, the inherited bounded temporal fallback is not being
relabelled as a mathematically complete continuous-contact proof. Existing
near-miss, grazing, fast-spin, large-origin, trajectory-reversal and contact
negative controls remain required.

## Qualification

Engine implementation: **`d07e22363f385089156fa43b4ba71ba021e39b5e`**.
The final documentation checkpoint and matching refreshed Linux SDK are identified
in the handover receipt; subsequent documentation changes do not change engine code.

- **184/184 native Experimental gates passed**, zero failures, 242.98 seconds.
  This covers current editor/scripting/rendering/portable paths, MK2, vehicles,
  terrain, particles, water policy, networking and persistence.
- New exact-cache/observer controls: **28 passed**, now a registered focused CI
  gate. Independent rotating/motion bounds: **470,408 passed**; existing
  deterministic parallel narrowphase controls: **55 passed**.
- [Engine CI 42923650169](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42923650169)
  completed successfully: Experimental **184 passed**; Alpha **180 passed plus
  four expected locked-water skips**. Both channel jobs completed; no pending
  job is counted as passing. The Ubuntu 24.04 SDK checks passed.
- [Tooling CI 42923650159](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42923650159)
  and [documentation CI 42923650223](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42923650223)
  passed at the implementation checkpoint.
- Linux SDK: **15 public tools**, **39 tool operations**, eight orchestration
  checks; fresh editor project creation, Play/Stop, integrity before/after and
  moved editor/CLI exports passed. An outer read-only mount namespace hid the
  checkout and private projects. The orchestrator's `packageOnlyNamespaces` flag
  is false because its separate `--rootfs` option was not used; do not confuse
  that flag with the actual outer isolation.
- An additional real private car export used only SDK tools, was moved, then ran
  in separate source-hidden write/read processes. Permanent bonnet bend
  **1.5111538670469509 radians** and all three released mounts restored exactly.
  No private vehicle data is in Git or the public SDK.
- Windows MinGW/GNU 13: all **16 public Windows tools**, the new CCD controls,
  MK2, parallel-query and motion-bound controls compile. Static SDK integrity and
  **26 PE/dependency/export contracts pass**. No Windows executable was run on
  Linux. This is not native Windows/MSVC qualification.
- No new Android, browser, controller, audio-hardware or clean-machine execution
  is claimed. The local Linux SDK uses host glibc 2.43 and remains an unsupported
  preview; CI's separate Ubuntu SDK establishes its own baseline.
- The post-commit nine-case rerun also matches all nine exact hashes. Two cold
  continuations, each another 900 steps in fresh accepted/candidate processes,
  match full final hashes. No gameplay/API break or physical simplification was
  introduced. The native observer is an optional diagnostic addition.

## Matched measurements

Milliseconds, nearest-rank percentiles. One complete 900-step run per case;
accepted f099 → candidate. The initial four-car accepted maximum includes a host
outlier; **do not use 149.881 → 94.957 as the representative improvement claim**.
The alternating comparison below exposes the repeatability limits.

| Workload | p50 | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| idle-one | 1.038 → 1.034 | 1.057 → 1.073 | 1.166 → 1.090 | 8.007 → 5.686 |
| drive-one | 1.113 → 1.107 | 1.163 → 1.159 | 1.176 → 1.173 | 1.269 → 1.508 |
| drive-four | 5.221 → 5.207 | 5.428 → 5.400 | 5.681 → 5.464 | 7.138 → 5.628 |
| frontal-15 | 1.166 → 1.172 | 1.596 → 1.576 | 2.152 → 2.125 | 22.197 → 21.636 |
| frontal-30 | 0.745 → 0.745 | 1.545 → 1.541 | 2.220 → 2.274 | 57.664 → 52.444 |
| offset-20 | 1.152 → 1.156 | 1.266 → 1.301 | 2.055 → 2.113 | 12.476 → 12.287 |
| cars-four | 6.216 → 6.183 | 8.123 → 7.591 | 11.072 → 9.085 | 149.881 → 94.957 |
| repeat-one | 1.246 → 1.243 | 3.288 → 3.241 | 17.598 → 16.147 | 66.580 → 51.961 |
| debris-four | 6.813 → 6.854 | 8.987 → 9.008 | 92.570 → 75.045 | 222.798 → 131.617 |

Two additional pairs ran accepted-A, candidate-A, candidate-B, accepted-B. Averages
below are averages of **per-run statistics**, not pooled percentiles.

| Workload | p50 | p95 | p99 | Mean of maxima |
| --- | ---: | ---: | ---: | ---: |
| frontal-30 | 0.743 → 0.776 | 1.542 → 1.611 | 2.239 → 2.287 | 57.260 → 54.302 |
| cars-four | 6.218 → 6.201 | 7.856 → 7.618 | 9.175 → 9.528 | 119.110 → 95.520 |
| debris-four | 6.820 → 6.909 | 8.970 → 9.184 | 91.964 → 75.256 | 222.276 → 131.549 |

The two accepted four-car maxima span 104.653–133.567 ms; candidate maxima span
95.114–95.926 ms. A conservative interpretation is about **8%** improvement against
the faster accepted run, rather than attributing every host outlier to the cache.
Frontal improvement is small (~5% averaged maxima). Dense-debris maxima repeatedly
fall ~41%, but p95 and quiet post-damage medians can be slightly worse; this is a
tail-cost improvement, not a universal FPS improvement.

Counts above thresholds in the complete nine-case comparison. Each cell is
`count (longest consecutive run)`; accepted → candidate. Other cases are in the raw
CSV/JSON. Thresholds apply to physics/world steps, not rendered frames.

| Workload | >16.7 ms | >33.3 ms | >50 ms | >100 ms |
| --- | ---: | ---: | ---: | ---: |
| frontal-30 | 1 (1) → 1 (1) | 1 (1) → 1 (1) | 1 (1) → 1 (1) | 0 (0) → 0 (0) |
| cars-four | 4 (4) → 4 (4) | 3 (3) → 3 (3) | 3 (3) → 3 (3) | 2 (2) → 0 (0) |
| debris-four | 18 (13) → 18 (13) | 14 (11) → 14 (11) | 14 (11) → 13 (10) | 9 (8) → 4 (2) |

### Independent trials

These are single diagnostic-design comparisons, not statistical confidence
intervals. Source/binaries and exact-state checks are retained per variant.

| Variant | Frontal maximum | Four-car maximum | Debris maximum |
| --- | ---: | ---: | ---: |
| type-only | 56.211 | 102.736 | 216.816 |
| cache4-1024 | 60.310 | 96.366 | 156.157 |
| cache4-4096 | 52.873 | 96.319 | 131.456 |
| ordered-merge | 52.735 | 96.457 | 130.319 |
| feature-metadata | 52.926 | 95.951 | 130.795 |
| cache2-4096 | 51.990 | 95.508 | 131.347 |
| cache1-4096 | 52.915 | 96.303 | 147.485 |

### Worker lanes

| Lanes | Four driving: p50 / p99 / max | Debris: p50 / p99 / max |
| --- | ---: | ---: |
| 1 | 5.259 / 5.535 / 5.605 | 6.850 / 75.080 / 131.672 |
| 2 | 5.239 / 5.507 / 5.547 | 6.870 / 75.354 / 131.236 |
| 4 | 5.229 / 5.487 / 6.170 | 6.949 / 75.873 / 134.334 |

All six worker trials match exact state. They submit **zero** jobs: existing
parallel initial-manifold eligibility excludes these compound structural shapes.
The extra idle lanes offer no meaningful gain. This is not a completed parallel
CCD implementation or a claim that independent queries cannot be parallelised.

### Reprofile

Root-call counts are identical across all 900 steps before/after: 65,428 frontal,
525,231 four-car and 2,146,910 debris. Dense-debris root memo hits increase from
986,570 to 1,212,525. Geometry requests fall from 3,010,500 to 2,216,868; occupied
geometry/search evictions fall from 753,364/611,254 to 416,881/233,635. Whole-run
root search time falls from 1,946.424 to 1,439.501 diagnostic milliseconds.
The most expensive root-search step moves from 426 to 429, with 184.253 → 108.564 ms
of root searches. The slowest individual query remains ~0.216 ms: the gain is
avoiding redundant work, not making a difficult pair disappear.
No pair aggregates were dropped in any of the six detailed 900-step captures.

Two genuine cold processes per executable restore the same saved frontal/debris
inputs and advance another 900 steps. Both initial restoration checks and final
full physical hashes match exactly. `cold-comparison.json` records save hashes.
This supplements the nine original hashes; it does not replace SDK/export checks.

Remaining hitches are real. Neither a 130-ms debris step nor a 95-ms four-car step
is a 60-Hz result. Continuous advancement and contact response remain dominant;
this pass does not claim to have eliminated severe-impact hitching.

## Individual pairs and resweeps

Representative accepted-baseline worst steps; IDs are physics body handles, not
scene entity IDs. All these pairs are offset compound Box/Box sections.
Unknown classifications include exact root-cache hits and early bound rejections;
no extra contact evaluation was introduced solely to fill a diagnostic category.

| Workload / step | Pair | Calls / cache hits | Trajectory-key changes | Cumulative search ms | Known separated / touching-or-overlapping |
| --- | --- | ---: | ---: | ---: | ---: |
| frontal / 130 | 3 / 7 | 435 / 221 | 127 | 13.348 | 97 / 117 |
| four-car / 134 | 1 / 25 | 542 / 166 | 173 | 20.670 | 372 / 4 |
| debris / 426 | 14 / 15 | 482 / 272 | 11 | 9.695 | 210 / 0 |

The slowest frontal root queried bodies 2/8 over
`[0.001895693305414172, 0.003316348986118103]` seconds, starting separated;
29 advancement iterations and 36 sampling tests cost 0.199 ms. The slowest debris
root queried 1/10 over `[0, 0.004166666883975267]`, starting touching/overlapping;
59 iterations and zero sampling tests cost 0.216 ms. The all-query time-interval
and cost histograms and the 32 slowest roots per step remain in raw JSONL.
Repeated actual trajectory changes explain why retaining old pair results across
arbitrary later searches would be incorrect.

## Cold-restored continuation timings

The same saved input bytes were restored before each executable, then 900 further
steps ran without concurrent build/test jobs. These are continuations of damaged
vehicles, not a claim that a fresh severe impact was added to each restored case.
Both initial and final full physical hashes agree. Milliseconds:

| Workload | p50 | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| frontal-30 | 1.587 → 1.583 | 2.203 → 2.204 | 2.347 → 2.357 | 2.408 → 2.412 |
| debris-four | 4.530 → 4.560 | 6.043 → 6.050 | 6.274 → 6.277 | 6.546 → 6.318 |

All four continuations have zero steps above 16.7, 33.3, 50 or 100 ms. Per-step CSVs
are retained. Cold restoration remains correct; no meaningful speedup is claimed
for these already quiet continuations.

## Presentation, genuine four-way and memory

Actual desktop: GTX 970, 4,096 MiB, NVIDIA 580.178.04, OpenGL 4.6; CPU confirmed
Ryzen 5 5600, 12 logical lanes. A bounded private SDL swap interposer measured
end-to-end presented-frame intervals in identical accepted/candidate runs.
These include compositor/swap waiting and multiple fixed steps when catching up;
they are not GPU execution time. GPU timestamps were disabled in these unprofiled
runs, so no new GPU-time improvement is claimed. Render CPU and mapping/upload
summaries remain available separately.

The first frontal presented maximum was **69.050 → 62.439 ms**, and four-car
**235.913 → 215.864 ms**. The four-car p99 initially worsened **12.282 → 15.823 ms**;
that result was retained and prompted alternating repeats, rather than being hidden.

| Four-car repeat | Presented p50 | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| candidate-a | 9.996 | 10.801 | 12.244 | 214.988 |
| accepted-a | 9.996 | 10.758 | 12.250 | 235.059 |
| accepted-b | 9.996 | 10.783 | 12.070 | 234.084 |
| candidate-b | 9.995 | 10.906 | 14.824 | 220.407 |

Accepted repeats have 7–8 frames above 16.7 ms and five above 33.3 ms; candidate
repeats have 7–10 and 3–4 respectively. Longest above-33.3 sequences reduce from
four to three frames, while all four runs still contain three frames above 50 ms
and one above 100 ms. Therefore **lower severe maxima do not imply uniformly
better rendered p99 or removal of visible hitches**. No renderer change is claimed.

The genuine four-way fixture uses four approaching physical cars. Its complete
96-part/interface/panel state and captured PNG are **exactly identical**. The
fixed-step harness intentionally renders only requested captures and does not
swap: the initial frame-recorder assertion failed because it expected swaps;
that failed attempt is preserved, not called an engine failure. Its existing
profiler's completed fixed-step clocks cover the same **119 steps, IDs 42–160**,
including the collisions, rather than the first 41 quiet steps:

| Genuine four-way, captured fixed steps | p50 | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| accepted → candidate | 7.924 → 8.089 | 33.955 → 32.581 | 78.168 → 63.157 | 103.237 → 84.036 |

Both captures report 3,797 truncated nested scope records, but no incomplete
fixed-step clock. These are diagnostic-window statistics, not a complete 160-step
phase census or presented-frame measurement. The headline nine-case tables remain
unprofiled. No missing individual scope was invented or treated as zero work.

Whole-process headless peak RSS spans **988,348–989,804 KiB** accepted and
**988,716–990,292 KiB** in the committed candidate rerun. Loading/cooking the large
private model dominates the high-water mark; it masks the independently measured
+2,670,592-byte per-world cache increase. No RSS reduction or asset-size reduction
is claimed. No new asset format or cooked data was needed.

## Human acceptance handover

Local review project:
`/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/Performance2-d07e2236-Acceptance/`.
Its `REVIEW_READY.json` records exact final code/documentation/SDK/artifact identities.
The earlier accepted projects and executables remain separate. Downloaded models
and local packages remain private and must not be redistributed.

1. Run `Play-Challenger.sh`. Drive/reverse/steer/brake normally, then make severe,
   offset and repeated impacts. Compare local bending, mounts, detachment and debris
   with the accepted build; assess hitching honestly rather than only the FPS average.
2. Run `Controlled-Crash.sh frontal 30 1`, then `Controlled-Crash.sh cars 20 4`.
   `P` pauses, Enter launches the declared controlled condition, Q/E orbit,
   F6 saves; ordinary W/S, A/D and Space use actual wheels/powertrain.
3. Run `Watch-Four-Way.sh` to inspect four actual approaching vehicles. `1`–`4`
   select a vehicle; the fixture supplies only the declared initial impulse.
4. In the normal editable project, **K** saves, close the process, reopen and **L**
   loads. Inspect permanent damage, detached parts, wheel behaviour and driving.
5. Run `Edit-Challenger.sh` for the matching SDK editor and normal Play/Stop.
   The separate automated cold export uses its own test-only physical launch input;
   it is not the normal human driving project.

Known camera-follow bobbing and private component/window authoring imperfections
remain deliberately accepted demo limitations. No asset polishing, Lucifer work,
main promotion, release tag, publication or RPG SDK update is part of this pass.
Further collision-search improvements remain possible, especially carefully proved
offset sweeps and deterministic independent work; neither is being sold as completed.
**This second optimisation pass stops awaiting the operator's human acceptance.**
