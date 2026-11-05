# Graffiti Workshop

A small editable first-person Judas game. Walk around the courtyard carrying a
spray can; your paint travels as actual Particles V2 droplets. The game places
flat paint discs from real native impact positions, normals and receiver-local
coordinates. It does not use a raycast painting shortcut.

Open `graffiti_workshop.judasproj` in Editor V2 and press Play, or run from the
engine checkout (in a packaged SDK, use `./judas` instead of `build/judas`):

```sh
build/judas projects/graffiti_workshop/graffiti_workshop.judasproj
```

| Controls | Action |
| --- | --- |
| WASD / mouse | Walk and look |
| Hold left mouse | Spray selected colour |
| 1 / 2 / 3 / 4 | Teal / pink / gold / violet |
| Space / Left Shift | Jump / run |
| F5 / F9 | Save / load your courtyard painting |
| Escape | Pause movement/spraying and release mouse; press again to resume |
| B | Hold/resume receiver delivery to demonstrate reliable backpressure |
| C | Explicitly clean paint marks; retained deposits can then finish |

The plaster wall, brick wall, concrete floor and moving canvas accept paint.
The dark steel screen on the left **blocks droplets but rejects paint**. Player
capsules are excluded by collision layer; paintable ownership is game-side tags.
Every selected colour has its own fixed-colour emitter, so changing cans cannot
recolour droplets already in flight. Try close/far spraying and angled strokes
while walking. Aim a little above a far-away target: droplets have mild gravity.

Deposited particles use `response: remove` with reliable CPU impacts. A full
native queue retains the captured hit until delivery succeeds, then consumes the
droplet once. Paint count and native queue/live counters are visible. Saving
stops new spraying, releases held delivery and waits for airborne particles and
captured work to finish. The normal save service then captures the world and
project script state. Restore reuses saved paint entities and repositions them
from saved receiver-local cells; it never sprays the old droplets again.

## Editing and limits

- `Assets/scripts/game.js`: movement, spray policy, receiver filtering, colour
  mixing, durable coating, input and UI. `MAX_MARKS`, `CELL` and emitter capacities
  define the deliberate game budget. Change these here, not in engine C++.
- `Scenes/courtyard.judas`: ordinary static/kinematic bodies, CharacterMotor,
  emitters and UI controller. Everything remains editable in the Inspector.
- `Assets/prefabs/paint.judasprefab`: reusable disc render geometry, without a
  physics body. Collisions belong to the wall/canvas, not thousands of decals.
- `Tools/build_game.py`: regenerates original procedural OBJ geometry and scene
  content, using the bundled project/UI templates and source assets. It needs
  Python only; it does not invoke an ML service or rebuild Judas.

Paint is a bounded game-owned coating, not a texture atlas, fluid film or mesh
projection system. Impact positions are grouped in 9 cm receiver-local cells;
each cell shows a 15 cm disc at its weighted contact point. Overlapping colours
mix by deposited count. Flat discs suit these planar receivers; arbitrary curved
surfaces would need a different game-owned representation. The 1,024-mark hard
budget reserves 384 slots for all four airborne pools; new spraying stops at 640
cells with an explicit HUD message. Already accepted contacts are retained, not
silently evicted. C is the player's deliberate cleanup action.

Esc pauses controls; existing physical movement and airborne droplets finish.
The moving canvas continues physically and its paint follows it. No NPCs,
missions or unrelated gameplay systems are included. Basic Water/fluid systems
are not needed; the Alpha Advanced Fluid lock is unchanged.

Original geometry/scripts are supplied. DejaVu Sans is bundled with its licence.
The small disc texture/math helper reuse Judas's existing workshop assets.

## Reproduce verification and export

From the engine checkout, with matching freshly built runtime/exporter binaries:

```sh
python3 projects/graffiti_workshop/Tests/qualify.py \
  --runtime build/judas --exporter build/judas_export \
  --output .cache/graffiti-check-NEW
```

Use a fresh output directory to preserve evidence. The test copies this project,
isolates user data, drives real gameplay inputs and runs separate OS processes
for cold loading. It exports, relocates the package, removes the original source
path and loads the saved painting using only the standalone package. No game
sources or normal user saves are changed by qualification.

```sh
build/judas_export projects/graffiti_workshop/graffiti_workshop.judasproj /tmp/GraffitiWorkshop
cd /tmp/GraffitiWorkshop
./judas
```

Linux native qualification is automated evidence, not human acceptance. New
Windows, Android and browser qualification is not claimed for this follow-up.

Human checklist: walk/jump and collide with walls; spray all four colours at
near/far/oblique surfaces while moving; paint the moving canvas; confirm steel
blocks paint; hold B, spray, resume B and check recovery; save with F5, quit,
relaunch and F9; confirm paint colours/locations remain without duplicate marks.
