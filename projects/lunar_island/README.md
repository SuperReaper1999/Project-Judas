# Lunar Island — Stage 2: A Place to Call Home

Copyright (c) 2026 Conner / SuperReaper1999. Original project code/assets use the
repository's MIT terms; cached ambientCG material scans use CC0-1.0. See `ArtSource/PROVENANCE.md` for font and asset origins.

An ordinary Judas consumer game on Experimental. **Stage 2; human gameplay acceptance pending.** Stage 2 was authorized after the operator reviewed Stage 1 screenshots. Gameplay lives in `Assets/scripts`. Two bounded native persistence corrections are documented in the Stage 2 report.

## Play

From the Judas repository:

```sh
./build/judas projects/lunar_island/lunar_island.judasproj
./build/judas_editor projects/lunar_island/lunar_island.judasproj
```

Open the project file in the editor, then Play. Stop restores the authored scene;
use the game's save/load commands to persist an expedition. Native save slots live
in the OS user-data directory, scoped to the project's save identity. They are
separate from editor scene saves and survive moving an exported package.

| Control | Action |
| --- | --- |
| WASD / mouse | Walk / look |
| Shift / Space | Sprint / jump; Space swims upward |
| Left Ctrl | Dive while swimming |
| V | Switch first/third person, same character |
| E | Gather, recover bundles, refill at well; interact with doors, openings and furnishings |
| Left click | Use tool/consumable; confirm a placement preview |
| B / R | Build catalogue / rotate placement by 90 degrees |
| 1–6 | Hands, axe, pickaxe, berries, flask, dressing |
| Tab | Backpack; Crafting and other tabs across the top |
| M / Escape | Map / pause or return |
| F6 / F9 | Save / load last expedition |

Menu buttons support mouse and keyboard focus. Menus pause simulation and release
the pointer. The Controls tab includes mouse sensitivity. Logical actions are
project-owned in `lunar_island.judasproj`; this stage has no in-game rebind screen.

## A first expedition

1. Begin your island. Gather the sticks, stones and green plants near camp with E.
2. Open Tab → Crafting. Make an axe (3 wood, 2 stone, 2 fibre); equip with 2.
3. Three axe hits release a real static trunk into dynamic physics. Let it fall.
   Three further hits process that trunk into 12 wood; the pack must have room.
4. Craft a pickaxe (4 wood, 3 stone, 2 fibre). Equip with 3 and mine deposits.
5. Gather berries. Beside the fire, craft berry porridge (5 berries and 1 wood).
   Select it in Backpack and Use / equip. Berries and initial rations also feed you.
6. E at the well fills your flask. 5 then click drinks it and returns an empty flask.
7. Explore the nearby spring pool; swim up with Space, dive with Ctrl. Air is finite.
8. Save with F6. Quit, start a fresh process, and F9 to resume the expedition.

The introductory journal guides this loop. Food, water, stamina, health, drowning,
fall injuries and dressings are active. Death recovery returns to the claimed bed (or camp), keeping the pack.

## World and scope

The supplied map guides the coast silhouette and north-eastern homestead location.
Two editable native terrain tiles cover **11.2 × 6.8 km bounding dimensions**, with
a 640 × 640 m higher-detail homestead tile. This is **not a verified 64 km² land
area**. Distant terrain is coarse (~44–53 m samples); homestead spacing is 5 m with gentle rolling terrain outside camp. The homestead
uses a 1024² colour atlas plus independently tiled normal/roughness detail through
a normal Judas material. Native terrain painting continues to update its base map.
Native paint layers and shoreline/path colours are editable.

The populated area includes the camp, forest, resource trail, spring and two furnished cottages. Town,
city, industrial district, harbour and other labelled reference locations
are **future content**, not implemented destinations. Basic ocean water is visual
and swimmable; no conserved liquid simulation is enabled. The spring well is the
authored fresh-water interaction. Water remains a prescribed surface, not flow.

The full terrain shell is currently resident. This stage does not demonstrate
regional unloading, world-wide resource population, terrain excavation, NPCs, combat or vehicles. Collision for foliage uses
simple trunk/plant/stone proxies; branches and individual leaves are not simulated.
Felled trees, dropped bundles, door leaves, barricades and constructed pieces use native rigid bodies. There is no resource respawn.

## Project ownership and files

