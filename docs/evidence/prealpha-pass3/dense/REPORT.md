# Pass3 dense contact and exact primitive derived work

## Scope and result

Accepted starting source is `f647d7fd95f60d3a37dba70a51f4e45dc41c2922`. These changes remove redundant primitive geometry work and replace quadratic event-support membership with an exact sorted lookup. They preserve contact truth, solver row/event encounter order, all numerical gaps, quality settings, CCD and cadence.

They do **not** close B-01: the severe 1,024-body near-global contact event cascade remains. One matched complete run is a useful scoped result, not a general speedup or real-time deadline claim.

## What changed

- `src/Contacts.cpp`: box-versus-sphere full and Boolean dispatch now construct only the reversed context they actually consume. Previously both created a complete unused forward context first.
- `src/Contacts.cpp`: every original interval/exact predicate, axis length check and represented numerical gap remains in its original loop/order. Only the selected response axis is normalized and oriented, rather than normalizing all up to 15 tested axes. Axis selection and all final witness arithmetic remain unchanged.
- `src/PhysicsWorld.cpp`, `AdvanceImpacts`: a separate reusable vector holds a sorted **copy** of actual event pairs. Incident support membership uses exact binary search instead of a linear scan of the entire event-pair vector for every support. The original event pairs, duplicate behavior, discovery order, actual rows and solver order are untouched. The copy exists only in one fixed-step event pass; free flight makes no allocation for it.

No gameplay branches, tolerances, fixtures, materials, iteration/event budgets, sleeping rules or renderer policy changed. No cross-time manifold is reused.

## Matched Linux measurements

The final combined build was measured again after the shared build/extractions were complete. `FINAL_COMBINED_RESULTS.json` records its actual source hashes, native build identity, binary hashes, all-step comparisons and the correctly relinked final control results. It is a separate final receipt; earlier experiments are not relabeled as final.

Intel Core i7-4790, four cores/eight threads, GNU 15.2.0, Release `-O3 -DNDEBUG`, exact preserved M91 room1024 authoring SHA256 `a2caf62a295243d301149202c8a0a5eedafff256d68de0e3cda988846b2e05bf`. Exact frozen clean source/binaries are recorded by the coordinator in `FROZEN_BASELINE.json`. Builds, owned heavy tests and Windows work were serialized with this measurement window.

The ordinary Application/RuntimeWorld benchmark ran 270 fixed steps, synthetic 60 Hz, profiler on, hidden rendering, watch tour off. Endpoint inspection/export is outside measured fixed spans.

| Complete 270-step distribution | Frozen accepted baseline | Isolated exact candidate | Final combined build |
|---|---:|---:|---:|
| Median fixed step |27.0517925 ms |25.8190855 ms |27.3338865 ms |
| p95 floor order statistic |379.672448 ms |372.143396 ms |376.826108 ms |
| Maximum |1938.679356 ms |1925.992117 ms |1887.046761 ms |
| Whole process wall time |54.249002 s |52.953603 s |53.465593 s |

These are **individual matched runs**, with one separate final combined repeat. The final median is slightly worse than baseline while p95 and maximum are slightly lower; these mixed observations do not establish a universal dense-contact speedup. The approximately 1.89 second final worst step remains far outside a real-time budget. At the start of the final timing window the coordinator ran approximately one second of metadata-only build-identity queries and file hashing; no other physical simulation, build or Windows workload ran. This further rules out a strong claim from tiny timing differences.

All 270 trajectory digests and recorded physical counters match exactly. Final digest `192f0f4bed728953`, endpoint population 1024, deepest measured penetration 7.2171085285mm, contact points 2475, no uncertain or unsupported endpoint candidates. Both runs have exactly 501599 TOI queries, 51 events, peak 973-body/2517-row impact island and 2798292 geometry predicates in the worst step.

Worst-step inclusive profiler costs (baseline→candidate): geometry883.736→871.671ms, discovery620.035→621.206ms, TOI313.971→320.548ms, impact solve162.537→167.276ms. They overlap and must not be summed. The unchanged 626049 geometry-call count demonstrates that no collision work was silently dropped. Only five exact fallbacks occur; this is not an exact-arithmetic fallback explosion.

Observed process resident memory in the worst captured frame is 377745408→376741888bytes, with the same 70809112 byte profiler reserved estimate. This includes application/driver/allocator state; the ~1MB difference is **not** claimed as an optimization or live-scratch measurement.

The final combined trace retains the same 270 physical digests/counters, actual endpoint population, penetration, contact counts and uncertainty results as the frozen baseline. Its worst-frame observed RSS is 374,509,568 bytes with the same 70,809,112 byte profiler reserve estimate. These are process observations, not a demonstrated memory reduction.

### Smaller controls

Original free 1024, pile 256, active machinery 256 and naturally sleeping machinery 256 workloads all pass their original physical gates on both experimental archives and on the final shared binary, including real motor movement and impulse wake controls. Their complete trajectory digests match baseline. Medians remain in the sub-millisecond range; no claim is made from small timing differences.

The first final-run driver also invoked the old shared scaling executable: its stale timestamp/hash was detected before any final-control claim. Those four baseline-binary duplicate invocations remain under `raw/final-combined/` and are **not** counted as final-source validation. The coordinator then relinked only `judas_physics_scaling_diagnostics`; four narrow reruns under `raw/final-controls/` all pass with the same physical fields and digests. The correction did not require another dense run.

## Bounded stage/count observer

