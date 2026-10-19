# Hearthworld — a little planet worth coming home to

An ordinary Project Judas survival sandbox. All game rules are project JavaScript.
The world uses radial gravity, a 120 m radius planet, enterable cottages, forest,
stone deposits, a starter home and a bounded excavatable backyard.

## Play

Open `hearthworld.judasproj` in the editor and press Play, or launch the exported
`judas` executable. Click **Start exploring** to begin, or **Continue home** when an existing save is found.
The explicit **Load game** button and F9 also restore your saved world.
Collect the three tools from the rack inside your starting cottage.

| Control | Action |
|---|---|
| WASD / mouse | Move / look |
| Space / Shift | Jump / sprint |
| E | Collect the aimed tool or log |
| 1 | Gravity gun |
| 2 / 3 / 4 | Axe / pickaxe / shovel, after collecting them |
| 5 | Construction hammer |
| Left click | Use tool / place selected piece / launch held object |
| Right click | Gravity grab or release; dismantle your aimed construction piece |
| Q / R | Next construction recipe / quarter-turn blueprint or held object |
| V | First / third person, same player |
| Tab / Escape | Pack and recipes / pause menu |
| F5 / F9 | Save / load the last complete home slot |

The welcome and pause menus release the pointer. Quit saves before closing; a
failed save remains visible and does not quit. Saved slots use the normal Judas
per-user game data directory, independent of the exported package location.

## Your first homestead

1. Enter the teal-roof cottage and collect axe, pickaxe and shovel with E.
2. Axe a tree three times. Stand clear while its finite-mass trunk actually falls.
   Once it leans far enough, chop twice more to process it into three physical logs.
   Collect each log for 3 wood, or move it with the gravity gun.
3. Mine a rock with the pickaxe: 1 stone per hit, 6 stone per deposit.
4. Equip the hammer. Q cycles floor, wall, fence, pillar and stone foundation;
   the HUD and pack show costs. Aim at clear ground or the top of an existing floor.
   R rotates; left click places. Right click dismantles your own piece and returns
   its materials. Removing a supporting piece can leave connected pieces falling.
5. Behind the cottage, use the shovel to dig the marked garden into a dry pool.
   Hold left click to keep excavating while committed collision updates publish.
6. Save, quit, relaunch and load. Inventory, harvested objects, construction,
   current physical bodies and the actual edited density field are saved together.

## Physical mechanisms

- CharacterMotor handles movement and gravity; project JS supplies movement intent.
- The gravity gun is a bounded force/torque controller on ordinary dynamic bodies.
  It does not teleport, parent, freeze or secretly make held props kinematic.
  Collisions can block the grip; excessive separation releases it.
- Trees are initially static for cost. The third chop switches the authored trunk
  to dynamic authority and applies an off-centre impulse. Falling is physics;
  processing is an explicit later player action, not a falling animation.
- Construction pieces are dynamic rigid bodies joined to their support through
  normal fixed constraints. Terrain publication retains the native body identity;
  game-side support checks release connections whose excavation anchor vanished.
- Camera and held-tool visuals follow presentation poses. Tool queries and physics
  use authoritative poses. First/third person share inventory and character identity.
- The save slot captures ordinary native state and the player's bounded script
  state. No saved native handle or module-global mutable inventory is required.

## Honest boundaries

This is a compact homesteading game, not a full survival crafting framework.
There is no hunger, combat, farming, water simulation or procedural infinite world.
The excavated pool is dry. Only the marked garden can be dug: M97 is a bounded
local density volume, not planet-wide excavation. Its footprint is composed into
an actual opening in the planet collider, so a hidden solid sphere does not seal
excavations. Depth and edge limits keep the local volume closed and supported.

Tree collision uses a slender trunk box; foliage is visual. Rocks use conservative
simple colliders. Deposits are finite. Four cottages and widely spaced groves
make the whole small planet explorable; the rest of the planet is immutable.
The compact scene stays resident; it does not invent a streaming requirement.
Construction is limited to 64 pieces and one fixed support per piece. It supports
physical support loss, but does not claim calibrated material-strength failure.
Restoring or explicitly saving releases the gravity grip without changing the
object's velocity. The physical object itself persists.

## Files and authorship

- `Assets/scripts/player.js`: lifecycle, inventory state, tools, save/load policy.
- `movement.js`, `camera.js`, `gravity-gun.js`: movement, views and force grip.
- `harvest.js`, `build.js`, `inventory.js`: gathering, construction and accounting.
- `hud.js`, `Assets/ui/hud.judasui`: ordinary runtime UI.
- `Scenes/home.judas`: editable authored world; `Assets/prefabs/`: reusable pieces.
- `Authoring/WORLD_DESIGN.md`: geometry, collision and authoring constraints.
- `Assets/ART_PROVENANCE.md`: actual Claude generator workflow and asset provenance.
- `tools/generate_art.py`, `tools/generate_ui_audio.py`: original asset authoring.
- Repository `tools/CreateHearthworld.py`: reproducible scene/planet/source authoring.
- Repository `scripts/hearthworld_validate.py`: actual application and cold-process
  verification; the fixture controls player placement/input, never target physics.

Use the repository `docs/JUDASJS.md` and `docs/judas.d.ts` for public APIs.
All scripts stay ordinary JavaScript. No package manager or TypeScript runtime
is needed. Main remains the human-tested engine release branch.

`jsconfig.json` connects the scripts to the repository declarations for normal
JavaScript editor completion. The runtime-only package does not copy the repository declaration tree; use this
editable project alongside `docs/judas.d.ts` for completion. Imported model
materials use the scene’s existing linear-rendering path; this is project
configuration, not an engine modification.
