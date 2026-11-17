# Judas V2 mission and continuation record

## Objective and authority

Deliver one coherent engine/editor/SDK development platform. Independently verified
source/runtime evidence governs decisions; older reports are not instructions.
Stage B's bounded collision/UI/coverage/docs/rendering programme was explicitly
approved on2026-11-17. It is implemented with native Linux integration evidence in
[RECONCILIATION](RECONCILIATION.md). Stage E achieved the scoped development-ready baseline recorded in [DEVELOPMENT_READINESS](../../DEVELOPMENT_READINESS.md).

No physics-core replacement, major restructuring, main promotion, selected external
SDK replacement or further broad cleanup is authorized. Ordinary validated work
may be committed/pushed to Experimental under root AGENTS.md. Main requires exact
operator acceptance. Automation/builds/synthetic input never imply human acceptance.

## Source boundary — recheck after compaction

Primary Experimental starts at ca0d437135ac5f5dd7adf9314b56526099d1c313 before this
checkpoint. Main stays1094d64515604fe9d03fdab479efc7f880b112a8.
Original17-file dirty contact-material/reduced-hull work was preserved byte-for-byte
in commit2e6079950625fedb59b343979fc47f29de15395b,
branch codex/preserved-contact-material-20261117, clean worktree
source-worktrees/contact-material-preserved. The nine tracked experiment paths
were restored to committed production MK2 and eight untracked experiment files
relocated only after hash verification. No experiment merge occurred.
codex/physics-core-v2 remainsd12490a9f1fa1b4a48672a7ab858d1b2f3347141.
See SOURCE_SELECTION.md / evidence/source-selection-preservation.json.

Current bounded changes: asset-ID diagnostic; responsive scrolling Willow Town UI;
expanded meaningful contracts; accurate architecture/water guidance; profiler-off
clock loss observation; measured exact-input skinned-bounds reuse; storage guard.
No original PhysicsWorld/physics implementation file remains dirty. The private
renderer cache replaces repeated original calculation on a hit; draws/frusta/
lights/cascades/poses/simulation policy remain. It owns at most256 entries/4MiB
pose snapshots, cleared at BeginFrame and Shutdown.

## Verified evidence

- Initial Stage A:199CTest cases; additional full SceneTests exposed the asset-ID
  diagnostic. Original dirty physics source was selected then. AUDIT.md and
  PERFORMANCE.md describe that historical audit state, not this candidate.
- Current production boundary:207/207CTest,422.45s;200native invocations193targets
  plus7tooling gates.16additional lifecycle/application/fresh-process invocations
  pass, with reduced-hull proof excluded. Python209run/206pass/3JDK/Ninja skips.
- Original full SceneTests passes unchanged. New layout oracle passes four native
  viewport cycles; original layout fails182assertions. Actual fonts and clipped
  control reachability are checked, alongside unchanged gameplay/life assertions.
- New clock test with profiling disabled accounts for0.75s incoming clipping,
  eight actual fixed steps and remaining discarded backlog during a1s hitch;
  actual body movement agrees. Existing scheduling/cap policy is unchanged.
- Real source-hidden SDK: create/author/undo/save/reopen/Play/Stop,39tool operations,
  moved editor/CLI exports and SDK checksum invariance pass. Repaired town's actual
  editor/moved runtime/packaged layout pass; authored state identical after Stop.
  Actual animation authoring project exports and runs after relocation.
- Source-hidden flat/planet terrain native author/cook/export/relocate/write/cold
  settle/traverse/read pass, including material detail and restored state.
- Matched three-repeat lean performance: animation p99 median13.435→13.233ms,
  IK17.507→16.008ms, combined8.087→7.410ms. All18captures zero clipping/discard/caps.
  Animation/IK120FPS, exceptional hitches and aggressive5ms goal remain unqualified.
  Bounds cost1.284→0.477ms/frame on untruncated intervals. Deep IK trace truncates
  records and is excluded from complete scope attribution. Do not add nested or
  CPU/GPU durations. Exact identities, memory/telemetry and distributions are in
  evidence/reconciliation-performance.json and reconciliation-qualification.json.