A single additional accepted-baseline experiment adds aggregate timers and derives the number of linear membership comparisons from each original `std::find` result. It does not increment a counter inside every comparison or change the lookup algorithm. All 270 physical digests/counters still match the frozen baseline.

At the worst step:

| Observed stage/count | Result |
|---|---:|
| Support membership lookups |71,980 |
| Original exact pair-equality comparisons |57,496,461 |
| Successful memberships |68,013 |
| Entire support retirement, including continuation proofs |57.920 ms |
| Discovery candidate bounds/tree queries/copy/sort, inside island discovery |107.730 ms |
| Dirty-pair construction and rescheduling, including TOI work |576.826 ms |
| TOI inside that rescheduling work |311.377 ms |

The linear membership work is genuine, but it is not the principal two-second stall. The approximately 265 ms difference between the rescheduling and nested TOI observations identifies another material derived-work cost. Candidate gathering, deduplication and the lookup-only ordered event index deserve a future bounded investigation. An exact hashed index could preserve vector-owned event order; that is a proposal, **not an implemented or measured improvement**.

Blindly retaining contact adjacency across changed event-time poses is unsafe: an edge can appear or disappear as bodies rotate/separate. A conservative candidate graph could be reused only while its proxy-growth coverage remains valid, with dirty incident candidates added deterministically. The current evidence does not establish that this would remove the dominant geometry/event cascade; no speculative graph or contact-truth cache was shipped.

Observer timings include their extra aggregate instrumentation and are not used for a speedup claim. Stage categories overlap. `OBSERVER_RESULTS.json` and its unchanged physical comparison preserve the actual result.

## Exact primitive contact equivalence

`PrimitiveWitnessProbe.cpp` runs 16000 deterministic box/sphere fixtures, both argument orders, prepared/unprepared orientations and full/Boolean queries: 128000 queries total. It includes represented exact/one-ULP boundaries, deep coincidence, nearly parallel slender bodies, local offsets, arbitrary non-unit orientation representations and large origins.

The observer serializes explicit fields, never structure padding: contact count/uncertainty, normal, precise gap, all local anchors/witnesses, debug point, separation state and every existing geometry diagnostic counter. Both candidate stages produce **the identical 11718912 byte stream** as the unchanged baseline; SHA256 `f017f2af3470e5382f33f469b24c60c46cf9dd8b342263121fb2ce079ed84de1`, FNV `7debc98c624233a5`. Actual queried manifolds include 17708 positive and 46292 negative results. Boolean/full agreement is checked on every fixture.

Three alternating-order kernel observations have median 307.377 ms baseline versus 300.068 ms combined candidate. This is this fixture only; it is not whole-engine or game FPS evidence. Exact original source and each experimental source are preserved separately.

## Remaining mechanism and practical limit

At each chronological event, discovery recomputes actual zero-margin contacts at the changed event-time poses. In a dense pile a small local impact can reach almost the entire graph; the ensuing solve changes many trajectories and invalidates hundreds of thousands of incident TOI predictions. Fifty-one events therefore repeat near-global graph/geometry work. Initial broadphase/narrowphase already uses four worker lanes; simply enabling threads does not remove that serial physical dependency.

Prior exact-pose memoization remains rejected: low 2352 hits/170326 stores, 3.4 MB retained and no improved worst latency. No repeated unsuccessful model was reinstated. Bounding, batching or persistent constraints would require a justified change to the connected-contact integration model, with separate fidelity evidence; dropping events, contacts or CCD is not a permitted latency fix. This pass establishes only exact derived-work savings and retains the global event-cascade limitation.

## Provenance and reproduction

- `SOURCE_AT_ASSIGNMENT.json`: source hashes, host/compiler/environment, accepted parent.
- `raw/baseline/RESULTS.json`, `raw/candidate/RESULTS.json`: raw commands, timestamps, binary/fixture hashes and summaries.
- `raw/baseline/*`, `raw/candidate/*`: unchanged input workloads, CSV samples, M56 profiler, endpoint JSON and logs.
- `MATCHED_RESULTS.json`: all 270 per-step physical-field comparisons and smaller-control digest matches.
- `FINAL_COMBINED_RESULTS.json`: final shared source/build identity, all-step physical equivalence, mixed timings, RSS and the four relinked final controls.
- `WORST_STEP_PROFILES.json`: bounded raw counters and memory observations.
- `OBSERVER_RESULTS.json`: comparison counts and bounded stage attribution, with unchanged physical results.
- `PRIMITIVE_RESULTS.json`: all three alternating-order probe trials and explicit byte comparisons.
- `BUILD_VARIANTS.json`: compile/link commands, parent archive hash and actual experimental source hashes. Isolated experiments retain the parent's embedded build identity; they are explicitly identified as changed-object experiments, **not** mislabeled clean parent builds. Final combined native verification is recorded separately in `FINAL_COMBINED_RESULTS.json` and the coordinator's final evidence.

```
python3 .cache/prealpha-pass3/dense/run_workloads.py --live FROZEN_LIVE --scaling FROZEN_SCALING --output NEW_BASELINE
python3 .cache/prealpha-pass3/dense/build_variants.py --variants baseline single-context selected-normal --engine
python3 .cache/prealpha-pass3/dense/run_workloads.py --live .cache/prealpha-pass3/dense/build-selected-normal/judas_physics_stress_live_benchmark --scaling .cache/prealpha-pass3/dense/build-selected-normal/judas_physics_scaling_diagnostics --output NEW_CANDIDATE
```

Windows performance/behavior is not inferred from these Linux samples. Protected history, existing projects, independent research and fluid source are untouched by this workstream. No human acceptance is inferred.