- `lunar_island.judasproj`: input, classifications, native settings, save identity.
- `Scenes/island.judas`: editable native scene, ground/water, motor, resources, UI.
- `Authoring/*.judasterrain`: native editable terrain sources and paint weights.
- `Authoring/items.json`, `recipes.json`, `survival.json`: editable game definitions.
- `Assets/scripts/player.js`: lifecycle, equipment, actions and save requests.
- `inventory.js`: atomic stack/weight/slot transactions, crafting and transfers.
- `survival.js`, `movement.js`: survival rules and CharacterMotor/swimming control.
- `harvesting.js`: public queries/impulses and resource depletion.
- `camera.js`: authoritative interaction aim, interpolated presentation, boom query.
- `hud.js`, `Assets/ui/game.judasui`: native modal/list/button/slider UI.
- `world-data.js`: generated entity/resource metadata, no hidden native dispatch.
- `Assets/models`, `textures`, `audio`, `fonts`, `prefabs`: self-contained runtime art.
- `ArtSource`: supplied map, Prompt3D settings, receipts, provenance.
- `tools`, `tests`, `Review`: authoring, reproducible checks and actual evidence.

### Add items and recipes

Edit `Authoring/items.json`: each key has a label, category, positive stack size and
non-negative weight. `use` modifies supported survival values; `returns` names a
container returned on consumption. `slot` equips an item, `tool` identifies axe/pick.
Add a recipe with unique ID, existing ingredient/output IDs and positive integer
quantities. Stations support `hand`, `campfire` and `workbench`.

```sh
python3 projects/lunar_island/tools/author_project.py --data-only
node projects/lunar_island/tests/inventory.test.js
./build/judas_script_check projects/lunar_island/lunar_island.judasproj
```

This changes definitions without compiling C++. To add a **new tool behaviour**,
implement it in project JS; an item definition alone cannot invent a mechanic.
The same inventory transactions serve the player, authored storage and constructed storage. Transfers enforce both containers' capacities and remove equipment use when an equipped item leaves the pack.

### Edit the world

Use the editor for normal scene/terrain authoring and keep your saved edits.
`tools/author_project.py --replace-world` intentionally rebuilds the initial world
and **overwrites the generated scene and terrain sources**. It is an explicit
reset tool, not a normal save action. Back up edited scenes before using it.
`--data-only` is safe for item/recipe/config iteration.

Resource authoring currently uses `resource(...)` calls in that tool, which emit
normal entities and `world-data.js` records. Tree visuals are children of collision
trunks; scripts identify the trunk's public entity ID. Maintain unique IDs and
matching world-data records when hand-authoring new harvestables. Generic scene
objects without those records do not become harvestable automatically.

### Regenerate models

```sh
PYTHONDONTWRITEBYTECODE=1 /path/to/Prompt3D/.venv/bin/python \
  projects/lunar_island/tools/generate_assets.py --prompt3d /path/to/Prompt3D --style realistic
```

Verified tool location: `/home/conner/Documents/GitHub/ClaudesNewML-AI-Thing`.
It uses a trained prompt parser and procedural mesh generation; it is not a
free-form neural mesh generator. Trees/fern use real prompt requests; the trail
sign uses explicit mesh lettering. The realistic preset adds PBR textures and
smoother shading. Static reused props receive native glTF material maps and UVs;
the original animated player stays intact. This remains simplified procedural
art, not photorealistic mesh reconstruction. `--style lowpoly` provides the
original alternate finish. Installed CC0 scans are an offline authoring dependency;
exact source arrays, origins and licences are retained in ArtSource/materials. Reused original Hearthworld assets are copied
from that existing Judas project. These are **offline authoring dependencies**;
playing, editing existing art and standalone export do not require Prompt3D.
The external tool is neither changed nor retrained. SFX Designer awaits its
separate Linux port; current short sound effects are original generated PCM.

### Build, validate and export

```sh
cmake --build build --target judas judas_editor judas_export -j1
python3 projects/lunar_island/tests/build_observer.py --build build
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-island-validation /tmp/lunar-island-observer \
  projects/lunar_island/lunar_island.judasproj write projects/lunar_island/Review/stage2/native
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-island-validation /tmp/lunar-island-observer \
  projects/lunar_island/lunar_island.judasproj read projects/lunar_island/Review/stage2/native
./build/judas_export projects/lunar_island/lunar_island.judasproj \
  '/tmp/Lunar Island Stage 2' ./build/judas
```

