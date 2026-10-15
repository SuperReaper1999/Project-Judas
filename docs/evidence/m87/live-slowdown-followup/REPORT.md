# M87 — human-reported live slowdown follow-up

## Status

This is a Linux-validated **cost-reduction checkpoint**, not closure of the
human-reported 1,024-piece pile failure. The largest mixed pile still develops
seconds-long fixed steps. It is **not comfortably interactive** and must not be
advertised as 60 Hz or accepted on the strength of a short timing window.
Human acceptance is pending. Main is unchanged; Windows was not run for this
follow-up and remains informational/nonblocking.

Starting Experimental commit: `cee55a898aa0e4938665903f3b207d828acdb1df`.
The original M87 report, measurements and fingerprints are preserved separately.
This report supersedes any implication that its short startup smoke established
sustained performance of the 1,024-piece interactive room.

## Reproduction and cause

The operator's live package was in the **1,024 loose-piece room**: 1,285 total
bodies, 1,152 dynamic, 133 static and 96 joints. The loose bodies are mixed boxes
and spheres in falling stacks. The cabinet articulations and their balls are
separate. An exhaustive authored AABB scan found zero initial positive-volume
loose-body overlaps and a 17.4 m gap to the cabinets.

The preserved original core was inside step 188. Only 3.821390 ms of its
16.666668 ms physical interval had elapsed, despite 396 impact events and
314,709 TOI queries. Repeatedly involved pieces could move only around 0.02 m/s.
931 loose bodies had participated; no cabinet bodies or spin-target pairs had.
Time advanced by positive, sometimes nanosecond-sized increments. This is a
low-speed **continuous-impact/contact cascade**, not a demonstrated infinite
zero-time loop, initial-overlap explosion or duplicate cabinet simulation.
The ordinary outer loop's catch-up steps amplify the visible freeze.

Exact-predicate arithmetic was not responsible for the captured stall: its
3,187,842 predicates were all interval-resolved, with zero exact/numeric-gap
fallbacks or unresolved results. Whole-history exact fallbacks were only 10
among 200,450,511 predicates. The original core/assembly extraction and selected
physical state are retained under [provenance](provenance/original-failure/).
The core and executables are deliberately excluded from repository evidence.

The original profiler also overflowed because every native script-event attempt
emitted a scope, including contacts between unscripted bodies. Both genuine
overflow captures remain in the [profile index](raw/PROFILE_INDEX.json); they are
not used as complete timing attribution.

## Retained generic corrections

1. **Joined initial contact geometry jobs.** The ordinary RuntimeWorld borrows
   its ResourceManager's existing job service. The default cap is four execution
   contexts including the owner. Only independent Box/Sphere pair geometry is
   dispatched, for 256–16,384 eligible pairs. Owner-prepared orientation data is
   immutable during the batch; output slots have one writer. All jobs join before
   owner-ordered row creation, warm starting, observations, callbacks and solving.
   Null/stopped pools and oversized/small batches use the normal unbatched serial
   path. With a busy pool the owner can finish the indexed batch, cancelling only
   its unnecessary queued physics jobs and joining its own running jobs; no
   unrelated WaitAll.
2. **Geometry-only anchored motion sampling.** Queries reuse the original
   represented position/orientation expressions without doing unused physical
   angular-velocity transport. Physical event/endpoint publication retains the
   full motion evaluation. Common event-start samples are kept separately from
   future/bisection samples, with exact time/generation/trajectory invalidation.
3. **Conservative primitive bounds.** Proven translation and rotating-box
   interval enclosures can reject an entire remaining interval before expensive
   query preparation. Uncertain/unsupported cases retain the prior path. Exact
   current-pose bounds reuse prepared orientations. No CCD tolerance changes.
4. **Equivalent contact existence and bookkeeping.** Zero-margin Box/Sphere
   Boolean queries retain the robust predicates without constructing unused
   witnesses. SAT rows are prepared lazily. Membership tables remove repeated
   island queries without ordering a solve. Discovered canonical manifolds are
   reused; contact observations receive one stable publication sort, including
   the failure path. No response rows are reduced.
5. **Bounded profiling.** Script contact dispatch is scoped once per batch.
   Worker-completed manifold, scratch-memory and predicate/fallback counters
   identify actual work. Existing inclusive work timers remain overlapping.
6. **Build integration.** The existing JobSystem implementation is factored into
   `judas_jobs`, also linked by headless fixtures compiling PhysicsWorld directly.
   This corrects the preserved missing-link-dependency development failure.

There is no production change to timestep, solver iteration count, materials,
contact margins, CCD/event policy, body population, sleep rules, collision
filtering, game scripts or scene geometry. No full solver was parallelized.
No GPU offload or scene-specific physics branch was added.

## Matched final measurements

