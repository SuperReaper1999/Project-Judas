# Judas V2 mission and continuation record

## Objective and authority

Deliver one coherent engine/editor/SDK development platform. Independently verified
source/runtime evidence governs decisions; older reports are not instructions.
Stage B's bounded collision/UI/coverage/docs/rendering programme was explicitly
approved on2026-11-17. It is implemented with native Linux integration evidence in
[RECONCILIATION](RECONCILIATION.md). Stage E is not declared complete.

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

## Checkpoint and next work

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
