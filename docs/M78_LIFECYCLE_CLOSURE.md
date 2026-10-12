# M78 — lifecycle corrections after the corrective checkpoint

**Linux-validated lifecycle correction set, authorized for checkpointing; not human validated.**
Windows checks are informational and nonblocking.
The comprehensive M78 audit remains incomplete. Original requirements,
thresholds, numerical quality and prior failures are preserved.

## Checkpoint completed

`0695433b949df0c1d05ac6798bf118121cc53e2b` —
**Correct M78 physics, persistence, construction and navigation findings**.
Normal push succeeded; HEAD = main = origin/main. This checkpoints the previous
corrective source/documentation/evidence, including the authorized
Conner/SuperReaper1999 attribution and unchanged MIT terms. It does not complete
M78 or mark M77/M78 human validated.

Before that commit, all 4,855 original source/evidence fingerprints and 94 local
binary-output fingerprints matched. Staging initially normalized CRLF native
receipts; the narrowly scoped `.gitattributes` evidence rule preserved exact
bytes. Expected historical hashes were not regenerated. The
[checkpoint receipt](evidence/m78-checkpoint/CHECKPOINT_VERIFICATION.json) records
that exception and exact inventory. Its 4,495 pre-receipt paths plus the receipt
itself formed the 4,496-path commit.

[Previous checkpoint handoff](M78_FINAL.md) remains an exact record of that
candidate. Its unresolved LIF-001/LIF-002 descriptions belong to that checkpoint;
subsequent results below do not reinterpret its failed receipts.

## Corrections in the subsequent candidate

### LIF-001 — motor query representation ownership

Cause: lazy capsule creation published PhysicsWorld defaults rather than owner
classification/enabled state. Retained reconstruction disabled the capsule while
private, but region publication only restored ordinary SceneBody state. A normal
character update later repaired metadata, concealing the publication gap.
Independent controls also found a rotated-offset teleport error and query-body
leak on retirement in the same representation lifetime.

The generic correction:

- Publish authored enabled/layer/mask/tag state at query capsule construction and
  shape replacement/configuration.
- Update offset-only configuration and explicit teleports using root plus rotated
  local offset. Preserve motor intent velocity on offset-only changes.
- Keep retained private capsules disabled; restore authored enabled state at
  publication before callbacks.
- Destroy each owned query capsule on region retirement; stale generations fail.
- Register published motor geometry before frame/start and UI-only startup,
  matching existing fixed-start registration without advancing physics.

Changed runtime files: `WorldCharacter.cpp`, `WorldRegions.cpp`, `WorldScripts.cpp`.
No renderer, gravity selection, CharacterMotor solver or ownership API changed.
Metadata getters do not reset existing physical poses or replay continuation.

### LIF-002 — excluded-body sleep capture

Cause: entity/contact capture excluded a legally transient simple actor, but sleep
capture collected every entity-owned dynamic body. That created a required orphan
sleep record; strict private restore correctly rejected it.

`WorldPersistence.cpp` now shares the **existing unchanged contact inclusion
predicate** with sleep capture. Genuine mapped articulation bodies remain retained
through their full-generation physical mapping despite internal transient flags.
Ordinary excluded actors are omitted, and explicitly reincluded actors persist.
Sleep format 1, articulation 4/contact 2 and strict orphan/duplicate/truncated
rejection are unchanged. Previously emitted orphan-bearing slots are not repaired.

## Final LIF-003 and combined closure

[Final report and exact receipts](evidence/m78-lifecycle/lif003/REPORT.md):
**31/0** tag checks, **144/0** expanded publication/actual JS/streaming checks,
**77/0** save exclusion, **55/0** public Application audit; the full selected
physics/M70/M77/save/streaming interaction matrix passes on the final archive.
Exact legacy snapshot still fails **13/1**. Native Windows final tags/publication
pass **175/0**; the previous wider native matrix retains its source scope.
Linux acceptance permits this corrective checkpoint despite remaining audit gaps.

## Earlier V2 focused Linux results (preserved source scope)

| Route | Before / preserved control | Current result |
| --- | --- | --- |
| Immediate/staged/retained motor publication, isolated layer/tag filters, disabled actors, oblique offsets, frame/UI start, repeated retire/revisit | Exact current driver: **131/39** against checkpoint archive | **131/0** |
| Eligible save exclusion, retained/reincluded controls, actual sleeping body, real three-bone articulation, strict malformed rejection | Exact current driver: **75/2**; early failed restore skips later assertions | **77/0** |
| Public JS/Application prefab initialization + rejected/valid queued save load | New discriminating audit; no fabricated before failure | **55/0**, 23 frames |
| Continuation / unchanged M77 controller | Prior checkpoint proof retained | **112/0; 119/0** |
| Rigid motion / physical animation / joints | Unchanged fixtures | **10,239 pass; 64/0; 29/0** |
| Contacts / impact CCD | Unchanged fixtures, quality/cadence | **1,076/0; 89 cases, 3,197/0** |
| Core save / prefab / streaming / CharacterMotor / gravity | Unchanged fixtures | **27/0; 37/0; 82/0; 32/0; 70/0** |
| M77 step / lifecycle / radial | Original settings/inputs | **15/0; 26/0; 18/0** |
| Fresh M77 cold step / stand / recovery write→read | Separate fresh processes | **24→13; 23→13; 21→13**, zero failures |
| M70 application / fresh cold pair | Original CharacterLab fixture | **98/0; 10→14**, zero failures |
| Mixed motor/balance cold actual physical witness | Original observer/conditions | **25→13**, zero failures; actual mapped bodies/contact fields match |
| Application save/streaming write/read/read-b | Original registered project/driver | **11/0; 19/0; 19/0** |
| Editor Edit/Play/Stop, normal export, moved standalone | Owned project/package copies; software GL | All pass |
| Exact legacy `a106…` cold read | Original bytes and threshold | **13/1 — still failed** |

