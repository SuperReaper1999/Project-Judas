# Lunar Island — Stage 1: Homestead Coast

Copyright (c) 2026 Conner / SuperReaper1999. Original project code/assets use the
repository's MIT terms; cached ambientCG material scans use CC0-1.0. See `ArtSource/PROVENANCE.md` for font and asset origins.

An ordinary Judas consumer game on Experimental. **Stage 1 only; human acceptance
pending.** No engine source modifications. Gameplay lives in `Assets/scripts`.

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
| E | Gather loose supplies, recover bundles, refill at well |
| Left click | Use equipped tool or consumable |
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
fall injuries and dressings are active. Death recovery returns to camp with the
pack intact; this deliberate Stage 1 rule is forgiving rather than punitive.

## World and scope

The supplied map guides the coast silhouette and north-eastern homestead location.
Two editable native terrain tiles cover **11.2 × 6.8 km bounding dimensions**, with
a 640 × 640 m higher-detail homestead tile. This is **not a verified 64 km² land
area**. Distant terrain is coarse (~44–53 m samples); homestead spacing is 5 m with gentle rolling terrain outside camp. The homestead
uses a 1024² colour atlas plus independently tiled normal/roughness detail through
a normal Judas material. Native terrain painting continues to update its base map.
Native paint layers and shoreline/path colours are editable.

The populated Stage 1 area is the camp, forest, resource trail and spring. Town,
city, industrial district, harbour, houses and other labelled reference locations
are **future content**, not implemented destinations. Basic ocean water is visual
and swimmable; no conserved liquid simulation is enabled. The spring well is the
authored fresh-water interaction. Water remains a prescribed surface, not flow.

The full three-tile shell is currently resident. This stage does not demonstrate
regional unloading, world-wide resource population, terrain excavation, building,
NPCs, combat, vehicles or other later-stage systems. Collision for foliage uses
simple trunk/plant/stone proxies; branches and individual leaves are not simulated.
Only felled trees and dropped bundles become dynamic. There is no resource respawn.

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
quantities. Stations currently support `hand` and `campfire`.

```sh
python3 projects/lunar_island/tools/author_project.py --data-only
node projects/lunar_island/tests/inventory.test.js
./build/judas_script_check projects/lunar_island/lunar_island.judasproj
```

This changes definitions without compiling C++. To add a **new tool behaviour**,
implement it in project JS; an item definition alone cannot invent a mechanic.
Inventory transfer is reusable, but Stage 2 storage/container UI is not implemented.

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
  projects/lunar_island/lunar_island.judasproj write projects/lunar_island/Review/realistic
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-island-validation /tmp/lunar-island-observer \
  projects/lunar_island/lunar_island.judasproj read projects/lunar_island/Review/realistic
./build/judas_export projects/lunar_island/lunar_island.judasproj \
  '/tmp/Lunar Island Stage 1' ./build/judas
```

`write` creates a validation save in the game's `island` slot. Use a separate
`XDG_DATA_HOME` to avoid replacing personal expedition saves. `read` launches a
separate process and verifies actual restored physics as well as JS state. The
observer positions/aims the player as a test fixture; game actions still use actual
input, UI, rays and project scripts. `performance` measures 600 rendered frames.

Run the exported `judas` executable inside the package; no repository path is
required. Linux is the tested platform for this stage. This is not Windows,
Android, gamepad hardware, audio-listening or human gameplay acceptance.

See `Review/STAGE1_REPORT.md` and `Review/HUMAN_CHECKLIST.md` for measured results,
known limitations and the review boundary. Stage 2 requires separate approval.

Current built Linux package (untracked, reproducible):
`.cache/lunar-island-stage1/Lunar Island Realistic/`.
Run its `judas` executable. The compressed distribution is
`.cache/lunar-island-stage1/LunarIsland-Stage1-Linux.tar.gz`. It includes runtime assets, fonts/licences, engine
provenance and native dependency metadata. The whole directory must travel together.
