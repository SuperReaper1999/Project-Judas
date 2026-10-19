# Pre-Alpha: dense Physics Stress Lab scalability

This report describes checkpoint `8f6bd18b`. The subsequent measured support
correction and rejected precise-geometry experiment are recorded in the
[support continuation follow-up](PREALPHA_DENSE_SUPPORT_FOLLOWUP.md).

## Status and scope

**The requested 24–30 FPS target is not met.** This is a measured, physically unchanged optimization checkpoint. The numerical CCD experiments described below were rejected; subsequent coherent-geometry research remains isolated and unadopted. It is neither Alpha qualification nor new human acceptance.

The operator accepted the preceding curved-mesh correction and authorized its promotion to main. That accepted baseline is `e30d5f9bb72a1a341d3eff7485b8d6b3eb6f8ea9`. This follow-up develops on Experimental and targets **3** in the existing Physics Stress Lab. Main promotion of this follow-up requires separate operator acceptance.

The workload is unchanged: **1,024 loose pieces, 1,152 dynamic bodies, 1,285 total bodies and 96 joints**, at the existing **60 Hz** physics cadence. There are no population cuts, disabled contacts, relaxed CCD, hidden sleeping, increased solver budgets or new game-specific engine paths.

Raw records, exact source snapshots, numerical investigations and validation logs are indexed in [the evidence directory](evidence/prealpha-dense-scalability-20261019/README.md). Historical evidence and independent fluid research are unchanged.

## Why scenario 3 is still expensive

The late pile produces repeated chronological contact events involving large, connected body groups. Each event rechecks geometry and advances physical state, then invalidates affected future collision queries. This is more than rendering many boxes or independently simulating many machines.

In the matched baseline's step 270, the profiler recorded **51 impact events, 501,599 impact queries and 46,950 impact search iterations**. That step took 1,748.516 ms; 1,727.872 ms was in the solver/impact phase. The candidate executes the **same events, searches, contacts and physical results** more cheaply, but does not eliminate this event amplification.

The first native run also ruled out an earlier misleading rendering observation: the actual host uses the GTX 970 driver, not the software-rendered capture. Native opaque rendering was approximately 3 ms in the inspected workload. Physics remains the demonstrated bottleneck. This does not constitute a general GPU benchmark.

A read-only residual observer captured all 3,520 impact events without reaching its recording cap. Large contact systems exhibit remaining coupling after the unchanged ten velocity sweeps. In a preserved 969-body / 2,379-row actual event, kinetic energy decreased from 29.64094 J to 27.35156 J. The evidence does **not** establish a contact energy-injection defect. Raising iteration counts is costly and was not adopted.

A separate investigation established a temporal-observation discrepancy in captured near-tangent box pairs: binary32 quaternion/pose observations can change their apparent separation between closely spaced times. Exact rational checks distinguish those observations from the continuous source trajectory. Several interval-certified retiming experiments were evaluated, but none established a coherent, economical correction. **No numerical CCD correction from those experiments is merged into this fast-path checkpoint.**

## Implemented generic changes

### Certified primitive predicate filters

[ContactSatFilter.h](../src/ContactSatFilter.h) and [SpherePredicateFilter.h](../src/SpherePredicateFilter.h) avoid constructing expensive outward interval expressions only when a conservative arithmetic bound proves that the original interval path would already resolve the same strict sign.

These filters preserve the original interval's branch, including whether exact arithmetic is required for later response-gap construction. A true zero, insufficient bound, unsupported magnitude, invalid input or non-nearest floating-point rounding mode executes the existing interval/exact path. Neither helper supplies an alternative contact tolerance, normal, witness or response law.

The domain/range/rounding derivations are documented alongside the private implementations. Sphere/box uses a narrower source domain than box SAT because its homogeneous predicate has higher degree. Response construction and exact fallback arithmetic remain unchanged. [Contacts.cpp](../src/Contacts.cpp) constructs the interval pair lazily when it is actually required.

### Persistent support classification

[ImpactSupportSets.h](../src/physics/ImpactSupportSets.h) maintains the existing sorted, unique support/spinning primitive-pair sets using binary search. It avoids scanning a full list to remove an absent key and inserting a duplicate only to remove it again.

The original per-pair publication order is preserved. Moving a pair between classifications still removes it from the other set. A deliberately broken erase path is detected by the negative control.

## Matched native Linux measurements

Host: Intel Core i7-4790, four physical cores / eight logical CPUs, approximately 16 GB RAM; NVIDIA GeForce GTX 970, driver 580.178.04. Release build, GCC 15.2.0. Full compiler, platform, source/archive/binary identities and timestamps are retained in the snapshot manifests. The initial sandbox GPU probe failed; the actual native run log independently identifies the NVIDIA renderer. Host desktop activity is recorded separately rather than claimed to be zero.

The following pair used the same native application, project, fixture, fixed timestep, **profiling enabled (`--profile 1`)** and **270 actual physics steps**. They cover 4.5 seconds of simulated time, including the developing expensive pile. Exactly one fixed step is requested per synthetic benchmark frame; these frame counts are **not FPS evidence**.

