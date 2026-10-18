# Pass 4: exact impact scheduling work

## Result

This checkpoint removes redundant event-index and dirty-pair work. It preserves
all 270 measured physical digests, counters and actual endpoint geometry, and
passes the affected native fixtures. It **does not resolve the severe dense
contact stall**. The matched maximum remains approximately 1.9 seconds.

The accepted physical baseline is `7da56c2cc6ce7acda827a6e4019d610622b237e2`.
The implementation is on top of the independently validated, behavior-preserving
physics-world extraction `06318379b5dfc99fe54eaedebbe5e6d964b25de8`.
Only `src/physics/WorldImpactEvents.cpp` changes production behavior in this
checkpoint, and its changes concern derived scheduling work only. No collision
truth, physical field, CCD policy, tolerance, iteration/event budget, cadence,
material, sleeping rule, fixture or gameplay setting changes.

## Mechanism and correction

The previous chronological event loop used a lookup-only ordered map to locate
vector-owned events, expanded the same candidate pair from both changed bodies,
copied candidate scratch, then sorted and uniqued the duplicate key stream.

The correction:

1. Uses a hash lookup for existing scheduled indices. The hash container is never
   iterated. The original vector still owns event order and the original
   earliest-time/lexicographic rule still selects events.
2. Expands each canonical body pair once per event, retaining every primitive
   child pair. Existing scheduled keys receive one indexed dirty mark.
3. Keeps the final lexicographic sort. Previously unscheduled keys arise from one
   canonical body-pair expansion; known keys have one dirty mark, so the resulting
   ordered unique key union is the original one.
4. Retains the selected event even when its impulse does not change either
   endpoint. Incident events and newly eligible current fat-bound candidates are
   still rescheduled after trajectory changes.
5. Uses references to the existing `QuerySlots` scratch in two non-nested query
   loops. Inspection confirms that the nested geometry and wake paths do not
   re-enter `QuerySlots` or invalidate those references.
6. Reuses local scratch capacity during this fixed-step pass. Free flight and
   quiet worlds do not allocate the per-event membership scratch.

The final hash combines the body pair and primitive pair through an odd
multiplication before the avalanche, replacing the first experiment's simple
XOR. This avoids obvious compound-key cancellation. Exact array equality still
resolves collisions, and lookup order never supplies physical ordering.
`INTEGRATED_SCHEDULING.patch` preserves the exact reviewed production diff.

No previous-event manifold, actual-contact result, unvalidated adjacency edge or
old TOI is reused. A changed trajectory retains the original invalidation policy.
Generation checks are unchanged; these indices never escape one native step.

## Direct verification of the scheduling mechanism

A cache-only observer reconstructs the **original** dirty-key union after the
optimized query loops finish. It repeats current tree candidate queries but
performs no extra geometry, and compares the original sorted unique keys against
the actual optimized list before rescheduling.

Across ten unchanged native fixtures it compared **252 actual events**, reducing
871 pre-unique entries to the same 307 unique keys, with zero mismatches. One
observed key used a nonzero child-part index. These fixtures did not produce a
selected event with neither endpoint changed; that coverage limit is explicit,
and the original selected-retirement clause remains in the source.

The same observer binary was then run with a deliberately missing key. The
unchanged impact-robustness fixture failed with 54 detected union mismatches and
exit status 1. This is an intentional negative control, not a production defect.
Its failure and the successful positive execution are both preserved. The
observer/environment switch exists only in the recorded cache source; it is not
an engine API or production path. Observer costs are never performance evidence.

## Native correctness and controls

The exact same test-object bytes used for accepted-baseline/refactor validation
were relinked against the optimized production archive. All ten pass:

- Impact timing: 3197 checks.
- Contact cache: 13327 checks, including the unchanged independent fixtures.
- Kinematic: 96 checks.
- Physics continuation: 112 checks, including actual restored physical/query state.
- Contact lifecycle: 1076 checks.
- CharacterMotor: 32 checks.
- Joints: 29 checks.
- Finite-mass particle contacts: 77 checks.
- Rigid motion: 10239 checks.
- Impact robustness: 4412 checks.

Eight output streams are byte-identical to the original baseline. The other two
retain only their four explicitly printed timing differences; raw lines and
values are preserved. No output was overwritten to conceal a physical change.
These fixtures do not constitute new universal or cross-process save coverage.

The original four smaller workloads also pass, with exact physical digests:

| Workload | Digest |
|---|---|
| Free 1024 | `beba012f5dfe2273` |
| Pile 256 | `63b0f10cd668de91` |
| Active machinery 256 | `76999a0d1f196c6b` |
| Naturally sleeping machinery 256 | `294c73ad4249153b` |

Actual driven joints and impulse wake controls remain passing. Measured
owner-thread allocation calls/bytes in these four workloads are unchanged;
these are scoped observations, not universal allocation guarantees. Their
functional runs overlapped Windows ICU work, so their timings are excluded from
performance conclusions.

## Final strict matched measurements

Intel Core i7-4790, GNU 15.2.0, Release `-O3 -DNDEBUG`. The exact unchanged M91
room1024 scene ran through the ordinary Application/RuntimeWorld benchmark for
270 fixed steps at the original synthetic 60 Hz cadence, with profiler enabled,
hidden rendering and watch tour disabled. Windows was paused; Android compilation
had finished; root and other agents held heavy work during both runs. Each child
also has a native maximum-RSS receipt.

| Metric | Accepted baseline | Final optimized source |
|---|---:|---:|
| Median fixed step | 25.234815 ms | 25.337992 ms |
| p95 floor statistic | 366.814531 ms | 349.999855 ms |
| Maximum fixed step | 1899.674751 ms | 1892.499906 ms |
| Process wall time | 53.106494 s | 51.622092 s |
| Child-process maximum RSS | 410508 KiB | 408156 KiB |

This individual matched pair is mixed: median is 0.4% higher, p95 4.6% lower and
maximum effectively unchanged (0.38% lower). It does **not** establish a meaningful
worst-step improvement, a general speedup, or a demonstrated memory saving. We did
not repeat the workload until a favorable maximum appeared.

All 270 physical digests and recorded physical counters match exactly. The
complete digest includes every live body's identity, pose, velocities and sleep
state. Final digest is `192f0f4bed728953`. Actual endpoint quality is identical:
1024 authored loose pieces, 2475 contact points, deepest penetration
`0.007217108528522143` m, zero uncertain/unsupported candidates. There are 1285
total bodies at the worst step, not merely the 1024 loose pieces.

The worst step retains 51 chronological events, 501599 TOI queries and 46950 impact
iterations. Final inclusive profiler costs remain approximately 855 ms geometry,
599 ms island discovery, 309 ms TOI and 169 ms impact solve. These categories
overlap and must not be summed. They show that real repeated geometry and the
near-global contact/event cascade still dominate. No collision work was dropped.

`FINAL_MATCHED_RESULTS.json` and `FINAL_WORST_STEP_PROFILE.json` preserve these
observations. An earlier isolated experiment overlapped host source compression
in its baseline window; its original files remain separate and explicitly
confounded. It is not substituted for final strict measurements.

## Remaining limitation and next boundary

A local event can connect nearly the entire pile. Its solve changes many
trajectories, which invalidates many incident collision predictions. The exact
bookkeeping savings do not eliminate this chronological dependency.

Persistent candidate reuse would need conservative proxy-growth coverage and
explicit invalidation; adjacency is not contact truth. Parallel event queries
would need read-only prepared pose/shape contexts and isolated worker scratch,
followed by deterministic owner publication. Concurrent writes into the current
shared pose cache would be incorrect. Neither capability is claimed here.

B-01 remains **OPEN**. This checkpoint neither changes the original curved-mesh
CCD failure nor resolves it by weakening collision fidelity.

## Evidence and reproduction

- `INTEGRATED_BUILD.json`: exact final production module/archive/binary hashes.
- `FINAL_MATCHED_RESULTS.json`: strict matched metrics and physical comparisons.
- `INTEGRATED_CONTROLS.json`: unchanged physical and allocation observations.
- `DIRTY_WITNESS.json`: positive actual-event key comparison and negative control.
- `raw/integrated-checks/`: same-object correctness executions.
- `raw/observer-checks/`, `raw/dirty-negative/`: bounded mechanism proof.
- `raw/strict-baseline/`, `raw/strict-integrated/`: full original CSV/profiler,
  endpoint, usage and process logs.
- `reproduction/`: exact sources, patches and compile/link/workload drivers.

No binary archive or executable is committed. The candidate archive's embedded
identity belongs to the recorded dirty Experimental build; explicit hashes
identify the tested source. No new public API, human acceptance, Windows timing
acceptance, Alpha declaration or main promotion is implied. Protected history,
existing games, fluid implementation and independent research are unchanged.
