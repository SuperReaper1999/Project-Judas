# M78 remaining high-risk audit — 12 October 2026

Starting checkpoint: `7f83df131b5051c84e4314fde4508519f78daea4`.
**Partial audit; not comprehensive certification or human validation.**
New material is unstaged/uncommitted. Production, existing tests, project assets,
accepted evidence and independent research are unchanged.

## Results

| Case | Linux result | Meaningful observed envelope |
| --- | --- | --- |
| PUBLIC-UI-LIFETIME | 36 checks, 0 failures | Real QuickJS UI mutations, atomic invalid layout rejection, detached snapshots, independently expected hit rectangles, actual input/click delivery, modal pause, owner/slot/Stop cleanup and stale-handle rejection. |
| PUBLIC-IK-OWNERSHIP | 33 checks, 0 failures | Ordinary imported shared skeleton, actual JS configure/target/status/joint queries, independent analytic pose, fixed-step target publication, atomic batch rejection, independent instance and real physical final-pose authority. |
| MOD-STORAGE-SESSIONS | 36 checks, 0 failures | Interrupted storage publication, cancellation, private trust/dependency rejection, exact cached bytes, fixed session selection, cross-store lease protection and corrupt payload rejection. Native storage/admission, not JS module execution. |

105 assertions across three different envelopes are **not** 105 engine capabilities,
an engine pass percentage or evidence for every related public member.
No new production defect was demonstrated within these envelopes.

### Original probe failures preserved

- `runs/ui-01`: compile failed because the new probe used the UI serializer's
  arguments in the wrong order. Only the probe was corrected.
- `runs/ui-02`: 35 checks / 1 failure. The observer counted a legitimate automatic
  modal **focus** event as a click. `ui-03` retains the event and counts clicks
  specifically; the unchanged requirement is that the old rectangle cannot click.
  `ui-04` adds an independent queued-event cancellation observation.
- `runs/ik-01`: 33 checks / 2 failures. `UpdateScripts` calls start and update in
  the same frame; the probe submitted the next/rest target before the first fixed
  solve. `ik-02` waits for completed status before testing pending publication.
  Target, solver settings, geometry, tolerances and product code were unchanged.

- `runs/identity-01`: the first identity check passed all file hashes but failed
  inventory equality because the newly created inventory omitted its own filename.
  That packaging error was corrected; the first manifest/inventory are preserved.

These are probe setup/oracle/packaging failures, not discovered engine failures. Exact source
snapshots and failed receipts remain alongside the corrected runs.

## Contract traces and independent expectations

### PUBLIC-UI-LIFETIME

Expectation: [UI reference](../../judasjs/ui.md) documents instance-local mutable
UI, runtime layout, validity and owner lifetime.
Implementation: `ScriptSystem.cpp` UI bindings construct a validated patch;
`RuntimeUI.cpp::SetElementLayout` validates a candidate before publication;
`RuntimeUI::Layout/Input` share layout geometry; unload cancels queued document
events; script synchronization/destruction and `RuntimeWorld::EndScripts` release
owner UI.

The ordinary generated `.judasui` starts with a button at `(30,40)`, size `(100,40)`.
A public patch moves it to `(200,40)`, size `(120,50)`. The native computed rectangle
is checked against those constants; clicks at old/new positions independently
observe actual JS callbacks. Invalid later fields cannot apply an earlier valid
field. A queued click followed by owner destruction cannot reach a live observer.
Two owners share the asset but have independent runtime documents; retained JS
handles throw `ReferenceError` after owner removal. Reloaded authored bytes are
unchanged. Slot disable is exercised through normal native script synchronization,
not an invented public enable API or an editor gesture.

Missing: rendered pixels/fonts, physical mouse capture, every widget/layout field,
actual scene-transition/save UI restoration, document ID reuse across VMs and all
input-device/event ordering combinations.

### PUBLIC-IK-OWNERSHIP

Expectation: [animation reference](../../judasjs/animation-ragdolls.md) and supplied
original M70 brief: local poses belong to the shared resolver, submitted targets
are sampled at the next fixed reference, and dynamic mapped bones retain final
physical authority.
Implementation: `WorldAnimation.cpp::ConfigureFullBodyIK/SetFullBodyIKTargets`,
`PrepareAnimationReferences`, `ResolveAnimationPose` and `JointPose`;
`FullBodyIK.cpp` validates bounded chains; `WorldRagdoll.cpp` contributes physical
local pose through the normal resolver rather than skinning directly.

Fixture: original `projects/animation_demo/Assets/models/bend_bar.glb`, four nodes
(Root/Elbow/Tip/Mesh), three-joint chain, elbow rotation 0.45 rad about local Z,
root rotation disabled. Independent endpoint uses rest hierarchy and quaternion
rotation, not the solver's own status. Entity has arbitrary orientation and
positive uniform scale 1.3. Measured world position error **0.000006659 m**, world
quaternion angular error **0 rad** (float comparison resolution); the reported
local solve completed in **3 iterations**, position residual **0.000005066 m**,
orientation residual **0.000013947 rad**. Original default tolerances are retained.
Single measured solve **8.837 microseconds** is diagnostic, not a performance
benchmark.

