# Beacon Grove

A short, complete native 2D game built from Judas's fresh top-down starter.
Recover three lantern cells, restore the east beacon, and escape its locked gate.
Avoid the west brambles and the moving grove wisp. Three hits mean defeat; retry
as often as you like. A supply crate is an ordinary pushable physical object.

## Play

- WASD or arrows: move.
- E: play the restore animation beside the beacon, after collecting all cells.
- F6 / F7: save / load the current run, including collected cells and gate state.
- R: restart with full health and all cells restored.
- Escape: pause/resume through the normal runtime.

Open `Beacon_Grove.judasproj` in the editor and press Play. To run directly:

```sh
.cache/2d-stage1-build/judas projects/beacon_grove/Beacon_Grove.judasproj
```

The three cells and east beacon lie along the central path. You must complete
the restore animation near the beacon; merely pressing E elsewhere does nothing.
Walk east through the opened gate to finish. The bramble patch to the west is
intentionally dangerous; staying in it causes repeated damage, not a single hit.

## Edit and extend

- `Scenes/main.judas`: native sprite, planar physics and editable tilemap entities.
  Select the Grove grid, then View → 2D authoring → Tilemap painter. Turn painting
  off to select objects normally. Stop Play before saving authored changes.
- `Assets/grove.judassprite`: 32×32 tile palette. Edit/slice it in the sheet editor;
  deliberately rebuild the tilemap after changing palette frames.
- `Assets/traveller.judassprite`: idle/run/interaction clips and the `strike` marker.
  Change timings in the frame-animation editor. The marker triggers the gate action.
- `Assets/game.js`: health, objectives, triggers, UI and save/restart policy.
- `Assets/motion.js`: reusable starter movement/camera policy; native MK2 owns
  actual planar motion and collision.
- `Assets/patrol.js`: ordinary kinematic wisp movement, with its phase in saved state.

Tags are configured in Project Settings, not invented implicitly by a script.
The player script's `gate` entity property is selected through the Inspector.
Don't query a wrapper after `destroy()`; `entity.valid` allows deliberate stale
reference handling. Script errors show their actual source locations.

To add a collectible, duplicate a Lantern cell entity and keep its tag/sensor.
This small game's required cell count is three in `game.js`; update that rule and
UI together when expanding the objective. To add hazards, use the danger tag on
a sensor. These are ordinary project rules, not hardcoded engine behaviours.

Art is original Judas starter/workshop art, reused locally with its generation
sources preserved under `tools/demos/`. No downloaded/licensed third-party art.
A source-reproduction helper is `judas_beacon_grove <repo-root> <fresh-destination>`;
it refuses existing directories and uses the same starter/asset serializers as
the editor. It doesn't clone an existing game. This delivered project is editable
independently; rerunning the helper is not part of normal editing.

## Export

```sh
.cache/2d-stage1-build/judas_export projects/beacon_grove/Beacon_Grove.judasproj /tmp/beacon-package .cache/2d-stage1-build/judas
```

Move the entire package, then run its `judas` executable. Modern saves use its
configured writable user-data location. Linux automated qualification is recorded
in `docs/2D_GAME_WORKFLOW.md`; human, Windows and Android checks remain separate.

This is a small complete game for workflow qualification, not a large adventure:
one level, no combat system or audio track, static tilemaps and bounded cell count.
