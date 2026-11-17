# Reconciliation programme — approved bounded scope

This Stage B plan is based on the completed current-host audit runs in AUDIT.md
and PERFORMANCE.md. The operator approved bounded diagnostic,
project UI, test-coverage and guidance repairs, followed by a focused rendering/
instrumentation investigation. It does not authorize a solver replacement,
proof merge, main promotion, selected-SDK change or further broad cleanup.
Current platform/hardware qualification gaps remain explicit.

Current execution: approved bounded repairs and native Linux integration are
implemented; see [RECONCILIATION](RECONCILIATION.md). External platform/human and
remaining performance/feature qualification gaps stay open.

## Decision: retain the integrated production spine

Keep EngineHost → RuntimeWorld → Simulation/InteractivePlay, with editor Play
using that same runtime, and the native SDK staged from the shared tool inventory.
Actual source-hidden desktop SDK creation/reopen/Play/Stop/export tests pass.
The evidence does not justify rebuilding a second engine/editor/SDK architecture.

Keep separate authoring definitions, live state, asset IDs, runtime handles and
immutable worker products. Keep the existing shutdown order and stale-generation
checks. Keep Basic Water versus advanced water, CPU/GPU particle alternatives,
static baked navigation versus source-owned live terrain preparation, and
authoring previews where their responsibilities differ. Similar names are not
sufficient reason to consolidate them.

## Increment 1 — lock the auditable platform candidate

Question: which exact source state is the platform candidate, and which existing
experiments must stay isolated?

- Preserve the initial dirty work, untracked proof and alternative-core branch.
  The new native SDK is a disposable audit product, not a selected-game SDK.
- Produce the final input hashes and target identities with current run receipts.
  Preserve a current-source failing fixture before any implementation repair.
- Agree whether to finish, isolate or leave stopped the existing contact-material
  prototype. It currently changes shared MK2 even though proof targets are opt-in.
  The approved boundary is now implemented in SOURCE_SELECTION.md; original
  prototype source is isolated on its preservation branch/worktree.

Acceptance: one explicit candidate identity; every executable in its SDK matches;
tracked/untracked inputs and experiment selection are explained; no unrelated
work is staged or committed. Main remains unchanged.

## Increment 2 — repair reproduced behavior and authoring failures

The first concrete bounded repair is SCENE-DIAGNOSTIC-001 (P2 severity): include
the cooked collision asset ID in ResolveBodyCollision's resource-unavailable
message, then run the full original SceneTests unchanged. Register that full
contract so a selected-suite pass cannot hide this failure again. The failed
world already stays unbuilt; preserve that behavior. No solver rewrite is needed
for this defect. Also correct the verified architecture/water-preview guidance
drift without changing fluid selection or inventing another implementation.

The second bounded repair is TOWN-UI-001 (P2): make the curated Willow Town
header/sidebar layout readable in the default small editor Game view and normal
standalone view. Both currently reproduce overlap. Preserve the town rules and
controls; fix the application layout rather than replacing runtime UI. Add an
actual application/layout oracle using resolved text bounds and controls at
681x386 and 1280x720, then verify matching editor and relocated SDK output.

For each fresh test/application failure, state its input, owner and expected public
behavior, then apply the smallest source-level correction. Runtime oracles must
cover the actual application boundary, not merely duplicate implementation logic.
Run affected subsystem checks plus dependent editor/SDK workflows.

No broad rewrite is approved by this conditional repair item. Any change to
chronological collision authority, geometry publication or simulation-time policy
needs its own concrete design, invariant checks and operator approval.

Acceptance: preserved original regression reproduces before repair and passes
afterward; matched application behavior is shown; remaining failures stay explicit.

## Increment 3 — close validation coverage and document boundaries (P1)

The focused manifest has 192 entries while many existing native test programs,
including some lifecycle and cold-persistence contracts, are outside that suite.
Review omitted tests by contract and cost. Register meaningful routine or periodic
gates with explicit inputs/outputs, identity queries and actionable error messages;
do not mechanically add every historical target or call all tests equivalent.

Priorities: repeated editor lifecycle, import/reimport/cache invalidation,
scene transitions, actual network/application ownership, physics/material/wheel
continuation, and cold save/load. Static architecture regex guards remain useful
alongside application tests; they cannot certify application correctness.

Correct the verified Simulation phase-map drift in ARCHITECTURE.md, and mark
desktop/native-host vs Ubuntu baseline vs actual Windows/Android/browser evidence.
Describe the SDK's actual binary/JudasJS contract and known unsupported workflows.

