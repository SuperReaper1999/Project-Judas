# Integrated performance profiler — M56

Current CPU/GPU diagnostic facility based on M55 checkpoint
`7b1e5ea1eab46456bead72b0dcdc7b7fdc9b36fd`. Timings are observations, never
simulation inputs. Collection does not change quality, cadence, scheduling,
physics, authored content or gameplay.

## Getting started

Open a project in `judas_editor`, then **View → Profiler**. Toggle **Collect**;
opening/closing the window does not enable/disable collection. **Freeze capture**
stops recording while the game continues. Disable **Follow latest**, select a
history frame or spike, inspect its hierarchy/counters/GPU passes, then resume.
In Play, press **F8** (or enable **Inspect live Play**) to release the cursor for
profiler controls without pausing simulation. F8 again restores game capture;
closing the profiler also restores the ordinary capture policy. Project capture
intent is preserved; normal editor input claims suppress gameplay over panels.
**Startup** selects retained first-use work. **Clear** discards recordings;
**Export JSON** writes the bounded snapshot to the editable path (initially
`/tmp/judas-profile.json`). Stop keeps recordings and marks lifecycle boundaries.

Standalone and exported games default to collection **off**. Opt in before launch:

```sh
JUDAS_PROFILE=1 JUDAS_PROFILE_OUTPUT=/tmp/range-profile.json /path/to/package/judas
```

The report is written on normal application teardown. No automatic writes into
project/package assets or protected evidence. `JUDAS_PROFILE_SCRIPTS=0` removes
per-asset attribution; `JUDAS_PROFILE_SPIKE_MS=40` changes the diagnostic spike
threshold. Neither changes the simulation. CPU capture also works without GL.

## Reading time correctly

- **Interval:** time between successive captured outer-loop starts. First sample
  and the first after a capture toggle have no interval (0).
- **Outer work:** start/end of the actual standalone/editor iteration, including
  input, resource pump, Play/edit work, UI and swap. Nested Play has no second frame.
- **Wait:** union of main-thread intervals explicitly marked waiting, principally
  buffer swap. Worker queue waits belong to their independent lanes.
- **Unattributed:** outer elapsed minus the union of recorded main root scopes;
  not reassigned to a fictional subsystem. Saturated/incomplete frames are flagged.
- **Collection cost:** draining/aggregating/saving the frame after its end boundary;
  shown separately and included in the next start-to-start interval.
- **Inclusive/exclusive:** elapsed time on that thread, including/excluding nested
  recorded children. Hierarchy identity includes the parent path. Repeated calls
  aggregate count and duration. Percentages divide by selected outer elapsed;
  nested and worker percentages overlap and must not be added.
- The graph/statistics cover at most **120 completed frames**. Latest/mean/median/
  lower empirical p95/max use outer elapsed, including waits; UI refresh is 4 Hz.
  A scope tooltip reports duration/call and duration/retained frame (absent = 0).
  Workers appear in their completion frame, with original frame and absolute
  steady timestamps; cross-frame job time is not clipped into main-thread time.
- Fixed records have monotonic step IDs, actual elapsed and simulation dt. Frames
  can have zero/multiple steps. Accumulator, pause, cap and discarded backlog
  report the existing policy rather than changing it. Counters absent in a frame
  mean **no observation**, not a current zero-body world.

## Coverage

Native RAII scopes cover real boundaries: script lifecycle/update/fixed/UI/contact/
presentation and optional bounded script-asset attribution; CharacterMotor;
rigid broadphase, contacts, contact/joint solver and proxy refresh; navigation
queries/obstacles/avoidance; animation sampling/mixing/resolution/skin matrices;
ragdoll physics-to-pose; visual particles; main-thread audio; resource IO/decode
worker lanes, installation, pump/budget eviction and shutdown; shadows,
secondary/main cameras, editor viewport/camera, culling/submission, particles, UI
and liquid presentation.

