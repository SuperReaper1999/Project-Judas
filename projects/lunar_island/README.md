# Lunar Island — Stage 4: Roads, Water and Sky

Copyright (c) 2026 Conner / SuperReaper1999. Original project code/assets use the
repository's MIT terms; cached ambientCG material scans use CC0-1.0. See `ArtSource/PROVENANCE.md` for font and asset origins.

An ordinary Judas consumer game on Experimental. **Stage 4; human gameplay acceptance pending.** The operator reviewed Stages 1–3 and authorized this vehicle stage. Gameplay lives in `Assets/scripts`. Two bounded native persistence corrections are documented in the Stage 2 report.

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
| E | Gather, refill, interact with homes; speak to survivors and loot bodies |
| Left click / right mouse | Use tool, melee or fire / aim a firearm |
| B / R | Build catalogue / rotate an active placement, otherwise reload a firearm |
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
world-wide resource population or terrain excavation. Stage 4 adds six transport types and one small regional garage, not populated towns or a complete streamed island. Stage 3 adds six nearby NPCs, combat and quests. Collision for foliage uses
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
  projects/lunar_island/lunar_island.judasproj write /tmp/lunar-island-build-proof
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-island-validation /tmp/lunar-island-observer \
  projects/lunar_island/lunar_island.judasproj read /tmp/lunar-island-build-proof
./build/judas_export projects/lunar_island/lunar_island.judasproj \
  '/tmp/Lunar Island Stage 4' ./build/judas
```

`write` creates a validation save in the game's `island` slot. Use a separate
`XDG_DATA_HOME` to avoid replacing personal expedition saves. `read` launches a
separate process and verifies actual restored physics as well as JS state. The
observer positions/aims the player as a test fixture; game actions still use actual
input, UI, rays and project scripts. `performance` measures 600 rendered frames.

Run the exported `judas` executable inside the package; no repository path is
required. Linux is the tested platform for this stage. This is not Windows,
Android, gamepad hardware, audio-listening or human gameplay acceptance.

See `Review/STAGE4_REPORT.md` and `Review/stage4/HUMAN_CHECKLIST.md` for the current
review boundary. Earlier stage evidence and fingerprints remain historical and unchanged.
Stage 4 requires separate approval.

Current Linux package (untracked, reproducible):
`.cache/lunar-island-stage4/Lunar Island/`; run `judas` inside it.
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

Stage 2 introduced a separate project save identity (superseded by the separate Stage 3 identity below). Stage 1 saves remain
untouched, but are not automatically migrated. Native cold saves preserve moved
fixtures, spawned entities/joints, lamps, ownership, containers, home and integrity
alongside resources/survival. Editor Stop restores the original authored world;
F6/F9 are the persistent game workflow.


## Stage 3: People of the Fracture

Mara patrols near the camp at (8,18), Ivo near (-8,16). E opens dialogue;
**Ask about work** starts or turns in a job, **Trade supplies** opens finite stock.
The field map/journal includes the objectives. Mara gives a pistol and 24 rounds
once, then rewards clearing the two scavengers at the eastern camp (about 75,66).
Ivo exchanges six actual timber for 15 tokens and two rations. Talk again to turn
in completed work; rewards and gifts cannot be repeated. Tokens can also be earned
by selling materials. Trader money, stock and pack capacity are real constraints.

Open Tab → Backpack, select a weapon and **Use / equip**. R loads compatible
reserve ammunition into its finite magazine; a reload takes time. Right mouse
narrows the field of view, left mouse fires. The rifle supports held fire;
pistol/shotgun require separate clicks. Reloading and firing are blocked by menus.
Knife/bat/axe/pickaxe use the same physical query against enemies. Friendly survivors
are protected from damage. Guns recoil and have original sound/impact feedback.

Two scavengers patrol the eastern camp: a ranged lookout and a melee raider. They
use sight/occlusion and hearing, pursue using navigation guidance plus CharacterMotor,
attack with a windup, and retreat at low health. Wolves patrol the western trail
around (-63,8) and (-75,22). Give them space until equipped. Enemies can strike
actual nearby barricades when alerted. This is bounded NPC policy, not a universal
house-breaching or squad-tactics system. Runtime building changes physically block
motors/rays, but do not trigger automatic navmesh rebaking.

E on a defeated enemy opens its finite loot container. Ammo expended by the lookout
is removed from his loot. Meat can be cooked at the fire; hide, scrap, ammunition
and currency share the ordinary inventory. Corpses are ordinary dynamic rigid
proxies with collision and impulses, **not articulated ragdolls**. Human locomotion
uses the original rig plus a small authored attack clip. Wolf leg animation,
weapon-specific aim/reload/hit/death clips, and third-person held-weapon animation
remain art/polish work. There is no voice dialogue yet.

### Author encounters, weapons and quests

`Authoring/encounters.json` owns weapon values, actor profiles/routes/stock/prices,
dialogue and quest definitions. `person.js` owns NPC decisions; `combat.js` owns
weapons; `social-ui.js` renders dialogue/trade/loot; `encounter-rules.js` implements
atomic transactions. No game-specific native code or hidden dispatch is used.

Actor IDs occupy 6000–6399 in steps of ten. The authoring tool creates the motor,
visual child and optional held weapon. Choose friendly/hostile/animal, health,
speed, sight, damage, route points and finite stock. Add a quest with an item-delivery
objective or a supported `kills`/`wolves` counter, rewards and dialogue; assign
its key to a friendly actor. Weapon profiles reference existing inventory items,
with damage/range/interval, optional ammo/capacity/reload/recoil and six-pellet
shotgun spread. Adding an entirely new combat or objective type requires JS work.
There is one magazine per weapon type, not per individual item instance.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 projects/lunar_island/tools/author_stage3.py
./build/judas_navigation_bake projects/lunar_island/lunar_island.judasproj Scenes/island.judas
```