[Motor before/after](evidence/m78-lifecycle/character-publication/) ·
[save before/after](evidence/m78-lifecycle/save-exclusion/) ·
[public audit](evidence/m78-lifecycle/public-audit/README.md) ·
[regressions](evidence/m78-lifecycle/regressions/RESULTS.json) ·
[actual endpoint](evidence/m78-lifecycle/regressions/ACTUAL_ENDPOINT_COMPARISON.json) ·
[identity/editor/export](evidence/m78-lifecycle/smokes/RESULTS.json).

Actual physical comparison excludes only generation-local native handles and
array traversal order; it does not substitute a cached success flag. Same-platform
pair equality is not cross-platform bitwise certification. M70's route establishes
authority/finite pose, not complete articulated trajectory equality.

Builds are focused Release translation-unit replacements and relinks against
hashed unchanged checkpoint objects/dependencies. No header/ABI change or clean
full-suite rebuild is claimed. The first serial runner reused output names for
standard/observer executables; its exact runner/receipts remain. Distinct artifact
relinks and narrow 112/27/M77-cold identity checks establish final pre-run hashes.

A new fixture's initial sleep-byte inspector incorrectly decoded a Boolean as a
vector: **41/4 setup failures**, preserved before the inspector-only correction.
The original 22/1 product failure and the corrected 75/2 baseline remain separate.
The first motor fixture omitted project classification input; its 5/2 setup result
and all subsequent expanded failures are retained. No failed acceptance threshold
or expected hash was weakened.

## Native Windows follow-up

Current-source native Windows functional checks pass: **14 runs, 630 assertions,
zero failures**, across ten executables. The three new fixtures pass **131/0,
77/0 and 55/0** (28 Application frames). Existing motor/continuation/save/streaming
checks pass **32/0, 112/0, 27/0 and 82/0**. Unchanged M77 step and fresh cold pair,
M70 cold pair and the mixed actual-body cold pair pass. All **13 body records and
16 completed-contact rows** match exactly within the native uninterrupted/restored
pair after excluding only fresh handles and canonicalizing order. No physical
fields, rows or tolerances were discarded.

The four fresh production objects and the accepted PhysicsWorld object each link
once ahead of the frozen archive; linker receipts verify old members were not
extracted. All **223 prepared source/fixture identities** and archived outputs
match. Native compile logs retain existing fixture/conversion warnings; no local
shadow warning remains. No clean whole-engine rebuild is claimed.

The initial automatic approval blocks and stopped-transfer-server/late-receipt
tooling failures remain recorded. After direct operator approval, compression of
only the host-backed archive preserved its exact logical bytes and freed space.
Further explicit cleanup permission was applied only to new completed per-test
outputs, after independent host copy/size/SHA verification. Originals, snapshots,
historical evidence and VM configuration remain unchanged. No product test was
rerun for those tooling failures.

This matrix does not refresh native editor/moved-package, SaveStreaming/Application
signals or runtime-tag-mutator routes; previous Windows receipts retain their old
source scopes. The VM is native functional evidence, not the Linux performance
gate or physical GPU/input/audio validation.
[Native report](evidence/m78-lifecycle/native/REPORT.md) ·
[case receipts](evidence/m78-lifecycle/native/SUMMARY.json) ·
[actual endpoint](evidence/m78-lifecycle/native/ACTUAL_ENDPOINT_BY_ENTITY.json) ·
[final identities](evidence/m78-lifecycle/native/FINAL_VERIFICATION.json) ·
[preserved preparation blocks](evidence/m78-lifecycle/native/PREPARATION_BLOCK.json).

## Linux performance

One final-archive observation retains the original four articulations, **52 bodies,
48 constraints, 8,192-triangle floor, 900 steps at 60 Hz** and full contact/CCD.
Median **1.08558 ms**, p95 **7.12169 ms** (< unchanged **16.67 ms** gate), maximum
**71.359 ms**, **24/900** steps over 16.67 ms. Thread-CPU p95 is **7.03735 ms**;
wall time remains the gate. Physical aggregate is unchanged: **56,576 contact
rows, 19,163 CCD iterations, seven bounded fallbacks**. No performance engine
correction or causal speedup claim is made in this lifecycle batch.

