# M78 final lifecycle checkpoint — LIF-001 / LIF-002 / LIF-003

Linux is the authoritative acceptance platform under the operator's current rule.
Windows is best-effort functional evidence, not a release or performance gate.
This corrective checkpoint does not complete M78 or confer human validation.

## Mechanism and generic correction

LIF-003 had two independent publication defects. AddTag/RemoveTag published only
EntityCategories.body, while a CharacterMotor owns a separate query capsule.
Lazy construction, shape replacement and fixed refresh used SceneObject.tags,
which is the authored initial state rather than the mutable entity classification.

RuntimeWorld.cpp now publishes successful mutations to an existing owned capsule
without creating a lazy motor or changing collision layers/enabled state.
WorldCharacter.cpp uses TagsOf(id) for construction, replacement and fixed refresh.
No new API, fixture-name branch, solver budget, gravity, cadence or fluid change.
Authored tags remain the initial state. Existing streaming snapshots already use
TagsOf; the correction makes the restored physical proxy agree with that state.
LIF-001 publication/lifetime and LIF-002 ownership/sleep fixes are retained.

## Linux combined final-source results

| Check | Result |
| --- | --- |
| MotorRuntimeTags, independent ordinary rigid-body controls | same driver prior candidate **31/8** → final **31/0** |
| CharacterPublication, expanded actual JS and retained streaming tags | **144/0**; original 131-check predecessor and failures preserved |
| SaveExclusion, included/excluded/reincluded/articulation/malformed | **77/0** |
| Public Application prefab/save acceptance/rejection | **55/0**, 25 frames |
| Continuation / unchanged M77 controller | **112/0**, **119/0** |
| Rigid motion / physical animation / joints | **10,239 pass**, **64/0**, **29/0** |
| Contact lifecycle / impact CCD | **1,076/0**, **89 cases / 3,197/0** |
| Save / prefab / streaming / CharacterMotor / gravity | **27/0**, **37/0**, **82/0**, **32/0**, **70/0** |
| M77 step / lifecycle / radial | **15/0**, **26/0**, **18/0** |
| M77 fresh cold step / stand / recovery write→read | **24→13**, **23→13**, **21→13**, all passing |
| M70 application / fresh cold write→read | **98/0**, **10→14**, all passing |
| Mixed physical cold write→read | **25→13**, actual durable endpoint equal |
| SaveStreaming Application write / read / read-b | **11/0**, **19/0**, **19/0** |
| Exact original legacy a106 snapshot | **13/1**, intentionally preserved failure |

Actual physical endpoint comparison excludes only fresh native handles and array
traversal order; no physical fields/tolerances are removed. The unchanged tests,
inputs, controller settings, timestep and quality remain. No full production run.
Two production translation units were compiled with the existing Release flags;
unchanged objects/dependencies were reused and linked into one final archive.
No clean whole-engine rebuild claim. Build/driver receipts record exact identities.

The tag fixture also proves absent-tag negatives, no elapsed simulation,
lazy removal, replacement generation safety, disable/re-enable, unknown owner/tag
rejection and layer-query rejection. The JS fixture calls actual public entity
methods and physics.raycast; it does not inspect symbols or native dispatch.
Streaming retires real bodies and restores current tags into fresh handles.

## Native Windows informational results

Same final tag fixture **31/0** and expanded publication/JS/streaming fixture
**144/0**: **175 checks, zero failures**. Native MSVC Release compile/link inputs,
source bundle, output hashes and logs are retained under native/. New production
WorldCharacter/RuntimeWorld objects plus unchanged current PhysicsWorld,
WorldPersistence/WorldRegions/WorldScripts objects precede the immutable archive.
Link guards reject extraction of replaced old members. Host backups verify all
new test executable/object bytes; binaries remain ignored, not tracked evidence.

No VM configuration change, performance tuning or unrelated process termination.
Earlier V2 native 630-check results retain their earlier source scope; the other
Windows routes were not rerun here. New native editor/moved-package and physical
Windows GPU/input/audio validation remain missing. Linux acceptance is independent.

## Remaining issues and boundaries

- FLU-001 remains failed/deferred, unchanged; no fluid implementation, criterion,
  protected research or Solbert independent work modified.
- Exact legacy a106 bytes remain immutable. Missing historical continuation is
  not invented; new-format passes do not promise retroactive exact reconstruction.
  Old orphan-bearing excluded-body saves remain strictly rejected.
- Prior Linux four-ragdoll p95 **7.12169 ms** passed the 16.67 ms gate, but maximum
  **71.359 ms** and **24/900** late steps remain. That is the preserved V2 timing
  observation, not a fresh LIF-003 performance measurement or tail guarantee.
- Windows VM timing remains diagnostic. No claim of cross-platform bit-identical
  trajectories, hardware acceptance or complete Windows validation.
- Comprehensive M78 per-member/public/error/authoring/export coverage remains
  incomplete in the retained coverage register. No M79, tag or human validation.

## Files, evidence and reproduction

Whole combined intended inventory: ../CHANGED_FILES_V4.txt. Final source/evidence
and ignored binary fingerprints: ../FINAL_FINGERPRINTS_V4.json; verification:
../FINAL_VERIFICATION_V4.json. Previous V2 expected hashes remain unchanged.

Narrow final production correction: src/RuntimeWorld.cpp, src/WorldCharacter.cpp.
Permanent MotorRuntimeTagsTests + expanded CharacterPublicationTests are registered
in CMake. Earlier WorldPersistence/WorldRegions/WorldScripts changes are included.
Current README, architecture, JS character/save docs and lifecycle handoff agree.
No public type/signature change requires a new API surface.

Use a standard Release CMake build, then run:

```
judas_motor_runtime_tags_tests <fresh-output-directory>
judas_character_publication_tests <fresh-output-directory>
judas_save_exclusion_tests <fresh-output-directory> <repository-root>
```

run_combined.py and run_public.py preserve the exact focused serial relink/run
commands against the frozen archive, with logs and SHA identities. They use
existing local build inputs and refuse existing output directories. The portable
checker verify_final.py reads expected fingerprints without regenerating them.

## Human follow-up (not a checkpoint gate)

1. Mutate tags on a motor; check filtered casts before/after a fixed step.
2. Leave/revisit a streamed region; confirm current tags and no stale capsules.
3. Save an eligible excluded body; load and verify retained physical state.
4. Cold-save/load mid-motion in BalanceLab/CharacterLab; check actual support.

Final fingerprint tooling initially compared historical role metadata as though it
were byte identity; that false mismatch is preserved under lif003/fingerprint-setup-failure-v3.
V4 checks bytes/SHA only, retaining prior expected hashes and the failed V3 capture.