## Supported desktop and corrected setup failures

Use docs/TEST_HARNESS.md, existing EditorAutomation and SDK/terrain qualifiers.
The execution tool's reviewed require_escalated route reaches the unchanged real
NVIDIA desktop. A failed restricted DISPLAY/socket attempt does not prove absence.
No Linux/KDE/NVIDIA/display permission changes were made. No headless-only downgrade.

Source-hidden route: bwrap read-only host, tmpfs over entire checkout, writable bind
only /tmp/judas-platform-audit-20261116, real/dev and/proc. Explicit TMPDIR points
inside that writable workspace (runtime-temp); otherwise terrain pages correctly
fail on read-only/tmp. Keep current SDK copies checksum-verified: an overlay had
left one retired prototype document, which verification caught. Its source was
preserved before removing the stale disposable copy. The benchmark-only animation
startup is excluded from its export list; exporter correctly rejects it. Qualify
with the unchanged authoring project, not a weakened fixture.

Current disposable workspace has relocated-sdk, reconciliation-consumer-retry,
reconciliation-town-retry, reconciliation-flat-writable-temp and
reconciliation-planet-writable-temp. COMPLETE/RESULTS receipts record passes;
initial failed attempts remain explicitly classified. SDK is an audit copy, not a
selected game SDK. Raw logs/images are current generated output, not source.

## Storage and continuity

The separately explicit .cache/build reset safely unregistered four clean worktrees
and reclaimed approximately119.43GiB. No historical output archive was made.
Preservation commits remain reachable. One current guarded build/campaign now uses
those roots. Policy: .cache8GiB/build14GiB/total16GiB/capture256MiB; sampled process-
group termination, no automatic deletion. 14GiB build allowance reflects measured
12.09GiB atomic SDK peak; total stayed16GiB. Source worktrees stay outside outputs.
See docs/WORKSPACE_STORAGE.md. Never raise limits to conceal growth or delete
unknown artifacts. Only concise evidence belongs in accepted documentation.

## Accepted checkpoint and next bounded stage (2026-11-17)

The operator accepted ad41f73f as a completed bounded checkpoint after all three
CI workflows passed. Primary remains clean Experimental at the start of this
next stage; no main promotion or experiment merge is authorized.

Next defined question: how much remaining animation/IK submission time is material
binding versus elapsed GL primitive draw calls, and what dominates exceptional
hitch frames? Existing state/pose/bounds caches remain authoritative. Opt-in
JUDAS_PROFILE_RENDER_DETAIL aggregates timings without per-primitive trace events;
GL elapsed includes driver waits, never report it as GPU duration. Use depth4
for complete IK frame-level attribution; preserve profiler-off controls and
simulation-loss observations. No optimization selected until this measurement. The operator clarified that
development readiness means reliable integration within explicit supported
boundaries, not universal perfection or meeting every stretch target. This probe
must support one retain/repair/defer decision, followed by the finite qualification
matrix and readiness verdict, rather than an indefinite optimization programme.
Native target qualification and remaining integration gaps follow this bounded
rendering/hitch increment. No substantial replacement is authorized.

## Prior checkpoint procedure (completed for ad41f73f)

Finish explicit source/docs staging, commit on Experimental after the above checks,
then reconfigure/relink identities and verify the exact committed SDK candidate.
The compiled native behavioral inputs are hashed in evidence/reconciliation-inputs.json;
post-commit identity changes do not imply different renderer/physics behavior.
Do not claim final clean-head SDK qualification until it has actually run. Current
raw final-head status belongs in .cache/v2-reconciliation/checkpoint-verification.json
once present; read it alongside HEAD/build identity after compaction.

Remaining: native Windows/Android/browser/Ubuntu24.04 execution (host SDK needs
GLIBC2.43), physical audio/controller/IME, human acceptance, soak/leak/race work,
full feature-by-feature editor/SDK matrix and broader exceptional vehicle/material
workloads. Next rendering priority is measured actual shadow submission, not
assuming cache-key planning dominates. Any major replacement or promotion still
requires separate approval. Preserve failed evidence and fidelity; do not merge
isolated experiments on their proof tests alone.


