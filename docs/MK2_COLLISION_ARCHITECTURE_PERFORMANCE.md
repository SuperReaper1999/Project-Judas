# MK2 collision architecture and real-time performance

Status: implemented on Experimental for human review, with explicit remaining
absolute performance failures below. This is not a supported release or human
acceptance. Main and the Advanced Fluids lock remain unchanged.
The camera/terrain interaction correction preceded this work at `d5a5087a`;
`6fd9378b` added its project checks to CI. Both CI gates completed successfully.

## What failed

The accepted rigid-motion, mass/inertia, contact and material kernels were not
replaced. The major defect was ownership of continuous searches:

- Each mover retained its earliest result and repeatedly searched its entire
  candidate set. A nearby changed trajectory could reset that mover's query
  origin and prepared-row status, including pairs whose own trajectories had
  not changed. Tighter conservative bounds could consequently change physics.
- A detection at actual contact was sometimes resolved using a widened
  proximity manifold with different contact points. An original procedural box
  fixture reproduces an actual approach of approximately 0.127 m/s while those
  proximity points separate. The old response loops through 260 impact events
  every step and holds the box in place.
- Independent binary responses could repeatedly close neighbouring contacts at
  the same time, returning to geometry search rather than resolving compression
  within the connected contact set.
- Repeated wake requests rebuilt the full directed wake graph even when every
  live body already had exactly the awake/zero-quiet-time state being requested.
  Material contacts also repeated propagation already owned by the existing
  material-interval attachment publication set.

There was also a genuine coverage defect: two independent elastic impacts can
redirect initially stationary bodies into a collision outside both old proxies.
The accepted old library misses that collision in all nine tested distance/speed
layouts. The replacement detects it in all 36 layout, rotation and registration-
order combinations, and matches the independent elastic scattering solution,
total momentum and kinetic energy within relative `1e-4`. The old library was
compiled with its own matching headers for this comparison. The permanent test
is `TestRedirectedFutures` in `tests/Mk2PhysicsTests.cpp`.

The diagnostic four-car debris fixture recorded 112,259 root requests over 768
pairs in one expensive step, with 237 same-time events. This is many repeated
small searches, not one unusually expensive geometric intersection. Many
trajectory changes are real; the report does not classify every repeat as waste.

## Authoritative implementation

`src/physics/WorldMk2Continuous.cpp` owns chronological scheduling and trajectory
publication. Each primitive pair owns a full remaining query horizon, its two
trajectory revisions and a versioned pending event. Only a change to a participant
invalidates it. Unchanged negative observations are retained only for that exact
queried horizon and those trajectories; they are not promoted into certificates
for different intervals.

A deterministic heap orders events by time and canonical primitive key. Incident
pair lists identify actual dependencies. The existing world index remains authoritative wherever its fat proxy
contains the actual remaining trajectory. A transient `DynamicAabbTree` contains
only dynamic/kinematic futures that escape that enclosure. All changed ledgers
and delta proxies are published before new-pair discovery and conservative
sphere-slide certification. This avoids duplicating the entire world index. The old mover-wide scheduling implementation is removed.

First arrivals retain the existing sequential impact law, including elastic
impulse propagation. When a pair closes again at the same instant,
`WorldMk2ArrivalBatch.inl` discovers its connected contact set and reuses MK2's
existing row/block kernels for compression, restitution and secondary
compression. A static floor is a boundary, not a bridge between unrelated
assemblies. Actual contact geometry takes precedence; proximity margins remain
available for just-before-touch numerical roots. Energy checks and frictionless
retry remain. Finite plastic joints are not silently converted into rigid welds.

Prepared-row ownership follows the two actual trajectory revisions. Conservative
bounds now handle eligible offset identity-child compound boxes and unions of
existing motion leaves. Unsupported geometry/integrators or excessive preparation
use the previous conservative enclosure and full CCD; they do not lose collision
coverage to a preparation budget.

