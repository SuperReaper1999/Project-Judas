# Whole-engine performance engineering

**Status: measured optimisation pass; human review required. This is not universal performance qualification. Remaining absolute-budget failures are explicit.**
Experimental baseline: `f05577b95c84389ac5628936168456e669b0741e`.
Main, the Advanced Fluids lock, the RPG SDK and operator terrain edits are protected.

## Product budgets

120 FPS / 8.33 ms is the gameplay target; 60 FPS / 16.67 ms is the minimum for
defined supported workloads. The aggressive combined-engine critical-path goal
is 5 ms. These are whole-frame goals, not independent per-subsystem allowances.
A p99 pass does not certify that exceptional collisions meet the minimum.
Report exceptional hitch counts/durations and any discarded simulation backlog.

Reference: Ryzen 5 5600 (6 cores / 12 logical CPUs), GTX 970 4 GiB, 8 GiB RAM,
Kubuntu. Actual desktop GL 4.6 NVIDIA 580.178.04 verified for this audit. Restricted
software-rendering results must not be presented as hardware performance.

## Current pipeline and ownership

The shipping Application loop polls input, pumps owner-thread resource
publication, runs InteractivePlay (UI scripts/input, 60 Hz accumulated fixed
steps, presentation scripts/audio, world views and overlays), swaps, then
advances scene/save/stream services. Fixed steps retain the existing catch-up
policy; benchmark reports must identify cap activation and discarded time.

Fixed-step work includes script callbacks, navigation/weather/characters,
Advanced Fluids only under its existing policy, MK2 with vehicle/deformable
coupling, animation/ragdolls/sockets, particles and event/network publication.
Authoritative mutation remains ordered. Resources use a shared background pool;
MK2 already borrows it for eligible independent initial primitive manifolds.
The existing path does not parallelise every compound-body CCD search.

Renderer V2 already includes instancing, view blocks, persistent stream pages,
light tiling, portable fallbacks, shadow reuse, delayed timestamps and a
per-presentation transform/skin cache. These are not missing features to rebuild.
Particles already have GPU simulation and shared compatible collision snapshots;
Terrain V2 already prepares geometry/navigation on workers and publishes through
owner boundaries. More CPU utilisation is not by itself an improvement.

## Measurement foundation

`judas_whole_engine_benchmark` runs **Application.cpp itself**, with real wall-clock
simulation, rendering and swap. It disables VSync and records the achieved swap
interval. It does not substitute a synthetic physics clock. Bounded profiler
history remains available; JSONL frame intervals cover the full measured run.

`tools/benchmarks/run_whole_engine.py` records commands, binary/project hashes,
per-thread process CPU samples, host-core counters and NVIDIA device telemetry.
GPU utilisation includes the desktop; it is not a per-process utilisation oracle.
The preview capture occurs before measurement. Loading-inclusive exploratory
runs are preserved separately from resource-quiet steady-state captures.

`JUDAS_PROFILE_DEPTH=1..64` optionally limits recorded nested scopes while
preserving their enclosing elapsed times. Default depth and event/memory limits
are unchanged. The benchmark uses depth 4 and reports truncation/drop counters;
detailed overflowed captures cannot prove complete phase attribution. This
changes diagnostic recording only, not simulation iterations or work coverage.

GPU records retain source-frame IDs, CPU submission timestamps and raw device
start/end timestamps. Two bracketed CPU/device-clock calibration samples support
timeline alignment, with explicit uncertainty. Existing delayed availability
polling remains non-blocking. Calibration/readback happens outside measurement.
CPU and GPU durations overlap: never add them or nested scopes as frame time.

Report average FPS from total frame duration; nearest-rank p50/p95/p99;
1%/0.1% lows from the mean of the worst respective frame fractions; maximum,
consecutive hitch lengths/durations, memory, jobs and cap/discard events.
Profiler-off runs quantify instrumentation overhead. Matched hardware, inputs,
resolution, quality and content are required for comparisons.

## Historical initial findings (superseded by the measured results below)

Short pilot runs establish that lightweight existing projects can run quickly
uncapped; they do not qualify sustained or combined gameplay. The first long
captures exposed asset uploads beyond a fixed 2-second warm-up and profiler
overflow during heavy physics; those are retained as measurement failures rather
than used to certify steady-state attribution. Resource-quiet captures correct
those problems.

Measured populated-rendering and crowd runs saturate one CPU thread while
resource workers are idle; device utilisation is below full saturation. The
parked four-model yard also performs repeated wake traversals. These findings
justify targeted experiments, not an immediate scheduler or physics replacement.
The final measured tables below supersede these initial findings. They explicitly retain unsupported budget cases.

## Controlled experiments so far

The shadow-uniform-location cache was **rejected and reverted**. Three interleaved
populated-rendering pairs and three crowd pairs, plus profiler-off controls,
showed no meaningful net whole-frame improvement. Its deterministic populated
scene capture was pixel-identical; correctness alone did not justify retention.

User-space CPU sampling is now available after the operator temporarily set
`kernel.perf_event_paranoid=2`. Recording explicitly excludes kernel call chains
and disables build-ID-cache writes. One diagnostic crowd capture attributed about
48% of samples to the executable and 34% to NVIDIA's driver, with the rest mainly
C/C++ libraries. A saturated main thread is therefore not synonymous with useful
engine computation. These instrumented samples are diagnostic, not FPS evidence.

Sampling found repeated box-frame preparation in capsule projection. The retained
small correction prepares rotation/transpose/converted box inputs once per query
instead of eight times; all eight original projection iterations remain. Original
procedural controls compare 100,000 cases bit-for-bit against the previous call
sequence. Motor controls pass 32/32, character publication 144/144, contact
geometry 823/823, and profiler controls 29/29.

Three interleaved 10-second whole-game pairs on the same 129-character project,
1280x720, uncapped, with matching resource-quiet readiness, gave these medians:

| Metric | Original | Prepared capsule query |
|---|---:|---:|
| Average FPS | 112.12 | 114.64 |
| p50 frame ms | 10.94 | 10.62 |
| p99 frame ms | 12.04 | 11.69 |

This is a modest real whole-frame improvement, not achievement of 120 FPS or the
5 ms critical-path goal. The workload, iterations, collision coverage and render
quality were not reduced. This was an intermediate experiment; final results
and qualification are recorded below. Raw trials and exact controls are under
`.cache/whole-engine-performance/` and are not public model assets.

## Animation preparation experiment (historical checkpoint)

The runtime computed the final joint hierarchy twice per animation update:
once for the skin palette, then again for world-space joint motion. Sharing that
evaluation preserves the exact matrix operations and actor publication order.
The simplified serial implementation is undergoing affected regression checks.

A bounded helper on the existing worker pool was tested and **removed**. Pure
matrix work fell from about 0.79 to 0.44 ms per fixed step with four lanes, but
three interleaved 30-second whole-application pairs gave these median results:

| Same shared calculation | Average FPS | p95 ms | p99 ms |
|---|---:|---:|---:|
| One lane | 121.11 | 10.38 | 11.14 |
| Four lanes | 120.10 | 10.38 | 11.54 |

The worker version therefore did not establish a net whole-game improvement.
Its runtime integration and helper API were removed, rather than retained for
CPU-utilisation figures. Raw captures, tested source patch and negative result
remain in ignored audit evidence. These numbers are experiment results, not
qualification of the final simplified implementation or a 120-FPS p99 pass.

## Reproduction and future regression checks

`tools/benchmarks/prepare_workloads.py --revision COMMIT --out NEW_DIRECTORY`
extracts tracked benchmark projects at an exact revision, without copying local
terrain edits or private models. It creates the populated renderer case, a
129-performer crowd and the authored 100,000-particle stress case. The normal
terrain copy is explicitly the small workshop, not a terrain-heavy workload.
`WORKLOADS.json` records file hashes; pass it as `--workload-manifest` to the runner
to reject modified inputs before measurement. Private crash projects and the
large-terrain fixture remain separate declared workloads.

`tools/benchmarks/report_capture.py CAPTURE_DIRECTORY` reports actual frame
statistics, per-thread CPU use, memory, phase observations and GPU command
intervals. Calibrated CPU/GPU traces are exported where available. The reports
explicitly prohibit summing nested scopes or overlapping processors.

`performance/comparison.py` requires three matched captures per revision and
checks the median p99 against a declared regression allowance (default 10%). It
rejects mismatched content, resolution, hardware, driver, graphics policy,
profiling policy, short captures and dropped diagnostic records. Absolute frame
budgets and exceptional worst frames are separate from regression acceptance.
The metric and comparison logic runs in focused CI; hardware performance results
still require actual reference-desktop execution and are not certified by a
generic CI runner. Subsequent broader coverage and SDK/CI records are described
in the final qualification section.

## Initial reference captures and coverage limits

These are 20-second resource-quiet standalone captures on the desktop at
1280x720, uncapped, not a claim of complete workload qualification:

| Workload | Average FPS | p99 ms | Worst ms | Process RSS peak MiB |
|---|---:|---:|---:|---:|
| 129 performers with character motors | 112.2 | 11.41 | 21.17 | 137.8 |
| Populated renderer lab, 1,096 objects | 194.2 | 5.52 | 7.97 | 184.1 |
| Four private vehicle models, parked | 292.7 | 9.07 | 22.95 | 3200.9 |
| Selected Challenger launch in that yard | 257.8 | 10.63 | 168.55 | 3198.5 |
| Small terrain workshop traversal | 2306.0 | 0.99 | 5.29 | 157.3 |
| Ordinary particle workshop | 1275.2 | 1.51 | 2.61 | 132.5 |

The ordinary particle workshop is **not** the 100,000-particle stress scene.
The small terrain workshop is **not** a representative large streaming world.
The selected-car launch is **not** the four-way junction or debris qualification.
Average FPS conceals the severe crash failure: that capture contains a 168.55 ms
frame and six consecutive frames over 16.67 ms, totalling about 461 ms, including
catch-up capping and discarded simulation time. It fails the exceptional minimum
budget even though its p99 and average look much better.

The crowd used essentially one CPU-core equivalent and no resource-worker time
after loading. Its average render submission was 6.12 ms per rendered frame;
character motors averaged 1.82 ms per rendered frame. These observations nest
inside larger scopes and must not be summed. The parked yard also used roughly
one core equivalent. This motivates CPU-side preparation/submission analysis;
it does not prove that the GPU or all existing worker paths are unused.

Process RSS includes driver mappings and diagnostics. Device-wide sampled VRAM
in the yard reached about 2445 MiB, including the desktop; the evidence does not
establish VRAM exhaustion. No hardware fault is inferred from these costs.

## Material submission: measured critical-path improvement

Inclusive user-space samples attributed about 34% of crowd CPU cycles to
`BindMaterial`, including about 13% to uniform-name lookup and its string work.
Those percentages overlap and must not be added. Allocation-free `string_view`
lookup in the existing cache provided a modest improvement in matched runs.

The next candidate describes material-owned uniform writes as one bounded
packet. Renderer owns two last-published states, one for its main shader and one
for its shadow shader. Only exact program/name/type/value matches skip writes.
Commands carry their own names; there are no borrowed resource references,
per-object history, approximate comparisons or additional GPU allocations.
Texture/sampler bindings and raster state are always applied. Frame start,
shutdown and terrain's shared-uniform writer explicitly invalidate state.
The command packet itself is both the comparison key and the publication source,
so new material fields cannot accidentally omit a separate dirty-key list.

Three interleaved 10-second pairs, against allocation-free lookup with the same
assets, resolution and settings, gave these median whole-application results:

| Workload | Before FPS | After FPS | Before p99 ms | After p99 ms |
|---|---:|---:|---:|---:|
| 129 performers and motors | 124.74 | 180.81 | 10.34 | 8.39 |
| 1,096-object renderer lab | 198.36 | 199.18 | 5.51 | 5.15 |

The crowd improves substantially as a complete application; its p99 still misses
8.33 ms. The already-batched renderer lab gains little throughput. This is not
a universal FPS guarantee or full performance qualification.

GPU controls pass under modern and portable policies: 12 publication/colour/
texture/frame-restoration checks and 44 material checks including baked HDR
environment rotation/reflections. Scoped HDR pass checks pass 29/29; terrain
material checks pass 86/86 with zero analytic-normal pixel difference. These are
NVIDIA desktop results; portable policy is not a claim of testing another GPU.
This was an intermediate checkpoint. Consult the final qualification record below for subsequent evidence.

## Capsule projection: exact fixed-point work elimination

The closest segment/box query retains its eight-iteration result and ceiling.
It now stops only when the segment point is unchanged **bit for bit**, including
signed zero. Since the next iteration is the same deterministic function of that
same input, both output points would repeat identically. There is no convergence
epsilon, reduced coverage, changed precision or shortened unconverged search.

The retained original sequence still executes all eight unprepared projections
in 100,020 exact-output controls, including added floor/degenerate/signed-zero
cases; all pass. Character motor 32/32, publication 144/144 and geometry 823/823
also pass. Three interleaved crowd pairs against material-state reuse alone gave
179.47 to 190.05 average FPS and 8.40 to 7.28 ms p99. Median p50 changed from
4.23 to 4.58 ms; median p95 improved from 8.27 to 7.09 ms. Fixed-step/render
cadence produces a mixed frame distribution; report all of these metrics rather
than claiming every percentile improves. This case now meets the p99 120-FPS
budget, while its observed maximum still slightly exceeds 8.33 ms.

These are incremental experiment results. Final matched original/current runs,
broader workloads and qualification remain required.

## Coarse independent full-body IK: useful worker batching

The rejected matrix-only worker experiment is not the final word on animation
parallelism. Profiling the actual crowd plus 16 four-contact IK actors identified
about 8 ms of serial pose preparation per fixed boundary. The retained candidate
batches **pure full-body solves**, not authoritative world mutation. The owner
samples animation, converts world targets and prepares mappings; workers read
immutable inputs and produce separate results. The owner publishes in original
order before physics. Parent/socket consumers flush preceding work first.
Batches contain at most 32 actors and borrow at most three existing workers;
the owner participates. Queued helpers are cancelled and running helpers joined
before inputs expire, including exceptions and nested single-worker calls.
No new pool, asynchronous pose latency or worker-ordered physics was introduced.
`JUDAS_ANIMATION_LANES=1|2|3|4` is a bounded diagnostic override; default is four.

Actual NVIDIA whole-application captures, 1280x720, VSync off, same frozen
129-character/16-IK project, three interleaved 30-second runs per lane count:

| Lanes including owner | Median average FPS | p50 ms | p95 ms | p99 ms | Median run maximum ms |
|---|---:|---:|---:|---:|---:|
| 1 | 78.87 | 15.03 | 15.79 | 17.26 | 53.31 |
| 2 | 128.21 | 4.34 | 11.91 | 12.50 | 21.37 |
| 4 | 150.63 | 4.44 | 10.44 | 11.00 | 15.11 |

The one-lane runs had differing unrelated host activity (about 0.15–0.97 core
equivalents), which is retained in the evidence. Even the quietest one-lane
run was 83.57 FPS versus 149.65–151.20 with four lanes. This supports a useful
whole-game gain, not an exact universal percentage. The four-lane p99 meets
60 FPS but **does not meet 120 FPS**. Its worst observed frame was 19.52 ms,
including a three-frame 54.59 ms hitch sequence in the first run. No simulation
backlog was discarded and no profiler events were lost.

Process CPU use rose from about 1.00 to 1.43 core equivalents, while device-wide
GPU activity including warmup rose from about 41–45% to 77–79%. Workers recorded
13.55–13.65 seconds inside their solve callables per 30-second capture. This
is useful overlap, not a claim that all cores are continuously busy. Peak
sampled RSS rose from about 140 MiB to 157–158 MiB. The subsequent 300 s memory
qualification is reported below. Source output packet storage is bounded; platform allocator
and active worker overhead are included in those measured resident figures.

New native controls pass **135/135**: one/two/four-lane exact pose bits, clocks,
residuals, rejected stale mappings, parent/socket dependencies, nested batches,
exceptions and pool shutdown. Existing animation workflow passes 60/60 and
multi-target IK 38/38. Worker, skeletal, pose and physical-animation regressions also pass; separate-process
animation cold write/read pass 8/8 each. Subsequent whole-engine/SDK/editor evidence is recorded below. These results do not claim human acceptance.
Raw lane trials, timelines and decision data: `.cache/whole-engine-performance/ik-batch-experiment/`.

## Workload validity corrections

The first broad exploration is evidence, not a complete benchmark qualification.
Its 30-second public-vehicle forward-only runs left the authored driving area;
their low late-run costs must not certify normal supported driving. A bounded
project-owned drive/brake/reverse cycle now keeps each leg on the existing map,
with position/support telemetry and final screenshots required for review.
Likewise, historical four-way crash scripts auto-launched during warmup. A
private disposable copy moves those same 20 m/s impulses to the first measured
drive input; original project/model files remain unchanged and private.

The native capture now rejects resource failures and sampled backgrounded frames,
and captures the final presented image after timing. Same-frame completion is
`max(CPU end, calibrated GPU end) - CPU start`, including inherited queueing;
it is never CPU+GPU sum or converted into throughput FPS. Uncapped crowd traces
show roughly two frames of GPU queueing. Neither that latency nor separate
processor durations establishes the 5 ms active critical-work goal by itself.
Capped and profiler-off controls remain part of final qualification.

## Wake scratch experiment: rejected

