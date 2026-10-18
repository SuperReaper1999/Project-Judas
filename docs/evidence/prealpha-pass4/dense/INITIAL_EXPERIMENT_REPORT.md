# Pass 4: exact event-index and dirty-pair work

## Scope

Accepted source is `7da56c2cc6ce7acda827a6e4019d610622b237e2`. This is an isolated changed-object experiment against the coordinator's frozen clean native archive. It does not change contact truth, CCD policy, physical integration, event order, solver rows, iteration budgets, tolerance, sleeping, fixture content or cadence.

**The severe dense-contact limitation remains.** These changes remove redundant scheduling work; they are not a claim that 1,024 active contacting pieces are practical at 60 Hz.

## Mechanism and exact correction

`AdvanceImpacts` previously indexed its vector-owned events through a lookup-only `std::map`, built duplicate four-word dirty keys from both changed bodies, copied the shared query-result scratch vector, then sorted and uniqued the full duplicate stream at every event.

The cache candidate changes only derived work:

1. A hash index locates existing scheduled vector entries. It is never iterated. The original vector still determines encounter order and the original earliest-time/lexicographic rule still chooses events.
2. An exact per-event canonical-body-pair membership set expands each candidate body pair only once. Compound part indices are still expanded completely.
3. Per-scheduled-index dirty marks retain one occurrence of existing keys. Newly discovered keys cannot duplicate because the canonical body pair is expanded once. The final dirty stream is still sorted in the original lexicographic order.
4. The two candidate loops use references to the existing `QuerySlots` scratch vector. All nested geometry and `Wake` paths were inspected; neither loop re-enters `QuerySlots`, so the original vector remains valid until the next outer query.
5. Scratch capacity is retained only within this fixed-step pass. No prior-event contact truth, manifold, TOI or unvalidated graph edge is reused.

The original dirty union is therefore preserved exactly: every incident scheduled key, the selected key even if its impulse is zero, and every currently eligible primitive pair from each changed body's current fat-bound query. Body-pair membership and dirty marks remove duplicates only. Body handles cannot be created/reused in the middle of this native event pass; the scratch does not escape it.

## Experimental evidence

The baseline uses clean accepted native binaries from `FROZEN_BASELINE.json`. The candidate replaces only `PhysicsWorld.cpp.o` in that frozen archive; its embedded build identity remains the accepted parent, while explicit source/object/archive/binary hashes identify the actual changed-object experiment in `BUILD.json`.

Both runs use the exact unchanged M91 room1024 scene and the original ordinary Application/RuntimeWorld live benchmark: 270 fixed steps at synthetic 60 Hz, profiler enabled, hidden rendering, watch tour disabled. Windows VM was suspended. The coordinator disclosed host source-archive gzip work overlapping the baseline window; **the first pair is not strict-idle performance acceptance**. A final serialized pair is required after integration.

| Metric | Accepted experimental baseline | Exact-index candidate |
|---|---:|---:|
| Median fixed step | 30.485791 ms | 24.982529 ms |
| p95 floor statistic | 379.741307 ms | 370.379427 ms |
| Maximum fixed step | 2205.635272 ms | 2003.054355 ms |
| Process wall time | 57.229535 s | 53.867930 s |
| Child-process peak RSS | 411868 KiB | 383684 KiB |

These are individual diagnostic runs, not a universal speedup or a demonstrated memory reduction. The noisy baseline overlap and ordinary process/allocator/driver variation prevent attributing all differences to this patch.

All 270 physical fields and trajectory digests are identical, including event/query/iteration counters, body counts and velocities. Final digest is `192f0f4bed728953`. Endpoint quality is identical: 1024 authored loose pieces, 2475 contact points, deepest penetration `0.007217108528522143` m, zero uncertain/unsupported candidates. The largest step still has 1285 total bodies, 1024 awake pieces, 51 chronological impact events, 501599 TOI queries, and 46950 impact iterations.

All four unchanged controls pass, with exact baseline/candidate physical digests: free-flight 1024 bodies, 256-body pile, 256-body active machinery and 256-body naturally sleeping machinery. They include real driven joint motion and an impulse wake control.

## Limits and next work

The original Pass 3 finding remains: a local impact can reach nearly the entire connected pile; solving it changes many trajectories, which genuinely invalidates incident TOI predictions. No changed trajectory is left with an old predicted event time. No contact is suppressed for performance. Initial narrowphase already uses worker lanes for contact geometry; simply adding more threads does not remove the chronological dependency.

A persistent candidate graph would need explicit conservative proxy-growth coverage and deterministic additions; contact truth still needs current event-time evaluation. Parallel TOI discovery would need a genuinely read-only query context and isolated scratch/counters, not concurrent writes into the current shared pose cache. Neither mechanism is silently claimed here.

## Reproduction

```
python3 run_workloads.py --live FROZEN/judas_physics_stress_live_benchmark --scaling FROZEN/judas_physics_scaling_diagnostics --output NEW_BASELINE
python3 build_variant.py hash-dirty --archive FROZEN/libjudas_engine.a
python3 run_workloads.py --live build-hash-dirty/judas_physics_stress_live_benchmark --scaling build-hash-dirty/judas_physics_scaling_diagnostics --output NEW_CANDIDATE
python3 compare.py NEW_BASELINE NEW_CANDIDATE --output COMPARISON.json
```

Run from the repository root with the recorded Release flags and external cache paths. The coordinator supplies final integrated-source qualification and source/binary fingerprints separately. No production/shared source was edited by this workstream before the coordinator integrated its narrow patch.
