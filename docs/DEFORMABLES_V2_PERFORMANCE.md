# Deformables V2 — post-acceptance performance qualification

The operator human-accepted Stages 1–4 at Experimental
`69677c8c66d57149b6ee7fb02c08d9820d20f27c`. This separate optimisation pass
**was human accepted at `f0990a9ea00e6a769853d392136776ea28b80682`**. The accepted private-model quality and physical
behaviour remain the baseline. Ural window/door authoring, follow-camera suspension
bounce and missing mouse orbit are accepted demo limitations, not work for this pass.

The engine implementation is
`1481c831b8d382331254f5a18fdc25bca196efba`; acceptance records and diagnostics
are `996da923`. Subsequent qualification documentation changes do not change the
engine implementation. This remains an unsupported Experimental source preview,
not a published Alpha V2 distribution. Main, the Advanced Fluids lock, the RPG
project/selected SDK, original downloads and historical archives were preserved.

## Result and measurement contract

Severe-impact hitches are substantially reduced, **not eliminated**. The repeated
matched frontal, four-car and debris maxima fell approximately 68–71%. No solver
iterations, collision coverage, impact limits, structural resolution, material
strengths, damage rules or damping were reduced to obtain these results.

Ryzen 5 5600, Release/GNU 15.2, fixed 1/60 s intervals. The headless cases each
run 900 steps with the accepted 24 rigid sections, 40 finite plastic connections,
36-node bonnet, actual wheels and powertrain. The same private corrected Challenger
source, role data, source fingerprints, project paths, controls and solver settings
are used before and after. An immutable accepted executable was retained:
SHA-256 `89bd11d0145883c034794ddcabc3cc9f2c6a0c37f68a708951e840e201eb78a6`.
Timing cases ran serially without other local build, test or benchmark jobs. This
was a normal desktop host, not a fully isolated laboratory environment.

Numbers below are milliseconds, nearest-rank percentiles. **Simulation** includes
native coupled materials and the fixed world update. Tool panel telemetry is
measured separately. These are physics/world-step costs, not total frame times.
Preparation, rendering, swap, screenshots and save writes are outside these step
distributions. Profiled runs diagnose mechanisms; unprofiled runs establish gains.

### Original reproduction and full nine-case comparison

Each cell is accepted baseline → optimised, one 900-step run per case. A second
alternating-order comparison follows; the original measurements remain preserved.

| Workload | p50 | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| idle-one | 1.362 → 1.032 | 1.376 → 1.044 | 1.418 → 1.067 | 5.798 → 1.253 |
| drive-one | 1.485 → 1.115 | 1.553 → 1.170 | 1.568 → 1.185 | 1.585 → 1.257 |
| drive-four | 10.596 → 5.212 | 10.852 → 5.399 | 11.248 → 5.497 | 12.778 → 6.344 |
| frontal-15 | 1.548 → 1.175 | 2.441 → 1.612 | 4.270 → 2.173 | 59.146 → 23.148 |
| frontal-30 | 1.063 → 0.737 | 2.378 → 1.525 | 3.649 → 2.260 | 195.325 → 57.352 |
| offset-20 | 1.488 → 1.165 | 1.617 → 1.725 | 3.272 → 2.152 | 36.005 → 12.606 |
| cars-four | 12.340 → 6.172 | 15.273 → 7.601 | 21.576 → 9.542 | 321.782 → 104.613 |
| repeat-one | 1.614 → 1.241 | 5.808 → 3.273 | 46.980 → 17.508 | 196.521 → 66.349 |
| debris-four | 13.542 → 6.882 | 18.836 → 9.626 | 322.437 → 91.672 | 762.521 → 222.542 |

`drive-*` uses the existing acceleration/brake/reverse/steering course.
`frontal-*` declares launch speed in m/s. `offset-20` is the pole fixture.
`repeat-one` applies repeated physical launches. `debris-four` adds repeated
launches and subsequent wheel-powered travel through accumulated damage/debris.
The original `cars-four` and `debris-four` layouts have **two opposing cars and
two other active cars**, not four simultaneous opposing arrivals. A separate
true four-way junction collision is qualified below.

