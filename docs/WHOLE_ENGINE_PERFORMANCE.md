# Whole-engine performance engineering

**Status: active audit and implementation; not performance-qualified.**
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

## Initial findings, not final qualification

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
Final before/after results, accepted/rejected optimisations, supported workloads,
remaining failures and qualification are still outstanding.

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
quality were not reduced. Other audit/optimisation work and full qualification
remain in progress. Raw trials and exact controls are under
`.cache/whole-engine-performance/` and are not public model assets.

## Animation preparation experiment (ongoing qualification)

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
generic CI runner. Broader workload coverage and final SDK/CI qualification are
still outstanding.

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
Final broader workloads, SDK, editor and CI qualification remain outstanding.