Acceptance: the advertised validation command names its actual coverage; no missing
binary is counted as pass, stale identity fails, and documented ordering matches
source and recorded application behavior.

## Increment 4 — resolve demonstrated performance failures (P1)

Use fresh whole-Application captures, live scene/simulation checks and absolute
budgets to select work. Keep loading, steady-state, exceptional collisions and
editor overhead separate. Prioritize unnecessary repeated preparation/publication
and confirmed source-owned hot paths. Repeat the same frozen inputs after changes.
GPU/CPU overlap, profiler overhead and lost simulation time must remain explicit.

Measured first priority: the 129-character and IK profiler-off controls exceed
the 120 FPS target. In the traced animation case, World render submission
averages 8.198 ms, including 5.595 ms in the enclosing Lighting shadow planning
scope. Source confirms that this scope includes actual shadow draws. One bounded
measurement increment must separate preparation, draw/driver work and profiler
collection before choosing an optimization. Keep all lights/cascades/poses and
physical workload. Use lean simulation-loss/completion telemetry in profiler-off
controls so improved throughput cannot conceal dropped time. Combined gameplay
must remain the whole-platform regression workload.

The independent physics-core proof is not selected as the replacement merely
because it is simpler or has passing isolated controls. For a proposed replacement,
first specify native body/contact/joint/CCD/character/vehicle/deformable/persistence
contracts, editor/SDK migration and the exact old work removed from runtime.
Compare correctness, fidelity, complexity and whole-frame outcomes. Reject it
if it cannot satisfy those boundaries; preserve negative evidence.

Acceptance: an actual affected workload improves without reducing fidelity,
duplicating authority or hiding backlog loss; combined gameplay is rerun. An
absolute miss remains a known limitation even if relative p99 improves.

## Increment 5 — sustainable storage ownership (implemented by explicit authority)

The operator approved complete `.cache` and `build` cleanup after source safety.
All four worktrees were clean; all their commits remain reachable from branches.
Both directories were verified empty and approximately 119.43 GiB filesystem
space reclaimed. No historical build/capture backup was made for cleanup.

The non-deleting monitored guard, per-capture ceiling and current-campaign policy
are implemented and tested. See docs/WORKSPACE_STORAGE.md and cleanup-result.json.
Do not recreate a historical evidence archive. Source belongs on branches and
accepted behavior in actual code, assets, tests and concise documentation.
Future deletion needs its own reviewed scope. Further dependency-cache engineering
must justify its maintenance cost through measured preparation savings.

Acceptance for continued work: guarded commands stop at the sampled budget,
repeated work reuses its active directory, captures have a per-run ceiling, no
unknown files are automatically deleted, and bypass/platform limits are explicit.

## Increment 6 — integrated qualification and development baseline (P0 exit gate)

Rerun create/import → author → save → Play → Stop → reopen → cook/export →
source-hidden relocated SDK/game → verify behavior and cold persistence.
Include representative 2D/3D content, terrain, rendering, animation, particles,
water, vehicles, UI/input, audio and networking where supported. Test repeated
cycles, resource retirement, corrupt/missing data and meaningful diagnostics.

Run affected and combined whole-frame workloads serially, with exact hardware,
resolution/quality, VSync/cap, p50/p95/p99/max, hitches, memory and simulation loss.
Qualify Windows, Android and browser in their real applicable environments or
state the exact unsupported/untested boundaries; a cross-build is not native
target execution. Real audio/controller/IME and human acceptance stay separate.

Acceptance: one reproducible Experimental checkpoint with clear ownership,
meaningful tests, matched SDK identities, demonstrated integrated workflows,
bounded output ownership and an honest qualification matrix. No universal
performance or human acceptance claim. Release promotion is a separate operator
decision about the exact candidate.


## Current Stage E bounded continuation (operator clarification2026-11-17)

The accepted ad41f73f checkpoint and all three CI passes are complete. Continue
with one evidence-led animation/IK rendering increment and exceptional-hitch
attribution, then available native target checks and the explicit capability/
qualification matrix in ../../DEVELOPMENT_READINESS.md. Do not turn stretch FPS
misses or unavailable preview targets into an indefinite investigation. The
readiness decision concerns a reliable supported platform, not universal perfection.
No experimental physics merge, major replacement, main promotion or selected SDK
replacement is authorized. Existing qualified behavior may be reused when unchanged;
rerun affected applications, fidelity controls, lifecycle and SDK paths for actual
changes. Genuine architectural or environment blockers are reported explicitly.