Native Linux, Release `-O3 -DNDEBUG`, GCC/C++17, i7-4790, four cores/eight hardware
threads. Other user applications were left running. Judas builds/tests/benchmarks
were serialized; host scheduling/frequency were not controlled. These are
observations on this host, not an idle-machine certification.

The same frozen 1,024-piece scene, fixed input, 60 Hz cadence and 155 steps were
run in alternating order: before / final / final / before. Collection was off.
The observer renders hidden frames and inspects state outside the fixed-step
timer. Each run covers **2.58 simulated seconds**, not sustained acceptance.
Medians use the ordinary middle-value mean; p95 uses nearest rank over the two
pooled 155-step runs.

| Measurement | Original M87 | Final follow-up |
|---|---:|---:|
| Fixed-step median | 32.322 ms | 24.515 ms |
| Fixed-step p95 | 118.474 ms | 110.802 ms |
| Worst recorded step | 1,033.830 ms | 841.709 ms |
| Sum of 310 completed step times | 18,734.786 ms | 13,447.239 ms |
| Initial contact-pass sum | 5,747.308 ms | 3,573.314 ms |
| Peak process RSS, two runs | 150.31 / 150.28 MiB | 174.98 / 175.42 MiB |

Aggregate fixed-step time fell about **28.2%** and median about **24.2%** in this
comparison. Tail improvement is much smaller and variable: the first final run's
p95 was worse than its adjacent before run. Inspect all four rows in the
[raw receipt](raw/final-matched/RECEIPT.json), not just pooled averages. Retained
scratch and parallel execution consume additional memory; RSS includes the whole
rendered process/driver and is not an isolated physics allocation measure.

Every step's recorded pose/linear/angular velocity/sleep digest matches across
all four runs, ending at `8509c058f2122986`. Both before/final cohorts have exactly
7,560 events and 1,453,738 TOI queries, with no nonfinite recorded fields.
That supports equivalent physical behaviour for this fixture; it does not prove
all possible inputs or cross-platform bitwise identity.

The longer combined run requested 210 steps and was externally bounded at
75 seconds. It completed only 180 (3 simulated seconds), with a 4,567.298 ms
completed step before termination. The pending step is unmeasured: this is a
**censored failure**, not a passing run or a bounded maximum. This was an earlier
combined build before the JobSystem archive-link split. Its executable hash was
not captured: that provenance limitation is retained, and these samples are not
relabelled as final-binary measurements.
See [long-run receipt](raw/final-pre-link-split/dense-fixed-final-210/summary.json).

The final collection-on diagnostic has zero dropped/truncated/incomplete events
and zero open scopes after normal teardown. Across 118 retained physics frames,
workers actually completed 7,666–10,856 manifolds per frame; four contexts were
configured. Retained initial geometry scratch peaked at 9,542,880 bytes.
Predicate exact/numeric-gap fallbacks and unresolved results were zero. These
diagnostic timings are not mixed into the collection-off comparison.

The unchanged default 128-piece scene (256 dynamic / 389 total bodies) completed
a 30-second real-clock watch tour: median frame interval **17.098 ms / 58.49 FPS**,
p95 17.905 ms, worst post-startup interval 65.446 ms. Its 1,799 physical steps had
median **1.757 ms**, p95 3.355 ms and worst 23.983 ms. All 256 dynamic bodies were
naturally sleeping at the endpoint, after the motor/settle/impulse phases. There
were 61 catch-up frames, with essentially 30 seconds of physical progress in
30 seconds wall time; no dense-pile spiral occurred. This hidden rendered-window
observer includes sampling overhead and swap pacing, not isolated GPU throughput.
The [raw real-clock receipt](raw/final-realtime-128/SUMMARY.json) does not substitute
for the failed large-pile result.

## Rejected alternatives and remaining mechanism

[EXPERIMENTS.md](EXPERIMENTS.md) retains successful and rejected investigations:
full manifold caches, lazy/prefix/deadline scheduling, a private future-motion
tree, parallel CCD searches and extra impact-solver iterations. Several reduced
query counts without reducing total work enough to justify their complexity.

An exactly frozen 22-body/49-contact impact replay reproduced the original
10-sweep endpoint and row impulses. More sweeps reduced its local closing
residual, but 40/80 sweeps worsened complete-scene events and timing. Raising
budgets was rejected; the original production count remains ten.

A final isolated [support-continuation projection](experiments/support-continuation-rejected/RESULT.md)
refreshed prior positive-gap supports after a real impact, with normal-only
remaining-time targets and ordinary CCD still active. It changed numerical
policy and added another solve, so was never an exact optimization. It worsened
events 3,780 → 4,435, queries 726,869 → 1,188,480 and worst step 583.79 →
2,326.40 ms. It is **rejected**, with source/commands/raw results preserved.