Wake publication first examines authoritative live state. If every live body is
already awake with zero quiet time, there is nothing to publish and no graph is
built. Otherwise existing immediate propagation remains, including static-root
wake, joint connectivity and sleeping participants. The material interval reuses
its existing bounded publication set for both contacts and attachments; overflow
keeps ordinary propagation. There is no new persistent wake cache.

Material projection also rejects individual surface samples outside a certified
native primitive enclosure. It recomputes each current sample after preceding
projections, keeps the existing thickness/roundoff allowance and never changes
sample order. Cooked, terrain, capsule and recovery paths retain full queries.
The measured rigid-contact phase fell from 17.044 to 13.146 ms in the first
same-binary crash experiment. All 8,100 native step hashes match the new-frontier
reference after this guard and the delta-index consolidation; this is not a
claim of equivalence to the retired scheduler.

No scene/save format or JudasJS API is changed. The rigid-motion integrator,
physical geometry, structural resolution, panel material settings, wheel and
powertrain algorithms, four-distinct-arrival body limit and 256-arrival step
limit remain. The benchmark observes the existing frame-delta clamp and
accumulator discard; neither policy is changed to disguise a hitch.

## Alternatives and negative experiments

- Solving every first arrival as a connected group failed the analytic elastic
  cradle control. It was rejected, not excused as an insignificant wreck change.
- Tight bounds under the old mover scheduler changed the physical-state hash.
  A pair-revision-only row fix was insufficient: the first divergence was about
  0.58 mm. Those attempts and measurements remain preserved.
- A pair-owned proof made tight versus broad enclosures produce the same native
  crash hash, demonstrating that geometric culling no longer owns the episode
  origin. This justified replacing the old scheduler.
- Deterministic read-only query batches were tested with one, two and four lanes,
  private query scratch and owner-ordered publication. All retained the same
  native 900-step physical hash. They did not demonstrate a compelling net
  whole-frame win, so the additional worker branch was removed. Existing
  independent-manifold workers and resource jobs remain.
- A synthetic eight-body energy control failed on both the baseline and proof;
  it was retained as an inconclusive probe, not counted as a regression success.
- Preparing rigid reaction mass/inertia once per projection retained all 8,100
  native step hashes, but changed the actual contact phase from 16.581 to
  16.548 ms: no meaningful benefit. The additional preparation was removed.
- Explicit GL orphan/subdata uploads were tested in three alternating pairs.
  Median whole-frame p99 changed from 10.418 to 10.380 ms and median maximum
  from 102.817 to 102.480 ms, below 0.4%. The change was removed. Renderer
  modifications in this checkpoint only distinguish CPU topology validation
  from context-thread GL submission; they do not claim GPU execution time.
- The optional GPU/whole-physics replacement was not pursued: measured costs
  established avoidable scheduling work before justifying transfers, readbacks
  or a new authoritative solver.

