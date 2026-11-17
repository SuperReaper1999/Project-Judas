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

| Area | Demonstrated development workflow | Boundary |
| --- | --- | --- |
| Native project/scene authoring | Actual editor create/edit/undo/save/reopen/Play/Stop; failed startup and resource retirement contracts | Automated native Linux desktop evidence; no inferred human acceptance |
| SDK and project delivery | 15 matching tools, checksum verification, 39 tool operations, source-hidden consumption, moved editor/CLI exports | Use one matching SDK's tools/runtime; never mix arbitrary snapshots |
| 2D, UI and utility AI | Willow Town gameplay/life/absence/cold restore, responsive editor/runtime layout and packaged UI oracle | Small viewport and scrolling are tested; IME/controller hardware separate |
| 3D assets/materials | Model import/reimport, collision/navigation/environment cooking, runtime rendering and dependency closure | Explicit tested formats/fixtures; no blanket promise for every importer input |
| Animation and IK | Shared authoring/runtime poses, skinned rendering, actual animation project export, fresh-process phase/motor/event continuation | The 129-character rendering crowd has 16 active independent IK actors / 64 targets; costs are content-dependent; no time-loss concealment |
| Flat/planet terrain | Author/cook/export/relocate, edits, cold settlement/traversal/load, material dependencies, navigation/origin/particle restoration | Bounded workshop/residency workloads; no arbitrary-world streaming guarantee |
| Physics, wheels and powertrains | Committed production MK2, original contact/CCD/joint tests and continuation/lifecycle gates | Bounded CCD/energy/spin limits remain; no experimental core/contact-material merge |
| Particles and water | Actual combined 5000 GPU-particle contacts; CPU/portable policies and water quantity/coupling fixtures | Advanced water remains optional and opt-in; source-preview project availability differs from curated SDK contents |
| Saves and world transitions | Atomic/corrupt-data tests, scene transitions, world continuation, cold processes and repeated application sessions | No compatibility promise with retired historical project/API formats |
| Networking and audio | Actual loopback application ownership/lifecycle and native audio application contracts | Dummy-audio tests do not prove physical output; WAN/hostile-load/real devices separate |
| Resources and outputs | Shared host/runtime ownership, failed build cleanup, generation/lifetime checks, guarded build/capture storage | Sampled storage guard is not a filesystem quota or permission for automatic deletion |

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
owner; the full native material contract now runs in Windows qualification.
These failed runs remain recorded as earlier evidence, rather than overwritten.

## Target tiers

| Target | Current evidence | Support statement |
| --- | --- | --- |
| Linux x86-64, Kubuntu/Ubuntu 26.04.1, GTX 970 / NVIDIA 580.178.04, modern GL | Real desktop editor/runtime/SDK and whole-application captures | Primary current-host development qualification; requires matching host ABI/driver dependencies |
| Ubuntu 24.04 x86-64 CI reference | Current candidate passed native gates, SDK authoring/relocation, API checks and declared Ubuntu 24.04 / GLIBC 2.39 ABI checks with software GL | Reference build/CI evidence; local GCC 15 / GLIBC 2.43 SDK is not thereby Ubuntu 24-compatible |
| Portable GL policy on tested NVIDIA hardware | Actual native rendering/particle contracts, including shadow-depth equivalence | Tests the portable code policy on that driver, not every physical GL 3.3 / GLES device |
| Windows x64 MSVC | Native Windows 2025 / MSVC SDK build, selected tool/physics/network/UI/AI contracts, full material contract and SDK loader inspection passed for the current candidate | Build/tool scope qualified; no editor/GL/export or physical hardware qualification; material kernels use owner-only execution where no-throw transfer is unavailable |
| Android | SDK/NDK/ADB installed; no phone in current USB inventory, complete JDK/emulator absent; device qualification pending | Existing optional runtime/APK workflow and historical scoped acceptance remain; new snapshot is not device-certified |
| Browser | Firefox installed; current Emscripten toolchain absent | Explicit opt-in Experimental workflow; no new snapshot browser-runtime qualification yet |
| Other OS/architectures/devices | No new native evidence | Not included in this development-readiness verdict; see existing support documentation for historical scope |

Historical Windows/Android/browser acceptance is not withdrawn and is not silently
extended to this snapshot. A cross-build or a toolchain directory is not native
execution. Current physical controller/audio/IME checks require the relevant
hardware/session; unsupported targets need their own qualification environment.

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
development within this scope. Qualify additional platforms/hardware as their
actual environments become available; do not inherit older acceptance silently.

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
| Ubuntu/Windows qualification | [Exact non-publishing run](https://github.com/SuperReaper1999/Project-Judas/actions/runs/44101575648), scope in target table |
| Reproduction | Source checkout: [test harness](TEST_HARNESS.md), `scripts/qualify_sdk.py`, and [performance policy](WHOLE_ENGINE_PERFORMANCE.md) |

The [initial campaign receipt](audits/platform-20261116/evidence/stage-e-qualification.json)
is a historical snapshot including failed attempts and earlier dirty identities;
its pending fields are superseded by the final checkpoint table above. The
[performance receipt](audits/platform-20261116/evidence/stage-e-rendering.json)
retains actual measured executable/input identities. Substantial replacement, main promotion,
selected consumer SDK change or broad cleanup still requires operator approval.
