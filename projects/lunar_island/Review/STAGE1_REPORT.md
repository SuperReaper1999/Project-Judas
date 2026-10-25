# Lunar Island Stage 1 handoff

Status: implemented and Linux automation verified; **human acceptance pending**.
Stage 2 has not begun. This is a consumer game, not an engine milestone or Alpha.
Engine source/base: c2cdd376f1c20a9d6a8a75d8257f9ac44327ec09. Development:
Experimental only; main remains at that accepted baseline. Game checkpoint is the
commit containing this report; see Git history and the final handoff message.

## Delivered

Ordinary editable `projects/lunar_island/lunar_island.judasproj`, one native scene,
three editable/cooked terrain tiles, native prescribed water, public CharacterMotor,
first/third-person camera, project logical controls, inventory/equipment/hotbar,
physical resource harvesting, crafting, cooking, consumables and survival.

A complete first-session loop gathers sticks/stone/fibre, crafts tools, physically
fells and processes trees, mines deposits, cooks berries, refills/drinks a flask,
manages weight/slots, drops/retrieves physical bundles and saves the resulting world.
The journal and native modal menus explain the actions. Full saves preserve actual
physical bodies and destroyed entities as well as project-owned JS state.

397 authored entities, 226 harvest nodes (150 trees, 26 fibre plants, 15 berry
patches, 10 stone deposits, 6 ore deposits, 8 loose branches, 6 loose stones and
5 salvage nodes). 231 initial physics/query bodies; uncut resources are static.
Only felled trunks and dropped bundles become dynamic. Trunks use real 70 kg rigid
bodies and hit-point impulses. Processing is project gameplay; no engine tree system.

## Graphics direction

The operator requested a more realistic treatment after the initial low-poly pass.
Claude's verified local Prompt3D has a genuine `realistic` finishing preset:
procedural meshes, smoother shading, UV projection and embedded PBR material maps.
Judas's existing glTF importer accepts its colour, normal, roughness and occlusion
maps. Trees and plants were regenerated with recorded prompts/seeds. Static original
props received textured finishes with baked node transforms; the animated rig was
preserved. Source material scans/receipts/licences are retained under ArtSource.

The homestead now has modest natural undulations, painted paths, a higher-resolution
colour atlas and independently tiled ground normal/roughness detail. Paint still
feeds the native base-colour map. The source generator's potted fern was adapted
into rooted wild foliage, with separate visible berries.

This is **more natural, textured procedural art**, not photorealistic reconstruction.
Tree crowns remain clustered simplified geometry; the animated player and several
prop silhouettes remain visibly stylized. Realistic lighting cannot manufacture
missing detailed meshes. No source-only claim of AAA/photorealistic visuals is made.
The original low-poly results remain in Review/lowpoly-baseline and Review/native.

## Architecture and public APIs

All game rules are project JS. No engine C++, API, SDK, physics, research, historical
evidence or unrelated project was modified.

- `world.entity`, entity `transform`/`presentedTransform`, render visibility,
  `world.setView`/`clearView`: authoritative interaction versus interpolated cameras.
- `physics.raycast` / `sphereCast`, ignored shooter filter: tools, interactions, boom.
- CharacterMotor state/support/up/gravity/velocity/`accelerate`: movement and swimming.
- `world.fluidSample`: prescribed basic-water immersion, flow and underwater checks.
- `entity.setMotionType`, `applyImpulseAtPoint`, velocity and `destroy`: real felling,
  dropped props, depletion. No teleport-based tree fall.
- `world.spawnPrefab`: ordinary dropped-item prefab; accounting commits on success.
- `input.axis`/`held`/`pressed`, pointer capture: project-owned logical controls.
- `ui.get`, document modal state, buttons/lists/slider, native UI events: menus/HUD.
- `saves.refresh`/`exists`/`save`/`load`/`status`: asynchronous full-world persistence.
- `animation.info`/`crossFade`, loop: existing player rig, bounded animation changes.
- Audio readiness and `playAudioOneShot`: original gather/chop/craft/consume effects.

Advanced fluids are explicitly disabled, and the actual runtime flag is checked.
Basic water and swimming remain active. No missing engine capability blocked Stage 1.

## Verified results

Authoritative final-art evidence is `Review/realistic-*` and `Review/realistic/`.
Earlier logs are preserved development evidence, not additional independent coverage.

| Check | Actual result |
| --- | --- |
| Inventory/survival pure tests | 56 checks passed; positive and negative transactions, weights, slots, recipes/stations, transfers, returned containers, damage/air/healing |
| Native game loop | 52 checks, 0 failures |
| Fresh-process save restoration | 18 checks, 0 failures |
| Rendered movement/performance run | 10 checks, 0 failures; 600 measured gameplay frames |
| Script checker | 10 registered modules; 0 compile/link failures |
| Native scene/UI/prefab validators | Pass |
| Release runtime/editor/exporter | Built successfully; no runtime source changes |
| Real editor copy | Duplicate/undo and Play/Stop preserve IDENTICAL authored scene; scene save succeeds; 0 resource/job failures |
| Standalone export | 51 assets, 1 scene, about 120 MB; material/font notices included |
| Relocated native standalone | Launched from unrelated /tmp directory, path contains spaces; loaded the real source-game expedition and captured resumed HUD |