| Fixed-step cost | Accepted baseline A2 | Combined fast paths A1 | Observed reduction |
|---|---:|---:|---:|
| Mean | 98.175 ms | 66.612 ms | 32.2% |
| Median | 27.063 ms | 24.074 ms | 11.0% |
| p95 | 358.157 ms | 210.307 ms | 41.3% |
| Worst | 1,748.516 ms | 1,089.441 ms | 37.7% |

This is one matched pair, not a universal bound or a statistical significance claim. A prior baseline A1 independently produced 93.509 / 25.425 / 339.804 / 1,695.193 ms for mean / median / p95 / worst; variability is preserved rather than hidden.

A separate pair disabled profiling explicitly (`--profile 0`) for both variants. It used the same 270-step simulation horizon and produced exact physical/work/endpoint parity again:

| Unprofiled fixed-step cost | Accepted baseline A1 | Combined fast paths A1 |
|---|---:|---:|
| Mean | 92.742 ms | 63.033 ms |
| Median | 28.424 ms | 23.004 ms |
| p95 | 338.077 ms | 213.605 ms |
| Worst | 1,658.782 ms | 942.047 ms |

These are two separate single-pair observations, not a repeated statistical trial. Do not mix profiled/unprofiled variants or infer the profiler's isolated overhead by subtracting their timings. Unprofiled maximum process RSS was 177,236 / 177,992 KiB for baseline/candidate; this is process memory, not a measured physics allocation reduction. The unprofiled pair also falls far short of the real-time target.

**All 270 physical/work rows match exactly**, including trajectory digests, counts, speeds and event/search work. Final digest is `192f0f4bed728953` for both. The actual endpoint geometry also matches exactly: all 1,024 loose pieces observed, 9,327 candidate pairs, 2,475 contact points, 0 uncertain/unsupported pairs, deepest endpoint penetration **7.217 mm**. That endpoint is not an all-time penetration guarantee.

### Real application cadence: target still failed

The separate real-time captures requested a 35-second wall window. The baseline completed 289 steps over 36.15 observed application seconds; the candidate completed 310 steps over 50.12 seconds because an expensive catch-up frame overshot the requested window.

Their different physical horizons make aggregate FPS an **invalid before/after speedup comparison**. Their raw observations are retained: baseline mean output approximately 1.33 FPS, candidate approximately 1.02 FPS; median-interval rates approximately 5.87 and 6.11 FPS respectively. Neither run is remotely close to the requested real 24–30 FPS.

The candidate reached the existing eight-step catch-up branch on 36 frames. Its bounded retained profiler records 2.760 discarded simulation seconds; this is explicitly a retained-history observation, not a fabricated whole-run counter. Only 5.167 simulated seconds progressed in 50.116 application seconds. Faster synthetic frame counts must not conceal that slowdown.

At approximately 3 ms rendering cost, sustaining 30 output FPS while advancing physics at 60 Hz requires roughly **15 ms average per physics step**, with bounded tails. A 33 ms physics step alone would not establish 30 FPS. The late multi-second tails are still the principal remaining limit.

## Physical qualification and negative controls

The combined production candidate passed **27 focused invocations**: primitive SAT and sphere certificates; support sets; impact timing/solver/robustness; contact and collision geometry; original curved-mesh arrival/continuation; rigid motion and contacts; CharacterMotor; ragdolls; joints; kinematics; Boolean contacts; sphere impact bounds; continuation and validation/build-registration checks. This is a focused pass, not a full production-suite claim.

The three deliberately broken controls fail as required: omitted SAT bound, omitted sphere bound and omitted support-set erase. Sphere testing includes 200,000 deterministic predicate rows, independent exact expansions, true zero/tangency cases, magnitude limits, nonunit/invalid quaternions, positive margins, public response/Boolean queries, prepared orientations and reverse witness ownership. A separate 240,026-row full primitive response/Boolean corpus matches the original exactly.

A diagnostic observer outside timed physics spans additionally records all 271 states from step 0 through 270. All loose bodies remain finite and within the finite floor interior, and the sampled gravity admits the reported common-plane potential. Loose-piece total mechanical energy starts at 32,346.318 J and ends at 26,254.667 J; it never exceeds its initial value. This is not whole-world energy certification.

Existing physical limitations remain visible: minimum analytic floor clearance during that trajectory is **−63.582 mm**; periodic native pair observations find a maximum sampled penetration of **57.799 mm**, with the final native depth 7.217 mm. No new all-time 10 mm gate is invented. Pair geometry is sampled every ten steps, so unsampled maxima are unknown. The diagnostic run retains exact physical parity with the original.

## Approaches investigated and not adopted

