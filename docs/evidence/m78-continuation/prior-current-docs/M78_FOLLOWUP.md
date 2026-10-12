# M78 corrective follow-up — working candidate

Frozen M77 checkpoint: `9e77b2226edcf217ab789fe4f0fea42a3415a72d`, **NOT HUMAN VALIDATED**. This follow-up is authorized product/test/documentation correction, **uncommitted**. The [original M78 audit](M78.md) and its evidence remain frozen; they describe the previous read-only audit, not today's changed source.

Evidence: [follow-up directory](evidence/m78-followup/), [declared gates](evidence/m78-followup/ACCEPTANCE.md), [original M77 reconciliation](evidence/m78-followup/M77_REQUIREMENTS.md).

| Finding | Current status | Correction / remaining boundary |
|---|---|---|
| PHY-001 | Free-body defect fixed in working source; integration regression remains | Midpoint angular drift predictor and world angular-momentum transport; free-body momentum vector and energy convergence checked independently. Constraint/contact residuals remain approximate. M77 corrective stepping regresses on the combined candidate; see REG-001 below. |
| PHY-002 | Substantially improved; not closed | Complete-mesh enclosure advancement certificate and real per-witness clipped contact depths. Unchanged four-rig workload median 1.04 ms, p95 18.88 ms, max 33.79 ms; p95 misses declared 16.67 ms gate, 39/52 bodies sleep at step 900. No forced sleep or fixture retuning. |
| PERF-001 | Corrected source and measured scaling | Prefab ordering uses precomputed original ranks instead of linear scene scans inside sort comparisons. 6,400-object resolve 52–62 ms vs frozen ~1,850 ms; flatten/build are reported separately. |
| TST-001 | Corrected fixture and explicit pressure proof | Four 100-object retained snapshots need more than the default 4 MiB; fixture explicitly declares 16 MiB. Separate unchanged-budget test proves pin reason, full state retention, legitimate consumer shrink, retirement accounting and fresh-instance restore. Engine default unchanged. |
| FLU-001 | Unresolved; contract review explicitly deferred | Original 0.558701396 m/s failure preserved. Independent conditional continuum buoyancy/drag model also exceeds the 0.5 m/s-at-4-seconds deadline. No fluid retuning or threshold relaxation. [Derivation and limits](evidence/m78-followup/FLU-001.md). |
| WORK-001 | Implemented and focused/application tested | Saved-scene/kit dependency discovery and explicit transactional owned-scene navigation rebuild; old assets remain, published kit is not edited. Linux/native Windows 38-check transaction proof and Linux 20-check moved-package application proof passed. Private unpublished scenes cannot be inferred. |
| DOC-001 / REQ-001 | Current status and original-prompt reconciliation updated | M77 is committed but not human accepted. Historical preflight/results preserved; original prompt now available. Original missing-brief gaps remain historical and other missing originals are not invented. |

## Combined-candidate blocker: REG-001

The unchanged M77 29 N s corrective-step case fails on both Linux and native Windows: 119 controller checks, two failures. The actual Balance Lab public-JS `step-trace` case also fails (15 checks, one failure). A foot lifts and lands, but the second step loses stance support and the rig falls instead of returning to standing. No rig, actuator budget, acceptance threshold or controller gain was changed. [Failure and bounded next proposal](evidence/m78-followup/REG-001.md). **This is not a fully passing or accepted candidate.**

## Developer workflow

Terrain panel: save scene changes, save/cook terrain, inspect dependent navigation, explicitly rebuild stale owned navigation. CLI: `judas_navigation_bake project.judasproj --check-dependents` and `--rebuild-dependents`. Published kit scenes report a required owned fork/replacement; no silent private-base edits. Cancellation is checked around each bounded surface bake and before publication. Existing guarded publication preserves old outputs on failure; filesystem-wide crash atomicity is not claimed.

## Validation boundaries

Fresh combined Linux/native Windows results and exact fingerprints belong to the follow-up evidence, not the frozen audit. Linux affected Release build passed with two pre-existing RuntimeWorld warnings; native affected build passed with 102 warnings. Rigid motion (10,217 checks), collision geometry (153), prefab (37), streaming (82), navigation transactions (38), CharacterMotor, physical-animation and spacecraft momentum-contract checks passed on both platforms. Linux editor Edit/Play/Stop preserved the authored scene; offscreen pointer capture is unavailable and is not human input validation. Additional cavity-impact and fluid/rigid-coupling checks passed. Current test-by-test receipts are in [RESULTS.md](evidence/m78-followup/RESULTS.md). Windows VM checks are not physical hardware acceptance. No broad all-184-suite correctness claim is made. Failed/rejected first-order angular, extra plane-bound and tool/configure attempts are retained. Human validation of M77 and this correction candidate remains outstanding.

## Remaining proposals

Do not silently broaden this correction into humanoid recovery, a replacement liquid solver, exact multibody conservation or another profiler project. Post-touchdown corrective-step support handling, tightened mesh-articulation tail latency and an independently derived sustained fluid-settling contract require explicit bounded follow-up. No new milestone numbers are assigned.

## Review packages and acceptance

[Runnable packages and focused human checklist](evidence/m78-followup/HANDOFF.md). The balance package is for investigating REG-001, not a passing recovery certificate. M67 deferred review, M74, M76, M77, Wayfarer boarding/flight and Windows hardware acceptance remain pending. M75 historical acceptance is preserved. Nothing is staged, committed, pushed or tagged.