A genuine four-way presented-frame spike took 319.91 ms, with five fixed steps.
It recorded 13,301 wake calls totalling 48.20 ms and 494,271 CCD geometry requests.
The first low-risk experiment reused six temporary wake buffers while rebuilding
the same graph and traversal every call. It eliminated 60,000 allocations over
10,000 targeted calls, but that control took 19.13 ms before / 19.36 ms after;
allocation counts alone did not establish a latency win.

All nine native crash outcomes exactly match the preserved accepted physical
hashes and their paired reference outputs. Three interleaved repetitions of the
heavy cases produced these median run maxima (native physics step, **not frame**):

| Case | Current retained engine ms | Scratch candidate ms |
|---|---:|---:|
| Severe frontal | 51.464 | 52.124 |
| Four cars | 94.517 | 94.060 |
| Dense debris | 130.537 | 130.566 |

Small average/p50 gains did not meaningfully reduce hitch costs. The scratch
implementation and dedicated allocation test were removed. Its patch, native
source, raw CSVs, exact outcomes and decision remain under
`.cache/whole-engine-performance/wake-scratch-experiment/`. This is a negative
engineering result, not a claimed fix or an invitation to skip wake propagation.

## Ongoing qualification commands

This is an **engine developer/source-checkout workflow**, not a requirement to
install benchmark instrumentation in consumer games. Normal SDK/exported games
remain unchanged. No private model is part of the public workload generator.

```sh
cmake --build build --target judas_whole_engine_benchmark judas_present_capture -j2
python3 tools/benchmarks/prepare_workloads.py --revision BASELINE_COMMIT \
  --out NEW_INPUT_DIRECTORY --large-terrain-tool build/judas_terrain_author \
  --scene-tool build/judas_scene_author
python3 tools/benchmarks/run_matrix.py --baseline OLD_CAPTURE_BINARY \
  --candidate NEW_CAPTURE_BINARY --workloads NEW_INPUT_DIRECTORY --out NEW_RESULTS_DIRECTORY
```

Run captures serially on the actual desktop, without concurrent builds or other
benchmarks. The matrix freezes its executables, verifies the manifest before
launches, alternates order, uses at least three repetitions, and detects relative
p99 regressions. Select affected `--cases` for ordinary follow-ups. The full
matrix covers the lightweight scene, town gameplay, populated rendering,
animation/IK, 100k GPU particles, Basic Water, terrain, vehicles and a combined
terrain/32-character/5000-particle workload. Missing optional prepared inputs
fail explicitly rather than silently skipping coverage.

The report separately retains absolute-budget misses, exceptional worst frames
and discarded physics time. Relative-regression success is **not** acceptance.
Frame-cap policy is part of comparison identity; older millisecond-floor caps
must not be compared as if they were the newer high-resolution pacing policy.
Uncapped runs retain the same no-cap policy. Profiler-off measurements report
unavailable simulation-discard counters as unknown rather than zero.

`observe_editor.py` uses an opt-in Linux SDL presentation interposer to observe
an otherwise unmodified editor on X11, with fresh preferences and a disposable
project. It verifies the requested Edit/Play mode and closes only its launched
window. Native editor screenshots remain the visual reference where compositor
screen readback is unavailable. Editor chrome/viewport resolution differs from
a full-window export: label that limitation rather than claiming GPU equivalence.
The interposer is not linked into the engine or distributed in the SDK.

### Corrected terrain benchmark inputs

The first large-terrain editor timing was rejected after its native screenshot
showed sky. The generator had updated only the runtime native terrain, leaving a
stale small editor preview and an unsimulated player high above it. Current
preparation cooks the complete native/preview product set and places the player
above the bilinear surface. A new native screenshot confirms visible landscape;
old invalid timings remain labelled as such in their evidence directory.
Do not call a closed-source baked-preview measurement active sculpting, nor
claim streaming eviction unless its actual counters demonstrate it.

## Prepared detailed-panel frames

A fresh user-space profile after the earlier optimisations attributed 12.86% of
measured owner-thread samples in the four-model yard to `MapDeformablePanel`.
This was ordinary presentation work, not the dominant severe-impact search.
Many detailed vertices recomputed the same physical triangle's rest/current
basis. Asset preparation now derives shared triangle groups and immutable rest
frames; presentation interpolates each group's physical frame once and retains
the original per-vertex arithmetic. GPU structural skins, collision topology,
physical solving and render-upload policy are unchanged. Derived data is not
serialized and requires no format migration.

The independent original per-vertex oracle passes 38 checks, including raw float
bits for positions/normals/tangents/UVs over 32 motion/origin/degeneracy cases and
eight interpolation values. A 2209-vertex fixture uses 50 prepared groups and
19,968 additional shared resident bytes; its 425,346 cooked bytes are unchanged.
The current deformable suite also passes 152 checks.

Three alternating 20-second NVIDIA desktop whole-application pairs per case,
1280x720, identical private yard and controls; medians of independent trials:

| Workload / metric | Previous retained engine | Prepared panel frames |
|---|---:|---:|
| Parked yard average FPS | 308.71 | 366.71 |
| Parked p50 / p95 / p99 ms | 1.829 / 8.874 / 9.678 | 1.551 / 8.604 / 9.045 |
| Selected frontal impact average FPS | 303.16 | 334.21 |
| Impact p50 / p95 / p99 ms | 1.765 / 9.218 / 10.114 | 1.512 / 9.336 / 9.809 |
| Median impact maximum ms | 166.25 | 166.37 |

This is a useful ordinary whole-game gain, **not a collision-hitch fix**. Parked
maximum across trials increased from 18.20 to 23.22 ms; impact maximum across
trials was 167.17 versus 170.58 ms. The captured launch still discards accumulated
physics time under the existing catch-up policy. No claim of a 60-FPS exceptional
impact guarantee follows from its passing p99. Fixed-clock physical outcomes
are validated separately from wall-clock gameplay, whose grouping changes with
frame scheduling. Raw evidence: `.cache/whole-engine-performance/panel-frame-experiment/`.

