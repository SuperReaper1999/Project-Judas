# Gravity selection follow-up — approved checkpoint

Base: `0f3694bb7142dc38a59abbd7365a113b673bae5e`, the M72 checkpoint
initially pushed with human validation outstanding. On 2026-10-09 the operator
accepted the desktop M72 demo and authorized the gravity API follow-up for
commit/push. This follow-up is not a numbered milestone. New gameplay remains
project JS. See `checkpoint-review.md` for the exact acceptance boundary.

## Changes

Optional owner gravity intent (`uniform` or existing `field`) uses one shared
sampling path; absent intent preserves spatial routing. Motors, full/coarse local
rigid bodies, gravity-enabled particles and non-rigid deformables consume it.
Camera and support remain separate. Selection changes do not invalidate structural
metadata; streaming reads the current selection while iterating existing IDs. Persistence, prefab/editor/reference remaps,
retained streaming identity and source pins use existing infrastructure.

An existing root-scene rotated box gravity-volume defect was reproduced before
engine edits, then corrected with the same oriented predicate as streamed regions.
See `root-zone-before.log`: the rotated-zone interior returned zero rather than
expected +X acceleration. Source-field sampling remains ordinary GravityField.
No liquid solver, pairwise/orbital force, legacy player or physics solver redesign.

## Executed checks

| Check | Result | Receipt |
| --- | --- | --- |
| Selection, actual JS VM, motor wall support/up, independent rigid bodies, RadicalGravity, direct field sampling, atomic input, save reconstruction, scene/named/prefab/editor undo/duplicate/reference, streamed pins/unload/revisit, stale handles | 70/70 | `focused-final-70.log` |
| CharacterMotor regression and 100-motor sanity timing | 32/32 | `character-regression.log` |
| Modern persistence / resumed first steps | 27/27 | `persistence-regression.log` |
| Existing region/residency regressions | 71/71 | `streaming-regression.log` |
| Existing canonical fingerprints | 140/140 | `fingerprint-regression.log` |
| Independent existing uniform gravity fixtures | 12095/12095; 136 scenes, 272 constructions, 32640 steps | `uniform-regression.log` |
| New copyable example / live export enumeration setup | 9/9 each | `cookbook.log`, `api-enumeration.log` |
| Standard TS 5.9.3, all 37 examples, source/type/inventory + actual VM agreement, negative drift controls | PASS; 330 symbols, 228 bridge operations, 3 negative controls | `api-live-types.log` |
| Editor duplicate/undo and Play/Stop | authored scene IDENTICAL; save ok | `editor-smoke.log` |
| Export and moved package from unrelated /tmp cwd | 3 assets / 1 scene; 380 scripted frames, pause/resume/reload and captures | `export-final.log`, `moved-package-final.log`, `demo.harness` |

Release targets were rebuilt as affected. No full production-suite repeat.
The initial build has two existing M72 material-setter indentation warnings and
one new serializer warning (the latter corrected); final affected builds introduce
no new warnings. TypeScript was temporarily installed in `/tmp` with scripts
suppressed; it is not a project/runtime dependency.

Existing tests only read protected fixture/evidence paths; output goes to cache or
this new receipt directory. Old evidence remains unchanged. Offscreen tests use
dummy audio and cannot establish real desktop capture feel, physical
audio or native Windows acceptance. `Mouse capture failed` in offscreen logs is
expected; no desktop-human acceptance is claimed.

The harness CSV's legacy observer gravity columns are spatial gravity. Effective
motor/owner gravity is asserted by the focused tests and shown by the project HUD.
Screenshots document rendered state, not a human acceptance substitute.

## Development failures preserved

- `root-zone-before.log`: genuine pre-existing rotated-zone root-load failure.
- `build-02.log`: new test used the wrong metadata helper signature; corrected.
- `focused-01.log`: test script omitted its constructor's Entity assignment; corrected.
- `focused-02.log`: strict JS float equality and omitted normal snapshot preparation
  caused two test failures; tolerance and the normal Prepare path corrected.
- `api-types-01.log`: new example lacked typing/state annotations; corrected.
- `demo-01` through `demo-03`: new project quote/missing positional-field errors,
  followed by an unsupported harness resource-wait command; content/harness corrected.
- `focused-final.log`: reusing an already-created temporary project made the test
  fixture fail on repetition; its owned fixture is now recreated. Verified repeats
  are in `focused-verified.log` and `focused-final-70.log`.

These fixture/content mistakes are not claimed as existing engine defects.
Intermediate passing logs are retained alongside the final receipts.

## Performance and handoff

Final small-world sample loop (20000 each): spatial 37.171 ns/call, uniform 28.642,
selected-field 98.478. Earlier runs vary with machine load. Existing 100-motor
regression reports 3.856439 ms per whole batch over 120 samples. These are lightweight
sanity measurements, not an equivalent pre/post performance claim.

[Design/API/limits/human checklist](../../GRAVITY_SELECTION.md).
Project: `projects/gravity_selection_lab/gravity_selection_lab.judasproj`.
Moved package: `/tmp/judas-gravity-selection-moved/judas` (local ephemeral artifact).
G chooses an aimed surface; F restores spatial gravity; C selects an authored
uniform field; T selects RadicalGravity. WASD/mouse/Space, Escape pause, R reload.

`all-changed-files.txt` lists all changes including receipts.
`changed-files.txt` lists every implementation/docs/demo file, excluding this new
receipt directory and the unrelated pre-existing `asset_packs/`.
`source-sha256.txt` fingerprints those final files; `results.json` is the concise
machine-readable result. No protected evidence/research/prototypes, accepted
fluid implementation or historical project assets were changed. LowPolyAssets
is left alone. No later feature/milestone work or roadmap file was added.

Approved commit: `Add optional per-entity gravity selection`.
