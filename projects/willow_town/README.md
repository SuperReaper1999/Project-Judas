# Willow Town

A small top-down life game: six neighbours, two shared homes, a grocer/bakery,
a carpenter's workshop, allotments and a town green. Watch people balance their
jobs against hunger, tiredness and company, or click someone and take control.
This is an editable consumer project using Judas's actual optional Utility AI,
physical queries, target leases and normal save system. No external AI service.

## Play

Open **Willow_Town.judasproj** in the current Experimental editor and press Play,
or launch it with the matching `judas` runtime. This requires the AI helpers added
after Alpha v1.1.0; the older published Alpha runtime is not sufficient.

- **Click a person**, or their name along the bottom, to see their needs, wallet,
  job, current decision and native utility scores.
- **Take control** lets you walk that person with **WASD / arrows**. Everyone else
  continues living independently. **Return to AI / Escape** hands control back.
- **Pause / Space** freezes the town's needs and movement. **Speed** switches 1x/3x.
- **Hungry / Tired / Lonely** changes the selected person's needs. These buttons
  change inputs; they do not force the engine's choice.
- **Close shop** makes food unavailable even to a hungry person. **Open shop** and
  **Restock +8** let you watch it become available again.
- **Save / K**, **Load / L** use the `willow-town` slot. Jobs, supplies, money,
  needs, physical positions and decision continuation are saved.

A larger need bar means more urgency. An unavailable score is a rejected
opportunity, not merely an unattractive one. Small score differences do not
necessarily interrupt an action; minimum commitment and switching advantage are
part of the native decision policy. Walking to work and performing work are shown
separately. Selection never changes which character owns a decision.

## Meet the neighbours

| Person | Job | Actual work effect |
| --- | --- | --- |
| Mara | Baker | Turns available grain into meals |
| Eli | Carpenter | Adds wood to the town supply |
| Nia | Gardener | Produces grain |
| Theo | Shopkeeper | Stocks another meal |
| Ada | Courier | Completes a delivery and adds supplies |
| Sam | Park keeper | Maintains the green and earns wages |

A completed work task pays **3 coins**. A meal costs **2 coins** and consumes one
unit of food. Only arriving and spending time at a destination grants its effect.
Work is attractive during the daytime shift; needs can still win. Beds reduce
fatigue, while conversations require a genuinely sensed, nearby resident.
Exclusive leases prevent two people servicing the same food counter or bed at
once. Roles are deliberately small abstractions, not a full production economy.

## Edit or extend

- **Assets/game.js**: selection, controls, responsive UI, camera, save requests.
- **Assets/town.js**: readable game policy, resident data, service effects,
  cast-checked movement and a small deterministic grid route planner.
- **Assets/citizen.ai.js**: normal editable AI definition. Change action weights,
  curves, commitment or decision intervals in the existing AI authoring window.
- **Assets/layout.js / layout.json**: authored obstacle footprints and targets.
- **Scenes/main.judas**: actual sprites, native planar bodies and destination entities.
- **Assets/hud.judasui**: ordinary project UI. The engine's AI inspector also shows
  all six named agents; this project's coordinator owns them.
- **tools/CreateWillowTown.py** and **tools/CreateWillowTownUI.py** at repository
  root regenerate the original art/world and UI. Pillow and DejaVu are authoring
  dependencies only. Regeneration replaces the generated scene/layout/UI; keep a
  copy of your edits or edit the generators before running them again.

All gameplay rules stay in project JavaScript. Physical walls and furniture block
movement. **Residents intentionally pass one another** so narrow doorways cannot
turn into a crowd deadlock; they remain physical sensing targets, and service
spots still have exclusive ownership. This is a pedestrian policy for this game,
not a change to engine collision or an assertion of crowd-dynamics simulation.
The route grid covers this authored static map. Moving a wall in the editor also
requires updating its planning footprint; real casts still veto blocked movement.
No dynamic navmesh rebuild, combat, construction, online play or full Sims-style
social simulation is claimed.

## Build, test and export

From the repository root, with the qualified Linux dependencies:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target judas judas_editor judas_export judas_willow_town_application_tests
build/judas_editor projects/willow_town/Willow_Town.judasproj
build/judas projects/willow_town/Willow_Town.judasproj
```

The native application regression uses isolated project copies and real input:

```sh
build/judas_willow_town_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-check
build/judas_willow_town_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-check cold
build/judas_export projects/willow_town/Willow_Town.judasproj /tmp/Willow-Town build/judas
```

Use a fresh test directory. Move the **whole** exported package before launching;
its runtime and exporter must have matching build provenance. The game has no
Python, ML, downloaded model or network dependency at runtime. Original asset
ownership is recorded in **ASSET_LICENSE.txt**.

## Human check

1. Watch each job, shopping, going home and conversations for a few minutes.
2. Click different people; compare changing stats with actual native scores.
3. Take control, walk through doors, try a wall, then return to autonomous AI.
4. Close the shop, raise hunger, then reopen/restock it. No free meals while closed.
5. Save during movement, quit and load in a fresh process; inspect progress.
6. Resize the window and check the town, sidebar and buttons remain readable.

Automated Linux evidence and exact fingerprints are recorded in
[Tests/QUALIFICATION.md](Tests/QUALIFICATION.md). Human visual/gameplay acceptance and fresh Windows/Android/browser
qualification are separate; they are not inferred from native automation.
