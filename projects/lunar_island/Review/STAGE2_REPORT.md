# Lunar Island — Stage 2: A Place to Call Home

2026-10-26. Experimental development from Stage 1 `5424d85dbcdc8f8a603ea115d7d52d9048520b12`.
Operator authorized Stage 2 after reviewing Stage 1 screenshots. Neither Stage 1
nor Stage 2 is labelled as hands-on gameplay accepted. Main remains unchanged;
This report records the Stage 2 checkpoint before Stage 3 content. This file is the current report; Stage 1 evidence stays historical.

## Delivered game content

Two enterable cottages in the homestead, genuine window/door openings, pitched
roofs/gables and small entrance steps. 23 authored furniture/storage fixtures,
working physical door hinges, native point lamps, beds and workbenches. The
outside packing crate and indoor stores contain actual finite loot.

19 construction/furnishing blueprints: foundation, floor, wall, window wall, door,
roof, fence, gate, chest, bed, chair, table, sofa, cupboard, shelf, workbench, lamp,
rug and painting. Costs/health/extents/storage/use are project data in
Authoring/construction.json. Planks, nails and metal sheets connect gathering,
crafting, building, repair and reinforcement. Workbench recipes are station-gated.

B opens the catalogue; R rotates 90 degrees; click confirms; Escape cancels.
Queries reject unsuitable/occupied support, including the player. Walls snap to
floor edges; roofs fit from wall faces. Native prefabs spawn actual rigid bodies;
fixed joints connect them to the chosen support. Removing support releases actual
attachments and bodies fall under gravity. This is not an engineering-load solver.

E opens object actions and two-list storage. One-item/whole-stack transfers share
the same immutable inventory transaction code as the backpack. Capacity/weight,
source availability and output capacity are checked before committing either bag.
Storing equipped gear revokes its use. Storage retains identity/contents when
moved. Nonempty storage refuses dismantling. Furnishings are moved/dismantled,
not destructively damaged; hidden storage contents are never silently discarded.

Wood/metal barricades are actual collider bodies fixed across existing openings.
Tool hits change project-owned integrity and apply hit-point impulses. Repair
consumes wood/plank/sheet; reinforcement consumes sheet/nails, replaces physical
board geometry and increases strength. Separate openings retain independent state.
Dismantling refunds half the authored reusable cost, rounded down; damage refunds
nothing. Authored cottage shells are static and not demolishable in this stage.

Beds claim a persistent recovery point, heal/rest for food/water. They do not
advance an overnight clock. Lamps change native illumination. Workbenches gate
crafting. Chairs/sofas are furnishing props without seating animations.

## Ownership, art and handover

All game behaviour is ordinary project JavaScript: housing.js, construction.js,
player.js, hud.js and inventory.js. Native public entities, save references,
prefabs, ray/box queries, gravity, rigid bodies, impulses, fixed/hinge joints and
lights supply mechanisms. No hidden engine game/demo branch or game component.

Stage 2 geometry is original timber/stone architecture and nine furnishings from
Conner's existing LowPolyAssets starter pack, finished with UVs/PBR through the
verified local Prompt3D tools. Source paths/hashes, CC0 fabric/leather receipts,
licences and procedural authoring remain in ArtSource/tools. External generators,
original starter assets, independent research and unrelated projects were not
modified. This is simplified authored geometry with realistic material surfaces,
not photorealistic reconstruction. No external model downloads or SFX tool setup.

README documents controls, extending items/recipes/blueprints/fixtures/storage,
reset-tool overwrite boundaries, scripts, editor workflow and standalone export.
The Stage 2 author replaces only reserved entities 3000–5999. Normal editor saves
need no generator. Stage 1 saves keep their old identity; Stage 2 does not claim an
automatic migration. No NPCs, combat, vehicles, excavation or expanded island cities.
The resident homestead and coarse island shell retain Stage 1 geographic limitations.

## Validation and development failures

