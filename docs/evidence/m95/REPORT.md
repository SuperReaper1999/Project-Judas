# M95 — animation production workflow

Base: `16889b73423a8fb0e9c7682db1d8f66c648cbfc3` (accepted M94).
Development and delivery: Experimental only. **M95 human acceptance pending.**
Windows/Android M95 validation was not performed and is nonblocking.

## Implemented

- Immutable named clip event tracks, authored in normal selected-output import
  recipes. Bounded validation, deterministic model cooking/roundtrip and atomic
  last-good model publication. Marker times refer to output clips after trimming.
- An editor **Animation timeline** over the existing asynchronous import/preview
  pipeline: clip choice, play, scrub, skeleton/root-path preview, event selection,
  add/edit/delete, move-to-cursor, bounded draft undo, explicit save and reimport.
  Disk conflicts reject overwriting. Unsaved marker drafts survive clip/asset/
  project switching until explicit Save/Discard. Import completion is independent of the
  Asset Browser being open (the timeline regression closes it during recook). During Play it inspects the selected live
  animation's clock, blend, sample and diagnostic without mutating it.
- Fixed-step `Animation.advance(seconds)` and detached `Animation.sample`:
  crossed markers and blended extracted model-local motion. Forward/reverse,
  endpoint and multi-loop rules are explicit. Seeks and preview emit no actions.
  A marker overflow reports failure with no partial delivery.
- Generic `Character.displace(worldMetres)` and `Character.orient(worldQuaternion)`.
  They use the existing capsule sweep/slide/step, gravity, support and bounded
  pushing machinery. External velocity receives contact-plane response; authored
  displacement does not accumulate into momentum. Orientation intent preserves
  motion rather than invoking the transform teleport/reset path.
- Explicit advancement shares the existing mixer and M70 reference/final-pose
  ordering. There is no second animation clock, character solver, Animator state
  machine, built-in footstep/combat rule or camera behaviour.
- Optional saved observations and extended streamed records, with legacy record
  reading. Clips without events/extracted motion retain their original save and
  streaming observation layout. The maximum composed save envelope is 22 records
  (previously 21), matching the new optional participant; the next count still
  rejects. SaveService's exact schema accounting recognizes the new participant.
- Typings, references, machine inventory, structured result checks and an
  executed copyable example are synchronized: **37 exports / 428 symbols**.

Implementation locations and exact file inventory: [CHANGED_FILES.txt](CHANGED_FILES.txt).
Public contracts: [M95 workflow](../../M95_ANIMATION_WORKFLOW.md).

## Motion Studio

`projects/animation_studio/animation_studio.judasproj` is an ordinary editable
project. Original CC0 source art is derived from the existing CC0 M70 multipart
rig, with six authored performances: Idle, Walk, Run, Jump, Interact and Strike.
The imported rig has 72 hierarchy nodes including helpers/mesh/conversion nodes.

Project JS owns movement/clip selection, jumping, event policy, third-person
presentation camera, pause, save/load, scene changes, interaction and the physical
strike. Native physics receives a real impulse at the actual raycast hit point.
There is no normal-action target teleport. The flat and rotated studios use
ordinary authored uniform gravity regions; the rotated motor reports local up
along world negative X. The watch tour is optional ordinary project behaviour.

Controls and the consolidated human checklist are in the
[project README](../../../projects/animation_studio/README.md).

Standalone: `.cache/m95/checkpoint-delivery/package/judas` with its adjacent files.
The complete package was copied to an unrelated `/tmp` directory and launched
from a different unrelated working directory. File and runtime hashes matched.
Both scenes, pause/resume and reload ran there without script faults.

## Numerical/import finding discovered during regression

The unchanged M74 retarget/cook regression exposed a pre-existing trim mismatch.
An isolated build substituted accepted HEAD's exact `ModelCook.cpp` as the sole
import-library translation unit, using the same test/dependencies/candidate ABI.
It reproduced the same failure: 37 checks, one failure; indexed sample comparison
reported `0.01111111044883728`. This is not claimed as a full historical-binary
baseline or a measured visible pose error at equal timestamps.

Root cause: the older trim guard discarded legal, distinct rebased keys within
an arbitrary `1e-6` neighbourhood. Its original purpose was to remove keys that
actually collapse or overshoot when rebased in float. Dropping distinguishable
keys broke the exact schedule/sample contract.

Correction: reject only truly non-increasing or out-of-duration rebased keys.
The unchanged dense retarget comparison now passes with zero sample error.
The original dense-60-Hz rebased-key collision regression also remains passing.
Cook revision is bumped so deliberate reimports use the corrected algorithm.
No test tolerances, source fixtures, controller settings or physics budgets were
weakened. Original failure, accepted-source isolation and corrected receipts are
preserved under `development/trim-isolation`, `linux` and `cook`.

## Moving-root IK correction

A meaningful pinned-world-hand check exposed an initial M95 ordering defect:
explicit clip advancement solved world-space IK before the submitted root delta
moved the motor. Keeping that result produced a one-step world-target offset.
The earlier empty-target mapping check did not establish this spatial contract.

The corrected path samples the clock/markers first and defers world IK to the
existing appropriate motor/reference phase. M70 references resolve after its
pre-physics motor; legacy limbs resolve after the ordinary motor. Zero-time pose
recomposition advances neither clock nor markers. Plain root-motion instances
without IK retain the single sample path. IK is solved once in its proper frame;
its existing budgets, damping and tolerances are unchanged.

