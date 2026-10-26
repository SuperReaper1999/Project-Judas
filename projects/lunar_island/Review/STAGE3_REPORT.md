# Lunar Island Stage 3 — People of the Fracture

2026-10-26. Ordinary consumer project on Experimental, based on Stage 2 prerequisite
checkpoint `8cc90f316f4b5fcae5f89b5dce6935bde6f5441d`. Human gameplay review pending.
Main is preserved. Stage 4 vehicles and Stage 5 expansion have not started.

## Implementation and ownership

Six independently stateful NPCs: Mara (quartermaster), Ivo (forager), two hostile
scavengers and two wolves. Two repeat-safe quests connect real gathering/combat
with finite starter equipment, token rewards, trading, ammunition and food.
Finite vendor stock/currency, atomic single/bulk purchases and resale reuse the
ordinary shared bag/container system. Corpses expose the defeated actor's remaining
bag; ranged enemies consume their own loot ammunition when firing.

Pistol, two-shell/six-pellet shotgun, automatic rifle, knife and bat complement
hands/axe/pickaxe. Public raycasts, shooter exclusions and actual hit-point impulses
provide collision/hit meaning. Magazines transfer from real compatible reserve
rounds, with timed/cancellable reload, camera recoil, aim FOV, original PCM audio
and bounded impact particles. First/third-person views share one player/state.
Protected friendly survivors cannot be damaged. Shooting a dead body's dynamic
proxy applies physical impulse without another kill/reward.

`combat.js`, `person.js`, `social-ui.js`, `encounter-rules.js` and transient
`people-session.js` contain all game rules. `Authoring/encounters.json` owns weapon
profiles, routes, dialogue, quests, prices and stock. The author validates references,
IDs, quantities and route shape before replacing generated scene content.
`Authoring/encounter_manifest.json` is the generated receipt; no hidden engine path.

Navigation is advisory; ordinary CharacterMotor performs collision-resolved movement.
Patrol, line-of-sight/hearing pursuit, windup attacks, ranged ammunition and low-health
retreat are bounded JS decisions. Nearby reached barricades receive project integrity
damage; physical bodies/joints disappear only on actual destruction. Dynamic construction
blocks physical movement/rays but does not automatically rebake navigation.

The baked 220 x 220 m homestead surface contains 104 layers/782 polygons. Source-layer
selection excludes the duplicate coarse far-island shell, leaving physical collision
masks unchanged. Readiness UI waits for navigation publication and required models
before beginning active play, rather than assuming that an error-free loading request
has already completed.

## Assets and persistence

Existing original adventurer rig/palette variants, generated human Attack clip,
original starter-pack pistol/rifle and project-authored shotgun/knife/bat. The actual
Prompt3D trained prompt parser/procedural animal generator produced the wolf from
`a realistic gray wolf`, seed 91303, realistic preset. Raw settings/results and hashes
are preserved. No generator/research directories were modified. Existing CC0 PBR
material provenance remains in ArtSource. Original deterministic PCM supplies new
feedback; voice/SFX Designer remains deferred until its separate Linux port.

Ordinary native saves retain actor state/bags, quest/gift progress, player equipment,
magazines/reload and actual spawned corpse bodies alongside previous game systems.
Stage 3 has its own identity; earlier saves remain untouched, without a migration
claim. Content changes are subject to ordinary save compatibility checks.

Stage 2's two separately authorized generic persistence repairs were checkpointed
in 8cc90f3 before this Stage 3 content. Their 148 continuation, 29 joint, 27 world
persistence and 43 save-evolution checks are documented in the Stage 2 report and
`docs/PREALPHA_LUNAR_PERSISTENCE_CORRECTIONS.md`. No additional native engine/API
changes were needed for Stage 3.

## Preserved failures and corrected follow-ups

- Invalid query area declaration and initial wrong model metadata were authoring errors.
- Undefined wildlife/presentation state and missing loading-screen imports were game
  script errors, retained with corrected follow-ups.
- Early pursuit saw no loaded surfaces. The actual bake passed independently; the
  asynchronous resource was still loading. The explicit readiness condition corrects
  the project/test precondition. This is not a demonstrated engine navigation defect.
- A corpse-interaction fixture aimed through camp cover; a capsule/ray-verified
  unobstructed player viewpoint corrected the test, with unchanged game rules.
- Third-person test initially aimed from the eye rather than the offset active
  camera. Final proof centers the real reticle with logical mouse input, checks an
  independent native camera ray and aim FOV, then fires normally.
- A 55 m third-person shot independently covered the survivor with its native camera
  ray but missed with the gameplay shot: the camera intention query was capped at
  30 m. It now uses at least the requested weapon range, retaining eye-to-hit
  occlusion. The original 80-check/one-failure run is preserved in far-before.log.
- Cold restore called clear on an already-disabled saved agent. The NPC restore
  guard now respects that state. Changing scripts between save/read was separately
  rejected by normal compatibility checks, correctly; the final pair uses identical files.
- Independent barricade fixture initially selected a backpack row through a dangling
  temporary and accidentally reopened Pause after successful construction. These
  observer errors were corrected, not treated as game passes.
