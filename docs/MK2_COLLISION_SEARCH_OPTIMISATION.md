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

- **Retained provisionally:** 4,096 exact geometry observations and 4,096 exact
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

## Qualification status

The extended anchor/cache/observer controls currently pass **28 checks**. All
nine matched candidate cases retain their accepted exact physical-state hashes.
Full gates, CI, fresh SDK and final playable handover are still being completed;
this paragraph is not a passing receipt for pending work.

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
