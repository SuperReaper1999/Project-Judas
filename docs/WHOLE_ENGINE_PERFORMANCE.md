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