### CI fixture-path correction

The first independent-IK checkpoint's engine CI failed because its new test used
relative project paths while the gate correctly ran it from a disposable results
directory. Both channels failed that same fixture setup. The test now receives
an explicit source-root argument and reports a failed project load cleanly. Its
135 physical/output assertions are unchanged. That failed run is retained as
failure evidence; a later completed CI run is required before qualification.

### Input and mixed-scene truth checks

The first `terrain-excavation` timing was rejected: its final HUD still showed
revision 1 and zero excavated chunks. Directly setting physical state before
ordinary input polling loses update-loop pressed edges at `BeginFrame`.
Fixed-loop launch inputs did execute (their crash event logs prove the 25 m/s
impulse), so this does not invalidate independently observed crash controls.
The capture now uses the **existing** queued physical and player-device input
boundary. No production input API or simulation clock was replaced. A real
10-second control completes all cuts: revision 20, eight volume chunks and zero
queued cuts. An attempted external X11 pulse did not repair the controlled game
input and was removed; its negative captures remain labelled.

The initial mixed fixture correctly failed engine validation because a borrowed
emitter's project tag registry was absent. After copying its registry through
normal project authoring, a further physical check exposed actors sliding off
the steep hill and falling below the map. Those cheap captures cannot certify
mixed gameplay. Current preparation cooks a real flat terrain courtyard with
smooth surrounding transitions, places actors on its heightfield and uses an
elevated follow camera. All 32 animated CharacterMotors remain physically
supported and visible; 5,000 GPU particles originate above terrain and record
actual contacts. There are no position/velocity resets, artificial support
bodies, disabled motors or reduced particle counts.

Prepared inputs now publish bounded game-owned state telemetry. Matrix gates
reject an unexcavated or unfinished tunnel, missing actors, lost support,
out-of-world actor heights, missing particles, a wrong particle backend or zero
particle contacts. Seven Python metric/negative-control tests pass. Repeated hardware trials and the clean `fb1e3c57` SDK workflow subsequently passed; final matching-SDK qualification is recorded below.

The first independent-IK CI failure is corrected at `d8d87f8d`; [engine CI
42981229173](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42981229173)
completed successfully for Alpha and Experimental, including their Ubuntu 24.04
SDK checks. Subsequent `fb1e3c57` engine CI [42988727760](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42988727760) also completed successfully. Final particle follow-up qualification is recorded below.

### Counter and memory-accounting refinements

Runtime renderer statistics accumulate while the editor may reset them. World
submission now publishes transfer/material **deltas** after transparent work,
rather than mislabelling lifetime totals as a frame's work. Actual rendering
controls pass 16/16 on both modern NVIDIA GL and the forced 3.3 portable path,
including consecutive accumulated and reset-statistics frames. OS RSS samples
also remain available in profiler-off reports; unavailable profiler fields are
not fabricated as zero. Derived panel-frame storage is charged to resource
residency estimates; cooked asset bytes remain unchanged.


## Whole-engine results and remaining performance boundary (2026-11-11)

This audit is **not universal 120/60 FPS certification**. It preserves simulation
coverage and visual quality. Ordinary public workloads improved; severe CCD and
some animation tails still miss the minimum. No main promotion, release or human
acceptance is implied. Source baseline is `f05577b9`; common-instrumented frozen
executables, input manifests, command lines and raw measurements are retained in
`.cache/whole-engine-performance/`. Main remains `1094d645`.

The final public inputs are `final-public-workloads-v7`: 129 animated motors,
16 additional four-contact IK actors in the IK case, 100,000 GPU collision
particles, a 1 km cooked terrain, project-owned drive/brake/reverse cycles and
a real combined courtyard with 32 supported actors plus 5,000 colliding GPU
particles. Captures are 1280×720, VSync off, uncapped, three alternating matched
trials per case (20 s, IK 45 s), with resource-quiet warm-up. Each metric below is
the median across three trials; maximum is the worst across all three. Different
percentiles therefore need not describe one particular trial.

| Workload | Original FPS → current FPS | Original p99 → current p99 (ms) | Current worst (ms) |
|---|---:|---:|---:|
| lightweight | 1171.22 → 1171.47 | 0.924 → 0.903 | 1.658 |
| town | 785.67 → 800.43 | 2.625 → 2.595 | 5.674 |
| populated-rendering | 196.52 → 197.63 | 5.444 → 5.648 | 9.367 |
| animation | 113.27 → 187.57 | 11.306 → 7.998 | 16.055 |
| animation-ik | 36.24 → 149.56 | 33.887 → 11.527 | 31.339 |
| particles | 308.52 → 308.62 | 4.002 → 4.082 | 5.639 |
| basic-water | 872.74 → 871.82 | 1.439 → 1.439 | 3.003 |
| terrain-large | 1769.82 → 1800.78 | 0.947 → 0.929 | 5.186 |
| vehicle | 1030.18 → 1030.93 | 2.170 → 2.153 | 3.655 |
| fleet | 949.89 → 952.55 | 3.349 → 3.284 | 6.004 |
| combined | 335.37 → 458.43 | 6.206 → 5.462 | 14.221 |

All eleven matched public cases pass the relative p99 regression gate. That
means no significant relative regression under these inputs; it does not mean
every frame meets a product budget. IK p99 misses 120 FPS; its maximum misses
60 FPS. Ordinary crowd maximum is just under 16.67 ms. The combined case has a
14.221 ms exceptional frame, despite its 458 FPS average. Average FPS alone is
insufficient.