This replaces IDs 6000–6999, encounter metadata, corpse prefabs and original PCM
effects. Back up scene edits first. It does not require Prompt3D unless regenerating
art with `--prompt3d /path/to/tool`; raw wolf settings and copied source receipts
are retained. For a whole authored rebuild run Stage 1, Stage 2, Stage 3, Stage 4, then bake.
Existing art/gameplay/export do not require those offline tools.

The homestead navigation surface is bounded to 220 × 220 m. It uses the detailed
homestead collision, excluding the duplicate coarse far-island shell with a source
layer filter (physical collision masks stay unchanged). Startup shows a readiness
screen until the baked navigation and encounter models are available; it does not
spawn an active expedition before required services publish.

Stage 3 uses its own save identity; Stage 1/2 saves are preserved but not migrated.
F6/F9 retain health, NPC stock/death, loot, magazines and quests alongside existing
harvesting/housing state. Changing runtime content can require explicit save
migration; development saves are not silently forced into changed scripts.

```sh
node projects/lunar_island/tests/inventory.test.js
node projects/lunar_island/tests/encounters.test.js
python3 projects/lunar_island/tests/build_observer.py --output /tmp/lunar-stage3-observer
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-stage3-check /tmp/lunar-stage3-observer \
  projects/lunar_island/lunar_island.judasproj encounter /tmp/lunar-stage3-proof
SDL_AUDIODRIVER=dummy XDG_DATA_HOME=/tmp/lunar-stage3-check /tmp/lunar-stage3-observer \
  projects/lunar_island/lunar_island.judasproj encounterread /tmp/lunar-stage3-proof
```

Use a separate user-data directory and unchanged source for a write/read pair.
`tests/make_barrier_fixture.py /tmp/NewBarrierFixture` authors an isolated guard
outside a window; observer mode `barrier` verifies unalerted/alerted behaviour,
real supply acquisition, barricading and enemy destruction. It does not alter
production gameplay modules or inject damage/items.

Stage 4 transport is described below. Stage 5 regional population has **not begun**.

Animation priorities and a practical Blender handover are in
`ArtSource/STAGE3_ANIMATION_HANDOFF.md`.


## Stage 4: Roads, Water and Sky

From camp, walk south to the blue hatchback, green pickup and motorcycle.
The supply locker sits beside them. The yellow helicopter is west of this apron;
the plane is farther south on the short runway. The skiff is at the spring dock
north-east of camp. M includes directions. The extra garage pickup east of the
apron is region-owned until first driven.

**E on a vehicle opens one menu:** Drive/Pilot, Cargo, Refuel, Repair. Cargo uses
the same inventory rules as houses, loot and the personal backpack. Select a row
and use Move one / Move stack; weight or slot failures preserve both inventories.
The locker starts with fuel cans and repair kits; Mara also sells finite stock.
A repair kit can be crafted at a workbench from three scrap and one plank.
A sealed can adds 5 L, so leave enough tank space. A kit restores up to 35 condition.

