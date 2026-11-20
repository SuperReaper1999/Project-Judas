# Beacon Grove

A short, complete native 2D game built from Judas's fresh top-down starter.
Recover three lantern cells, restore the east beacon, and escape its locked gate.
Avoid the west brambles and the moving grove wisp. Three hits mean defeat; retry
as often as you like. A supply crate is an ordinary pushable physical object.

## Play

- WASD or arrows: move.
- E: play the restore animation beside the beacon, after collecting all cells.
- K / L (desktop also F6 / F7): save / load the current run, including collected cells and gate state.
- R: restart with full health and all cells restored.
- Escape: pause/resume through the normal runtime.

Open `Beacon_Grove.judasproj` in the editor and press Play. For direct launch,
run these commands from the directory containing `Beacon_Grove.judasproj`. Set
`JUDAS_TOOLS` to the absolute directory containing your SDK binaries or matching
source-build binaries:

```sh
JUDAS_TOOLS=/absolute/path/to/judas-sdk
"$JUDAS_TOOLS/judas" ./Beacon_Grove.judasproj
```

For a source build, configure a Release build and build the `judas` and
`judas_export` targets before setting `JUDAS_TOOLS` to its binary directory.
SDK users can run the supplied project directly with the SDK tools.

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

The art comes from Judas starter and workshop assets. Its generation sources
are under `tools/demos/` in the source repository.

## Export

From the project directory with the same `JUDAS_TOOLS`, choose a fresh package
path outside the project:

```sh
"$JUDAS_TOOLS/judas_export" ./Beacon_Grove.judasproj \
  /absolute/path/to/BeaconGrovePackage "$JUDAS_TOOLS/judas"
```

Move the entire package, then run its `judas` executable. Saves use the configured
writable user-data location.

## Source regeneration and automated checks

With a source checkout and a configured build, run these commands from the
repository root. The generator creates a project from starter assets and refuses
an existing destination. The supplied project is independently editable.

```sh
cmake --build build --target judas_beacon_grove judas_beacon_grove_application_tests -j2
./build/judas_beacon_grove "$PWD" /absolute/path/to/fresh-beacon-grove
python3 scripts/prealpha_validate.py --build-dir build \
  --test beacon-grove-application --test beacon-grove-loss
```

The registered cases exercise the win and loss/restart flows in an ordinary
Linux application. The runner needs a graphics-capable session. The source
generator and native test runner are source-checkout workflows. See
the [2D quick start](https://github.com/SuperReaper1999/Project-Judas/blob/Experimental/docs/2D_QUICKSTART.md)
for authoring guidance.

This is a small complete game for workflow qualification, not a large adventure:
one level, no combat system or audio track, static tilemaps and bounded cell count.