| Current workload | p50 / p95 / p99 / worst (ms) | 1% / 0.1% low FPS |
|---|---:|---:|
| lightweight | 0.847 / 0.884 / 0.903 / 1.658 | 1011.2 / 777.3 |
| town | 1.173 / 1.855 / 2.595 / 5.674 | 325.3 / 245.4 |
| populated-rendering | 5.036 / 5.155 / 5.648 / 9.367 | 163.5 / 131.5 |
| animation | 4.465 / 7.192 / 7.998 / 16.055 | 99.3 / 72.5 |
| animation-ik | 4.386 / 10.518 / 11.527 / 31.339 | 66.9 / 40.3 |
| particles | 3.107 / 3.799 / 4.082 / 5.639 | 235.1 / 218.5 |
| basic-water | 1.120 / 1.297 / 1.439 / 3.003 | 658.3 / 523.1 |
| terrain-large | 0.532 / 0.743 / 0.929 / 5.186 | 956.4 / 657.9 |
| vehicle | 0.895 / 2.107 / 2.153 / 3.655 | 439.8 / 374.6 |
| fleet | 0.905 / 3.199 / 3.284 / 6.004 | 288.8 / 240.3 |
| combined | 1.661 / 5.378 / 5.462 / 14.221 | 161.6 / 102.9 |
| actual-excavation-current-only | 0.798 / 0.997 / 1.122 / 3.201 | 788.8 / 487.3 |

Excavation is **current-only**, three genuine 30 s tunnel-edit trials. There is
no valid original-engine interactive baseline: the earlier harness lost the
pressed edge. Cheap static captures are excluded. The 1 km traversal fits its
configured residency budgets; it is not proof of repeated page eviction.
Previously qualified unchanged Terrain V2 paging/cold-persistence tests remain
valid but are separate evidence.

Profiler-off controls confirm real gains: IK p99 32.323 → 10.668 ms, combined
6.021 → 5.326 ms. The off-profiler IK maximum is still 53.986 ms. A 300 s current
IK run averages 151.84 FPS, p99 10.918 ms, maximum 20.143 ms. A high average does
not remove these long frames.

### Actual critical path, CPU cores and GPU limits

Input/resource publication precedes ordered fixed simulation. Character queries,
MK2 CCD/contact solving, scripts and state publication remain owner-thread work;
pure full-body IK solves overlap on the existing worker pool, then join before
ordered pose/socket publication. Render preparation, GL driver submission and
swap run on the context owner; the GPU consumes previously submitted work while
the CPU continues. Delayed timestamp polling avoids an in-frame blocking GPU
readback. GPU calibration, previews and final readback occur outside measurement.
Nested CPU scopes and GPU command intervals must not be added as frame time.

Representative OS-sampled application CPU usage is approximately one core for
populated rendering, crowd and combined gameplay. IK rises to 1.43 core-equivalents
with useful parallel work, not twelve fully saturated logical CPUs. In the
300 s IK capture, workers execute 135.46 aggregate busy seconds, 4.10% of the
11-worker pool's possible capacity. This denominator includes deliberately idle
workers; it is not a claim that all engine work can safely be parallelised.
The measured 1/2/4-lane experiment, rather than utilisation alone, justified the
retained IK batching. Resource and terrain cooking workers are active during
preparation; steady resource-quiet gameplay appropriately leaves them idle.

The populated rendering case uses one CPU core and about 46% device GPU activity:
its limit is CPU submission/driver work, not a fully busy GPU. The 100k-particle
case uses only 0.225 CPU cores and about 97.7% device GPU activity. At 2560×1440
it averages about 272 FPS with p99 4.37–4.42 ms, versus about 309 FPS at 720p;
GPU timing/utilisation and resolution scaling support a GPU-limited classification.
Device telemetry includes desktop, loading and warm-up; it is not process-only
GPU utilisation. Full CPU/GPU traces and per-thread endpoint samples are retained.

The calibrated queue-inclusive completion metric is
`max(CPU frame end, GPU frame end) - CPU frame start`. It excludes scan-out and
compositor latency, includes GPU queueing, and is not inverted to report FPS.
Representative current p99 completion is 1.42 ms lightweight, 2.59 ms large
terrain, 7.81 ms populated rendering, 7.52 ms combined and 14.42 ms IK. Clock
calibration uncertainty is approximately 11–15 microseconds in these trials.
**The aggressive 5 ms critical-path goal is not generally met**, even where
throughput exceeds 120 FPS. The GPU root spans command gaps; it is not active
shader time. Frame-cap runs make this especially important: deliberate sleeping
can appear inside a root interval. Requested 120/60 caps deliver 117.62/59.33 FPS
with OS sleep granularity, so neither is used as uncapped engine throughput.

### Accepted changes, rejected work and outstanding bottlenecks

Retained: exact capsule frame preparation/fixed-point exit; shared final animation
hierarchies; exact bounded material uniform publication; coarse independent IK
jobs with deterministic publication; invariant panel render-frame preparation;
and truthful whole-frame/GPU/resource counters. Each is supported by matched
whole-game evidence and physical/output regression controls above.

Rejected: matrix-only threading (faster kernel, worse whole-game result), shadow
uniform-location caching (no meaningful net gain), and reusable wake scratch
(fewer allocations, no meaningful crash-hitch reduction). Their negative evidence
and patches remain in the ignored audit directory. No scheduler, renderer or CCD
replacement was undertaken. Existing GPU particles, instancing, streaming and
renderer portability were reused, not replaced merely to increase utilisation.

Severe CCD remains dominant. The genuine four-way junction has an approximately
319.91 ms frame (original 322.97 ms), eleven consecutive hitch frames spanning
about 1,067 ms, and four >100 ms frames spanning about 730 ms. That frame grouped
five existing fixed steps and discarded **no** accumulator time in that frame.
The following two capped frames each ran eight steps and discarded 0.128489 s
and 0.042806 s respectively (0.171295 s across the run). The same current trial
averages 117.56 FPS, p50 3.135 / p95 13.716 / p99 14.902 ms, but its 1% / 0.1%
lows are only 18.56 / 4.81 FPS. A passing p99 minimum is not a hitch pass.