| Actual resolved world-point error | Before | After |
|---|---:|---:|
| Legacy limb after approximately 0.1 m root movement | 0.109755576 m | 0.000000060 m |
| Shared IK after 0.1 m root movement | 0.099996328 m | 0.000019018 m |

These are numerical endpoint errors from the unchanged planted-target test,
not physical measurement-accuracy promises. The reference world targets stay
fixed; the test queries the actual resolved joint after the motor moves. Both
failures and the corrected follow-up are preserved in `development/root-ik-*.log`.

## Linux validation

Release builds passed. The combined final-source build is recorded in
[closing-build.log](closing-build.log); earlier build/follow-up receipts are retained.
No full production-suite sweep was run. Focused native results:

| Check | Result |
|---|---:|
| New event, authoring, root-motion, collision, save and IK workflow | 59 / 0 failures |
| Real Application gameplay, timed impulse, rotated scene, pause/UI and F6/F7 | 15 / 0 |
| Separate-process animation/motor save write / read | 8 / 8 checks, 0 failures |
| CharacterMotor / runtime tags | 32 / 31 checks, 0 failures |
| Skeletal animation / pose composition / physical animation | 34 / 16 / 64 checks, 0 failures |
| Retarget cook / model reimport / retarget authoring | 37 / 39 / 20 checks, 0 failures |
| Existing animation Application | 4 / 0 |
| M94 debugger/reload regression | 49 / 0 |
| Save storage, including 22/23 participant boundary | 38 / 0 |
| Existing save lab fresh-process write / read | 13 / 20 checks, 0 failures |
| Composed streaming save write / read / read-backtrack | 11 / 19 / 19 checks, 0 failures |
| Executed animation-motion / animation / actual VM surface examples | 9 / 9 / 9 checks, 0 failures |
| TypeScript 5.9.3 + inventory/live VM + negative drift controls | Pass |
| 47 documentation examples statically checked | Pass; not 47 execution claims |
| Studio registered modules compile/link | 2 modules, 0 failures |
| Two relocated deterministic cooks | Identical SHA256 |
| Invalid event time | Rejects, accepted model bytes retained |
| Editor timeline edit/async recook, undo, save, Play/Stop | Pass |
| Export and relocated standalone | Pass |

Cold continuation checks compare actual motor position/velocity, clip phase,
manual clock ownership and marker count before and on the first resumed solver
boundary. They do not substitute a cached controller status for physical state.
The real F6/F7 test additionally traverses normal SaveService staging/publication.

Logs and measured samples are retained in `linux/`, `cook/`, `delivery/` and the
selected `RESULTS.json`. Genuine early failures/follow-ups remain separately
classified in [development/INDEX.md](development/INDEX.md).

## Performance

Intel i7-4790, native Linux Release, GTX 970 desktop. Animation-stage CPU timings:
20 warm-up and 300 measured 60 Hz updates per population, each with the same
72-node extracted-motion Walk asset. These exclude rendering, game JS and general
physics. Raw per-step values are in `linux/judas_animation_workflow_tests.log`.

| Instances | Median ms | p95 ms | Worst ms |
|---:|---:|---:|---:|
| 1 | 0.013050 | 0.024215 | 0.042909 |
| 32 | 0.428746 | 0.547677 | 0.673295 |
| 100 | 1.366545 | 1.642800 | 2.151414 |

A two-marker interval query averaged 0.164717 microseconds over 100,000 calls.
Desktop timing varied across retained runs; an earlier 100-instance run reached
6.405540 ms worst / 2.663421 ms p95. These are bounded workload observations,
not a guaranteed frame-time ceiling or a historical speedup claim.
See [hardware and method](HARDWARE.json).

## Candidate identity

The closing candidate passed all 25 focused driver runs, plus editor/export/
relocated-package and independent deterministic-cook checks. `linux/RUNS.json`
records exact commands and timestamps. `CHANGED_FILES.txt` lists the 206 intended
checkpoint paths. `FINAL_SOURCE_SHA256SUMS` covers 459 source/input files;
`BINARY_SHA256SUMS` covers 23 local build/export products. The exported runtime
matches the validated build byte for byte. `CHECKPOINT_FILES_SHA256SUMS` covers
every intended checkpoint file except itself; all three manifests were verified.
Build products remain outside Git. Pre-existing unrelated untracked files are
preserved and excluded from staging.

## Limits and handoff

- Timeline edits clip events, not skeletal bone keys; DCC tools still author poses.
  DCC notify tracks are not inferred; author output markers in the import recipe.
- Morph targets, motion matching, foot locking and stride warping are deferred.
- Marker payloads are bounded text; embedded NUL/binary payloads are unsupported.
- Fading root deltas use fixed-interval midpoint weights, not exact continuous
  blended SE(3) integration. Events are observed at fixed boundaries, not substep
  callbacks. JS chooses blend-source/weight/reverse policy.
- Automatic `sample` is a snapshot, not a queue: catch-up can replace it. Use
  explicit `advance` and consume its return for guaranteed per-tick game actions.
- CharacterMotor retains its existing unit-scale and bounded recovery/slide
  limitations. Orientation uses existing capsule overlap recovery, not a new
  continuous angular cast. Dynamic bodies do not gain automatic root authority.
- Source character art is a readable prototype robot, not finished human motion.
- Windows/Android M95 and human acceptance are outstanding.

Ready for an Experimental checkpoint and operator testing. `main` is not promoted.
Protected historical evidence, independent fluid research, accepted fluid work and
existing project content were not modified. No post-M95 feature work was started.