Native assertions exercise actual logical controls, UI actions and physical ray
hits. They prove same-player camera switching, pause isolation, exact resource
credits, recipe debits, physical felling, fallen-timber processing, actual mining,
cooking, refill/drink/returned flask, dynamic drop and single recovery, basic-water
swimming, underwater air consumption and hunger progression. The cold reader checks
exact inventory/depletion, actual trunk position AND orientation, dynamic status
and absence of depleted bodies. It does not substitute a cached gameplay flag for
physical restoration.

The observer's only fixture controls place/aim the player using actual ground
queries. It never grants gathered inventory or changes a tree/deposit to manufacture
success. Pure capacity/station negative checks are distinguished from human checks
for every native full-bag/terrain arrangement. Audio uses a dummy backend during
automation; sound quality requires listening.

No ceremonial full production suite, Windows, Android, hardware gamepad or broad
Linux clean-machine qualification was run for this ordinary project-only change.
The executable uses the development host's accepted Release engine; consult its
runtime requirements before distributing to another Linux installation.

## Measurements

Ryzen 5 5600, GTX 970 4 GB, Linux 7.0.0-38, GNU 15.2 Release/OpenGL 3.3, controlled
1/60-second simulation clock, VSync off, hidden but genuinely rendered window,
profiler enabled, dummy audio. Full measured values and raw profile/CSV are retained
in Review/realistic. These are frame costs, not a claim of observed monitor FPS.

Final realistic-art run: median frame 2.46 ms, mean 2.76 ms, p95 4.94 ms, worst
7.81 ms. Physics mean 0.0133 ms, p95 0.0173 ms, worst 0.0391 ms. Script inclusive
phase mean 0.323 ms over the retained 120-frame profiler window (nested scopes not
added twice). Process resident memory approximately 393 MB; editor resource-resident
estimate approximately 358 MB, a different overlapping quantity, not additive RAM.
231 initial bodies. The workload is ordinary traversal, not a dense dynamic-tree
pile stress certification. No before/after speedup is claimed: art AND terrain
composition changed between the low-poly baseline and this direction.

## Preserved failures and corrections

- Source rock filename mismatch; corrected to the existing rock_cluster asset.
- Invalid initial project/scene/UI/prefab authoring fields; corrected to native
  schemas and validated. Initial basic-water position used a volume centre rather
  than the actual surface-origin contract; corrected, then verified swimming.
- Test input kind error and an observer linked to stale application objects;
  corrected the test setup to the freshly built runtime target. Not engine fixes.
- Test attempted to parse JS text as JSON after formatting; now consumes the
  explicit Authoring/world_manifest.json fixture instead.
- Continuous locomotion could accumulate interrupted animation contributors.
  Game now finishes its short blend and ignores insignificant vertical support
  noise before selecting an airborne pose. Long third-person run then passed.
- Automated Load initially preceded asynchronous save discovery. The game reported
  service busy; the corrected export check explicitly waits for discovery and load.
- Static material converter first rejected pre-existing transformed nodes safely;
  implemented hierarchical transform baking rather than discarding them or rigs.
- On rolling ground, the old processing fixture could place the player inside the
  fallen log. Ground queries and side-of-trunk placement corrected the fixture;
  actual ray/tool processing now passes. No altered collision, resource threshold,
  physics cadence or capacity rule was used to hide that failure.

## Limits and review boundary

The 11.2 × 6.8 km coast bounding dimensions are provisional; 64 km² actual land is
not established. Coarse terrain samples are unsuitable for detailed far-region
play yet. The populated 640 m homestead area, trail and spring are the Stage 1 game.
The rest is terrain shell, not authored settlements. Three tiles currently remain
resident; this stage does not claim regional streaming validation.

No house interiors, construction/barricades/furniture, combat/NPCs/quests, vehicles,
excavation or full-island population: these belong to later separately approved
stages. No synthetic voices. Unity SFX Designer and its VM installation were deferred
by the operator pending a native Linux port; existing original PCM effects are used.
No ambient soundtrack, bespoke tool-body animations or realistic replacement hero
rig are claimed. Existing Idle/Walk/Run/Jump and simple first-person tool swing are
foundation presentation, not finished character animation.

Save identity/version is explicit; future content changes need a deliberate save
compatibility/migration decision. Recovery retains the pack. No resource respawn.
Container transfer is a pure reusable helper; storage/UI/vehicles wait for Stage 2+.
Input mapping is project-owned, but this stage has no in-game key rebinding screen.

Read README.md and HUMAN_CHECKLIST.md. Stop here for Stage 1 human review.
