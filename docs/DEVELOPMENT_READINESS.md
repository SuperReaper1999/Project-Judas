# V2 development-readiness boundary

Verdict: **development-ready for native Linux engine/editor/JudasJS SDK projects
within the qualified boundaries below**, subject to final clean-checkpoint identity
verification. This is an Experimental development baseline, not main promotion,
a published release or inferred human acceptance. Accepted predecessor `ad41f73f`
passed all three CI workflows; the next exact hosted-target run is recorded
separately. Qualification receipts identify the actual measured builds.

Development-ready means a reproducible, usable engine/editor/SDK for the supported
scope below, with actionable diagnostics and known limits. It does not mean every
stress workload meets120FPS, every device is certified or every preview is complete.

## Integrated capability and evidence matrix

| Area | Demonstrated development workflow | Boundary |
| --- | --- | --- |
| Native project/scene authoring | Actual editor create/edit/undo/save/reopen/Play/Stop; failed startup and resource retirement contracts | Automated native Linux desktop evidence; no inferred human acceptance |
| SDK and project delivery |15matching tools, checksum verification,39tool operations, source-hidden consumption, moved editor/CLI exports | Use one matching SDK's tools/runtime; never mix arbitrary snapshots |
| 2D, UI and utility AI | Willow Town gameplay/life/absence/cold restore, responsive editor/runtime layout and packaged UI oracle | Small viewport and scrolling are tested; IME/controller hardware separate |
| 3D assets/materials | Model import/reimport, collision/navigation/environment cooking, runtime rendering and dependency closure | Explicit tested formats/fixtures; no blanket promise for every importer input |
| Animation and IK | Shared authoring/runtime poses, skinned rendering, actual animation project export, fresh-process phase/motor/event continuation | The129-character rendering crowd has16active independent IK actors/64targets; costs are content-dependent; no time-loss concealment |
| Flat/planet terrain | Author/cook/export/relocate, edits, cold settlement/traversal/load, material dependencies, navigation/origin/particle restoration | Bounded workshop/residency workloads; no arbitrary-world streaming guarantee |
| Physics, wheels and powertrains | Committed production MK2, original contact/CCD/joint tests and continuation/lifecycle gates | Bounded CCD/energy/spin limits remain; no experimental core/contact-material merge |
| Particles and water | Actual combined5000GPU-particle contacts; CPU/portable policies and water quantity/coupling fixtures | Advanced water remains optional and opt-in; source-preview project availability differs from curated SDK contents |
| Saves and world transitions | Atomic/corrupt-data tests, scene transitions, world continuation, cold processes and repeated application sessions | No compatibility promise with retired historical project/API formats |
| Networking and audio | Actual loopback application ownership/lifecycle and native audio application contracts | Dummy-audio tests do not prove physical output; WAN/hostile-load/real devices separate |
| Resources and outputs | Shared host/runtime ownership, failed build cleanup, generation/lifetime checks, guarded build/capture storage | Sampled storage guard is not a filesystem quota or permission for automatic deletion |

Tests establish specific contracts. A unit test, native application check, source-
hidden SDK workflow, performance capture and human review are distinct evidence.
The accepted predecessor passed207CTest cases and16additional workflows. This
increment passed208expanded cases plus the new permanent fresh-process animation/IK
continuation contract. Its registered suite now has209cases. Exact checkpoint
verification must distinguish reused unchanged-code evidence from fresh runs.
The first clean checkpoint's expanded run passed 207/209: a cwd-relative cold
fixture path and one terrain image visibility check failed. The former now uses
the normal engine-root resolver; the latter retains its assertions on an owned
image target. Exact repaired-checkpoint results are recorded separately.
The moved SDK IK consumer restored script state at tick142,16enabled IK actors
and64targets after a real resumed solve. `ikStatus.ready` reports asset availability;
residuals may be empty before the resumed fixed solver publishes them.

## Target tiers

| Target | Current evidence | Support statement |
| --- | --- | --- |
| Linux x86-64, Kubuntu/Ubuntu26.04.1, GTX970/NVIDIA580.178.04, modern GL | Real desktop editor/runtime/SDK and whole-application captures | Primary current-host development qualification; requires matching host ABI/driver dependencies |
| Ubuntu24.04 x86-64 CI reference | Accepted ad41 native focused gates and SDK host inspection passed on ubuntu24.04 with software GL | Reference build/CI evidence; local GCC15/GLIBC2.43 SDK is not thereby Ubuntu24-compatible |
| Portable GL policy on tested NVIDIA hardware | Actual native rendering/particle contracts, including shadow-depth equivalence | Tests the portable code policy on that driver, not every physical GL3.3/GLES device |
| Windows x64 MSVC | Native hosted run exposed a material-transfer compile assertion; bounded owner-kernel fallback implemented, exact retry pending | Build/tools/contracts can be qualified there; workflow explicitly does not certify editor/GL/export or real hardware |
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
ordering remain. Three interleaved20s/45s/20s profiler-off pairs improved median
p99 animation11.302→9.747ms, IK14.569→12.134ms and combined6.553→6.267ms. All18trials
had zero incoming clipping, discarded backlog or catch-up caps. Candidate worsts
were13.452/33.142/8.568ms respectively. Actual populated scenes were inspected.

Animation/IK still miss120FPS; IK exceptional jitter remains a known limit. A
complete5154-frame shallow IK trace had no lost records/time and max14.977ms;
it does not erase the33.142ms lean hitch. An earlier incomplete78ms profiled frame
was not reproduced or attributed to a speculative engine/driver/OS cause. Callback
observation max0.411ms in the complete capture does not explain that earlier gap.
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
development within this scope after the final repaired checkpoint checks. Qualify additional platforms/hardware as their
actual environments become available; do not inherit older acceptance silently.

Final clean-head identities, remaining hosted CI state and reproducible commands
belong in the current checkpoint record. The concise
[qualification receipt](audits/platform-20261116/evidence/stage-e-qualification.json)
and [performance receipt](audits/platform-20261116/evidence/stage-e-rendering.json)
separate current and historical evidence. Substantial replacement, main promotion,
selected consumer SDK change or broad cleanup still requires operator approval.