## Stage E exit — qualified Experimental baseline (2026-11-17)

**Verdict: development-ready native Linux engine/editor/JudasJS SDK within the
explicit [supported boundaries](../../DEVELOPMENT_READINESS.md).** Qualified code
candidate: **eb2123684e77494007a4452cee6bb0918f8b4eff**. Final documentation-only
closure preserves that exact runtime/test candidate; it is not a second native run.

### Demonstrated qualification

- **209/209 CTest passed in 471.71 seconds**; all 195 native executable identities
  report the clean candidate. Python: 206 passed, three explicit JDK/Ninja skips.
  Public API/type contracts passed (524 symbols, 353 operations, 58 examples).
- Source-hidden real NVIDIA desktop SDK: 15 matching tools, 39 tool operations,
  actual create/author/undo/save/reopen/Play/Stop, town packaged layout, animation
  export, moved IK runtime and cold restoration passed with checksum invariant.
  IK restored tick 141, 16 enabled actors / 64 targets after a real resumed solve.
  Build ID: `2.0.0-alpha.1-Experimental-eb21236-clean-6337fffb54e9`.
- All three regular CI workflows passed: focused engine44101521448,
  tooling44101521391, current documentation44101521437.
- [Exact non-publishing target run44101575648](https://github.com/SuperReaper1999/Project-Judas/actions/runs/44101575648)
  passed both native jobs. Ubuntu24.04 includes SDK workflows, API and GLIBC2.39
  boundary checks on software GL. Windows2025/MSVC includes full SDK build,
  selected native contracts, all 166 material checks and SDK loader inspection.

### Bounded rendering decision

Retain compatible opaque shadow coalescing: original parts are skipped when one
merged draw is eligible; colour geometry and simulation ownership stay unchanged.
28 shadow-depth fidelity cases passed per modern/portable NVIDIA policy. The
matched whole-application campaign passed all 18 trials: median p99 animation
11.302→9.747ms, IK14.569→12.134ms, combined6.553→6.267ms; worst candidate
13.452/33.142/8.568ms. No clipping, discarded backlog or catch-up caps.
Complete 5154-frame shallow trace: no lost records/time; shadow332.5 draws/frame,
GL CPU elapsed0.518ms, material0.177ms, callback observation max0.411ms.
The earlier incomplete78ms frame was not reproduced or attributed; the33.142ms
lean hitch and animation/IK120FPS misses remain explicit known limitations.
No additional optimization loop is needed for this scoped baseline.

### Failures and repairs retained

The first clean79acc124 full run passed207/209, not209/209. The new cold fixture
used cwd-relative project/IK paths; the terrain image visibility check failed once,
then passed three isolated repeats. Repaired fixtures use ResolveEngineDataPath
and an owned image/depth target, retaining all assertions. The initial hosted
Windows build failed MaterialBatch's no-throw transfer assertion. When instance
transfer is not guaranteed safe, the same ordered kernels now execute on the
owner; GCC's safe worker path remains selected. This is a portability fallback,
not a physics-core replacement. Final full/local/SDK/hosted reruns passed.
Initial snapshots and negative attempts remain in stage-e-qualification.json;
its pending fields are historical and superseded by this exit record.

### Scope and continuity

No Windows editor/GL/export qualification is inferred from hosted contracts.
Android/browser snapshot runtime, physical audio/controllers/IME, long soak,
hostile WAN/load and broad vehicle/material exceptions remain unqualified.
Android lacks a complete JDK/emulator/current device; Emscripten is absent.
Historical scoped acceptance is neither withdrawn nor extended to this snapshot.
Primary Linux readiness does not require universal platform or stretch-budget
qualification. Continue feature/game development within the documented boundary.

Main1094d645, preserved contact-material2e607995 and separate physics-cored12490a9
remain unchanged. No selected consumer SDK, release publication or promotion
changed. Generated roots remain guarded (8GiB cache /14GiB build /16GiB total;
no automatic deletion). The current campaign is disposable; concise source receipts
and this verdict establish the baseline. No new roadmap or substantial replacement
is authorized by this exit. Existing operator approval boundaries still apply.
