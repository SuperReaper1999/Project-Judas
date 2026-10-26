# Lunar Island — developer handover

Copyright (c) 2026 Conner / SuperReaper1999. See repository MIT terms and asset provenance.

## Open, run and preserve your work

Open `lunar_island.judasproj` in the existing Judas editor. Play runs ordinary JS;
Stop restores authored content. Save your edited scenes deliberately. Use F6/F9
inside the game for expeditions, not editor changes. Keep content and matching save
fixtures together when changing compatibility-sensitive assets.

`README.md` contains controls, data schemas and per-system extension instructions.
`PLAYER_GUIDE.md` is suitable for distribution. `Review/STAGE6_REPORT.md` separates
measured results, operator acceptance and remaining work. Earlier Review directories
are immutable development history, not current operating instructions.

## Where to work

| Change | Source of truth / implementation |
| --- | --- |
| Item, stack, weight, use | Authoring/items.json; generated definitions.js; inventory.js |
| Crafting costs/stations | Authoring/recipes.json; definitions.js; Player.craft |
| Survival balance | Authoring/survival.json; survival.js and movement.js |
| Weapons, ammunition, quests, people | Authoring/encounters.json and encounter_manifest.json; encounter-data.js, combat.js, person.js, social-ui.js |
| Construction, furniture, barricades | Authoring/construction.json; housing-data.js; housing.js, construction.js |
| Vehicle capacities/stock/flight policy | Authoring/vehicles.json; generated vehicle-data.js and vehicle_manifest.json; vehicle.js, transport.js |
| New district/buildings/resources | Authoring/regions.json; tools/author_stage5.py; region_manifest.json and region-data.js |
| Roads and intersections | Authoring/routes.json; tools/author_stage5.py; native cooked road geometry |
| Terrain | Authoring/*.judasterrain; native terrain editor/cooker; Scenes/*.judas |
| HUD/input/cameras | Assets/ui/game.judasui; hud.js; project input map; camera.js |
| Art and generation receipts | ArtSource/; asset_catalog.json; PROVENANCE.md |

**Generated files are committed runtime content.** Authoring scripts write real
scene/asset files; they are not non-destructive editor layers. Commit or back up
manual edits before invoking them. Read a tool's scope first. Stage 1/2/3 generators
may rebuild earlier content; do not run every historical authoring tool to change
one item. `author_stage4.py` regenerates vehicle content; Stage 5 adds regional
population afterward. Check the resulting diff and rerun affected application cases.
Maintain generated data and its authoring JSON together.

For a model replacement, preserve its `.judasmeta` identity, units, local pivot and
collision convention. Visual GLB replacement does not change collision automatically.
Use ordinary Judas collision assets/components. Record source, creator, licence and
redistribution rights. Claude's installed Prompt3D project is an authoring dependency,
not a runtime dependency. Stage 5 records the actual procedural/trained-parser tool
fingerprints; no unsupported neural mesh generation capability is implied.

## Ownership you must preserve

- Personal progression and constructed root objects live with Player.
- Each regional RegionContent script owns its own housing/harvest JSON. The transient
  registries and regional housing view are not saved. Do not collapse all district
  records into the player's bounded script-state blob.
- Stable native save references identify objects across region unload/reconstruction.
  New regional constructs are adopted into their supporting region; claimed homes
  keep their region resident until the claim is replaced or the bed dismantled.
- First boarding adopts a regional vehicle assembly to root. Driven vehicles remain
  resident. Do not remove their owner during streaming or pretend they are suspended.
- Judas owns bodies, motors, powertrains, constraints, collision and integration.
  Scripts apply forces and decide gameplay. No body teleports implement driving.
- Body-free vehicle meshes inherit native interpolation. Do not write the parent's
  `presentedTransform` back into a child world transform: it is already inherited.
  Cosmetic rotor rotation is authored through the authoritative parent frame.
- UI pauses simulation; requests can continue while paused. Keep save/load failures
  visible and preserve input blocking through modal transitions.

## Validate and export

Run pure rules with Node (`tests/inventory.test.js`, `encounters.test.js`,
`vehicles.test.js`). Use the native `judas_script_check` on this project.
`tests/build_observer.py` links the consumer observer to an existing matching Release
engine build; it does not build a new engine. `Review/stage6/REPRODUCE.sh` records the
focused final scenarios, including true fresh-process reads. It needs a working Linux
GL desktop and an isolated proof/user-data directory. Do not change runtime source
between a writer and reader. Test binaries and application fixtures are not shipped
or used by ordinary gameplay.

`python3 tools/package_game.py --output /absolute/new/package/path` uses the existing
Release `judas_export` and runtime, then adds the player guide and a working-directory
safe launcher. Specify `--build` for a different validated build tree. The output
must not already exist. It is a desktop Linux package, not Android/Windows qualification.
The exporter retains engine provenance, licences, resource metadata and requirements.
Move the entire package, not just its executable. Final-source/binary/package hashes
in Review/stage6 identify the delivered candidate.

## Honest next-content boundaries

The six reviewed development stages establish a maintainable playable foundation,
not a completed commercial RPG matching every long-term vision item. Current limits:

- Authored quarters and 17 roadside patches, not dense content over a verified 64 km².
  Resident coarse island terrain; no runtime terrain excavation or water flow here.
- Simple human/wolf detection/pursuit/combat, trade and delivery/patrol quests;
  no deep survivor schedules, systemic faction economy or dynamic raid simulation.
- Building/furniture/door/barrier proxy collisions; basic impact damage. No universal
  destructible architecture or full articulated character death in this project.
- Weapon animation and animal animation gaps remain in ArtSource/STAGE3_ANIMATION_HANDOFF.md.
  Human corpses are physical proxies; the wolf mesh needs a real rig and clips.
- One coat/body equipment path and supported tools/weapons/consumables, not a complete
  wardrobe/armour simulation. No farming calendar or medical injury anatomy.
- Assisted aircraft/bike and simplified hull flotation; no commercial flight model,
  cargo-induced inertia, automatic parking suspension, traffic or AI pilots.
- Manual single-slot expedition saves, content-strict compatibility and separate stage
  identities. Design actual game-update migration policy before distributing updates.
- Linux automation does not substitute for operator gameplay, audio listening,
  controller, Windows/Android or clean-machine acceptance.

Prioritize human play reports and the existing animation handover before adding more
systems. Keep game rules here; report generic engine defects separately.