### Alternating-order matched repeats

One additional accepted/optimised pair per row, alternating which executable runs
first. Same 900 intervals, inputs and paths; raw per-step CSVs were preserved.

| Workload | p50 | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| drive-four | 10.460 → 5.218 | 10.707 → 5.401 | 10.765 → 5.459 | 11.926 → 5.654 |
| frontal-30 | 1.064 → 0.743 | 2.357 → 1.550 | 3.663 → 2.219 | 195.700 → 57.043 |
| cars-four | 12.266 → 6.172 | 15.270 → 7.720 | 21.650 → 9.052 | 323.373 → 104.203 |
| debris-four | 13.457 → 6.852 | 18.723 → 10.989 | 321.321 → 91.932 | 757.466 → 221.890 |

The independently repeated runs agree on the improvement. A 222 ms extreme
debris step is still a visible hitch. These results do not certify every dense
pile or a universal 60 Hz crash budget.

## Measured bottlenecks and changes

The original severe frontal diagnostic step spent 178.724 ms in continuous
collision search, including 106.934 ms of geometry work, out of 203.949 ms total.
It made 48,944 impact queries and 232,779 geometry calls. Actual impact solving
was 0.221 ms; joint preparation 0.070 ms; coupled local material work 0.958 ms.
Broadphase/candidate generation, initial manifolds, proxy refresh and fracture
publication were small in this step. Exact/numeric-gap fallbacks were zero:
high-precision arithmetic and sheet-plasticity solving were not the main cause.

The original four-car diagnostic similarly spent 218.251 ms in search, including
141.162 ms geometry, within a 335.931 ms step. It also exposed roughly 62 ms in
material rigid contacts. Further instrumentation confirmed **70.349 ms across
1,778 wake traversals** in the matched pre-wake-optimisation step. Those wake
costs are inclusive of callers, not an additional amount to add to the total.

Accepted corrections:

- **Safe compound bounds:** the already outward-rounded enclosing radius now
  rejects disjoint sweeps of identity-child box/sphere compounds. Actual-pose
  bounds support the offset single-box compounds used by structural sections.
  Child offsets retain the same represented subtraction as actual contacts.
  Rotated-child/cooked/other unsupported fast cases retain their existing path.
  No margin or contact truth is inferred from an overlapping bound.
- **Event-frontier pose reuse:** later collision sweeps retain their actual
  nonzero start pose separately from trial/bisection samples. Retiming invalidates
  both; the cache no longer acts as though every sweep starts at time zero.
- **Exact, bounded contact and search memoisation:** two 1,024-entry tables per
  world reuse identical observations. Contact keys include primitive IDs, all
  represented pose bits and margin. Root-search keys additionally include the
  exact interval/target and both trajectory revisions. A continuous-pass epoch
  prevents shape/slot changes from borrowing prior-pass results. Hash collisions
  recompute; they never drop contacts. Worker lanes never mutate this owner cache.
  No response, irreversible damage or speculative future state is cached.
- **Material contact preparation:** rigid candidate primitives, centres and
  inverse transforms are prepared once per projection call rather than for every
  node/edge/face sample. Reactions continue to read current velocities and effective
  mass; the call's frozen geometry and sample order are preserved.
- **Linear wake traversal:** snapshot current directed joint/contact/support
  adjacency once per wake, then traverse it. Previously every reached body
  rescanned every global relationship and allocated a set node. Static-ground
  propagation rules, generation checks, reachability and quiet-timer resets remain.
  The graph is rebuilt each call, so fracture or geometry changes cannot leave a
  stale persistent island graph. No wake is suppressed merely because its first
  body was already awake.

Final diagnostic frontal search is 50.722 ms in the earlier cache-only profile;
geometry predicates fall from 2,473,668 to 428,132 while its 435 impact events and
269 applied responses are unchanged. With all corrections, representative worst
profiled steps are 63.704 ms frontal, 111.422 ms four-car and 239.009 ms debris.
Wake traversal at the four-car spike falls to 4.757 ms. Diagnostic overhead makes
these totals different from the unprofiled timing tables. Categories overlap;
search includes geometry, and MK2 includes coupled material callbacks.

