# V2 development-readiness boundary

Verdict: **development-ready for native Linux engine/editor/JudasJS SDK projects
within the qualified boundaries below**. Clean code candidate `eb212368` passed
the final qualification recorded below. This is an Experimental development baseline, not main promotion,
a published release or inferred human acceptance. Accepted predecessor `ad41f73f`
passed all three CI workflows; the current candidate also passed all three.
The separate hosted target run is recorded below. Qualification receipts identify the actual measured builds.

Development-ready means a reproducible, usable engine/editor/SDK for the supported
scope below, with actionable diagnostics and known limits. It does not mean every
stress workload meets 120 FPS, every device is certified or every preview is complete.

## Integrated capability and evidence matrix

Tests establish specific contracts. A unit test, native application check, source-
hidden SDK workflow, performance capture and human review are distinct evidence.
The accepted predecessor passed 207 CTest cases and 16 additional workflows.
The current clean candidate passed **209/209 cases in 471.71 seconds**, with all
195 native executables reporting its exact source identity. Python tooling passed
206 cases, with three explicit JDK/Ninja skips; the API contract checker passed.
The source-hidden SDK passed its 39 tool operations and actual editor/runtime
application workflows. Its moved IK consumer restored tick 141, 16 enabled IK
actors and 64 targets after a real resumed solve. `ikStatus.ready` reports asset
availability; residuals may be empty before that solve publishes them.

The initial `79acc124` run passed 207/209 and exposed two fixture defects: a
cwd-relative cold-project path and a transient terrain-image visibility failure.
The repaired fixtures use the normal engine-root resolver and an owned image/depth
target, retaining all continuation, visibility, pixel and depth assertions.
The initial Windows build also exposed an invalid no-throw material-transfer
assumption. Platforms without safe transfer run the same ordered kernels on the
owner; the full native material contract ran in the recorded Windows qualification.
These failed runs remain recorded as earlier evidence, rather than overwritten.

## Target tiers

Linux is the sole official platform. Windows is unsupported. The Windows
results below are historical evidence. See the [Windows policy](WINDOWS.md).

## Performance and reliability limits

The retained change combines only compatible opaque shadow parts; full native
shadow-depth/triangle equivalence and fallback contracts pass under both tested
policies. Main colour geometry, poses, culling, lights/cascades and simulation
ordering remain. Three interleaved 20s / 45s / 20s profiler-off pairs improved median
p99 animation 11.302→9.747 ms, IK 14.569→12.134 ms and combined 6.553→6.267 ms. All 18 trials
had zero incoming clipping, discarded backlog or catch-up caps. Candidate worsts
were 13.452/33.142/8.568 ms respectively. Actual populated scenes were inspected.

Animation/IK still miss 120 FPS; IK exceptional jitter remains a known limit. A
complete 5154-frame shallow IK trace had no lost records/time and max 14.977 ms;
it does not erase the 33.142 ms lean hitch. An earlier incomplete 78 ms profiled frame
was not reproduced or attributed to a speculative engine/driver/OS cause. Callback
observation max 0.411 ms in the complete capture does not explain that earlier gap.
CPU/GPU overlap, throughput versus latency, loading and diagnostics stay separate.
No simulation scheduling policy or fidelity was weakened to obtain these results.

These are workload/hardware-specific development budgets, not promises for arbitrary
scenes. Supported content still needs game-specific profiling. Long-duration soak,
hardware audio/controllers/IME, hostile WAN/load, every import variation and broad
vehicle/material exceptional workloads remain unqualified. None is concealed as
passing. Severe defects in the supported author/save/play/load/export contract
would block readiness; no such unresolved reproduced defect remains in these
qualified workflows.

## Exit decision

The native-Linux verdict is supported by the shared runtime ownership, integrated
editor/SDK workflows, actual application/persistence checks, meaningful fidelity
regressions, observed performance and bounded output policy. Continue feature/game
development within this scope. Qualify additional Linux environments and hardware as they become available.

The exact qualified code candidate is
`eb2123684e77494007a4452cee6bb0918f8b4eff`, build
`2.0.0-alpha.1-Experimental-eb21236-clean-6337fffb54e9`. The final documentation
checkpoint does not change its runtime or test code. Primary Linux whole-application
performance evidence was measured before the portability/fixture repairs; the
GCC material worker path and measured rendering implementation are unchanged.

| Final checkpoint check | Evidence |
| --- | --- |
| Local native desktop | 209/209, 471.71 seconds; 195 clean native identities |
| Source-hidden matching SDK | 15 tools; 39 tool operations; author/Play/Stop/save/reopen/export/move/cold restore; checksum invariant |
| Regular CI | Documentation, tooling and focused engine gates passed for `eb212368` |
| Historical Ubuntu/Windows qualification | [Exact non-publishing run](https://github.com/SuperReaper1999/Project-Judas/actions/runs/44101575648), scope in the baseline receipt |
| Reproduction | Source checkout: [test harness](TEST_HARNESS.md), `scripts/qualify_sdk.py`, and [performance policy](WHOLE_ENGINE_PERFORMANCE.md) |

The [baseline receipt](qualification/baseline.json) records the exact qualified
code, workflows, CI IDs, scoped failures and limits. The
[rendering receipt](qualification/rendering.json) retains actual measured
executable/input identities. Superseded audit diaries, captures and pending
snapshots are not active developer documentation. Source/provenance records and
CI retention carry their explicit scopes. Main promotion, selected consumer SDK
changes and releases still require their operator approval.