[Jolt's architecture](https://jrouwe.github.io/JoltPhysics/) demonstrates
island-based parallel work, but its documented linear-cast motion-clamping policy
is not a drop-in replacement for Judas's rotational/remainder contracts.
[PhysX's CCD documentation](https://nvidia-omniverse.github.io/PhysX/physx/5.1.2/docs/AdvancedCollisionDetection.html)
distinguishes swept and speculative CCD, finite passes and dropped time; adopting
those policies silently would change the requested coverage.
[Box2D's island analysis](https://box2d.org/posts/2023/10/simulation-islands/)
explains static-body separation and why a single dense connected island can
remain a bottleneck despite more workers. It is an architectural reference, not
a claim that 2D physics directly qualifies Judas's 3D contacts.

## Controlled proof measurements — not final qualification

Hardware: Ryzen 5 5600, GTX 970 / NVIDIA 580.178.04, 8 GiB RAM, Kubuntu. Real
desktop OpenGL; 1280x720, VSync off, uncapped, unchanged rendering and physical
settings. Whole-frame runs use the actual application accumulator and presentation.
CPU/GPU and nested CPU durations overlap and are never added as frame time.

The latest corrected private Challenger, Shelby, Blazer and Ural yard is the
primary four-way integration input. One real prelaunch cold save was restored
for both algorithms; all four launch masses and impulse deltas match exactly.
The original yard retains selection, orbit, launch speeds and varied targets.
The separate benchmark moves targets aside and launches all four into a junction.
Private assets and all derived projects remain outside Git and public SDKs.

| 20-second proof | p50 / p95 / p99 frame ms | Worst frame ms | Longest >16.67 ms sequence | Simulation time lost |
| --- | --- | ---: | --- | ---: |
| Old mover scheduler | 2.166 / 9.436 / 13.509 | 695.406 | 10 frames / 1335.782 ms | 644.928 ms |
| Pair frontier, serial queries | 1.980 / 8.945 / 10.391 | 103.883 | 10 frames / 469.498 ms | 0 |
| Pair frontier, two query lanes | 1.973 / 8.898 / 10.409 | 103.381 | 9 frames / 448.151 ms | 0 |
| Pair frontier, four query lanes | 1.962 / 9.288 / 10.551 | 122.113 | 10 frames / 537.911 ms | 0 |

These are single controlled trials, not confidence intervals or final qualified
performance claims. The old time-loss total includes 199.484 ms accumulator
discard plus 445.444 ms from the earlier incoming-delta clamp. Older captures
without the latter field cannot certify zero total loss.

The old cold-run maximum physics step was 160.353 ms. The four-lane proof's
maximum was 25.419 ms. Its worst presented frame contained six catch-up steps:
CCD took 19.469 ms in total, material integration 42.267 ms and shadow submission
24.035 ms. These inclusive scopes must not be summed. The bottleneck has shifted;
this does not establish that every severe crash meets 60 FPS.

A separate 900-step native debris experiment compared equivalent serial and
worker execution: serial p50/p95/p99/max 7.307/9.322/11.238/16.328 ms; four lanes
7.388/9.450/11.004/15.832 ms. Native geometry requests were 266,238 versus 266,228;
all configurations ended with the same `51d1707e...` state hash. That small timing
difference did not justify retaining the worker implementation. This native
fixture is not the mixed-model, genuine four-way whole-game workload.

## Final matched desktop measurements

The following are three 20-second captures per revision, in alternating order,
from the same genuine cold prelaunch snapshot and latest corrected private
models. Both use a recorded ten-second loading exclusion. An initial two-second
warm-up captured a 4,204 ms cold world rebuild before gameplay; that invalid
steady-state trial, its 4.84 seconds of lost time and the subsequent unchanged
physical states are preserved, not relabelled as collision work. Final captures
reject any measured `World build` phase. No compile, test or other benchmark
ran concurrently.

| Whole-game measure | Old scheduler | Replacement |
| --- | ---: | ---: |
| Median average FPS | 232.80 | 285.84 |
| Median p50 / p95 / p99 frame ms | 2.206 / 9.802 / 12.915 | 1.985 / 8.829 / 10.309 |
| Median 1% / 0.1% low FPS | 24.63 / 4.01 | 61.27 / 16.55 |
| Worst frame, trials 0 / 1 / 2, ms | 712.402 / 723.726 / 628.762 | 100.715 / 98.389 / 97.352 |
| Accumulator discard per trial, ms | 206.821 / 212.253 / 298.959 | 0 / 0 / 0 |
| Incoming-delta clamp loss | Not instrumented in this frozen binary | 0 / 0 / 0 |

The new longest >16.67 ms sequences are 10/9/9 frames lasting
454.173/409.552/422.419 ms. The old sequences last approximately
1.35 seconds. New >100 ms frame counts are 1/0/0. All three runs contain 1,199
actual simulation steps; the first old run contains 1,161. Old total time loss
cannot be certified from an unobserved clamp. The separately instrumented old
trial above establishes that this loss is real and not limited to the accumulator.
The user's earlier 320 ms observation and these cold-start 629–724 ms old
measurements use different protocols; they are not interchangeable baselines.

The first new trial's maximum physics step is 26.711 ms; trials 1/2 peak at
26.225/26.155 ms. Its worst presented frame performs four catch-up steps totalling
73.107 ms, plus 25.632 ms world render submission. Within those inclusive scopes,
CCD is 23.175 ms, local materials 26.435 ms, rigid/terrain material contacts
13.310 ms, skin mapping 6.912 ms and geometry preparation/upload 16.684 ms. The
last splits into 13.818 ms context-thread GL submission and 2.801 ms CPU topology
validation. Nested scopes are not additive and GL submission is not GPU time.

The diagnostic old cold collision made 1,055,096 geometry requests in its worst
frame; the new trial makes 14,676. Repeated same-time arrivals fall from 1,820 to
93 in those diagnostic frames, and actual wake graph traversals from 2,978 to
zero, with 1,714 valid no-op wake publications. Those counts come from separately
identified diagnostic executables, not the uninstrumented old timing binary.
The new frame retains six body-limit holds under the unchanged four-distinct-
arrival policy, but no response or step-limit holds. This is not unlimited CCD.

### Native physical workloads

These are matched 900-step headless runs with the same model, settings, inputs
and profiler depth. They measure simulation, not presented FPS. The four-car
native case contains two colliding cars and two unstruck cars; it is not the
mixed-model four-way junction. No damaged geometry or resolution was reduced.

| Workload | Old p50 / p95 / p99 / max ms | New p50 / p95 / p99 / max ms |
| --- | --- | --- |
| idle-one | 1.062 / 1.073 / 1.100 / 8.937 | 1.030 / 1.168 / 1.355 / 1.936 |
| drive-one | 1.151 / 1.203 / 1.219 / 1.577 | 1.116 / 1.367 / 1.569 / 1.768 |
| drive-four | 5.276 / 5.454 / 5.518 / 6.666 | 4.419 / 4.602 / 4.660 / 5.276 |
| frontal-15 | 1.205 / 1.608 / 2.178 / 23.472 | 1.155 / 1.442 / 1.670 / 4.313 |
| frontal-30 | 0.768 / 1.586 / 2.391 / 56.053 | 1.024 / 1.417 / 3.054 / 13.157 |
| offset-20 | 1.182 / 1.296 / 2.248 / 13.286 | 1.172 / 1.447 / 2.020 / 3.703 |
| cars-four | 6.199 / 8.201 / 9.714 / 102.389 | 6.028 / 7.569 / 9.569 / 15.422 |
| repeat-one | 1.264 / 3.534 / 18.191 / 59.275 | 1.090 / 1.850 / 2.730 / 10.438 |
| debris-four | 7.026 / 9.505 / 85.097 / 143.979 | 6.913 / 8.944 / 10.588 / 15.284 |

Ordinary idle/driving retain their original accepted final hashes. The six impact
hashes change because the response/coverage defects are corrected. The original
hashes, complete traces, damage and attachment outcomes remain preserved; the
result is not certified merely by visual resemblance of the final wreck.
The full new reference is additionally checked at every one of 8,100 steps for
implementation-equivalent refinements. Frontal-30 and four-car debris saves
restore the exact durable deformation/wheel/rotor state in fresh processes and
continue for 120 steps, including independently detached panels.

### Other whole-game workloads and hardware use

| Matched workload, three trials | Old median p99 ms | New median p99 ms | New worst frame across trials ms |
| --- | ---: | ---: | ---: |
| Vehicle driving | 2.153 | 2.201 | 5.577 |
| Fleet driving | 3.284 | 3.314 | 12.222 |
| Combined terrain / 32 supported actors / 5,000 GPU droplets | 5.462 | 5.743 | 633.779 |

All pass the pre-existing relative p99 gate; none is claimed faster from these
small differences. Combined runtime checks confirm all 32 supported actors,
5,000 live GPU droplets and actual particle contacts. Frozen workload manifests,
controls, rendering policies and hardware identities match the prior valid
whole-engine captures. Vehicle/fleet worst frames remain within 60 FPS. Their
p99 is within 120 FPS; the fleet maximum does not meet 120 FPS.

**The combined worst frame remains an unresolved presentation outlier.** Trial 0
blocks for 631.933 ms in `Swap present wait`, performs no physics step in that
frame, and loses 511.630 ms of subsequent simulation time through the existing
clamp/discard policies. Trials 1/2 peak at 10.481/8.374 ms with no discarded time.
This does not establish a driver fault or absolve the engine. It is retained as
a whole-frame limitation, not removed by the relative p99 pass, and was not
addressed through an unrelated renderer rewrite in this collision milestone.

The first cold four-way trial uses 0.961 CPU-core equivalents before and 0.977
after; the owner thread dominates and steady resource workers are idle. The
one/two/four query-lane experiments above explain why extra threading was not
retained. No claim is made that all six cores are effectively used by dense CCD.
Whole-device GPU utilisation including warm-up/desktop averages 55.6%/61.4%;
peaks reach 100%. GPU whole-frame intervals average 4.303/3.508 ms with p99
10.222/6.452 ms. These are command intervals and may include gaps.

Calibrated same-frame completion (maximum of CPU and GPU completion, **not their
sum**) improves p99 from 16.043 to 13.540 ms in that pair. Vehicle/fleet samples
reach 3.274/4.160 ms p99, but combined remains around 7.7–7.9 ms. The five-ms
combined-engine goal is therefore not achieved. Completion excludes compositor
and scanout; it is not inverted into FPS.

Native process high-water RSS is approximately 1,001–1,002 MiB in both versions.
The private four-way sample is stable during gameplay but rises from roughly
2,953 to 3,227 MiB peak RSS; the difference is already present before impact and
is not attributed to pair records. The measured pair/queue payload peaks around
9.3 KiB in the quoted worst frame, excluding map/vector allocator overhead.
Whole-device VRAM maxima are 2,334/2,387 MiB including desktop/warm-up. This pass
does not claim a general memory reduction. Hardware PMU profiling was unavailable
(`perf_event_paranoid=4`); internal scopes, OS thread samples and actual desktop
NVIDIA timestamp/telemetry measurements provide the stated evidence.

## Qualification and performance contract

The existing nine accepted hashes remain immutable references. Wake-only
publication changes pass all nine exactly. The replacement changes collision
response and ordering, so it must not be described as nine-hash equivalent.
The old planar suite's exact 3D Stage 0 hash also changes deliberately:
`f67106f8392d2826` becomes `cba95a924ae713f1`. Matched 300-step traces first
separate at step 31, where the old scheduler makes 22 arrivals/two holds and the
new one eight/one. The initial positional change is 2.8 micrometres; maximum
sampled position/velocity differences over the complete settling trajectory are
24.413 mm and 0.712 m/s. These are measured differences, not invented equivalence
tolerances. The exact current 3D control remains pinned so subsequent planar
work cannot silently alter it. All 52 actual planar checks pass, including
independent support, mobility, elastic energy/momentum, rotating-wall coverage,
kinematic support, persistence and frame covariance.

Its qualification includes analytic trajectories, energy/momentum, actual
contact observations, sleeping/waking, deterministic repeats, damage and
attachment outcomes, physical persistence and the original vehicle references.

Current completed integrated checks: MK2 physics including the new zero-time
cycle regression; 31,404 rotation checks; 127 diagnostics checks; genuine cold
rotation writer (5 checks) and reader (3,139 checks); 156 deformable checks.
The initial full native run passed 187/188; the only failed gate was the old
3D hash in the planar suite described above. Its updated exact control and all
52 physical checks pass. The final clean-commit native/CI run and packaged SDK
workflow results must be read with the matching handover receipt; pending gates
are never treated as passes.

`tools/benchmarks/run_whole_engine.py` records binary/project identity, prototype
controls where applicable, frame/step distributions, GPU telemetry, memory,
hitch sequences and both forms of discarded time. Detailed pair tracing is
bounded and diagnostic; its overhead is not a headline benchmark. It reattaches
after actual world replacement. The ordinary CI physics suite exercises the
procedural response-cycle regression without private vehicle assets.

120 FPS / 8.33 ms, minimum 60 FPS / 16.67 ms in defined supported workloads and
5 ms combined critical-path work remain engineering targets. A relative p99 pass
or high average FPS does not certify severe-impact latency. The remaining
material/render and catch-up hitches are explicit performance limitations until
measured qualification establishes otherwise.

Local evidence root: `.cache/mk2-collision-architecture/`, including immutable
baseline executables, failed proofs, exact-source/binary snapshots, cold-start
input hashes, raw frame JSONL and native logs. It is development evidence, not
an SDK dependency or a public source of downloaded models.

## Human handover and reproduction

The exact handover commit, SDK build identity, completed CI run IDs and final
local gate results are recorded together in the local qualification receipt
`.cache/mk2-collision-architecture/FINAL_QUALIFICATION.json`. The SDK's own
`BUILD_INFO.json` must identify that same Experimental commit and a clean source.
The frozen pre-change executables and complete raw comparisons remain under the
evidence root; no private model data is included in this document or public SDK.

From the repository root, final package verification and the native gate are:

```sh
python3 build/linux-sdk/tools/package_sdk.py verify build/linux-sdk
ctest --test-dir build -L prealpha --output-on-failure
```

Local private crash yard, using the packaged runtime rather than an old proof:

```sh
/home/conner/Documents/GitHub/Project-Judas/build/linux-sdk/judas \
  /home/conner/Documents/GitHub/Project-Judas/.cache/mk2-collision-architecture/private-current-yard/Crash_Yard.judasproj
```

The editable authoring entry is the same project passed to the matching
`build/linux-sdk/judas_editor`. It retains the corrected Challenger, Shelby,
Blazer and Ural, vehicle selection, stable orbit camera, varied wall/pole/corner
lanes and multiple launch speeds. Immutable reference binaries are not active
playable launch paths.

Human checks:

1. Select all four vehicles with **1–4**, inspect materials/wheels/interiors;
   hold **RMB + mouse** to orbit, use wheel zoom and **C** to return behind.
2. Drive/reverse with **W/S**, steer **A/D**, brake with **Space**. Try light
   impacts before the severe cases; inspect suspension and wheel mounts.
3. **X** cycles physical launch speeds; **Enter** launches the selected assembly.
   Test barrier, offset pole, thin wall and inside corner. Damage is physical,
   never a scripted animation. Reset with **R** between matched trials.
4. Repeat impacts; inspect permanent local bending, progressive hinge/latch
   failure, independent bonnet/debris motion and continued driving.
5. Use **K/L** to save/load, then exit the process and restore damaged vehicles.
   Confirm plastic deformation, detached parts and surviving connections.
6. For the deterministic mixed-model collision use `Frozen_Four_Way.judasproj`
   in the same private directory, with the evidence-root `frozen-xdg` data home;
   **F4** releases the loaded snapshot and launches all four. The normal yard
   is the default human gameplay project, not this measurement protocol.
7. Judge the remaining crash hitches explicitly. Functional/relative-regression
   success does not accept their 60 FPS latency failure on the operator's behalf.

Permanent automated guards include actual-contact continuation, redirected
future collision/elastic scattering, original rotating/thin-wall/near-miss
controls, coupled contacts and energy, sleeping/waking, bounded wake overflow,
conservative motion enclosures, current 3D/planar trajectories and cold state.
The whole-engine relative comparator also rejects changed loading policies;
frame metrics retain hitch runs and both discard paths. Future collision work
must reuse the genuine junction and public combined workload, not only native
subsystem timings.

Windows, Android and browser runtime qualification have not been performed for
this replacement. Desktop OpenGL and available CI portability gates retain only
the coverage actually executed. Dense connected collision queries and state
publication remain substantially owner-thread work. Finite arrival policies,
residual dissipation/phase tolerances and the unresolved eight-body diagnostic
remain scoped limitations; there is no universal 60/120 FPS guarantee.