Pure gameplay: 68 passing checks (stacks, capacity/weight, exact recipe costs,
atomic failed transfers, same-container rejection and workbench gating).
13 registered scripts compile/link without error. Scene/UI and every prefab
validate through ordinary tools. Engine remains Release on Linux.

Actual Application loop: 133 checks, 0 failures. It gathers/crafts, physically fells
and processes trees, mines, consumes food/water, drops/retrieves items, switches
camera/pause, enters an opened door, loots/moves/rotates storage, claims bed,
toggles light, crafts sheets, barricades/damages/repairs/reinforces, destroys another
board, rejects occupied placement, cancels previews, builds foundation/wall/roof,
dismantles support and observes real falling bodies, stores stone in a new chest,
and opens a newly constructed physical door. No items or target health are granted
by the observer. Only player placement/aim are controlled fixture operations.

Editor autotest on an isolated project copy: native resources/jobs report zero
failures; duplicate/undo restores identical scene; Play/Stop preserves identical
authored scene; scene save succeeds. Screenshots/log retained. Human usability,
visual judgement and listened audio remain operator checks.

Genuine rejected development attempts are retained under Review/stage2: wrong
compound shape declaration, insufficient door-jamb clearance, a fixture blocked
by collision, legacy mouse injection that did not drive logical axes, overbroad
placement-distance guard, incorrect modal/test aim sequences, and audio voice-cap
feedback exceptions. Corrections use ordinary game-side APIs/valid authoring;
optional audio feedback cannot abort a committed gameplay transaction.

Cold-process validation found generic engine defects LI-SAVE-001/002. Exact failed
save, contexts, logs and reproduction scripts are retained in cold-failure and
mass-failure. Signed-zero context comparison was separately authorized by the
operator; strict typed comparison preserves nonzero checks and binary format.
Final native continuation regression: 148 checks pass, including exact custom mass
and tensor restoration, signed-zero equality, unchanged physical endpoint and
negative controls for one-ULP/velocity/type/trailing context, nonfinite/invalid
mass and authority changes. Joints: 29/29; WorldPersistence: 27/27; SaveEvolution:
43/43. The new consumer write passes 133/133 and separate cold read 42/42, including
actual restored inventory/physical endpoints, native lamp state and 120 resumed
hinge steps. A sleeping hinge's cached coordinate was not published; the final
observer measures opening from the actual body/frame geometry with the same angle
threshold. This correction to the test oracle is recorded, not counted as a physics
repair. Native simulation and collision algorithms are unchanged.

The entity participant is now version 2 with actual inverse mass/inertia. Exact
nonzero context validation remains. New-format saves pass; the old affected
version-1 input lacks separate durable mass state and remains preserved/unqualified.
See docs/PREALPHA_LUNAR_PERSISTENCE_CORRECTIONS.md for the bounded generic correction.
No engine game rules or JudasJS bindings were added.

## Measured workload

600 real rendered application frames, controlled 60-Hz fixed clock, hidden window,
vsync off, profiler enabled and dummy audio. Ryzen 5 5600, GTX 970 4 GB, Linux
Release; 299 native bodies including static terrain/fixtures and physical leaves.
Current measurement: frame mean 5.33 ms, median 5.33, p95 7.11, worst 9.80;
physics mean 0.020 ms, p95 0.029, worst 0.087; JS phase mean 0.372 ms/p95 0.382
in retained 120-frame profiler window. Resident memory about 547 MB, time peak
536548 KiB. Raw CSV/profile/performance JSON retained. These are scoped gameplay
costs, not a claim about fully populated island or human displayed FPS. More
content than Stage 1 means its older measurements are not a matched speedup test.

## Final qualification status

New-format cold continuation passes. Editor Play/Stop and relocated export evidence
are retained in Review/stage2. Linux only; no Windows/Android qualification or human
gameplay acceptance claimed. Main remains unchanged. Stage 3 was separately authorized
by the operator after this Stage 2 implementation; its results belong in a separate report.