| While driving | Control |
| --- | --- |
| Car, pickup, motorcycle | W/S forward/reverse, A/D steer, Space brake |
| Skiff | W/S propulsion/reverse, A/D rudder, Space water brake |
| Helicopter | W/S tilt, A/D yaw, Space climb, Ctrl descend; release for assisted hover |
| Plane | W/S increase/decrease throttle, A/D turn, Space pull up, Ctrl pitch down, Shift wheel brake |
| All | Mouse orbit/look, V chase/bonnet view, E safe exit, Tab cargo when stopped |

The same player is retained. Exit requires low speed and clear space, and aircraft
need ground within reach. Stop the plane throttle with S, descend gently and brake
on landing. Helicopter Ctrl commands descent; release near the ground or keep Ctrl
to settle. Boats can be left into water; swimming, air and drowning still apply.
Esc pauses, F6 saves, F9 loads. Save only replaces this stage's own expedition slot.

Native wheels, suspension and powertrains move ground vehicles. Helicopter/plane
lift and boat buoyancy are ordinary project-script forces driven by actual shaft
speed and sampled basic water. No vehicle pose or velocity is scripted to fake
travel. Flight and bike balance are deliberately assisted game models. Vehicles
consume fuel; impact/firearm damage reduces condition and propulsion. Repairs
restore condition, not visibly deformed body panels. Vehicle cargo does not change
chassis mass/inertia; capacities are gameplay inventory limits. Collision uses
simple physical compound proxies. Rotor blades are visual, not separate colliders.

### Persistence and regional ownership

Fuel, condition, cargo and driving lease use normal JudasJS save hooks; native
wheel/powertrain/body state uses the existing engine save service. Stage 4 has a
new content/save identity. Stages 1–3 saves remain untouched, with no claimed
migration. Complete write/read pairs must use identical runtime content.

The small `transport_garage` region unloads and reconstructs its undriven pickup,
including depleted cargo and mechanics. On entry, `scenes.adopt` moves the entire
assembly to persistent root ownership, retaining its stable regional reference.
This prevents the driver's vehicle disappearing at its original garage boundary.
Driven vehicles remain root-resident; automatic re-suspension/traffic is not claimed.
The six initial vehicles and terrain shell are resident. Regional physical publication
can precede asynchronous script binding; gameplay only exposes registered, restored
vehicle scripts. Do not interpret `active` alone as an instantiated script state.

### Add or change a vehicle

Edit `Authoring/vehicles.json`: stable key/entity range, kind, position, mass,
half-extents, fuel/tank, cargo slots/weight and initial cargo. Reserve ten IDs per
assembly and avoid IDs in other authoring manifests. `tools/author_stage4.py`
builds the native body, wheels, power graph, visual children, scripts and ordinary
prefab. The game binding resolves remapped children by project tags and component
presence; it does not assume original prefab IDs survive instantiation.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 projects/lunar_island/tools/author_stage4.py
./build/judas_navigation_bake projects/lunar_island/lunar_island.judasproj Scenes/island.judas
```

Authoring overwrites Stage 4's reserved scene objects, prefab outputs and generated
`vehicle-data.js`; keep manual variants outside that range or update the author.
To regenerate source art, add `--prompt3d /path/to/ClaudesNewML-AI-Thing` and run
through that tool's Python environment. This offline dependency is unnecessary
for editing, playing or exporting the included assets. See provenance for the
three supported generator types and project-authored motorcycle/helicopter.

`vehicle.js` owns each durable cargo/condition/fuel state and physical actuation.
`transport.js` owns control transfer, camera, service transactions and safe exit.
`vehicle-rules.js` contains pure supply rules; `vehicle-ui.js` uses the existing HUD.
`vehicle-session.js` is a transient registry, rebuilt through normal lifecycle hooks.
Vehicle inventories reuse `inventory.js`, not a second storage architecture.

Run `Review/stage4/REPRODUCE.sh` with fresh output/user-data paths for connected
cargo/build gameplay, cold-process continuation, real region unload/reload,
all six physical drive checks, aircraft landings, Stage 3 regression and profiling.
The native observer only positions the player to reach authored fixtures; it does
not inject vehicle movement, fuel, damage, loot or cargo.