The existing interactive loop also clamps incoming simulation delta to 0.25 s
before accumulation. Profiler `discarded_seconds` measures only accumulator
backlog dropped at the eight-step catch-up cap; it does **not** include wall time
excluded by that earlier clamp. Therefore the reported discarded values are not
total lost wall-clock simulation time in very long stalls. Neither timing policy
was changed for the audit. Interval/frame timings remain the actual wall-clock
measurement; profiler-off discarded counters remain unknown, not invented zero.
Individual fixed-step physical-state controls, rather than wall-clock grouping, establish
physics equivalence. The native matched nine-case suite retains **all nine exact
accepted crash state hashes**. Frontal, four-car and debris cold restorations
also pass. Improved render mapping does not fix the remaining continuous-search
cost. Reducing collision coverage, deformation fidelity or sampling was not used.

A fresh post-mapping diagnostic still attributes peak crash work to interval SAT
and arithmetic inside geometry search; wake propagation alone consumed about
48 ms across 13,301 nested calls in one 319 ms whole frame. These scopes overlap
and are not additive attribution. The earlier panel mapping hot spot was 12.86%
of sampled CPU work; prepared panel groups improve ordinary yard frames but
leave severe collision peaks around 166–171 ms. Further large CCD architecture
changes require a separately reviewed proposal, not an unbounded extension here.

Allocation evidence is scoped: invariant panel preparation removes per-frame
maps/allocations, profiling measures retained memory/workload counters, and the
rejected wake experiment counted allocations in its controlled fixture. There
is no claim of a complete process-wide allocation census. General-purpose scripts
and frequent authoritative geometry searches cannot simply be moved to GPU
without transfer, determinism and latency costs. No such speculative move was
accepted without a whole-frame win.

### Memory and loading

The 300 s IK run rises from 153.91 to 160.95 MiB RSS; last-minute medians converge
from 160.66 to 160.93 MiB, without process swap. The 90 s terrain traversal rises
199.20 → 199.53 MiB, without swap. These bound the tested working sets; they do
not prove absence of leaks under all projects or unlimited travel.

The private four-model yard has a much larger retained asset footprint: RSS
2,537 → 2,502 MiB, process swap 655 → 690 MiB. RSS plus swap is essentially
stable around 3,193 MiB. This is real memory pressure on an 8 GB machine, not
an unqualified no-leak result. Resource estimates include about 789 MiB GPU
payload and 1.70 GiB total ready resources. Active/pinned assets exceed the
256 MiB resource cache target; a budget cannot evict assets still required by
live cars. Device-wide VRAM telemetry peaks near 2,376 MiB including the desktop.
Detailed immutable skins retain decoded image payloads for renderer recreation;
independent derivatives do not universally deduplicate embedded texture images.
Blindly freeing them would break current recreation/Play contracts. A focused
cooking/resource-footprint follow-up is warranted; no unrelated resource rewrite
or private asset modification was performed. Panel derived data costs 19,968
shared bytes for the 2,209-vertex fixture, with its 425,346-byte cooked file unchanged.

Startup, resource preparation and gameplay are separate in `application/run.json`
and OS logs. Resource-quiet benchmark warm-up is not a claim about shipping
startup duration. The private yard's multi-gigabyte loading cost is not hidden by
including it in an average of otherwise cheap steady frames.

### CPU particle follow-up and final qualification

The initial portable-auto combined capture failed the minimum-frame gate:
only 38–39 frames in 20 s. A separate actual-desktop user-space profile (199 Hz,
zero lost samples) attributed 27.40% to native casts, 20.08% to ContactRotation,
11.72% to primitive preparation and 11.48% to SegmentGeometry. Every CPU droplet
visited every captured collider, including terrain chunks nowhere near its path.

The retained focused correction prepares conservative static bounds and the
immutable stationary flag once per snapshot. Disjoint queries skip expensive
casts; the original finite-feature kernels, collider/tie order, moving-solid
queries, unsupported/exhausted policy, impact backpressure and delivery remain.
Bounds include native float rounding and convergence skins. This is candidate
rejection, not a reduced collision representation or a new particle solver.
Two double-precision bounds add 48 bytes per collider (96 KiB at the existing
2,048-collider limit); derived body state adds at most a few KiB. Cooked assets
and persistence formats are unchanged.

Three alternating matched 20 s trials preserve 5,000 CPU particles, 32 supported
actors and terrain: original median 1.954 FPS, p99 515.854 ms, worst 515.894 ms;
corrected median 310.670 FPS, p99 9.030 ms, worst across trials 17.005 ms. The
original discards 4.43–4.55 s of accumulated physics time; corrected trials
discard zero. Original results are failure diagnostics with too few samples,
not qualified gameplay or reliable tail distributions. Current state gates and
viewed images verify real supported actors, live CPU droplets and contacts.
The CPU fallback still misses the 120 FPS p99 target and has one >16.67 ms frame.
Do not apply the GPU workload's budget classification to this CPU fallback.

A committed 4,096-seeded-query differential control compares exact hit identity,
time, point, normal, initial-overlap state and feature against exhaustive native
casts across rotated boxes, spheres, capsules, offset compounds and finite meshes.
All 38 collision checks, 49 CPU particle checks, 44 persistence checks (including
genuine process-exit restoration) and 35 Terrain V2 particle-region checks pass.
Desktop world-lifecycle checks pass 146 modern and 36 portable controls; particle
kernel controls pass 6 on each policy. Existing moving/rotating-boundary,
replacement, retained-snapshot and origin-shift controls remain unchanged.
Final matching SDK and CI identities are taken from their actual completed
records; no pending CI or unexecuted platform is counted as passing.
An explicit GPU-only emitter on the 3.3 portable renderer correctly reports
`unavailable` with zero live particles: its fast frame rate is **not particle
performance or CPU fallback qualification**. The CPU-auto workload retains the
same 5,000 particles, collision coverage, 32 supported actors and terrain.