- Once active, native geometry proved the human attack ray at height 1 m hit the
  window sill (entity 3023), while the existing human sight height 1.4 m hit the
  physical board. NPC reach now uses that appropriate human/animal height and keeps
  actual ray, distance and integrity checks. The original geometry/failure is retained.
- Accelerated dummy-audio runs may fill the 32-one-shot voice cap. Optional feedback
  logs once and cannot abort committed inventory/combat transactions. Listening and
  ordinary human audio cadence remain acceptance checks.

## Validation and measurements

Final results and exact commands are recorded under `Review/stage3` and below.
The native observer controls only player placement/aim and normal logical input/UI.
It does not grant supplies, change enemy health/poses or bypass quest conditions.
The independent barricade copy changes only authored guard start/route/sight, keeping
all production behaviour modules unchanged, with an unalerted negative control.

- Pure inventory/crafting: 79 checks; encounter transactions/quests: 36 checks.
- Native combat/quests: 80/80; fresh-process restored gameplay/physical corpses:
  33/33. The 55 m third-person camera ray and shot agree after the correction.
- Existing harvesting/housing/building/swimming loop: 133/133; independent cold
  restoration: 42/42, including actual hinge geometry after 120 resumed steps.
- Independent unalerted/alerted barricade fixture: 29/29.
- 19 script modules compile/link; scene, UI and all prefabs validate normally.
- Editor: duplicate/undo and Play/Stop restore identical authored scene, scene save
  succeeds, native resource/job failures zero (isolated project copy; human UX pending).
- Export: 119 assets/one scene, 202,439,723 bytes, 5.85 seconds. A genuinely moved
  package in a path with spaces launched from unrelated /tmp and loaded the final
  expedition through the ordinary save service; no script/harness error.

600 rendered frames on Ryzen 5 5600/GTX 970 4 GB, Linux Release, hidden 1024x768
window, vsync off, profiler on, dummy audio and controlled 60-Hz simulation clock.
This measures walking through the homestead with six active NPCs, not full island
population or displayed human FPS. Frame mean 6.42 ms, median 6.30, p95 7.65, worst
12.50. Native rigid-body Step mean 0.024 ms, p95 0.032, worst 0.418; 307 bodies. Retained
120-frame profiler window: JS fixedUpdate mean 0.477/p95 0.589 ms, JS presentation
0.292/0.437, UI update 0.123/0.135, navigation update 0.414/0.673. Main-camera GPU
inclusive mean 1.604/p95 2.935 ms; nested GPU passes are not added. Resident memory
~489 MB, time peak 537064 KiB, no measured swaps. Raw profile is losslessly gzip
compressed, with original decompressed hash in performance/SUMMARY.json. These are
scoped costs, not a before/after speedup claim against different Stage 2 content.
CharacterMotor, navigation and JS are outside that rigid-body Step scope; the full
frame includes them. Nested profiler scopes are not added together.

The engine is the Stage 2 corrected Release binary; embedded provenance honestly
identifies the dirty Stage 1-based build that produced those native fixes, not a
fictional clean Stage 3 engine rebuild. Runtime/editor/exporter/observer bytes and
current source fingerprints are recorded separately; no native implementation
changed during Stage 3.

## Limitations and handoff

This is Stage 3 of an incremental project, not a completed island RPG. The two
cottages, homestead resources, nearby camps/trails and full coarse terrain shell
remain the playable scope; towns/city/harbour are reference plans. No vehicles,
regional NPC unloading, respawning economy, navigation runtime rebake, voice dialogue
or universal combat/quest framework is claimed. NPCs are six modest authored actors,
not tactical squads. Friendly fire is deliberately protected.

Corpses are physical rigid proxies, not articulated ragdolls. Wolves lack gait
animation; human clips lack weapon-specific aim, recoil/reload, hit reactions and
proper melee/death sets. Third-person weapon presentation is incomplete although
camera, reticle, ammo and hit behaviour are exercised. These are explicit next art
requirements; see README authoring instructions and human checklist. No Windows,
Android, controller hardware, listened-audio or human gameplay acceptance claimed.

Project: `projects/lunar_island/lunar_island.judasproj`.
Standalone: `.cache/lunar-island-stage3/Lunar Island/judas` and archive alongside it.
Run with the ordinary runtime/editor. Controls and reproducible tests are in README;
operator checklist is `Review/stage3/HUMAN_CHECKLIST.md`.


## Checkpoint scope and integrity

`Review/stage3/CHANGED_FILES.txt` lists the exact Stage 3 commit inventory.
316 current project/prerequisite source fingerprints and ten binary/archive hashes
are retained separately. The Stage 2 report/evidence and its historical fingerprints
remain unchanged; current source legitimately differs as Stage 3 adds content.
All packaged JS modules and the baked navigation asset were compared byte-for-byte
with the final source. The losslessly compressed raw profiler hash was also verified.
Generated build outputs, executables and temporary packages are excluded from Git.
The retained `.save` is a small synthetic gameplay test fixture, not a build output.

No Stage 3 native engine changes, main promotion, release tag or invented human
acceptance. Protected historical work, independent fluid research and unrelated
untracked experiments remain untouched. Stage 2's genuine persistence fixes remain
in their own prerequisite checkpoint. Human acceptance is the next stage gate.