`write` creates a validation save in the game's `island` slot. Use a separate
`XDG_DATA_HOME` to avoid replacing personal expedition saves. `read` launches a
separate process and verifies actual restored physics as well as JS state. The
observer positions/aims the player as a test fixture; game actions still use actual
input, UI, rays and project scripts. `performance` measures 600 rendered frames.

Run the exported `judas` executable inside the package; no repository path is
required. Linux is the tested platform for this stage. This is not Windows,
Android, gamepad hardware, audio-listening or human gameplay acceptance.

See `Review/STAGE2_REPORT.md` and `Review/stage2/HUMAN_CHECKLIST.md` for the current
review boundary. Stage 1 evidence and fingerprints remain historical and unchanged.
Stage 3 requires separate approval.

Current Linux package (untracked, reproducible):
`.cache/lunar-island-stage2/Lunar Island/`; run `judas` inside it.
Archive: `.cache/lunar-island-stage2/LunarIsland-Stage2-Linux.tar.gz`.
The complete package must travel together.

## Occupy and improve a home

Seabird Cottage is just behind the camp/trail sign; Birchside Cabin is east across
the homestead. E on a door drives its real hinge; E on furniture opens its actions.
The outside packing crate, indoor chest, cupboard and shelf hold useful supplies.
Take/store one item or a stack using the two storage lists. Empty storage before
dismantling it. Moving storage retains its contents and identity.

Use a bed to rest and claim that home. Rest restores health/stamina and costs food
and water; it does not simulate an overnight clock. Use a workbench to open
station-gated crafting. Lamps toggle actual native lights. Chairs/sofas/tables are
placeable furnishings, not character seating/animation systems.

E on a window frame or closed door frame opens barricade options. Wood costs
2 planks + 1 nail; metal costs 1 sheet + 2 nails. Axe/pick hits damage the board;
E exposes repair, reinforcement and dismantling. Reinforcement replaces the actual
board geometry, retaining its opening and integrity record. Furniture damage is
intentionally excluded; storage must not silently lose hidden contents.

B opens 19 building/decorating choices with costs. Choose one, aim at a valid
support, R to rotate, click to confirm, Escape to cancel. The preview reports
obstructions and insufficient resources; costs debit only after native creation.
Foundations require terrain. Walls/doors/fences/gates snap to a floor edge; R chooses
the edge. To fit a roof, aim at a broad wall face. Paintings require a vertical
surface; other furnishings need upward support. This is a modest 90-degree
placement system, not a universal CAD/stacking tool. Hover the desired object and
E → Move to reposition furnishings, with the old object intact until confirmation.

Pieces have real collision, mass and native support joints. Removing a foundation
releases direct attachments; unsupported pieces physically fall, retaining their
remaining connections. This is a game-side fixed-joint construction system, not
an M79 engineering-load solver or arbitrary building integrity certification.
Authored cottages are static shells with real door/window openings and pitched
roofs; they cannot be demolished. They have no AI/navmesh occupants in Stage 2.

### Extend Stage 2

- `Authoring/construction.json`: blueprint names, costs, half extents, health,
  storage slots/weight, category and functional `use` (`bed`, `workbench`, `lamp`).
- `tools/author_stage2.py`: original modules, material assignments, house layout,
  opening sockets, furniture placements/stock, prefabs and build previews.
- `Assets/scripts/housing.js`: storage, fixture actions, openings, integrity and
  stable save-reference ownership. `construction.js`: preview, queries, snapping,
  native spawning and physical attachment. No game rules live in engine C++.

After editing construction definitions, regenerate Stage 2 assets/world:

```sh
PYTHONDONTWRITEBYTECODE=1 /path/to/Prompt3D/.venv/bin/python \
  projects/lunar_island/tools/author_stage2.py --prompt3d /path/to/Prompt3D
```

This deliberately replaces scene entities 3000–5999 and generated housing metadata,
UI and prefabs. Back up hand-edited houses first. `--art-only` regenerates only
models. The original Stage 1 `--replace-world` resets the whole world; run Stage 2
after it if performing a full authored rebuild. Neither generator is a normal
editor save operation. Existing art, gameplay and export require no generator.
New catalogue keys need a corresponding generated model/prefab/preview; a JSON key
alone does not invent a shape. Use existing examples when adding storage or props.

Stage 2 intentionally uses a new project save identity. Stage 1 saves remain
untouched, but are not automatically migrated. Native cold saves preserve moved
fixtures, spawned entities/joints, lamps, ownership, containers, home and integrity
alongside resources/survival. Editor Stop restores the original authored world;
F6/F9 are the persistent game workflow.