The normal GPU-backed combined workload also passes three alternating trials
after the CPU correction: incremental p99 5.760 → 5.623 ms, worst current
11.135 ms, no discarded physics. Collision coverage, counts and quality remain
unchanged. These incremental numbers compare against `fb1e3c57` behaviour, not
against the original `f05577b9` whole-audit baseline.

### SDK, editor and human review

A clean isolated local SDK at `fb1e3c57` passed fresh editor creation, Play/Stop,
39 packaged-tool workflow operations, package integrity before/after use, and
relocated CLI/editor standalone exports. The surrounding Linux namespace hid
the engine source, RPG project and private cars; `qualify_sdk.py`'s own rootfs
flag remains false because this used an external namespace, not its Ubuntu
rootfs option. This is native NVIDIA Linux evidence, not clean-machine or
Ubuntu baseline compatibility. The host preview uses GNU 15/glibc 2.43;
separate Ubuntu 24.04 SDK compatibility belongs to the completed CI gates.

The final refreshed SDK lives at
`.cache/whole-engine-performance/sdk-build/linux-sdk`. Its exact clean source
revision is authoritative in `BUILD_INFO.json`; final matching qualification is
retained in `sdk-final-namespace-work/qualification/COMPLETE.json`. No RPG SDK
selection or downloaded vehicle assets are part of this package. The mixed
acceptance project also retains the original engine-owned model's import recipe,
source glTF and licence, so it is editable and exportable through normal tools.

No native Windows, Android device or browser runtime performance qualification
was performed in this audit. Linux CI platform/tool/boundary checks do not imply
those devices passed. Historical accepted coverage is not relabelled as new
measurements. The Advanced Fluids lock is unchanged; Basic Water is exercised,
while locked Advanced Fluids performance is not claimed. This work is not
Lucifer, a scheduler replacement, a deformation-fidelity reduction or a release.

Use a disposable copy when editing a frozen benchmark input. The prepared
editable combined copy is `.cache/whole-engine-performance/acceptance-projects/combined`.
From the repository root, launch:

```sh
.cache/whole-engine-performance/sdk-build/linux-sdk/judas_editor \
  .cache/whole-engine-performance/acceptance-projects/combined/terrain_v2_workshop.judasproj
```

Review in order:

1. Open/edit the combined courtyard; Play/Stop; verify terrain, all supported
   characters and particle collisions remain visible. No frozen input is edited.
2. Open the animation studio/IK workload, inspect motion, sockets and controls.
   Compare ordinary pacing with the documented IK tail limitation.
3. Use Terrain Workshop to sculpt, paint and cut a tunnel; traverse and save,
   exit the process and reload. Keep operator-authored terrain files separate.
4. Drive the Vehicles V2 workshop, brake/reverse and inspect support. Open the
   private corrected four-vehicle yard only locally: orbit, switch vehicles,
   collide, detach and restore damaged cars. Severe collision stalls remain a
   documented failure, not an acceptable finished performance target.
5. Run the relocated combined export; inspect the same 32 actors and 5,000
   droplets. Test portable auto separately; do not request required GPU on GL3.3
   and interpret an unavailable emitter as a particle success.
6. Check `BUILD_INFO.json`, completed CI and SDK qualification records against
   the handed-over commit before accepting this checkpoint.

Prioritised follow-ups require separate scope: reduce remaining CCD search
critical-path cost with preserved contact outcomes; reduce retained private
model/image duplication; investigate IK exceptional tails and context-thread
submission/queue latency. There is no evidence supporting a wholesale engine
or scheduler replacement as a prerequisite for the measured gains here.


Direct observations of the clean `82f38bf1` SDK confirm the harness improvements
reach a real exported game. Each is a separate 20 s sample with the optional SDL
present observer, VSync disabled, ordinary wall-clock runtime and no test-clock
injection:

| Actual SDK application | FPS | p50 / p95 / p99 / worst (ms) |
|---|---:|---:|
| Relocated combined game, 1280×720 | 461.71 | 1.636 / 5.582 / 5.832 / 6.809 |
| Combined editor Edit, 1280×800 | 1013.25 | 0.984 / 1.007 / 1.145 / 2.155 |
| Combined editor Play, 1280×800 | 404.64 | 1.908 / 5.647 / 5.845 / 8.182 |

The exported game verifies all 32 supported actors, 5,000 live GPU particles and
positive contacts. Editor chrome/viewport sizes differ from the full-window
export; Edit does not run gameplay. These are application-path controls, not
matched-resolution proof that the editor is faster than a game. Desktop X11
screen capture returned `X get_image` errors; native renderer screenshots were
used instead. A separate controlled native-export capture shows the same visible
courtyard, crowd and droplets. That screenshot run uses scripted frames and is
**not** FPS evidence. Full observer logs, hashes, native images and OS samples
are retained under `matching-sdk-observations` and `matching-sdk-native-images`.


Engine implementation ends at `82f38bf19fbe3b59b479bc35158f04235eec9921`.
Its independent Alpha/Experimental engine and Ubuntu SDK workflow is
[42999299307](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42999299307).
The final handover must verify its completed outcome, not infer success from
this link. Later documentation-only checkpoints do not change engine behaviour;
the final local SDK is rebuilt with their exact clean identity and reverified.
Documentation controls pass 10/10, metric/state controls 7/7, and the current
reference checker covers 174 documents and 2,242 local links. These link checks
are not an API or platform qualification substitute.


## Subsequent MK2 collision architecture work

The measured C2/82f results above remain the whole-engine baseline, not a claim
that its severe collision tail is resolved. The subsequent Experimental
[pair-owned collision architecture report](MK2_COLLISION_ARCHITECTURE_PERFORMANCE.md)
records the replacement scheduler, actual-contact and redirected-future defects,
matched cold four-way trials, current native/whole-frame costs, rejected worker
experiments and remaining latency failures. It preserves the prior public
vehicle/fleet/combined workload identities. The new report also retains a
separate combined presentation-stall outlier rather than calling a relative
p99 pass an absolute performance pass.