The current event loop removes a prior support exemption when an impact changes
its trajectory and the pair is no longer an actual touching member. Keeping that
exemption blindly could skip a subsequent collision. A future improvement needs
coherent persistent-contact/first-touch coupling, with geometric CCD protection,
not an arbitrary event cap, damping, sleeping shortcut or suppressed contacts.
The evidence does not establish a safe small correction that removes the cascade.
This remains an engine scalability problem; it is not declared unavoidable or
solved by the retained optimizations.

## Validation and retained failures

- Release runtime/editor/export and focused targets rebuilt successfully. The
  first broader invocation exposed the missing standalone JobSystem linkage;
  the corrected build and original failure are both retained.
- 26 focused core targets: **25 pass**. The unchanged M77 foundation fixture
  retains its original 34 checks / one participant-limit expectation failure.
  It is not converted into a pass or fixed in this performance task.
- New focused coverage: geometry-only motion 39,088 checks; conservative bounds
  470,408; Boolean contacts 32,372; SAT preparation parity 338,317 including 48
  exact boundary cases; parallel contact geometry/lifetime 55. No failures.
- Existing contact geometry/cache/lifetime/storage, rigid contacts, CCD timing/
  robustness/mesh cost, kinematics, ragdolls, CharacterMotor, joints, physical
  animation and balance-controller checks pass. Known mesh-cost fast-box
  crossings remain recorded; no new no-tunnelling guarantee is invented.
- M70 and M77 fresh-process cold-save write/read pass. M77 uninterrupted/restored
  **actual physical endpoints are exactly equal**, both physically standing;
  the saved bytes remain unchanged by read.
- Ordinary stress-project application: **51 checks pass**, including real
  movement/impulses, sleeping/wake, runtime prefabs, pause, save/load, scene
  transitions/reload and stale-handle rejection.
- Original Tuppence application fixtures: **30 ball-mode / 31 coin-mode checks
  pass**. These are functional follow-up runs, not a new matched speedup claim.
- Editor duplicate/undo, profiler inspection/frozen capture while simulation
  continues, capture restoration and Play/Stop pass; authored state remains
  identical. The first freeze probe omitted collection enablement and failed
  that expectation; its unchanged repeat with `JUDAS_PROFILE=1` passes. Both
  outputs are retained, with no editor/profiler implementation change for it.
- Normal export includes all four registered scenes. The moved package runs
  from an unrelated `/tmp` working directory with `JUDAS_ENGINE_ROOT` unset;
  scene/pause/reload assertions pass. Runtime SHA256 matches the Release build.
  Its short scene transitions are not sustained large-pile performance proof.
- No full production-suite marathon or Windows claim is made.

The new parallel fixture initially overclaimed uninterrupted-versus-restored
bitwise equality for a **no-joint contact pile**. Independent controls show warm
serial = warm parallel, cold serial = cold parallel, but warm ≠ cold from the
first resumed step. Current persistence does not retain its solved warm-contact
rows. The original failure and control are [preserved](validation/development/NOTES.md).
The final test validates exact serial/parallel cold behaviour and actual resumed
impulse/displacement; it does not claim that missing continuation mechanism was
repaired. Persistence implementation is unchanged.

## Reproduction

Build the normal Release targets plus `judas_physics_stress_live_benchmark`.
Its CLI is:

```text
judas_physics_stress_live_benchmark PROJECT OUTPUT [wall-seconds] [frames]
  [swap=1] [tour=1] [hidden=1] [frame-seconds=0:real-clock]
  [profile=1] [periodic-profile-export=0]
```

Use the ordinary project for input/tour checks. To repeat the frozen no-tour
comparison with current source, select its `Scenes/room1024.judas`, tour `0`,
swap `0`, hidden `1`, frame seconds `0.016666667`, profile `0`. The archived frozen
fixture constructor already enables its tour; that historical comparison
deliberately supplies no additional input. Actual commands/binary hashes are in
the receipts and [runner](provenance/final-matched-runner.py). Synthetic fixed
frame cadence provides repeated inputs; it is not displayed FPS.

## Human checklist and release boundary

1. Use the default 128-piece room and **F5** watch tour. Inspect physical shelves,
   springs and balls, motor-off settling, then impulse-driven wake.
2. Use **M/J/K/P** for motors, single/all impulses and real prefab drops.
3. Check **4** for the same behaviour under rotated gravity.
4. Save/load moving machinery; pause, reload and editor Play/Stop.
5. Repeat the exported package from an unrelated working directory.
6. Treat **3 / 1,024 pieces** as a known severe stress failure. It may prevent
   input processing for seconds. Its inclusion does not request acceptance of
   that performance or imply that the operator's original complaint is fixed.

Validated exact-cost reductions are eligible for an Experimental checkpoint.
The largest-pile performance defect remains open. Main promotion and human
acceptance have not occurred. Existing games, protected historical evidence,
accepted fluid implementations and independent research are unchanged.