A real ragdoll body starts at the resolved elbow pose, receives an ordinary linear
impulse, advances through PhysicsWorld and maps back through the final skeletal
pose. Final joint position matches the actual body within float precision despite
IK participation. Shared asset/rest pose and a second instance stay unchanged;
leaving ragdoll/removing IK returns to rest without accumulating final-pose edits.

Missing: multi-chain conflicts, singular/unreachable chains, all joint limits,
nonuniform scale, shared-asset reimport while jobs are pending, authored root/socket
interactions, save/stream transitions and physical balancing/drive/contact matrices.
This is not proof of an entire ragdoll or shared-IK capability.

### MOD-STORAGE-SESSIONS

Expectation: [modding guide](../../MODDING.md) and supplied original M76 brief:
explicit exact-hash trust; privately resolved loadout; immutable leased session
content; interrupted operations preserve the last good published membership.
Implementation: `modding/ContentStorage.cpp`, `ModContent.cpp`, `ModStore.cpp`:
synchronized temp publication, snapshot leases, whole-selection Resolve and local
Snapshot materialization before assignment to the caller.

Probe uses a unique fresh directory, never the real user mod store. Injected
pre-publication exceptions retain exact prior bytes; a successful subsequent
write changes them. A second Store disables the next launch while the active
Snapshot and cache stay intact, blocks uninstall until the lease is released,
and then successfully uninstalls. Executable selection without trust, denied
contract, absent dependency and deliberately corrupted payload all reject.
The old loadout/caller output snapshot remains unchanged. Interrupted install
retains prior index membership and can reuse the exact immutable orphan later.

Missing: actual power loss/process kill durability, cross-process locks, OS-specific
link/reparse races, low-disk failures, concurrent installer processes, real VM code
admission/exports, modded saves, content mounting/streaming and moved packages.
Trusted mods are not a security sandbox. Native admission is not a public JS proof.

## Reproduction and exact identities

Use the preserved Release archive `.cache/m78-lifecycle/lif003-build/libjudas_engine.a`
and committed runner `docs/evidence/m78-lifecycle/run_driver.py`. It creates a fresh
output directory, records compiler/link commands and hashes, and refuses reuse.
For UI/mod: run the corresponding `probes/*.cpp` without special environment.
For IK: `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1`.
Example (choose a new output path):

```sh
python3 docs/evidence/m78-lifecycle/run_driver.py \
  --source docs/evidence/m78-risk-audit/probes/PublicUILifetime.cpp \
  --archive .cache/m78-lifecycle/lif003-build/libjudas_engine.a \
  --output .cache/m78-risk-audit/ui-reproduction-NEW
```

The existing archive is local, not distributed in evidence. Reproduction on a fresh
checkout requires building the same Release engine first. Exact local recipes,
archive/link-input hashes, source snapshots, receipts, result JSON and binary hashes
are in `runs/`. Executables/objects remain ignored cache artifacts. The imported
GLB and traced source hashes are in `FINAL_FINGERPRINTS.json`.

`python3 docs/evidence/m78-risk-audit/verify.py` verifies frozen audit/source/binary
identities without regenerating expectations. Audit-only descendant commits are
accepted; production-path differences from the audited engine are rejected. `CHANGED_FILES.txt` lists all new
files; existing 73 excluded diagnostic entries were left alone.

## Preservation and platform scope

Prior V4 read-only verifier still passes: 888 current files, 5 frozen manifests,
4,849 protected original files, 656 preserved V2 files, 113 current and 94 prior
local outputs, 72 prior plus 1 later excluded artifacts match recorded hashes.
`PREVIOUS_CHECKPOINT_VERIFICATION.json` preserves the exact result. No builds or
suites were rerun merely to revalidate earlier passing physics.

Linux is the current acceptance platform. Windows VM was observed running; none
of these three new probes was executed natively on Windows. Previous checkpoint
Windows receipts remain historical evidence only, not a pass for these new cases.
No VM configuration changed. Windows remains nonblocking; no hardware/performance
claim is made. No full production suite or new benchmark was run here.

FLU-001 remains failed/deferred; fluid implementation, thresholds and independent
research are untouched. Original `a106…` save bytes and missing historical
continuation limitation remain unchanged. Previous Linux physics timing tails
remain documented; these tests do not refresh their measurements.

## Handoff

See [remaining coverage](REMAINING_COVERAGE.md) and additive machine-readable
`CAPABILITIES_OVERLAY.json` / `REQUIREMENT_OVERLAY.json`.
The previously committed lifecycle fixes remain checkpointed; this audit evidence
is ready for review as an independent additive batch. M78 is **not fully validated**.
No code correction is proposed from these results and no human validation is claimed.
No commit, push, tag or M79 work was performed.

### Human checklist

1. Move/resize a runtime UI control; confirm its new click region and modal focus.
2. Destroy/disable its owning script and Stop/reload; confirm no orphan UI or clicks.
3. Exercise shared IK on two instances, then switch one to physical ragdoll; confirm
   mapped bones follow physics and the other instance retains independent state.
4. In a disposable mod project, disable a package during a session; confirm the
   current session is unchanged and a new session observes the new selection.
5. Review the preserved probe failures and remaining coverage before any acceptance.

These manual checks remain pending and do not replace the unexecuted matrices.