### Cooking, locality and rejected directions

Existing cooked assets already own connectivity, rest/mass data, panel constraints,
seams/attachments, collision proxies and detailed visual binding. The expensive
remaining queries depend on actual changing poses and trajectories. There is no
new cooked format, no new per-asset solver topology and no additional cooked
physics payload required by this pass. The new caches are derived runtime data,
not saved state. Model preparation remains about 5.7–6.5 seconds in this private
proof; warm-file differences are not claimed as a cooking optimisation.

The first bounds-only trials produced limited frontal improvement. Exact reuse
and the measured wake defect account for the substantial combined improvement;
there is no claim that every small extraction independently solved a hitch.
Further high-resolution panel activation changes were not justified: internal
material solving was small, and existing local/sleep behaviour remains intact.
Speculative approaching-contact preparation was not implemented: reusing known
geometry answers removes cost without guessing future impacts or adding cancellation.
No solver-quality reduction, velocity reset or artificial rotation lock was used.

## Physical, visual and persistence evidence

All nine 900-step final durable-state hashes exactly match the accepted binary.
The hash includes positions/velocities, permanent translations/rotations/work,
broken and removed parts, enabled rigid poses, motion, mass and sleeping flags,
wheel persistence, powertrain rotor state and physical panel state. No tolerance
was needed for those comparisons. The repeated key cases also match exactly.

The supplementary native **four-way** project has four cars approaching a junction,
receiving one declared mass-scaled 20 m/s launch through ordinary game-owned APIs.
No damage or release is scripted. All four develop permanent physical damage
(2, 2, 2 and 1 broken structural connections, plus panel plasticity/detachment).
The accepted and optimised ordinary runtimes produce identical full sampled
physical states and **pixel-identical screenshots** at step 159. The native
profiler retained 119 recent frames; nested export records were bounded/truncated,
so this is supplementary physical/visual evidence, not an exhaustive phase profile.

Two separate cold-restored 900-step continuations verify the accepted damaged
frontal and four-car debris snapshots before advancing. All 136 and 544 sampled
physical telemetry lines respectively match the accepted runtime exactly. Median
costs are 2.087 → 1.587 ms frontal and 7.850 → 4.545 ms debris.

A freshly SDK-exported private normal project was relocated, physically crashed,
saved and closed, then loaded in another process with the SDK, development checkout
and private originals hidden and networking disabled. Permanent bonnet bending
**1.5111538670469509 radians** and all three released mounts restore exactly.
This uses ordinary SaveService/world participants and an ordinary project script.
It is not a special crash-tool persistence substitute.

Desktop visual checks use the actual GTX 970 / 4 GiB, NVIDIA 580.178.04, GL 4.6.
Cold-restored damage photographs are also pixel identical. Wall-paced 360-step
one/four-car render runs preserve damage outcomes. Single-car GPU median is
1.051 ms before and after. Four-car medians are 1.880 and 1.754 ms, but render
sample counts differ as faster physics permits more presentation frames: **no
matched GPU performance improvement is claimed**. CPU panel mapping/upload is
about 0.44 ms single-car and 1.4–1.6 ms four-car. Initial GPU preparation is about
171 ms / 534–544 ms; it is distinct from collision spikes. The full portable
compute/render fallback gate also passes. No renderer/shader path was changed.

## Memory and cost limits

The two collision tables occupy **868,352 bytes per world**, allocated once and
reused, with fixed entry counts independent of crash/debris count. Their storage
is included in geometry-cache accounting. Wake graph scratch is transient and
bounded by current bodies/relationships; measured high-water capacities are
recorded in the private per-step profile, not retained indefinitely.

