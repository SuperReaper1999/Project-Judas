# Approved V2 reconciliation — native Linux checkpoint

The approved bounded repairs are implemented and verified together through the
engine, editor and SDK. This is an Experimental candidate, not main promotion,
human acceptance or universal platform/performance qualification.

## Source selection

The candidate uses committed production MK2 plus the explicit reconciliation
changes. Original dirty contact-material/reduced-hull work is preserved byte-for-byte
in `codex/preserved-contact-material-20261117` at `2e607995`, with a clean source
worktree outside generated roots. `codex/physics-core-v2` remains at `d12490a9`.
Neither is merged or selected. Main stays at `1094d645`. No selected consumer SDK
was replaced. See [SOURCE_SELECTION](SOURCE_SELECTION.md).

## Repairs and ownership

- Collision failures now identify the cooked asset when project resources are
  unavailable. The original full SceneTests passes unchanged and is registered.
- Willow Town uses a responsive header, readable fixed rows in the existing
  scroll widget and a map fitted to the available viewport. All gameplay controls
  remain reachable. The shaped-text/control application oracle passes four
  resize cycles; the untouched original fails 182 assertions. Gameplay, life,
  absence and fresh-process restoration contracts pass. Editor and moved SDK
  output were inspected at the actual small Game-view size.
- The focused manifest now includes full scene, repeated editor lifecycle,
  world-persistence, wheel and powertrain contracts, plus real layout, clock-loss
  and skinned-bound cache regressions. Missing/stale binaries still fail closed.
- Architecture phase guidance and Water Workshop availability language now match
  the inspected implementation. Advanced water stays opt-in; no selection policy
  or Alpha capability was silently changed.
- Profiler-independent clock observations expose incoming clipping, actual fixed
  steps, discarded backlog and residual accumulator. A deliberate one-second hitch
  is checked against actual body advancement. Scheduling policy is unchanged.
- Measured repeated skinned-bound work is replaced by a renderer-private exact-input
  cache: full pose/source-bound validation, 256 owners / 4 MiB pose snapshots,
  frame/context retirement. All per-camera tests, lights, cascades, poses and draws
  remain. The original calculation runs on a miss; there is one culling authority.
- Generated storage uses the non-deleting monitored policy in
  [WORKSPACE_STORAGE](../../WORKSPACE_STORAGE.md). The separately approved reset
  reclaimed approximately 119.43 GiB; fresh outputs now reuse one guarded build.

## Demonstrated integration

| Gate | Result and boundary |
| --- | --- |
| Expanded focused CTest | 207/207 pass, 422.45 s: 200 native invocations / 193 targets and seven tooling gates |
| Additional application/lifecycle/cold-process tests | 16/16 pass, excluding the isolated reduced-hull proof |
| Python tooling | 209 run, 206 pass, three existing JDK/Ninja dependency skips |
| Native source-hidden SDK | Create, author/undo, save, reopen, Play/Stop, 39 tool operations, moved editor/CLI exports; inventory unchanged |
| Repaired town through SDK | Actual editor/export/Play/Stop, moved runtime at three sizes, packaged layout oracle, authored state identical after Stop |
| Skinned animation through SDK | Actual authoring project exports and runs after relocation with checkout hidden |
| Flat and planet terrain through SDK | Author/cook/export/relocate/save/cold settle/traverse/cold restore; material detail and restored volume/navigation/origin/particle state |
| Documentation | 177 managed documents / 2257 source links pass before checkpoint staging |

The strict SDK verifier correctly caught one obsolete document left by the audit's
overlay copy; its preserved-source hash was checked before removing that stale
copy. The benchmark-only animation project was correctly rejected for excluding
its startup scene from export; the unchanged authoring project passed instead.
The read-only namespace initially blocked standard temporary terrain pages;
explicit caller-owned writable TMPDIR resolved that exact setup error. These
failed attempts are not counted as successful application qualification.

## Whole-application performance

Three interleaved profiler-off pairs per workload, same frozen inputs, real
GTX 970 / NVIDIA580.178.04 and Ryzen5600, modern GL, 1280×720, VSync off, uncapped.
Animation/combined trials20s; IK45s. Repository/capture storage monitors active.
CPU/GPU work is not added; throughput is not compositor/scanout latency.

| Workload | Baseline median p99 | Candidate median p99 | Candidate worst |
| --- | ---: | ---: | ---: |
| Animation | 13.435 ms | 13.233 ms | 27.588 ms |
| IK | 17.507 ms | 16.008 ms | 44.132 ms |
| Combined | 8.087 ms | 7.410 ms | 14.862 ms |

All18 runs report zero clipped/discarded simulation time and no catch-up caps.
All pass the relative gate; animation's tail gain is small. Median animation
frames fall consistently from10.6–11.1 to9.9–10.0ms. The scoped bounds probe falls
from1.284 to0.477ms/frame on complete intervals; five truncated intervals per
probe were excluded. Deep IK detail is incomplete and is not used for attribution.
Actual crowd/terrain/actor/particle content and simulation counts were checked.

## Remaining qualification and decisions

Animation and IK still miss120FPS, exceptional hitches remain, and the aggressive
5ms combined critical-path goal is not established. A relative pass is not absolute
performance acceptance. Detailed scopes point primarily to actual shadow draw
submission, not light selection; no physics-core replacement is justified by this.
Further optimization must retain workload/fidelity and whole-application evidence.

Native Windows, Android, browser and Ubuntu24.04 execution remain unqualified;
this host's SDK requires GLIBC2.43. Physical audio/controller/IME, longer soak,
race/leak coverage and human acceptance remain separate. SDL dummy audio does not
prove hardware audio. Source-hidden native SDK evidence is not another OS test.
A complete per-feature editor/SDK qualification matrix and broader exceptional
vehicle/material workloads remain open; Stage E is not declared complete.

Substantial replacements, broad restructuring, main promotion, selected-SDK
replacement and further broad cleanup still require separate approval. The next
engineering priorities are measured shadow submission, unresolved exceptional
hitches and closing the native-platform/feature qualification gaps, not merging
preserved experiments on isolated proof results.

Compact [qualification receipts](evidence/reconciliation-qualification.json) and
[performance receipts](evidence/reconciliation-performance.json) identify tested
builds, executables, inputs, trial distributions, memory, device telemetry and loss.
Generated logs/images remain disposable current-campaign outputs. They are not
accepted source or an indefinite diagnostic archive.