M54/M55 distinguish containers, paired transfers, detached parcels, solid geometry
batch/**Solid excluded storage rebuild**, **Dynamic surface solve**, pressure/
continuity attempts, body loading/submersion, surface mesh/upload and optical/
water-boundary work. Storage-call count includes cheap cache hits; **cells rebaked**
and aperture counts describe actual changed work. Cell/face/iteration counters
sum executed solves in the frame, not an extra recount of all liquid topology.
Useful resource/render/physics/navigation counts reuse existing observations.
No per-contact/tetrahedron/glyph/draw timestamp queries are installed.

Audio backend callbacks are deliberately not instrumented: the collector has
registration/clock machinery not certified for that real-time callback.
The legacy specialized diagnostics remain in a labelled collapsible section.
The lightweight runtime FPS/debug HUD remains intact.

### M87 physics stages and work counters

Physics now separates kinematic publication, predicted reach/wake, velocity
integration, candidate generation, contact row preparation, contact-history
publication and endpoint contact publication. Candidate generation may run more
than once in a step; root-query/overlapping-leaf/pass counters cover those passes.
The old `StepStats.broadphaseMilliseconds` retains its existing definition, which
includes final proxy/publication work and excludes the early prediction pass.
Use the hierarchy to attribute those operations rather than adding that legacy
field to nested scope times.

Sleep graph counters expose current islands/edges and retained scratch capacity.
Joint preparation has aggregate elapsed/call counters, including impact events;
it measures `JointSolver::Prepare`, excluding construction of its input vector
(covered by the enclosing stage). It never emits a scope per joint or impact
island. `Latest` counters describe the
last native step in a frame; `Sum` counters accumulate substeps. A coupled
macro-step's final contact publication follows its last native-step diagnostics,
so that final scoped work is not included in the preceding step counters.
Cooked collision counters report visited BVH nodes, triangles and convex tests
within the rigid step. They do not include independent caller queries outside it.
`Physics CCD search inclusive ms`, `Physics CCD geometry inclusive ms`, impact
island discovery and impact solve costs are aggregate counters, with constant
storage per world. Timers run only during active collection. Recursive searches
do not double-count their own category; geometry includes work inside searches
and contact-island discovery, so these categories **overlap**. Do not sum them.
This avoids emitting one scope event per contact/search predicate. The aggregate
geometry-call counter counts nested calls; it is work, not unique body pairs.

These diagnostics do not change solver iterations, physical thresholds, wake
semantics or scheduling. The [M87 scaling report](M87_PHYSICS_SCALING.md) explains
the separate owner-thread allocation probe used by its benchmark; ordinary M56
capture is not a general-purpose heap-allocation tracer.

## GPU and memory

Renderer owns a fixed pool of **128 timestamp pairs** (256 GL queries), created
only when capture is enabled with a context. End timestamps are polled for
availability; older completed pairs are then read. No profiling `glFinish`,
synchronous frame readback or current-frame result request. Timestamp pairs allow
nested coarse passes. Frame/pass/camera identity travels with delayed results.
Camera identity is 0 for the main/editor view, the authored camera entity ID for
secondary views, and the light-slot index for shadow passes.
Pending results export as `null`, unavailable as a reason; never CPU substitution.
Frozen recordings do not accept arriving results. Pool saturation/expired results
increment GPU drops. Driver/GPU scheduling can influence elapsed GPU intervals;
GPU time, CPU submission and CPU waiting are not additive frame components.

Profiler reserved bytes estimate bounded CPU object/vector capacity, excluding
consumer copies, allocator bookkeeping and GL driver memory. Resource resident/
budget values estimate owned asset bytes, **not measured VRAM**. Linux process
RSS and virtual bytes are independent `/proc/self/statm` observations at 1 Hz.

## Bounds and lifetime

Process-owned service; records contain owned names/IDs/values, no live world pointers.
32 thread lanes, 2,048 labels, 4,096 hierarchy nodes, depth 64, 4,096 completion
events/lane; 2,048 scope records, 256 counters, 64 fixed records and 64 GPU records/
frame. History 120, retained spikes 8, startup 1. JS caches 128 custom labels/VM.
IDs remain registered for process lifetime; Clear does not reclaim label identity.
After registration/warmup, ordinary native scope recording allocates no heap and
uses no shared lock. First label/hierarchy/thread registration has a slower path.
Frame draining and UI/report construction are separate diagnostic work.

Overflow/exhaustion/truncation/incomplete/GPU-drop counters are cumulative for the
process, not silently reset by Clear. Capture toggles/clear invalidate recording
epochs; scopes straddling them are omitted and counted incomplete. Thread teardown
closes its lane without retaining references. Cross-boundary frames are marked.
Freeze affects diagnostics only. It does not synchronize/stop simulation workers.

## Script instrumentation and verification

See [JudasJS profiling](judasjs/profiling.md) and the
[executed example](judasjs/examples/profiling.js). Public API remains two methods.

Focused targets: `judas_profiler_tests`, `judas_profiler_script_tests`,
`judas_profiler_application_tests`. The latter uses the actual application loop,
Spring Range and M55 liquid project with identical input/120 fixed steps across
interleaved off/on/UI cases. Bucket probe placement exercises normal opening
exchange, carrying and pouring; it never writes liquid quantities. Offscreen GL
software timings are diagnostic workload evidence, not desktop FPS certification.
Candidate results and fingerprints are in `docs/evidence/m56/`.

## Known limits

Bounded recent capture, not an exhaustive trace or allocator profiler. Worker
records are published on completion; an open/long wait has no partial duration.
Long labels are dropped rather than truncated into ambiguous identities. First
use and history warmup may allocate snapshot storage; no zero-overhead claim.
Data access/export/UI belongs to the main consumer thread. Frozen GPU arrivals
are discarded. Process ID registrations survive repeated hosts. GPU pool capacity
is bounded and driver memory is unknown. Per-instance script labels and audio
callback timing are intentionally absent. Human responsiveness/UI acceptance is
operator-owned.