Matched headless process peaks are roughly 975,000 KiB accepted and 990,000 KiB
optimised (about 15 MiB higher). They include the large private model decode,
materials and diagnostic infrastructure, not just physics. Earlier initial runs
have different peaks; they are not used to claim a memory reduction. No new cooked
asset size or render-polygon simulation cost is introduced. Native allocator
wall time was not independently isolated: allocation/temporary construction is
included in the measured preparation/contact/wake phases, with container-byte
and process-RSS accounting. No full malloc trace or leak certification is claimed.

The worst remaining tails are continuous collision searches across many changing,
spinning rigid sections and debris. Wake adjacency still rebuilds once per call;
this is bounded and much cheaper, not a universal large-world wake-cost certificate.
No major chassis/CCD architecture replacement was attempted or needed for these gains.

## Qualification and platform scope

- Current native Experimental gates: **183 passed, zero failures**. This includes
  MK2, vehicles, terrain, materials, persistence, particles, Basic Water/locked
  Advanced Fluids policy, renderer, scripting and editor workflows.
- Added exact-query/frontier/reversal/offset/large-origin controls: **22 passed**.
  New public wake-isolation controls verify that a shared static floor does not
  spuriously wake a separate dynamic island, while explicitly waking the floor
  reaches both supported stacks.
- [Engine CI 42903976581](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42903976581):
  completed successfully at `1481c831`; Experimental **183 passed**, Alpha
  **179 passed + four expected locked-water skips**. Ubuntu 24.04 SDK checks pass.
- [Documentation CI 42903976421](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42903976421)
  passed at the implementation checkpoint. The final documentation-only follow-up
  has its own check recorded in the private handover receipt.
- Matching Linux SDK: read-only integrity/tool checks, **39 workflow operations**,
  fresh project creation, editor Play/Stop and both moved exports pass. An external
  mount namespace hid source/private projects; the orchestrator's own
  `packageOnlyNamespaces` flag remains false because its `--rootfs` mode was not used.
- Windows MinGW: current engine/editor/tools and the new CCD/MK2 tests compile;
  **16-tool static SDK staging and 26 PE/dependency/export contracts pass**.
  This is compilation/static verification, **not native Windows/MSVC execution**.
  The native SDK CMake target initially attempted to execute PE identity probes on
  Linux; cross staging was rerun without native execution, not reported as a
  successful Windows functional test.
- No new Android, browser, controller, audio or clean-machine hardware qualification.
  Host SDK glibc 2.43 is a local preview; the separate CI Ubuntu SDK establishes its
  own baseline. Initial local gates rejected old pre-checkpoint test identities;
  all wrappers were relinked and the complete 183-gate run passed unchanged.

## Reproduction and human handover

Private evidence is under `Judas_Deformables_V2_Crash_Proof/Performance-69677c8c/`:
`baseline.json`, `trial1` through `trial7` logs, `matched.json` and per-step CSVs,
`final-profiles.json`, `cold.json`, native four-way state/pixel comparisons and GPU
logs. Downloaded model data and all these generated vehicle projects stay outside
Git/public SDKs. Committed controls use original engine fixtures.

The operator-local review directory is
`/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/Performance-1481c831-Acceptance/`.
`REVIEW_READY.json` records the final engine/documentation/SDK identities and results.
The accepted 69677c8c project and executables remain separate.

1. Run `Play-Challenger.sh`; drive, reverse, brake and steer before impact.
2. Make minor and severe wall impacts, then repeat/offset impacts. Confirm normal
   wheel behaviour, local permanent bending and genuine progressive detachment.
3. Use the controlled-crash launcher and four-way fixture to inspect hitching,
   debris and damage under load. Compare to the retained accepted executable if needed.
4. Save with **K**, close the process, reopen and load with **L**. Inspect retained
   damage, independent detached bonnet and continuing vehicle functionality.
5. Open `Edit-Challenger.sh`, Play/Stop and inspect normal components/diagnostics.

Known private camera/door authoring limitations remain deliberately untouched.
The operator explicitly accepted this first optimisation pass at `f0990a9e`.
A separately authorised [second collision-search pass](MK2_COLLISION_SEARCH_OPTIMISATION.md) is complete and awaits its own human review.
Main promotion and publication are outside this task.