The prior checkpoint p95 7.0623 ms/max 33.3996 ms and earlier failed raw p95/max
measurements remain intact. External host load is uncontrolled; tails remain.
Linux and Windows CPU-intensive workloads are scheduled separately. Windows VM
56.376 ms historical maximum remains diagnostic only.
[Current raw timing/identity](evidence/m78-lifecycle/performance/RESULT.json).

## Resumed audit and remaining findings

The [additive coverage overlay](evidence/m78-lifecycle/coverage/README.md) preserves
all **485 overlapping discovery IDs / 970 baseline platform states** and all
**77 original requirement identities**. **19 briefs are available; 58 remain
unavailable.** Scoped case receipts attach separately; symbol presence, source
locations and suite counts do not promote whole members/groups.

The new public audit executes ordinary registered JS, asynchronous services and
Application boundaries. It checks two independent prefab hierarchies' complete
constructor payload/properties/motion, internal/live external references, malformed
second-slot and stale-reference rejection without partial publication. It checks a
checksum-valid unsupported required save participant, unchanged live physical/
script/session/slot state, continued callbacks and one actual 60 Hz step, followed
by ordinary successful load and restore-not-start. It does not establish arbitrary
late-callback side-effect rollback, every participant, storage fault, schema limit
or all UI/IK/lifecycle combinations.

Ranked unresolved issues/limits:

1. **LIF-003 corrected in the final follow-up:** current entity tags are
   authoritative for the motor query capsule. Immediate mutation, lazy creation,
   replacement, disable/re-enable, actual JS callbacks and retained streaming
   pass. The original 20/5 failure and expanded same-driver 31/8 failure remain.
   [Current correction and receipts](evidence/m78-lifecycle/lif003/REPORT.md).
2. **FLU-001, deferred and unchanged:** legacy-PBF four-second settling failure;
   no fluid source, tests, tolerances, M54/M55, FTFT/JSGT or independent research
   changed. Separate future criterion remains a proposal, not an accepted pass.
3. **Legacy continuation / old excluded slots:** exact `a106…` snapshot lacks
   historical continuation, still 13/1. Newly captured saves cannot invent missing
   old state. Orphan-bearing excluded-actor slots still reject strictly.
4. **Physics tails / incomplete mesh sleeping:** p95 passes, maxima/fallbacks
   remain; no unconditional response guarantee under host contention.
5. **Coverage/platform/human gaps:** per-member errors/phases/stale handles,
   all authoring/cook/mod/export combinations, original requirement availability,
   complete UI/shared-IK public routes, additional native editor/package/application
   routes and physical Windows hardware remain explicit. No engine-wide
   certification percentage.

## Combined human review checklist

1. Review baseline versus corrected motor filters/enabled state before the first
   step; walk out/back across streamed regions and repeat without stale capsules.
2. Exclude a legally eligible simple dynamic actor, save and reload; retained
   actors preserve real sleeping/velocity state. Required articulation bodies must
   remain. Review malformed/orphan rejection and runtime motor tags before/after streaming.
3. In BalanceLab, exercise F1–F4 step/support/recovery/radial scenes; save mid-step
   and cold-load. Confirm actual feet/bodies reach supported standing.
4. In CharacterLab, exercise active/passive motion and moving anchors, then cold
   save/load. Confirm physical authority and no stale or teleported anchor.
5. Review the public prefab/save audit and the exact original failures. Check
   independent initialized instances, rejected private load and normal restore.
6. Edit/Play/Stop/reload, export and launch the moved package outside the engine
   directory; check focus/rendered motion manually. Later test physical Windows
   GPU/input/audio/controllers. Automated software GL/VM is not human acceptance.

## Inventory, fingerprints and readiness

Final accepted-source inventory and fingerprints are recorded in
`evidence/m78-lifecycle/CHANGED_FILES_V4.txt`, `FINAL_FINGERPRINTS_V4.json` and
`FINAL_VERIFICATION_V4.json`. V2 remains the exact earlier handoff identity; its
expected hashes were not overwritten to hide intentional follow-up changes.
The final read-only checker is `evidence/m78-lifecycle/lif003/verify_final.py`.
Builds/packages remain ignored hash-only outputs. Unrelated diagnostics and
protected history are preserved. No tag, M79 work or human-validation claim.

The initial capture and failed verifier remain unchanged: a basename filter
incorrectly excluded the nested Windows verification receipt from inventory
comparison. All source/evidence/binary hashes matched. The corrected full-path
filter and separate V2 capture preserve that
[tooling failure](evidence/m78-lifecycle/fingerprint-setup-failure/README.md);
no runtime/test source, original expected hash or accepted receipt was changed.

The combined LIF-001/LIF-002/LIF-003 corrections pass focused Linux acceptance
and are authorized to checkpoint independently of remaining M78 audit coverage.
The final follow-up repeats critical physics, M70/M77 actual cold endpoints,
saves and streaming on the combined archive. Deferred fluid/legacy limits,
physics tails and coverage gaps remain. This is not comprehensive certification.

Final fingerprint tooling initially compared historical role metadata as though it
were byte identity; that false mismatch is preserved under lif003/fingerprint-setup-failure-v3.
V4 checks bytes/SHA only, retaining prior expected hashes and the failed V3 capture.