| Experiment | Actual result and disposition |
|---|---|
| Contact-graph BFS solver ordering | Direct controls passed; live run regressed severely and completed only 182 steps before its wall limit. Maximum observed step 6.865 s. Rejected. |
| Previous support-force guess retention | Changed trajectories; mean 136.442 ms / p95 609.249 ms. No practical improvement. Rejected. |
| Four-point manifold block normals | Actual event's ten-sweep residual slightly worsened; setup plus solve cost increased. Rejected. |
| Frozen-friction normal Schur solve | Hundreds of ms, incomplete convergence and Coulomb violations after correction. Rejected. |
| Safeguarded full-law Anderson iteration | More work for worse residual than the ordinary comparison at similar cost. No measured practical win. |
| Parallel TOI queries | Exact query/physical parity obtained, but modest live gain did not address chronology. Broader frontier prefetch regressed worst-step cost. Not adopted. |
| Isotropic inertia shortcut | Exact trajectory parity, insufficient measured benefit to justify the extra branch. Not adopted. |
| ISA-specific FMA certificate | Exact physical parity, insufficient incremental benefit over generic SAT. Not adopted. |
| Continuous-motion CCD A1 | Changed trajectories; endpoint penetration increased to 20.875 mm and fallback semantics were not complete. Preserved as research, not adopted. |
| Continuous-motion CCD A3 | Positive/negative interval-helper controls pass, but all 16 separated replay attempts reach the old eight-small-advance heuristic and fall back. All 18 returned native times remain unchanged. Rejected. |
| Continuous-motion CCD A4 | Uses conservative continuous advances without that heuristic, retaining the existing 64-advance cap. All 16 separated queries exhaust that cap and literally retry native; all 18 returned times remain unchanged. Approximately 0.82 s for the diagnostic replay. No full-world run or production adoption. |

The evidence preserves original compilation/fixture-authoring failures separately from physical failures. As-created prototype metadata is retained with its original timestamp; later result receipts determine what was actually executed.

A3/A4 demonstrate why a sound separation helper alone is insufficient: repeatedly advancing toward an ill-conditioned grazing encounter is still expensive and can make no useful retiming progress. Increasing the budget was rejected. Further work is investigating consistent geometry observations across CCD, contact discovery and motion re-anchoring; its outcome is unknown and it remains outside production.

## Final checkpoint build and application

The final separate-translation-unit Release runtime, editor, exporter and focused targets rebuild with **zero warnings**. All 27 focused invocations pass again with current embedded source provenance; the measurement helper has 11 passing synthetic unit tests. The unchanged project exports six assets and four scenes. Its actual exported runtime starts from a relocated, read-only package under `/tmp`, runs the default room without script or asset faults and closes its own window normally with exit 0. This is startup/lifetime qualification, not human gameplay or the scenario-3 frame-rate target. See `final-fastpath-build`, `final-fastpath-checks` and `application` in the evidence directory.

The measured frozen candidate still matches all **513 recorded engine/build/fixture source inputs**. Final source and binary fingerprints describe this verified fast-path checkpoint. Further precise-geometry work is isolated and unadopted.

## Reproduction and remaining work

The opt-in [measurement helper](../scripts/physics_dense_measure.py) freezes actual source/build artifacts, records native commands/exits/timeouts, verifies fixture/binary identities and compares physical rows before reporting timing. Explicit `--extra-source` arguments include intended new private headers/tests without sweeping unrelated untracked research into the snapshot. Earlier incomplete experimental source inventories are not rewritten; their linked supplements are retained where relevant. Eleven new synthetic helper unit tests pass, including early-quit, changed fixture/binary, relocated archive/hash, short capture and catch-up accounting controls; these do not count as physics/FPS validation.

See the evidence index for commands, including explicit `--profile 1` for the first pair or `--profile 0` for the separate unprofiled pair. Run workloads serially on the native desktop renderer. The helper rejects early successful exits, short exact captures, changed fixtures and mismatched snapshot binaries. Capture completion does not certify FPS or physical correctness. Timeout/short-run results remain failed captures.

Next work is to evaluate a coherent geometry-observation approach with adversarial controls, then measure any justified implementation over a longer common simulation horizon. A 270-step improvement alone is insufficient: the existing longer real-time capture proves the pile becomes more expensive afterward. Preserve original query fallback boundaries and contact fidelity; do not substitute endpoint-only or sampled “separation” for a continuous proof.

Windows and Android were not requalified in this Linux-focused follow-up. No human acceptance of this candidate or Alpha readiness is claimed. Existing games, historical evidence and independent fluid research remain outside this change.

## Human checklist

1. Open the ordinary [Physics Stress Lab](../projects/physics_stress_lab/physics_stress_lab.judasproj); use **F5** for the watch tour.
2. Press **3**, observe the initial fall and the later dense pile. Expect remaining severe stalls; this checkpoint does not promise 24–30 FPS.
3. Check **1 / 2 / 4**, motor toggle **M**, impulses **J / K** and dropped pieces **P**. Bodies should retain real contacts and natural sleeping/waking.
4. Check moving-mechanism **F6 / F7** save/load, pause/resume and reload.
5. Compare the rendered frame cadence and actual simulation progress rather than accepting a slowed simulation as improved FPS.
6. Human-test any later CCD candidate separately after its final numerical/regression report is available.
